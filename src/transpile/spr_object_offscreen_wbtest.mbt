///|
fn lower_spr_object_offscreen_source() -> @lower.AsmLowering {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  lowering
}

///|
fn emit_spr_object_offscreen_from(
  subroutine : @lower.AsmFlatSubroutine,
  lowering : @lower.AsmLowering,
) -> String {
  let emitter = Emitter::new()
  emitter.emit_flat_subroutine(subroutine, lowering)
  emitter.output.to_string()
}

///|
test "reviewed sprite-offscreen shape and dependencies emit guarded hook" {
  let lowering = lower_spr_object_offscreen_source()
  let mut routine : @lower.AsmFlatSubroutine? = None

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "SprObjectOffscrChk" {
      routine = Some(subroutine)
      inspect(is_reviewed_spr_object_offscr_chk(subroutine), content="true")
    }
    if subroutine.name == "MoveESprColOffscreen" ||
      subroutine.name == "MoveColOffscreen" {
      inspect(
        is_reviewed_spr_object_offscr_dependency(subroutine),
        content="true",
      )
    }
  }
  inspect(
    has_reviewed_spr_object_offscr_dependencies(lowering),
    content="true",
  )
  let code_c = emit_spr_object_offscreen_from(routine.unwrap(), lowering)
  inspect(
    code_c.contains(
      "if (smb_core_fast_spr_object_offscr_chk()) { return; }",
    ),
    content="true",
  )
  inspect(code_c.contains("ldx_zp(ObjectOffset);"), content="true")
}

///|
test "changed sprite-offscreen shape falls back without hook" {
  let lowering = lower_spr_object_offscreen_source()
  let changed : @lower.AsmFlatSubroutine = {
    name: "SprObjectOffscrChk",
    items: [],
  }
  let code_c = emit_spr_object_offscreen_from(changed, lowering)

  inspect(is_reviewed_spr_object_offscr_chk(changed), content="false")
  inspect(
    code_c.contains("smb_core_fast_spr_object_offscr_chk"),
    content="false",
  )
  inspect(code_c.contains("void SprObjectOffscrChk(void) {\n}"), content="true")
}

///|
test "missing folded dependency falls back without hook" {
  let complete = lower_spr_object_offscreen_source()
  let missing_column = @lower.AsmLowering::new([])
  let missing_move_column = @lower.AsmLowering::new([])
  let mut routine : @lower.AsmFlatSubroutine? = None

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "SprObjectOffscrChk" {
      routine = Some(subroutine)
    }
    if subroutine.name == "MoveESprColOffscreen" {
      missing_move_column.flat_subroutines.push(subroutine)
    }
    if subroutine.name == "MoveColOffscreen" {
      missing_column.flat_subroutines.push(subroutine)
    }
  }
  inspect(
    has_reviewed_spr_object_offscr_dependencies(missing_column),
    content="false",
  )
  inspect(
    has_reviewed_spr_object_offscr_dependencies(missing_move_column),
    content="false",
  )
  inspect(
    emit_spr_object_offscreen_from(routine.unwrap(), missing_column).contains(
      "smb_core_fast_spr_object_offscr_chk",
    ),
    content="false",
  )
  inspect(
    emit_spr_object_offscreen_from(
      routine.unwrap(),
      missing_move_column,
    ).contains("smb_core_fast_spr_object_offscr_chk"),
    content="false",
  )
}
