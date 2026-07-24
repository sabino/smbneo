///|
fn transpile_test_source(source : String) -> Transpiler {
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  Transpiler::transpile(lowering)
}

///|
test "data emission includes guards and shifts the next table address" {
  let transpiler = transpile_test_source(
    (
      #|FirstTable:
      #|  .db $40, $c0
      #|  .rom_fallthrough $86
      #|SecondTable:
      #|  .db $ff, $50
      #|Entry:
      #|  rts
      #|; end
      #|
    ),
  )
  let data_h = transpiler.files[1].contents
  let data_c = transpiler.files[2].contents

  inspect(data_h.contains("extern const uint8_t data[5];"), content="true")
  inspect(data_h.contains("#define FirstTable 0x8000"), content="true")
  inspect(data_h.contains("#define SecondTable 0x8003"), content="true")
  inspect(
    data_c.contains(
      "// Original-ROM fall-through guard; not logical table data.\n  0x86,",
    ),
    content="true",
  )
}
