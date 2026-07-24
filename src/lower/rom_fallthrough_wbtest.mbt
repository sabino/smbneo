///|
fn lower_test_source(source : String) -> AsmLowering {
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = AsmLowering::new(items)
  try! lowering.lower()
  lowering
}

///|
test "ROM fall-through guards occupy storage but not logical table size" {
  let lowering = lower_test_source(
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
  let first = lowering.tables[0]
  let second = lowering.tables[1]

  inspect((first.offset, first.size, first.storage_size), content="(0, 2, 3)")
  inspect(
    (second.offset, second.size, second.storage_size),
    content="(3, 2, 2)",
  )
  inspect(lowering.table_offset, content="5")

  match first.lines[1] {
    RomFallthrough(data~, ..) =>
      match data[0] {
        Const(value~, size=Byte, ..) =>
          inspect((data.length(), value), content="(1, 134)")
        _ => fail("expected a literal byte guard")
      }
    _ => fail("expected a ROM fall-through guard")
  }
}
