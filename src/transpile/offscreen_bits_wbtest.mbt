///|
test "reviewed offscreen shapes select table-driven specializations" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed table-driven horizontal offscreen specialization; guarded by exact lowered structures.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "Reviewed table-driven vertical offscreen specialization; guarded by exact lowered structures.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint8_t object_pixel_address = (uint8_t)(SprObject_X_Position + object_index);",
    ),
    content="true",
  )
  inspect(
    code_c.contains("table_index = defaults[side + 1u];"),
    content="true",
  )
}

///|
test "offscreen specialization requires the reviewed DividePDiff helper" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  let helper_missing = @lower.AsmLowering::new([])
  let emitter = Emitter::new()

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "GetXOffscreenBits" ||
      subroutine.name == "GetYOffscreenBits" {
      emitter.emit_flat_subroutine(subroutine, helper_missing)
    }
  }
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains("Reviewed table-driven horizontal offscreen specialization"),
    content="false",
  )
  inspect(
    code_c.contains("Reviewed table-driven vertical offscreen specialization"),
    content="false",
  )
  inspect(code_c.contains("void GetXOffscreenBits(void) {"), content="true")
  inspect(code_c.contains("void GetYOffscreenBits(void) {"), content="true")
  inspect(code_c.contains("lda_absy(ScreenEdge_X_Pos);"), content="true")
  inspect(code_c.contains("lda_absy(HighPosUnitData);"), content="true")
}

///|
test "changed offscreen routine shapes fall back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed_x : @lower.AsmFlatSubroutine = {
    name: "GetXOffscreenBits",
    items: [],
  }
  let unreviewed_y : @lower.AsmFlatSubroutine = {
    name: "GetYOffscreenBits",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed_x, lowering)
  emitter.emit_flat_subroutine(unreviewed_y, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains("Reviewed table-driven horizontal offscreen specialization"),
    content="false",
  )
  inspect(
    code_c.contains("Reviewed table-driven vertical offscreen specialization"),
    content="false",
  )
  inspect(
    code_c.contains("void GetXOffscreenBits(void) {\n}\n"),
    content="true",
  )
  inspect(
    code_c.contains("void GetYOffscreenBits(void) {\n}\n"),
    content="true",
  )
}
