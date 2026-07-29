///|
test "reviewed MoveNormalEnemy shape selects the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains("if (smb_core_fast_move_normal_enemy()) { return; }"),
    content="true",
  )
}

///|
test "mutated MoveNormalEnemy shape omits the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|MoveNormalEnemy:
      #|       ldy #$00
    ),
    new=(
      #|MoveNormalEnemy:
      #|       ldy #$01
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_move_normal_enemy()) { return; }"),
    content="false",
  )
}

///|
test "reviewed MoveNormalEnemy fingerprint is stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "MoveNormalEnemy" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(66, 2406, 793acbe5404009d34891d7ac022eefe7e6e86fdc784a8a953d62fc2bb20bbc37)",
      )
      return
    }
  }
  fail("MoveNormalEnemy was not lowered")
}
