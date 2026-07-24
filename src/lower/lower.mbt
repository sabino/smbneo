
pub(all) struct AsmLowering {
  items: Array[@parse.SourceItem]
  lowered: Array[AsmItem]
  defines: Map[String, TypedExpr]
  jump_target_labels: Set[String]
  tables: Array[AsmTable]
  tables_by_label: Map[String, AsmTable]
  mut table_offset: Int
  targets: JumpTargets
  basic_blocks: Array[BasicBlock]
  bb_by_label: Map[String, BasicBlock]
  subroutines: Array[AsmSubroutine]
  flat_subroutines: Array[AsmFlatSubroutine]
} derive (Show)

pub suberror LoweringError { LoweringError(String) } derive(Show)

pub fn AsmLowering::new(items: Array[@parse.SourceItem]) -> AsmLowering {
  {
    items,
    lowered: [],
    defines: {},
    tables: [],
    table_offset: 0, // in bytes
    jump_target_labels: Set::new(),
    tables_by_label: {},
    targets: { jump_targets: Set::new(), subroutines: Set::new(), externals: Set::new() },
    basic_blocks: [],
    bb_by_label: {},
    subroutines: [],
    flat_subroutines: [],
  }
}

fn AsmLowering::detect_table(self: AsmLowering, name: String, index: Int) -> AsmTable? raise LoweringError {
  let lines: Array[AsmTableLine] = []
  let mut elem_size: @parse.ConstantSize? = None
  let mut length = 0
  let mut fallthrough_length = 0
  let mut has_fallthrough = false
  let mut i = index + 1

  while i < self.items.length() {
    match self.items[i] {
      Directive(name="db" | "dw" as name, args~) => {
        if has_fallthrough {
          raise LoweringError(
            "ROM fall-through bytes must follow all logical table data",
          )
        }

        let directive_elem_size: @parse.ConstantSize = if name == "db" { Byte } else { Word }

        match elem_size {
          Some(size) => {
            if size != directive_elem_size {
              raise LoweringError("Inconsistent element size in table")
            }
          }
          None => elem_size = {
            Some(directive_elem_size)
          }
        }

        length += args.length()

        let data = []

        for arg in args {
          data.push(self.type_expr(arg))
        }

        lines.push(Data(data~, comment=Ref::new("")))
      }
      Directive(name="rom_fallthrough", args~) => {
        if args.is_empty() {
          raise LoweringError(
            "ROM fall-through guards require at least one literal byte",
          )
        }

        match elem_size {
          Some(Byte) => ()
          Some(Word) => {
            raise LoweringError(
              "ROM fall-through guards are only supported on byte tables",
            )
          }
          None => {
            raise LoweringError(
              "ROM fall-through guard must follow logical table data",
            )
          }
        }

        let data = []

        for arg in args {
          let typed = self.type_expr(arg)

          match typed {
            Const(value~, size=Byte, ..) if value >= 0 && value <= 0xff => {
              data.push(typed)
            }
            _ => {
              raise LoweringError(
                "ROM fall-through guards require literal byte values",
              )
            }
          }
        }

        fallthrough_length += data.length()
        has_fallthrough = true
        lines.push(RomFallthrough(data~, comment=Ref::new("")))
      }
      Comment(comment~, is_inline~) => {
        if is_inline {
          match lines.last() {
            Some(
              Data(data=_, comment=comment_ref)
              | RomFallthrough(data=_, comment=comment_ref)
            ) => {
              comment_ref.val = comment
            }
            _ => ()
          }
        } else {
          lines.push(Comment(comment))
        }
      }
      _ => break
    }

    i += 1
  }

  match elem_size {
    Some(elem_size) => {
      Some({
        name,
        offset: self.table_offset,
        size: length * elem_size.byte_size(),
        storage_size: length * elem_size.byte_size() + fallthrough_length,
        lines,
        elem_size,
        items_count: i - index,
      })
    }
    None => None
  }
}

fn AsmLowering::collect_labels_and_tables(self: AsmLowering) -> Unit raise LoweringError {
  let mut idx = 0

  while idx < self.items.length() {
    let item = self.items[idx]
    let mut idx_increment = 1

    match item {
      Label(name) => {
        let is_alias = match name {
          "GameText" | "AreaAddrOffsets" => true
          _ => false
        }

        match self.detect_table(name, idx + (if is_alias { 1 } else { 0 })) {
          Some(table) => {
            self.tables.push(table)
            self.table_offset += table.storage_size
            self.tables_by_label[table.name] = table

            if is_alias {
              let alias_ = match self.items[idx + 1] {
                Label(name) => name
                _ => raise LoweringError("Expected alias label")
              }

              self.tables_by_label[alias_] = table
            }

            idx_increment = table.items_count
          }
          None => {
            self.jump_target_labels.add(name)
          }
        }
      }
      _ => ()
    }

    idx += idx_increment
  }
}

fn AsmLowering::type_expr(self: AsmLowering, expr: @parse.SourceExpr) -> TypedExpr raise LoweringError {
  match expr {
    Const(base~, value~, size~) => {
      let ty = match size {
        Byte => ConstTy(Byte)
        Word => ConstTy(Word)
      }

      Const(base~, value~, size~, ty~)
    }
    Ident(name) => {
      match self.tables_by_label.get(name) {
        Some(table) => Ident(name~, ty=ArrayTy(table.elem_size))
        None => match self.defines.get(name) {
          Some(expr) => Ident(name~, ty=expr.ty())
          None => Ident(name~, ty=LabelTy)
        }
      }
    }
    Unary(op~, expr~) => {
      let expr = self.type_expr(expr)
      Unary(op~, expr~, ty=expr.ty())
    }
    Binary(op~, lhs~, rhs~) => {
      let lhs = self.type_expr(lhs)
      let rhs = self.type_expr(rhs)
      Binary(op~, lhs~, rhs~, ty=lhs.ty())
    }
  }
}

fn AsmLowering::collect_defines(self: AsmLowering) -> Unit raise LoweringError {
  for item in self.items {
    match item {
      Define(name~, rhs~) => {
        self.defines[name] = self.type_expr(rhs)
      }
      _ => ()
    }
  }
}

fn AsmLowering::collect_instructions(self: AsmLowering) -> Unit raise LoweringError {
  let mut inside_table = false
  let mut item_idx = 0

  while item_idx < self.items.length() {
    let item = self.items[item_idx]
    let mut idx_increment = 1

    match item {
      Label(name) => {
        if not(self.tables_by_label.contains(name)) {
          // this label is a jump target if it's not a table label
          
          self.lowered.push(Label(name))
          inside_table = false
        } else {
          inside_table = true
        }
      }
      Inst(op~, mode~) => {
        if op == JSR && (match mode { AbsoluteOrZeroPage(addr=Ident("JumpEngine")) => true; _ => false }) {
          let jump_table: Array[String] = []
          inside_table = true

          for i in (item_idx + 1)..<self.items.length() {
            match self.items[i] {
              Directive(name="dw", args~) => {
                for arg in args {
                  match arg {
                    Ident(label) => {
                      if not(self.jump_target_labels.contains(label)) {
                        raise LoweringError("Unknown label in jump table")
                      }

                      jump_table.push(label)
                    }
                    _ => raise LoweringError("Expected label in jump table")
                  }
                }
              }
              Comment(_) => ()
              _ => {
                idx_increment = i - item_idx
                break
              }
            }
          }

          self.lowered.push(JumpEngine(jump_table~))
        } else {
          let addr_mode: AsmAddressingMode = match mode {
            Accumulator => Accumulator
            Implied => Implied
            Immediate(operand) => Immediate(self.type_expr(operand))
            Relative(addr~) => Relative(self.type_expr(addr))
            AbsoluteOrZeroPage(addr~) | AbsoluteOrZeroPageX(addr~) | AbsoluteOrZeroPageY(addr~) => {
              let addr = self.type_expr(addr)
              let size: @parse.ConstantSize = match addr.ty() {
                ConstTy(size) => size
                ArrayTy(_) | LabelTy => Word
              }

              match (mode, size) {
                (AbsoluteOrZeroPage(_), Byte) => ZeroPage(addr)
                (AbsoluteOrZeroPage(_), Word) => Absolute(addr)
                (AbsoluteOrZeroPageX(_), Byte) => ZeroPageX(addr)
                (AbsoluteOrZeroPageX(_), Word) => AbsoluteX(addr)
                (AbsoluteOrZeroPageY(_), Byte) => ZeroPageY(addr)
                (AbsoluteOrZeroPageY(_), Word) => AbsoluteY(addr)
                _ => raise LoweringError("Invalid addressing mode")
              }
            }
            Indirect(addr~) => Indirect(self.type_expr(addr))
            IndexedIndirect(addr~) => IndexedIndirect(self.type_expr(addr))
            IndirectIndexed(addr~) => IndirectIndexed(self.type_expr(addr))
          }

          let comment = match self.items[item_idx + 1] {
            Comment(comment~, is_inline=true) => Some(comment)
            _ => None
          }

          self.lowered.push(Inst(op~, mode=addr_mode, comment~))
        }
      }
      Comment(comment~, is_inline=false) => {
        if not(inside_table) {
          self.lowered.push(Comment(comment))
        }
      }
      Directive(name="db" | "dw" as name, args~) => {
        if not(inside_table) {
          raise LoweringError("Raw data in code detected: \{name} \{args}")
        }
      }
      Directive(name="rom_fallthrough", args~) => {
        if not(inside_table) {
          raise LoweringError(
            "ROM fall-through guard outside table detected: \{args}",
          )
        }
      }
      _ => ()
    }

    item_idx += idx_increment
  }
}

pub(all) struct JumpTargets {
  jump_targets: Set[String]
  subroutines: Set[String]
  externals: Set[String]
} derive (Show)

fn AsmLowering::collect_jump_targets(self: AsmLowering) -> Unit {
  for item in self.lowered {
    match item {
      Inst(
        op=JMP | JSR | BCC | BCS | BEQ | BMI | BNE | BPL | BVC | BVS as op,
        mode=Relative(Ident(name=label, ..)) | Absolute(Ident(name=label, ..)),
        ..
      ) => {
        if op == JSR {
          self.targets.subroutines.add(label)
        } else {
          self.targets.jump_targets.add(label)
        }
      }
      JumpEngine(jump_table~) => {
        for label in jump_table {
          self.targets.subroutines.add(label)
        }
      }
      Label(name) => {
        // manually annotated subroutines
        if name.has_suffix("__sub") {
          self.targets.subroutines.add(name)
        }
      }
      _ => ()
    }
  }
}

fn JumpTargets::contains(self: JumpTargets, label: String) -> Bool {
  self.jump_targets.contains(label) || self.subroutines.contains(label)
}

fn AsmLowering::next_non_comment_item_index(self: AsmLowering, start: Int) -> Int? {
  for i in start..<self.lowered.length() {
    match self.lowered[i] {
      Comment(_) => ()
      _ => return Some(i)
    }
  }

  None
}

fn AsmLowering::collect_basic_block_leaders(self: AsmLowering) -> Array[Int] {
  self.collect_jump_targets()
  let leader_indices = []

  fn insert_leader_index(idx: Int?) {
    match idx {
      Some(idx) => {
        // avoid duplicates
        if leader_indices.is_empty() || leader_indices[leader_indices.length() - 1] != idx {
          leader_indices.push(idx)
        }
      }
      None => {
        leader_indices.push(self.lowered.length() - 1)
      }
    }
  }

  // the first non-comment instruction is always a leader
  insert_leader_index(self.next_non_comment_item_index(0))
  
  for i in 0..<self.lowered.length() {
    match self.lowered[i] {
      // the target of a jump instruction is a leader
      Label(label) => {
        if self.targets.contains(label) {
          insert_leader_index(Some(i))
        }
      }
      Inst(op~, ..) => {
        if op.is_branching() {
          // the next non-comment instruction after a branching instruction is a leader
          insert_leader_index(self.next_non_comment_item_index(i + 1))
        }
      }
      JumpEngine(_) => insert_leader_index(self.next_non_comment_item_index(i + 1))
      _ => ()
    }
  }

  leader_indices
}

priv struct ItemInfo {
  item: AsmItem
  index: Int
}

fn BasicBlock::last_non_comment_item(self: BasicBlock) -> ItemInfo? {
  let mut i = self.items.length() - 1

  while i >= 0 {
    match self.items[i] {
      Comment(_) => ()
      item => return Some({ item, index: i })
    }

    i -= 1
  }

  None
}

fn BasicBlock::has_fallthrough(self: BasicBlock) -> Bool raise LoweringError {
  let last_item = self.last_non_comment_item().unwrap_or_error(LoweringError("Empty basic block"))

  match last_item.item {
    Inst(op~, ..) => {
      match op {
        RTI | RTS | BRK | JMP => false
        _ => true
      }
    }
    _ => false
  }
}

fn AsmLowering::collect_basic_blocks(self: AsmLowering) -> Unit raise LoweringError {
  let leader_indices = self.collect_basic_block_leaders()

  // Starting from a leader, the set of all following instructions until and not including the next leader
  // is the basic block corresponding to the starting leader.
  for i in 1..<leader_indices.length() {
    let prev_leader = leader_indices[i - 1]
    let curr_leader = leader_indices[i]

    let bb = {
      id: self.basic_blocks.length(),
      name: None,
      items: [],
      predecessors: [],
      successors: [],
      is_subroutine_entry: false,
      subroutine: None,
    }

    for j in prev_leader..<curr_leader {
      match self.lowered[j] {
        Label(label) => {
          bb.name = Some(label)
          self.bb_by_label[label] = bb
        }
        _ => ()
      }

      bb.items.push(self.lowered[j])
    }

    self.basic_blocks.push(bb)
  }

  fn connect(a: BasicBlock, b: BasicBlock, is_fallthrough: Bool) -> Unit {
    if not(a.successors.iter().any(fn(link) { link.bb.id == b.id })) {
      a.successors.push({ bb: b, is_fallthrough })
    }

    if not(b.predecessors.iter().any(fn(link) { link.bb.id == a.id })) {
      b.predecessors.push({ bb: a, is_fallthrough })
    }
  }

  // connect basic blocks
  for bb in self.basic_blocks {
    let last_item = bb.last_non_comment_item().unwrap_or_error(LoweringError("Empty basic block"))

    match last_item.item {
      Inst(
        op=JMP | BCC | BCS | BEQ | BMI | BNE | BPL | BVC | BVS,
        mode=Relative(Ident(name=label, ..)) | Absolute(Ident(name=label, ..)),
        ..
      ) => {
        match self.bb_by_label.get(label) {
          Some(target_bb) => {
            connect(bb, target_bb, false)
          }
          None => {
            if not(label.has_prefix("__")) { // allow special labels
              raise LoweringError("Unknown jump target: \{label}")
            }
          }
        }
      }
      JumpEngine(jump_table~) => {
        for label in jump_table {
          match self.bb_by_label.get(label) {
            Some(target_bb) => {
              connect(bb, target_bb, false)
            }
            None => {
              if not(label.has_prefix("__")) { // allow special labels
                raise LoweringError("Unknown jump target: \{label}")
              }
            }
          }
        }
      }
      _ => ()
    }

    // check for fallthrough to the next basic block
    if bb.has_fallthrough() && bb.id + 1 < self.basic_blocks.length() {
      connect(bb, self.basic_blocks[bb.id + 1], true)
    }
  }
}

fn AsmLowering::rewrite_bb(self: AsmLowering, bb: BasicBlock, visited: Set[Int]) -> Unit raise LoweringError {
  if visited.contains(bb.id) {
    return
  }

  match bb.name {
    Some(name) => {
      if self.is_subroutine_label(name) {
        let sub: AsmSubroutine = {
          name,
          blocks: [bb],
          entry: bb,
          exit: bb,
          internal_labels: Set::new(),
        }

        let mut i = bb.id + 1

        while i < self.basic_blocks.length() {
          let next_bb = self.basic_blocks[i]

          // if this is an exit bb, or if it's a subroutine entry, we're done for this function
          if next_bb.predecessors.is_empty() || next_bb.is_subroutine_entry {
            break
          }

          match next_bb.name {
            Some(next_name) => {
              if self.is_subroutine_label(next_name) {
                // insert fallthrough call to the next subroutine
                let prev_bb = sub.blocks[sub.blocks.length() - 1]
                if prev_bb.successors.length() > 0 && prev_bb.has_fallthrough() {
                  prev_bb.items.push(Inst(op=JSR, mode=Absolute(Ident(name=next_name, ty=LabelTy)), comment=Some("<fallthrough>")))
                  prev_bb.items.push(Inst(op=RTS, mode=Implied, comment=None))
                }
                break
              }
            }
            None => ()
          }

          sub.blocks.push(next_bb)
          visited.add(next_bb.id)
          i += 1
        }

        sub.exit = sub.blocks[sub.blocks.length() - 1]
        self.subroutines.push(sub)
      }
    }
    None => ()
  }
}

pub fn AsmLowering::is_subroutine_label(self: AsmLowering, label: String) -> Bool {
  self.targets.subroutines.contains(label) || label.has_suffix("__sub")
}

fn AsmLowering::collect_subroutines(self: AsmLowering) -> Unit raise LoweringError {
  let components = self.collect_connected_components()
  let visited = Set::new()

  for component in components {
    for bb in component {
      let _ = self.rewrite_bb(bb,visited)
    }
  }
}

fn BasicBlock::dfs(self: BasicBlock, visited: Set[Int]) -> Array[BasicBlock] {
  let stack = [self]
  let blocks = []

  while not(stack.is_empty()) {
    let bb = stack.pop().unwrap()

    if not(visited.contains(bb.id)) {
      visited.add(bb.id)
      blocks.push(bb)

      for link in bb.successors {
        stack.push(link.bb)
      }
    }
  }

  blocks.sort_by((a, b) => a.id - b.id)
  blocks
}

fn AsmLowering::collect_connected_components(self: AsmLowering) -> Array[Array[BasicBlock]] {
  let components = []
  let visited = Set::new()

  for bb in self.basic_blocks {
    if not(visited.contains(bb.id)) {
      let blocks = bb.dfs(visited)
      components.push(blocks)
    }
  }

  components
}

fn AsmLowering::remove_unused_rts(self: AsmLowering) -> Unit raise LoweringError {
  for sub in self.subroutines {
    let last_bb = sub.exit
    let last_item = last_bb.last_non_comment_item().unwrap_or_error(LoweringError("Empty basic block"))

    match last_item.item {
      Inst(op=RTS, ..) => {
        let _ = sub.exit.items.remove(last_item.index)
      }
      _ => ()
    }
  }
}

fn  AsmLowering::_cfg_dotfile(self: AsmLowering) -> String {
  let builder = StringBuilder::new()

  builder.write_string("digraph G {\n")

  for bb in self.basic_blocks {
    let label = match bb.name {
      Some(name) => name
      None => bb.id.to_string()
    }

    builder.write_string("  \{bb.id} [label=\"\{label}\"]\n")

    for link in bb.successors {
      builder.write_string("  \{bb.id} -> \{link.bb.id}\n")
    }
  }

  builder.write_string("}\n")

  builder.to_string()
}

priv struct ControlFlowStackEntry {
  exit_label: String
  cond: @parse.OpCode
  body: Array[AsmItem]
  comment: String?
}

fn negate(cond: @parse.OpCode) -> @parse.OpCode raise LoweringError {
  match cond {
    BCS => BCC
    BCC => BCS
    BEQ => BNE
    BNE => BEQ
    BPL => BMI
    BMI => BPL
    _ => raise LoweringError("Invalid condition")
  }
}

fn AsmLowering::recognize_control_flow(self: AsmLowering, sub: AsmSubroutine) -> AsmFlatSubroutine raise LoweringError {
  let items = sub.blocks.map(fn(bb) { bb.items }).flatten()
  let labels = Set::new()
  let new_items = []
  let stack: Array[ControlFlowStackEntry] = []
  let loop_labels = Set::new()
  let unconditional_jump_labels = Set::new()

  // detect backward jumps
  for item in items {
    match item {
      Label(name) => {
        labels.add(name)
      }
      Inst(op=JMP, mode=Relative(Ident(name=label, ..)) | Absolute(Ident(name=label, ..)), ..) => {
        unconditional_jump_labels.add(label)
      }
      Inst(
        op=BCC | BCS | BEQ | BMI | BNE | BPL | BVC | BVS,
        mode=Relative(Ident(name=label, ..)) | Absolute(Ident(name=label, ..)),
        ..
      ) => {
        if labels.contains(label) {
          loop_labels.add(label)
        }
      }
      _ => ()
    }
  }

  fn push_item(item: AsmItem) {
    match stack.last() {
      Some(entry) => {
        entry.body.push(item)
      }
      None => {
        new_items.push(item)
      }
    }
  }

  let mut is_simple_control_flow = true
  
  for item in items {
    match item {
      Label(name) => {
        while true {
          match stack.last() {
            Some(entry) => {
              if entry.exit_label == name {
                let _ = stack.pop()

                push_item(AsmItem::If(
                  cond=negate(entry.cond),
                  then=entry.body,
                  otherwise=None,
                  comment=entry.comment,
                ))
              } else {
                break
              }
            }
            None => break
          }
        }

        // if an old control flow block is closed
        // before the current one ends
        // then we failed to recognize the control flow
        if stack.iter().any(fn(entry) { entry.exit_label == name }) {
          is_simple_control_flow = false
          break
        }

        if loop_labels.contains(name) || unconditional_jump_labels.contains(name) {
          push_item(item)
        } else if name != sub.name {
          push_item(AsmItem::Comment("\{name}:"))
        }
      }
      Inst(
        op=BCC | BCS | BEQ | BMI | BNE | BPL | BVC | BVS as cond,
        mode=Relative(Ident(name=label, ..)) | Absolute(Ident(name=label, ..)),
        comment~,
      ) => {
        // if this is a subroutine call
        if self.targets.subroutines.contains(label) {
          push_item(If(
            cond=cond,
            then=[
              Inst(op=JSR, mode=Absolute(Ident(name=label, ty=LabelTy)), comment=None),
              Inst(op=RTS, mode=Implied, comment=None),
            ],
            otherwise=None,
            comment=comment,
          ))
        } else if not(loop_labels.contains(label)) {
        // if this is not a loop
          let entry = {
            exit_label: label,
            cond,
            body: [],
            comment,
          }

          stack.push(entry)
        } else {
          push_item(item)
        }
      }
      _ => {
        push_item(item)
      }
    }
  }

  if stack.length() < 2 {
    match stack.last() {
      Some(entry) => {
        new_items.push(
          AsmItem::If(
            cond=negate(entry.cond),
            then=entry.body,
            otherwise=None,
            comment=None,
          )
        )
        let _ = stack.pop()
      }
      None => ()
    }
  } else {
    // some control flow blocks are not closed
    is_simple_control_flow = false
  }

  if is_simple_control_flow {
    { name: sub.name, items: new_items }
  } else {
    { name: sub.name, items: items }
  }
}

fn AsmLowering::collect_flat_subroutines(self: AsmLowering) -> Unit raise LoweringError {
  for sub in self.subroutines {
    self.flat_subroutines.push(self.recognize_control_flow(sub))
  }
}

pub fn AsmLowering::lower(self: AsmLowering) -> Unit raise LoweringError {
  self.collect_labels_and_tables()
  self.collect_defines()
  self.collect_instructions()
  self.collect_basic_blocks()
  self.collect_subroutines()
  self.remove_unused_rts()
  self.collect_flat_subroutines()

  // @fs.write_string_to_file(path="cfg.dot", content=self.cfg_dotfile())
}
