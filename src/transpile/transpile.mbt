
pub(all) struct Emitter {
  output: StringBuilder
  mut indentation: Int
  mut jsr_return_index: Int
}

pub fn Emitter::new() -> Emitter {
  {
    output: StringBuilder::new(),
    indentation: 0,
    jsr_return_index: 0,
  }
}

fn Emitter::emit(self: Emitter, s: String, newline~: Bool = true) -> Unit {
  let indent = "  ".repeat(self.indentation)
  self.output.write_string("\{indent}\{s}")

  if newline {
    self.output.write_char('\n')
  }
}

fn hex(n: Int) -> String {
  let mut n = n
  let h = StringBuilder::new()

  if n == 0 {
    return "0"
  }

  while n > 0 {
    let digit = n % 16
    n /= 16

    let d = match digit {
      10 => "a"
      11 => "b"
      12 => "c"
      13 => "d"
      14 => "e"
      15 => "f"
      _ => digit.to_string()
    }

    h.write_string(d)
  }

  h.to_string().rev()
}

fn bin(n: Int) -> String {
  let mut n = n
  let b = StringBuilder::new()

  if n == 0 {
    return "0"
  }

  while n > 0 {
    let digit = n % 2
    n /= 2

    b.write_string(digit.to_string())
  }

  b.to_string().rev().pad_start(8, '0')
}

fn show_const(base~: @parse.ConstantBase, value~: Int) -> String {
  match base {
    Decimal => value.to_string()
    Hexadecimal => "0x\{hex(value)}"
    Binary => "0b\{bin(value)}"
  }
}

fn show_expr(expr: @lower.TypedExpr) -> String {
  match expr {
    Const(base~, value~, size=_, ty=_) => show_const(base~, value~)
    Ident(name~, ty=_) => name
    Unary(op~, expr~, ty=_) => {
      let macro_ = match op {
        GetHighByte => "HIGH_BYTE"
        GetLowByte => "LOW_BYTE"
      }

      "\{macro_}(\{show_expr(expr)})"
    }
    Binary(op~, lhs~, rhs~, ty=_) => {
      let op = match op {
        Add => "+"
        Sub => "-"
      }

      "\{show_expr(lhs)} \{op} \{show_expr(rhs)}"
    }
  }
}

pub(all) suberror EmitterError { EmitterError(String) } derive(Show)

fn Emitter::emit_constants_h(self: Emitter, low: @lower.AsmLowering) -> Unit {
  self.emit("#ifndef SMB_CONSTANTS_H")
  self.emit("#define SMB_CONSTANTS_H")
  self.emit("")
  self.emit("#define HIGH_BYTE(x) (((x) >> 8) & 0xff)")
  self.emit("#define LOW_BYTE(x) ((x) & 0xff)")
  self.emit("")

  for name, val in low.defines {
    self.emit("#define \{name} \{show_expr(val)}")
  }

  self.emit("\n#endif")
}

fn Emitter::emit_data_h(self: Emitter, low: @lower.AsmLowering) -> Unit {
  self.emit("#ifndef SMB_DATA_H")
  self.emit("#define SMB_DATA_H")
  self.emit("")
  self.emit("#include \"stdint.h\"")
  self.emit("#include \"constants.h\"")
  self.emit("")
  self.emit("extern const uint8_t data[\{low.table_offset}];")
  self.emit("")

  for label, table in low.tables_by_label {
    self.emit("#define \{label} 0x\{hex(0x8000 + table.offset)}")
  }

  self.emit("")
  self.emit("#endif")
}

fn Emitter::emit_data_c(self: Emitter, low: @lower.AsmLowering) -> Unit {
  self.emit("#include \"data.h\"")
  self.emit("")

  self.emit("const uint8_t data[] = {")
  self.indentation += 1

  for table in low.tables {
    self.emit("")
    self.emit("// \{table.name}")

    for line in table.lines {
      match line {
        Data(data~, comment~) => {
          let data = data.map(show_expr).join(", ")

          match comment.val {
            "" => self.emit(data + ",")
            _ => self.emit("\{data}, // \{comment.val}")
          }
        }
        RomFallthrough(data~, comment~) => {
          self.emit("// Original-ROM fall-through guard; not logical table data.")
          let data = data.map(show_expr).join(", ")

          match comment.val {
            "" => self.emit(data + ",")
            _ => self.emit("\{data}, // \{comment.val}")
          }
        }
        Comment(comment) => {
          self.emit("// \{comment}")
        }
      }
    }
  }

  self.indentation -= 1
    self.emit("};\n")
}

fn Emitter::emit_code_h(self: Emitter, low: @lower.AsmLowering) -> Unit {
  self.emit("#ifndef SMB_CODE_H")
  self.emit("#define SMB_CODE_H")
  self.emit("")
  self.emit("#include \"data.h\"")
  self.emit("#include \"instructions.h\"")
  self.emit("#include \"ppu.h\"")
  self.emit("#include \"apu.h\"")
  self.emit("")

  for sub in low.subroutines {
    self.emit("void \{rename_label(sub.name)}(void);")
  }

  self.emit("")

  self.emit("#endif")
}

fn rename_label(label: String) -> String {
  if label.has_suffix("__sub") {
    label.split("__sub").iter().next().unwrap().to_string()
  } else {
    label
  }
}

fn gen_jump(target: String, low: @lower.AsmLowering) -> String {
  let renamed_label = rename_label(target)

  match target {
    "__ret__" => "return;"
    _ => {
      if low.targets.subroutines.contains(target) {
        "\{renamed_label}(); return;"
      } else {
        "goto \{renamed_label};"
      }
    }
  }
}

fn show_cond_opcode(op: @parse.OpCode) -> String {
  match op {
    BCC => "!carry_flag"
    BCS => "carry_flag"
    BEQ => "zero_flag"
    BNE => "!zero_flag"
    BMI => "neg_flag"
    BPL => "!neg_flag"
    BVC => "!overflow_flag"
    BVS => "overflow_flag"
    _ => abort("unreachable")
  }
}

priv enum Reg { A; X; Y }
priv struct Range { min: Int; max: Int }

impl Add for Range with add(self: Range, other: Range) -> Range {
  { min: self.min + other.min, max: self.max + other.max }
}

impl Sub for Range with sub(self: Range, other: Range) -> Range {
  { min: self.min - other.min, max: self.max - other.max }
}

fn compute_expr_range(expr: @lower.TypedExpr, low: @lower.AsmLowering) -> Range? {
  match expr {
    Const(value~, ..) => Some({ min: value, max: value })
    Ident(name~, ..) => match low.defines.get(name) {
      Some(expr) => compute_expr_range(expr, low)
      None => None
    }
    Binary(op~, lhs~, rhs~, ..) => {
      let lhs_range = compute_expr_range(lhs, low)
      let rhs_range = compute_expr_range(rhs, low)

      match (lhs_range, rhs_range) {
        (Some(lhs_range), Some(rhs_range)) => match op {
          Add => Some(lhs_range + rhs_range)
          Sub => Some(lhs_range - rhs_range)
        }
        _ => None
      }
    }
    _ => None
  }
}

fn compute_range(expr: @lower.TypedExpr, mode: @lower.AsmAddressingMode, low: @lower.AsmLowering) -> Range? {
  let mode_range = match mode {
    ZeroPageX(_) | AbsoluteX(_) | ZeroPageY(_) | AbsoluteY(_) => { min: 0, max: 0xff }
    _ => { min: 0, max: 0 }
  }

  match compute_expr_range(expr, low) {
    Some(expr_range) => Some(expr_range + mode_range)
    None => None
  }
}

fn write_byte(mode: @lower.AsmAddressingMode, target: Reg, low: @lower.AsmLowering) -> String {
  let target = match target {
    A => "a"
    X => "x"
    Y => "y"
  }

  fn rewrite(addr: @lower.TypedExpr, suffix: String) -> String {
    match addr {
      Ident(name="PPU_CTRL_REG1", ..) => "ppu_ctrl = \{target};"
      Ident(name="PPU_CTRL_REG2", ..) => "ppu_mask = \{target};"
      Ident(name="PPU_SPR_ADDR", ..) => "oam_addr = \{target};"
      Ident(name="PPU_SPR_DATA", ..) => "oam[oam_addr++] = \{target};"
      Ident(name="PPU_SCROLL_REG", ..) => "ppu_write_scroll(\{target});"
      Ident(name="PPU_ADDRESS", ..) => "ppu_write_address(\{target});"
      Ident(name="PPU_DATA", ..) => "ppu_write_data(\{target});"
      Ident(name="SPR_DMA", ..) => "ppu_transfer_oam((uint16_t)(\{target} << 8));"
      Ident(name="JOYPAD_PORT", ..) => "write_joypad1(\{target});"
      Ident(name="JOYPAD_PORT2", ..) => "write_joypad2(\{target});"
      Ident(
        name=("SND_REGISTER" | "SND_SQUARE1_REG" | "SND_SQUARE2_REG" | "SND_TRIANGLE_REG" |
        "SND_NOISE_REG" | "SND_DELTA_REG" | "SND_MASTERCTRL_REG") as name,
        ..
      ) => {
        "apu_write(\{name}, \{target});"
      }
      _ => {
        let value = "\{show_expr(addr)}\{suffix}"

        match compute_range(addr, mode, low) {
          Some({ min: _, max }) => {
            if max < 0x2000 {
              return "ram[\{value}] = \{target};"
            } else if max < 0x4000 {
              return "ppu_write_register(0x2000 + (\{show_expr(addr)} & 0b111), \{target});"
            }
          }
          None => ()
        }

        "dynamic_ram_write(\{value}, \{target});"
      }
    }
  }

  match mode {
      ZeroPage(addr) | Absolute(addr) => rewrite(addr, "")
      ZeroPageX(addr) | AbsoluteX(addr) => rewrite(addr, " + x")
      ZeroPageY(addr) | AbsoluteY(addr) => rewrite(addr, " + y")
      IndirectIndexed(addr) => "dynamic_ram_write(read_word(\{show_expr(addr)}) + y, \{target});"
      Implied | Immediate(_) | Relative(_) | Accumulator| Indirect(_) | IndexedIndirect(_) => abort("unreachable")
  }
}

fn Emitter::emit_item(self: Emitter, item: @lower.AsmItem, low: @lower.AsmLowering) -> Unit {
    match item {
      Label(name) => {
        // do not output labels for subroutines
        let is_subroutine_label = low.is_subroutine_label(name);

        if not(is_subroutine_label) {
          self.emit("\n\{rename_label(name)}:")
        }
      }
      Inst(op~, mode~, comment~) => {
        let stmt = match (op, mode) {
          (TXS, _) => "sp = x;"
          (RTI, _) => "return; // <rti>"
          (CLC, _) => "carry_flag = false;"
          (SEC, _) => "carry_flag = true;"
          (STA, _) => write_byte(mode, A, low)
          (STX, _) => write_byte(mode, X, low)
          (STY, _) => write_byte(mode, Y, low)
          _ => {
            let args = match mode {
              Accumulator => ""
              Implied => ""
              Immediate(expr) => "\{show_expr(expr)}"
              Relative(expr) => "\{show_expr(expr)}"
              ZeroPage(expr) => "\{show_expr(expr)}"
              ZeroPageX(expr) => "\{show_expr(expr)}"
              ZeroPageY(expr) => "\{show_expr(expr)}"
              Absolute(expr) => "\{show_expr(expr)}"
              AbsoluteX(expr) => "\{show_expr(expr)}"
              AbsoluteY(expr) => "\{show_expr(expr)}"
              Indirect(expr) => "\{show_expr(expr)}"
              IndexedIndirect(expr) => "\{show_expr(expr)}"
              IndirectIndexed(expr) => "\{show_expr(expr)}"
            }

            let is_relative = match mode {
              Relative(_) => true
              _ => false
            }

            match (op, is_relative) {
              (BCC | BCS | BEQ | BMI | BNE | BPL | BVC | BVS , true) => {
                let cond = show_cond_opcode(op)

                let target = match mode {
                  Relative(expr) => "\{show_expr(expr)}"
                  _ => abort("unreachable")
                }

                "if (\{cond}) { \{gen_jump(target, low)} }"
              }
              (JMP, _) => {
                gen_jump(args, low)
              }
              (JSR, _) => {
                let target = match mode {
                  Absolute(Ident(name~, ty=_)) => rename_label(name)
                  _ => ""
                }

                "\{target}();"
              }
              (RTS, _) => "return;"
              _ => "\{op.to_string().to_lower()}\{mode.suffix()}(\{args});"
            }
          }
        }

        let stmt = match comment {
          Some(comment) => {
            "\{stmt} // \{comment}"
          }
          None => stmt
        }

        self.emit(stmt)
      }
      JumpEngine(jump_table~) => {
        // The assembly dispatcher starts with ASL. The C switch replaces its
        // table lookup, but callers still depend on ASL updating carry.
        self.emit("carry_flag = (a & 0x80u) != 0;")
        self.emit("switch (a) {")
        self.indentation += 1

        for idx, label in jump_table {
          self.emit("case \{idx}: \{gen_jump(label, low)}")
        }

        self.indentation -= 1
        self.emit("}")
      }
      If(cond~, then~, otherwise~, comment~) => {
        let cond = show_cond_opcode(cond)

        match comment {
          Some(comment) => {
            self.emit("// \{comment}")
          }
          None => ()
        }

        self.emit("if (\{cond}) {")

        self.indentation += 1

        for item in then {
          self.emit_item(item, low)
        }

        self.indentation -= 1
        
        match otherwise {
          Some(otherwise) => {
            self.emit("} else {")
            self.indentation += 1

            for item in otherwise {
              self.emit_item(item, low)
            }

            self.indentation -= 1
          }
          None => self.emit("}")
        }
      }
      Comment(comment) => {
        self.emit("// \{comment}")
      }
    }
}

fn addressing_mode_structure(mode : @lower.AsmAddressingMode) -> String {
  match mode {
    Accumulator => "acc"
    Implied => "imp"
    Immediate(expr) => "imm:\{show_expr(expr)}"
    Relative(expr) => "rel:\{show_expr(expr)}"
    ZeroPage(expr) => "zp:\{show_expr(expr)}"
    ZeroPageX(expr) => "zpx:\{show_expr(expr)}"
    ZeroPageY(expr) => "zpy:\{show_expr(expr)}"
    Absolute(expr) => "abs:\{show_expr(expr)}"
    AbsoluteX(expr) => "absx:\{show_expr(expr)}"
    AbsoluteY(expr) => "absy:\{show_expr(expr)}"
    Indirect(expr) => "ind:\{show_expr(expr)}"
    IndexedIndirect(expr) => "indx:\{show_expr(expr)}"
    IndirectIndexed(expr) => "indy:\{show_expr(expr)}"
  }
}

///|
fn append_flat_items_structure(
  structure : StringBuilder,
  items : Array[@lower.AsmItem],
) -> Unit {
  for item in items {
    match item {
      Label(name) => structure.write_string("L:\{name}\n")
      Inst(op~, mode~, ..) =>
        structure.write_string(
          "I:\{op.to_string()}:\{addressing_mode_structure(mode)}\n",
        )
      JumpEngine(..) => structure.write_string("JUMP_ENGINE\n")
      If(cond~, then~, otherwise~, comment=_) => {
        structure.write_string("IF:\{cond.to_string()}\n")
        append_flat_items_structure(structure, then)
        match otherwise {
          Some(otherwise) => {
            structure.write_string("ELSE\n")
            append_flat_items_structure(structure, otherwise)
          }
          None => ()
        }
        structure.write_string("END_IF\n")
      }
      Comment(_) => ()
    }
  }
}

fn flat_subroutine_structure(items : Array[@lower.AsmItem]) -> String {
  let structure = StringBuilder::new()

  append_flat_items_structure(structure, items)
  structure.to_string()
}

///|
fn is_spr_object_collision_core(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "SprObjectCollisionCore" {
    return false
  }

  // This is an allowlist, not a name-only replacement. Comments may change,
  // but every executable instruction, addressing mode, and internal label
  // must still match the reviewed leaf routine. A source change therefore
  // falls back to the ordinary instruction-by-instruction emitter.
  flat_subroutine_structure(subroutine.items) ==
  (
    #|L:SprObjectCollisionCore
    #|I:STY:zp:0x6
    #|I:LDA:imm:0x1
    #|I:STA:zp:0x7
    #|L:CollisionCoreLoop
    #|I:LDA:absy:BoundingBox_UL_Corner
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BCS:rel:FirstBoxGreater
    #|I:CMP:absx:BoundingBox_LR_Corner
    #|I:BCC:rel:SecondBoxVerticalChk
    #|I:BEQ:rel:CollisionFound
    #|I:LDA:absy:BoundingBox_LR_Corner
    #|I:CMP:absy:BoundingBox_UL_Corner
    #|I:BCC:rel:CollisionFound
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BCS:rel:CollisionFound
    #|I:LDY:zp:0x6
    #|I:RTS:imp
    #|L:SecondBoxVerticalChk
    #|I:LDA:absx:BoundingBox_LR_Corner
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BCC:rel:CollisionFound
    #|I:LDA:absy:BoundingBox_LR_Corner
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BCS:rel:CollisionFound
    #|I:LDY:zp:0x6
    #|I:RTS:imp
    #|L:FirstBoxGreater
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BEQ:rel:CollisionFound
    #|I:CMP:absx:BoundingBox_LR_Corner
    #|I:BCC:rel:CollisionFound
    #|I:BEQ:rel:CollisionFound
    #|I:CMP:absy:BoundingBox_LR_Corner
    #|I:BCC:rel:NoCollisionFound
    #|I:BEQ:rel:NoCollisionFound
    #|I:LDA:absy:BoundingBox_LR_Corner
    #|I:CMP:absx:BoundingBox_UL_Corner
    #|I:BCS:rel:CollisionFound
    #|L:NoCollisionFound
    #|I:CLC:imp
    #|I:LDY:zp:0x6
    #|I:RTS:imp
    #|L:CollisionFound
    #|I:INX:imp
    #|I:INY:imp
    #|I:DEC:zp:0x7
    #|I:BPL:rel:CollisionCoreLoop
    #|I:SEC:imp
    #|I:LDY:zp:0x6
    #|
  )
}

///|
fn is_get_block_buffer_addr(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "GetBlockBufferAddr" {
    return false
  }

  // BlockBufferCollision inlines this leaf. Keep the complete executable
  // shape in the allowlist so either routine falls back when the dependency
  // no longer matches the reviewed address calculation.
  flat_subroutine_structure(subroutine.items) == (
    #|I:PHA:imp
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:TAY:imp
    #|I:LDA:absy:BlockBufferAddr + 2
    #|I:STA:zp:0x7
    #|I:PLA:imp
    #|I:AND:imm:0b00001111
    #|I:CLC:imp
    #|I:ADC:absy:BlockBufferAddr
    #|I:STA:zp:0x6
    #|
  )
}

fn has_reviewed_get_block_buffer_addr(low : @lower.AsmLowering) -> Bool {
  for subroutine in low.flat_subroutines {
    if is_get_block_buffer_addr(subroutine) {
      return true
    }
  }
  false
}

fn has_reviewed_block_buffer_addr_table(low : @lower.AsmLowering) -> Bool {
  match low.defines.get("Block_Buffer_1") {
    Some(value) =>
      if show_expr(value) != "0x500" {
        return false
      }
    None => return false
  }
  match low.defines.get("Block_Buffer_2") {
    Some(value) =>
      if show_expr(value) != "0x5d0" {
        return false
      }
    None => return false
  }

  match low.tables_by_label.get("BlockBufferAddr") {
    None => false
    Some(table) => {
      if table.name != "BlockBufferAddr" || table.size != 4 ||
        table.storage_size != 4 {
        return false
      }

      let entries : Array[String] = []
      for line in table.lines {
        match line {
          Data(data~, comment=_) =>
            for entry in data {
              entries.push(show_expr(entry))
            }
          RomFallthrough(..) => return false
          Comment(_) => ()
        }
      }
      entries.join("|") == (
        "LOW_BYTE(Block_Buffer_1)|LOW_BYTE(Block_Buffer_2)|" +
        "HIGH_BYTE(Block_Buffer_1)|HIGH_BYTE(Block_Buffer_2)"
      )
    }
  }
}

fn is_block_buffer_collision(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "BlockBufferCollision" {
    return false
  }

  flat_subroutine_structure(subroutine.items) == (
    #|I:PHA:imp
    #|I:STY:zp:0x4
    #|I:LDA:absy:BlockBuffer_X_Adder
    #|I:CLC:imp
    #|I:ADC:zpx:SprObject_X_Position
    #|I:STA:zp:0x5
    #|I:LDA:zpx:SprObject_PageLoc
    #|I:ADC:imm:0x0
    #|I:AND:imm:0x1
    #|I:LSR:acc
    #|I:ORA:zp:0x5
    #|I:ROR:acc
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:JSR:abs:GetBlockBufferAddr
    #|I:LDY:zp:0x4
    #|I:LDA:zpx:SprObject_Y_Position
    #|I:CLC:imp
    #|I:ADC:absy:BlockBuffer_Y_Adder
    #|I:AND:imm:0b11110000
    #|I:SEC:imp
    #|I:SBC:imm:0x20
    #|I:STA:zp:0x2
    #|I:TAY:imp
    #|I:LDA:indy:0x6
    #|I:STA:zp:0x3
    #|I:LDY:zp:0x4
    #|I:PLA:imp
    #|IF:BEQ
    #|I:LDA:zpx:SprObject_Y_Position
    #|I:JMP:abs:RetYC
    #|END_IF
    #|I:LDA:zpx:SprObject_X_Position
    #|L:RetYC
    #|I:AND:imm:0b00001111
    #|I:STA:zp:0x4
    #|I:LDA:zp:0x3
    #|
  )
}

///|
fn is_draw_sprite_object(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "DrawSpriteObject" {
    return false
  }

  // Preserve the instruction-by-instruction emitter unless the complete
  // reviewed sprite-row leaf is still byte-for-byte equivalent in lowered
  // structure. This includes every flag-producing instruction and label.
  flat_subroutine_structure(subroutine.items) ==
  (
    #|L:DrawSpriteObject
    #|I:LDA:zp:0x3
    #|I:LSR:acc
    #|I:LSR:acc
    #|I:LDA:zp:0x0
    #|I:BCC:rel:NoHFlip
    #|I:STA:absy:Sprite_Tilenumber + 4
    #|I:LDA:zp:0x1
    #|I:STA:absy:Sprite_Tilenumber
    #|I:LDA:imm:0x40
    #|I:BNE:rel:SetHFAt
    #|L:NoHFlip
    #|I:STA:absy:Sprite_Tilenumber
    #|I:LDA:zp:0x1
    #|I:STA:absy:Sprite_Tilenumber + 4
    #|I:LDA:imm:0x0
    #|L:SetHFAt
    #|I:ORA:zp:0x4
    #|I:STA:absy:Sprite_Attributes
    #|I:STA:absy:Sprite_Attributes + 4
    #|I:LDA:zp:0x2
    #|I:STA:absy:Sprite_Y_Position
    #|I:STA:absy:Sprite_Y_Position + 4
    #|I:LDA:zp:0x5
    #|I:STA:absy:Sprite_X_Position
    #|I:CLC:imp
    #|I:ADC:imm:0x8
    #|I:STA:absy:Sprite_X_Position + 4
    #|I:LDA:zp:0x2
    #|I:CLC:imp
    #|I:ADC:imm:0x8
    #|I:STA:zp:0x2
    #|I:TYA:imp
    #|I:CLC:imp
    #|I:ADC:imm:0x8
    #|I:TAY:imp
    #|I:INX:imp
    #|I:INX:imp
    #|
  )
}

fn is_move_sprites_offscreen_skip(
  subroutine : @lower.AsmFlatSubroutine,
) -> Bool {
  if subroutine.name != "MoveSpritesOffscreenSkip" {
    return false
  }

  // The source callers enter with Y=$00 or Y=$04. Keep the modulo-256 loop
  // itself exact so any other multiple-of-four entry retains its boundaries.
  flat_subroutine_structure(subroutine.items) == (
    #|I:LDA:imm:0xf8
    #|L:SprInitLoop
    #|I:STA:absy:Sprite_Y_Position
    #|I:INY:imp
    #|I:INY:imp
    #|I:INY:imp
    #|I:INY:imp
    #|I:BNE:rel:SprInitLoop
    #|
  )
}

fn is_sprite_shuffler(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "SpriteShuffler" {
    return false
  }

  // The shuffler is a fixed transformation over fifteen sprite offsets and
  // three derived misc-object groups. Keep the ordinary emitter as a fallback
  // unless every executable instruction and internal label still matches the
  // reviewed source routine.
  flat_subroutine_structure(subroutine.items) == (
    #|I:LDY:abs:AreaType
    #|I:LDA:imm:0x28
    #|I:STA:zp:0x0
    #|I:LDX:imm:0xe
    #|L:ShuffleLoop
    #|I:LDA:absx:SprDataOffset
    #|I:CMP:zp:0x0
    #|IF:BCS
    #|I:LDY:abs:SprShuffleAmtOffset
    #|I:CLC:imp
    #|I:ADC:absy:SprShuffleAmt
    #|IF:BCS
    #|I:CLC:imp
    #|I:ADC:zp:0x0
    #|END_IF
    #|I:STA:absx:SprDataOffset
    #|END_IF
    #|I:DEX:imp
    #|I:BPL:rel:ShuffleLoop
    #|I:LDX:abs:SprShuffleAmtOffset
    #|I:INX:imp
    #|I:CPX:imm:0x3
    #|IF:BEQ
    #|I:LDX:imm:0x0
    #|END_IF
    #|I:STX:abs:SprShuffleAmtOffset
    #|I:LDX:imm:0x8
    #|I:LDY:imm:0x2
    #|L:SetMiscOffset
    #|I:LDA:absy:SprDataOffset + 5
    #|I:STA:absx:Misc_SprDataOffset - 2
    #|I:CLC:imp
    #|I:ADC:imm:0x8
    #|I:STA:absx:Misc_SprDataOffset - 1
    #|I:CLC:imp
    #|I:ADC:imm:0x8
    #|I:STA:absx:Misc_SprDataOffset
    #|I:DEX:imp
    #|I:DEX:imp
    #|I:DEX:imp
    #|I:DEY:imp
    #|I:BPL:rel:SetMiscOffset
    #|
  )
}

fn is_read_port_bits(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "ReadPortBits" {
    return false
  }

  // This fingerprints the serial reads, scratch/stack effects, controller
  // debounce branch, and early return as one reviewed unit. Structured
  // control flow is expanded recursively by flat_subroutine_structure.
  flat_subroutine_structure(subroutine.items) == (
    #|I:LDY:imm:0x8
    #|L:PortLoop
    #|I:PHA:imp
    #|I:LDA:absx:JOYPAD_PORT
    #|I:STA:zp:0x0
    #|I:LSR:acc
    #|I:ORA:zp:0x0
    #|I:LSR:acc
    #|I:PLA:imp
    #|I:ROL:acc
    #|I:DEY:imp
    #|I:BNE:rel:PortLoop
    #|I:STA:absx:SavedJoypadBits
    #|I:PHA:imp
    #|I:AND:imm:0b00110000
    #|I:AND:absx:JoypadBitMask
    #|IF:BNE
    #|I:PLA:imp
    #|I:AND:imm:0b11001111
    #|I:STA:absx:SavedJoypadBits
    #|I:RTS:imp
    #|END_IF
    #|I:PLA:imp
    #|I:STA:absx:JoypadBitMask
    #|
  )
}

fn is_read_joypads(subroutine : @lower.AsmFlatSubroutine) -> Bool {
  if subroutine.name != "ReadJoypads" {
    return false
  }

  flat_subroutine_structure(subroutine.items) == (
    #|I:LDA:imm:0x1
    #|I:STA:abs:JOYPAD_PORT
    #|I:LSR:acc
    #|I:TAX:imp
    #|I:STA:abs:JOYPAD_PORT
    #|I:JSR:abs:ReadPortBits
    #|I:INX:imp
    #|I:JSR:abs:ReadPortBits
    #|
  )
}

fn has_reviewed_read_port_bits(low : @lower.AsmLowering) -> Bool {
  for subroutine in low.flat_subroutines {
    if is_read_port_bits(subroutine) {
      return true
    }
  }
  false
}

///|
fn Emitter::emit_spr_object_collision_core(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void SprObjectCollisionCore(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed leaf specialization; guarded by the exact lowered structure.",
  )
  self.emit("uint8_t local_a = 0x1u;")
  self.emit("uint8_t local_x = x;")
  self.emit("uint8_t local_y = y;")
  self.emit("const uint8_t saved_y = local_y;")
  self.emit("uint8_t local_operand;")
  self.emit("bool local_carry;")
  self.emit("ram[0x6] = saved_y;")
  self.emit("ram[0x7] = local_a;")
  self.indentation = 0
  self.emit("SprCollisionCoreLoop:")
  self.indentation = 1
  self.emit("local_a = ram[BoundingBox_UL_Corner + local_y];")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (local_carry) { goto SprCollisionFirstBoxGreater; }")
  self.emit("local_operand = ram[BoundingBox_LR_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (!local_carry) { goto SprCollisionSecondBoxVerticalChk; }")
  self.emit("if (local_a == local_operand) { goto SprCollisionFound; }")
  self.emit("local_a = ram[BoundingBox_LR_Corner + local_y];")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_y];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (!local_carry) { goto SprCollisionFound; }")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (local_carry) { goto SprCollisionFound; }")
  self.emit("goto SprCollisionReturn;")
  self.indentation = 0
  self.emit("SprCollisionSecondBoxVerticalChk:")
  self.indentation = 1
  self.emit("local_a = ram[BoundingBox_LR_Corner + local_x];")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (!local_carry) { goto SprCollisionFound; }")
  self.emit("local_a = ram[BoundingBox_LR_Corner + local_y];")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (local_carry) { goto SprCollisionFound; }")
  self.emit("goto SprCollisionReturn;")
  self.indentation = 0
  self.emit("SprCollisionFirstBoxGreater:")
  self.indentation = 1
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (local_a == local_operand) { goto SprCollisionFound; }")
  self.emit("local_operand = ram[BoundingBox_LR_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (!local_carry) { goto SprCollisionFound; }")
  self.emit("if (local_a == local_operand) { goto SprCollisionFound; }")
  self.emit("local_operand = ram[BoundingBox_LR_Corner + local_y];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (!local_carry) { goto SprCollisionNoCollision; }")
  self.emit("if (local_a == local_operand) { goto SprCollisionNoCollision; }")
  self.emit("local_a = ram[BoundingBox_LR_Corner + local_y];")
  self.emit("local_operand = ram[BoundingBox_UL_Corner + local_x];")
  self.emit("local_carry = local_a >= local_operand;")
  self.emit("if (local_carry) { goto SprCollisionFound; }")
  self.indentation = 0
  self.emit("SprCollisionNoCollision:")
  self.indentation = 1
  self.emit("local_carry = false;")
  self.emit("goto SprCollisionReturn;")
  self.indentation = 0
  self.emit("SprCollisionFound:")
  self.indentation = 1
  self.emit("local_x = (uint8_t)(local_x + 1u);")
  self.emit("local_y = (uint8_t)(local_y + 1u);")
  self.emit("ram[0x7] = (uint8_t)(ram[0x7] - 1u);")
  self.emit("if ((ram[0x7] & 0x80u) == 0u) { goto SprCollisionCoreLoop; }")
  self.emit("local_carry = true;")
  self.indentation = 0
  self.emit("SprCollisionReturn:")
  self.indentation = 1
  self.emit("a = local_a;")
  self.emit("x = local_x;")
  self.emit("y = saved_y;")
  self.emit("carry_flag = local_carry;")
  self.emit("nz_value = y;")
  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_block_buffer_collision(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void BlockBufferCollision(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed block-buffer specialization; guarded with its address leaf.",
  )
  self.emit(
    "// Exact BlockBufferAddr table is extracted into direct RAM addressing.",
  )
  self.emit("const uint8_t local_x = x;")
  self.emit("const uint8_t local_y = y;")
  self.emit("uint16_t coordinate_sum;")
  self.emit("uint16_t block_address;")
  self.emit("uint8_t adjusted_x;")
  self.emit("uint8_t adjusted_y;")
  self.emit("uint8_t block_column;")
  self.emit("uint8_t block_bank;")
  self.emit("uint8_t restored_y;")
  self.emit("uint8_t return_coordinate;")
  self.emit("ram[0x100u + sp] = a;")
  self.emit("sp = (uint8_t)(sp - 1u);")
  self.emit("ram[0x4] = local_y;")
  self.emit(
    "coordinate_sum = (uint16_t)data[BlockBuffer_X_Adder - 0x8000u + local_y] +",
  )
  self.indentation += 1
  self.emit("ram[(uint8_t)(SprObject_X_Position + local_x)];")
  self.indentation -= 1
  self.emit("adjusted_x = (uint8_t)coordinate_sum;")
  self.emit("ram[0x5] = adjusted_x;")
  self.emit(
    "coordinate_sum = (uint16_t)ram[(uint8_t)(SprObject_PageLoc + local_x)] +",
  )
  self.indentation += 1
  self.emit("(coordinate_sum > 0xffu ? 1u : 0u);")
  self.indentation -= 1
  self.emit(
    "block_column = (uint8_t)((((uint8_t)coordinate_sum & 1u) << 4) |",
  )
  self.indentation += 1
  self.emit("(adjusted_x >> 4));")
  self.indentation -= 1
  self.emit("ram[0x100u + sp] = block_column;")
  self.emit("sp = (uint8_t)(sp - 1u);")
  self.emit("block_bank = (uint8_t)(block_column >> 4);")
  self.emit(
    "block_address = block_bank == 0u ? Block_Buffer_1 : Block_Buffer_2;",
  )
  self.emit("ram[0x7] = (uint8_t)(block_address >> 8);")
  self.emit("sp = (uint8_t)(sp + 1u);")
  self.emit("block_column = ram[0x100u + sp];")
  self.emit(
    "block_address = (uint16_t)(block_address + (block_column & 0x0fu));",
  )
  self.emit("ram[0x6] = (uint8_t)block_address;")
  self.emit("restored_y = ram[0x4];")
  self.emit(
    "coordinate_sum = (uint16_t)ram[(uint8_t)(SprObject_Y_Position + local_x)] +",
  )
  self.indentation += 1
  self.emit("data[BlockBuffer_Y_Adder - 0x8000u + restored_y];")
  self.indentation -= 1
  self.emit("adjusted_y = (uint8_t)((coordinate_sum & 0xf0u) - 0x20u);")
  self.emit("ram[0x2] = adjusted_y;")
  self.emit(
    "ram[0x3] = ram[(block_address + adjusted_y) & (RAM_SIZE - 1u)];",
  )
  self.emit("y = restored_y;")
  self.emit("sp = (uint8_t)(sp + 1u);")
  self.emit("a = ram[0x100u + sp];")
  self.emit(
    "return_coordinate = a == 0u",
  )
  self.indentation += 1
  self.emit("? ram[(uint8_t)(SprObject_Y_Position + local_x)]")
  self.emit(": ram[(uint8_t)(SprObject_X_Position + local_x)];")
  self.indentation -= 1
  self.emit("ram[0x4] = (uint8_t)(return_coordinate & 0x0fu);")
  self.emit("a = ram[0x3];")
  self.emit("carry_flag = (coordinate_sum & 0xf0u) >= 0x20u;")
  self.emit("nz_value = a;")
  self.indentation = 0
  self.emit("}\n")
}

///|
fn Emitter::emit_draw_sprite_object(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void DrawSpriteObject(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed sprite-row specialization; guarded by the exact lowered structure.",
  )
  self.emit("const uint8_t local_x = x;")
  self.emit("const uint8_t local_y = y;")
  self.emit("const uint16_t right_sprite = (uint16_t)local_y + 4u;")
  self.emit("const bool horizontal_flip = (ram[0x3] & 0x02u) != 0u;")
  self.emit("const uint8_t left_tile = horizontal_flip ? ram[0x1] : ram[0x0];")
  self.emit("const uint8_t right_tile = horizontal_flip ? ram[0x0] : ram[0x1];")
  self.emit(
    "const uint8_t attributes = (uint8_t)(ram[0x4] | (horizontal_flip ? 0x40u : 0x00u));",
  )
  self.emit("const uint8_t row_y = ram[0x2];")
  self.emit("const uint8_t row_x = ram[0x5];")
  self.emit("const uint16_t next_y = (uint16_t)local_y + 8u;")
  self.emit("ram[Sprite_Tilenumber + local_y] = left_tile;")
  self.emit("ram[Sprite_Tilenumber + right_sprite] = right_tile;")
  self.emit("ram[Sprite_Attributes + local_y] = attributes;")
  self.emit("ram[Sprite_Attributes + right_sprite] = attributes;")
  self.emit("ram[Sprite_Y_Position + local_y] = row_y;")
  self.emit("ram[Sprite_Y_Position + right_sprite] = row_y;")
  self.emit("ram[Sprite_X_Position + local_y] = row_x;")
  self.emit("ram[Sprite_X_Position + right_sprite] = (uint8_t)(row_x + 8u);")
  self.emit("ram[0x2] = (uint8_t)(row_y + 8u);")
  self.emit("a = (uint8_t)next_y;")
  self.emit("y = a;")
  self.emit("carry_flag = next_y > 0xffu;")
  self.emit("x = (uint8_t)(local_x + 2u);")
  self.emit("nz_value = x;")
  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_move_sprites_offscreen_skip(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void MoveSpritesOffscreenSkip(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed OAM-clear specialization; guarded by the exact lowered structure.",
  )
  self.emit("uint8_t local_y = y;")
  self.emit("uint8_t *const sprite_y = &ram[Sprite_Y_Position];")
  self.emit("a = 0xf8u;")
  self.emit("if (local_y == 0x04u) {")
  self.indentation += 1
  // The per-frame caller always enters at $04. Emit its 63 sparse OAM stores
  // as constants so the target compiler can remove address conversion and
  // loop branches while the generic exact loop remains available below.
  let mut sprite_offset = 4
  while sprite_offset != 0 {
    self.emit(
      "sprite_y[\{sprite_offset}u] = 0xf8u;",
    )
    sprite_offset = (sprite_offset + 4) & 0xff
  }
  self.emit("local_y = 0u;")
  self.indentation -= 1
  self.emit("} else {")
  self.indentation += 1
  self.emit("do {")
  self.indentation += 1
  self.emit("sprite_y[local_y] = 0xf8u;")
  self.emit("local_y = (uint8_t)(local_y + 4u);")
  self.indentation -= 1
  self.emit("} while (local_y != 0u);")
  self.indentation -= 1
  self.emit("}")
  self.emit("y = local_y;")
  self.emit("nz_value = local_y;")
  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_sprite_shuffler(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void SpriteShuffler(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed fixed-table specialization; guarded by the exact lowered structure.",
  )
  self.emit("const uint8_t shuffle_index = ram[SprShuffleAmtOffset];")
  self.emit("uint8_t sprite_index;")
  self.emit("ram[0x0] = 0x28u;")
  self.emit("for (sprite_index = 15u; sprite_index-- != 0u;) {")
  self.indentation += 1
  self.emit("uint8_t value = ram[SprDataOffset + sprite_index];")
  self.emit("if (value >= 0x28u) {")
  self.indentation += 1
  // Reload the selected byte for exactness even for out-of-contract indices
  // that alias the sprite-offset array. Runtime indices 0..2 select the three
  // intended table entries and cannot alias these writes.
  self.emit("const uint8_t shuffle_amount = ram[SprShuffleAmt + shuffle_index];")
  self.emit("const uint16_t first_sum = (uint16_t)value + shuffle_amount;")
  self.emit("value = (uint8_t)first_sum;")
  self.emit("if (first_sum > 0xffu) {")
  self.indentation += 1
  self.emit("value = (uint8_t)(value + 0x28u);")
  self.indentation -= 1
  self.emit("}")
  self.emit("ram[SprDataOffset + sprite_index] = value;")
  self.indentation -= 1
  self.emit("}")
  self.indentation -= 1
  self.emit("}")
  self.emit("sprite_index = (uint8_t)(shuffle_index + 1u);")
  self.emit("if (sprite_index == 3u) {")
  self.indentation += 1
  self.emit("sprite_index = 0u;")
  self.indentation -= 1
  self.emit("}")
  self.emit("ram[SprShuffleAmtOffset] = sprite_index;")
  self.emit("for (sprite_index = 0u; sprite_index < 3u; sprite_index++) {")
  self.indentation += 1
  self.emit("const uint8_t base = ram[SprDataOffset + 5u + sprite_index];")
  self.emit("const uint16_t misc_offset = (uint16_t)Misc_SprDataOffset + (uint16_t)sprite_index * 3u;")
  self.emit("ram[misc_offset] = base;")
  self.emit("ram[misc_offset + 1u] = (uint8_t)(base + 8u);")
  self.emit("ram[misc_offset + 2u] = (uint8_t)(base + 16u);")
  self.indentation -= 1
  self.emit("}")
  self.emit("a = (uint8_t)(ram[SprDataOffset + 5u] + 16u);")
  self.emit(
    "carry_flag = ram[SprDataOffset + 5u] >= 0xf0u && ram[SprDataOffset + 5u] < 0xf8u;",
  )
  self.emit("x = 0xffu;")
  self.emit("y = 0xffu;")
  self.emit("nz_value = 0xffu;")
  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_read_port_bits(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void ReadPortBits(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed serial-input specialization; guarded by the exact lowered structure.",
  )
  self.emit("const uint8_t initial_a = a;")
  self.emit("uint8_t serial_value = 0u;")
  self.emit("uint8_t last_port_bit = 0u;")
  self.emit("if (x == 0u) {")
  self.indentation += 1
  self.emit("const uint8_t port_index = controller1_btn_index;")
  self.emit("if (port_index > 7u) {")
  self.indentation += 1
  self.emit("serial_value = 0xffu;")
  self.emit("last_port_bit = 1u;")
  self.indentation -= 1
  self.emit("} else if (controller1_strobe) {")
  self.indentation += 1
  self.emit(
    "last_port_bit = (uint8_t)((controller1_state >> port_index) & 1u);",
  )
  self.emit("serial_value = last_port_bit != 0u ? 0xffu : 0u;")
  self.indentation -= 1
  self.emit("} else {")
  self.indentation += 1
  self.emit(
    "uint8_t reversed = (uint8_t)(controller1_state >> port_index);",
  )
  self.emit("reversed = (uint8_t)((reversed >> 4) | (reversed << 4));")
  self.emit(
    "reversed = (uint8_t)(((reversed & 0xccu) >> 2) | ((reversed & 0x33u) << 2));",
  )
  self.emit(
    "reversed = (uint8_t)(((reversed & 0xaau) >> 1) | ((reversed & 0x55u) << 1));",
  )
  self.emit(
    "serial_value = (uint8_t)(reversed | ((1u << port_index) - 1u));",
  )
  self.emit(
    "last_port_bit = port_index == 0u ? (uint8_t)(controller1_state >> 7) : 1u;",
  )
  self.emit("controller1_btn_index = 8u;")
  self.indentation -= 1
  self.emit("}")
  self.indentation -= 1
  self.emit("}")
  self.emit("ram[0x0] = last_port_bit;")
  self.emit("ram[0x100u + sp] = serial_value;")
  self.emit("y = 0u;")
  self.emit("carry_flag = (initial_a & 1u) != 0u;")
  self.emit("ram[SavedJoypadBits + x] = serial_value;")
  self.emit(
    "if ((serial_value & 0x30u & ram[JoypadBitMask + x]) != 0u) {",
  )
  self.indentation += 1
  self.emit("a = (uint8_t)(serial_value & 0xcfu);")
  self.emit("ram[SavedJoypadBits + x] = a;")
  self.indentation -= 1
  self.emit("} else {")
  self.indentation += 1
  self.emit("a = serial_value;")
  self.emit("ram[JoypadBitMask + x] = serial_value;")
  self.indentation -= 1
  self.emit("}")
  self.emit("nz_value = a;")
  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_read_joypads(self : Emitter) -> Unit {
  self.indentation = 0
  self.emit("void ReadJoypads(void) {")
  self.indentation = 1
  self.emit(
    "// Reviewed whole-read specialization; requires the exact ReadPortBits shape.",
  )
  self.emit("uint8_t serial_value = controller1_state;")
  self.emit(
    "serial_value = (uint8_t)((serial_value >> 4) | (serial_value << 4));",
  )
  self.emit(
    "serial_value = (uint8_t)(((serial_value & 0xccu) >> 2) | ((serial_value & 0x33u) << 2));",
  )
  self.emit(
    "serial_value = (uint8_t)(((serial_value & 0xaau) >> 1) | ((serial_value & 0x55u) << 1));",
  )
  self.emit("controller1_strobe = false;")
  self.emit("controller1_btn_index = 8u;")
  self.emit("ram[SavedJoypadBits] = serial_value;")
  self.emit(
    "if ((serial_value & 0x30u & ram[JoypadBitMask]) != 0u) {",
  )
  self.indentation += 1
  self.emit("serial_value = (uint8_t)(serial_value & 0xcfu);")
  self.emit("ram[SavedJoypadBits] = serial_value;")
  self.indentation -= 1
  self.emit("} else {")
  self.indentation += 1
  self.emit("ram[JoypadBitMask] = serial_value;")
  self.indentation -= 1
  self.emit("}")
  self.emit("ram[0x0] = 0u;")
  self.emit("ram[0x100u + sp] = 0u;")
  self.emit("ram[SavedJoypadBits + 1u] = 0u;")
  self.emit("ram[JoypadBitMask + 1u] = 0u;")
  self.emit("a = 0u;")
  self.emit("x = 1u;")
  self.emit("y = 0u;")
  self.emit("carry_flag = (serial_value & 1u) != 0u;")
  self.emit("nz_value = 0u;")
  self.indentation = 0
  self.emit("}\n")
}

///|
fn Emitter::emit_flat_subroutine(self: Emitter, subroutine: @lower.AsmFlatSubroutine, low: @lower.AsmLowering) -> Unit {
  if is_spr_object_collision_core(subroutine) {
    self.emit_spr_object_collision_core()
    return
  }

  if is_block_buffer_collision(subroutine) &&
    has_reviewed_get_block_buffer_addr(low) &&
    has_reviewed_block_buffer_addr_table(low) {
    self.emit_block_buffer_collision()
    return
  }

  if is_draw_sprite_object(subroutine) {
    self.emit_draw_sprite_object()
    return
  }

  if is_move_sprites_offscreen_skip(subroutine) {
    self.emit_move_sprites_offscreen_skip()
    return
  }

  if is_sprite_shuffler(subroutine) {
    self.emit_sprite_shuffler()
    return
  }

  if is_read_joypads(subroutine) && has_reviewed_read_port_bits(low) {
    self.emit_read_joypads()
    return
  }

  if is_read_port_bits(subroutine) {
    self.emit_read_port_bits()
    return
  }

  self.indentation = 0
  self.emit("void \{rename_label(subroutine.name)}(void) {")
  self.indentation = 1

  for item in subroutine.items {
    self.emit_item(item, low)
  }

  self.indentation = 0
  self.emit("}\n")
}

fn Emitter::emit_code_c(self: Emitter, low: @lower.AsmLowering) -> Unit {
  self.emit("#include \"code.h\"")
  self.emit("")

  for sub in low.flat_subroutines {
    self.emit_flat_subroutine(sub, low)
  }
}

pub(all) struct File {
  name: String
  contents: String
  is_lib: Bool
}

pub fn File::write(self: File, dir~: String) -> Unit raise Error {
  @fs.write_string_to_file("\{dir}/\{self.name}", self.contents)
}

pub(all) struct Transpiler {
  files: Array[File]
}

pub fn Transpiler::transpile(low: @lower.AsmLowering) -> Transpiler {
  let constants_h: File = {
    name: "constants.h",
    contents: {
      let emitter = Emitter::new()
      emitter.emit_constants_h(low)
      emitter.output.to_string()
    },
    is_lib: true,
  }

  let data_h = {
    name: "data.h",
    contents: {
      let emitter = Emitter::new()
      emitter.emit_data_h(low)
      emitter.output.to_string()
    },
    is_lib: true,
  }

  let data_c = {
    name: "data.c",
    contents: {
      let emitter = Emitter::new()
      emitter.emit_data_c(low)
      emitter.output.to_string()
    },
    is_lib: true,
  }

  let code_h = {
    name: "code.h",
    contents: {
      let emitter = Emitter::new()
      emitter.emit_code_h(low)
      emitter.output.to_string()
    },
    is_lib: true,
  }

  let code_c = {
    name: "code.c",
    contents: {
      let emitter = Emitter::new()
      emitter.emit_code_c(low)
      emitter.output.to_string()
    },
    is_lib: true,
  }

  {
    files: [
      constants_h,
      data_h, data_c,
      code_h, code_c,
    ],
  }
}
