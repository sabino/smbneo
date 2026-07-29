///|
test "reviewed EnemyGfxHandler shape selects the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains("if (smb_core_fast_enemy_gfx_handler()) { return; }"),
    content="true",
  )
}

///|
test "mutated EnemyGfxHandler shape omits the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|EnemyGfxHandler:
      #|      lda Enemy_Y_Position,x
    ),
    new=(
      #|EnemyGfxHandler:
      #|      ldy Enemy_Y_Position,x
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_enemy_gfx_handler()) { return; }"),
    content="false",
  )
}

///|
test "mutated folded sprite-offscreen dependency omits enemy fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|SprObjectOffscrChk:
      #|         ldx ObjectOffset          ;get enemy buffer offset
    ),
    new=(
      #|SprObjectOffscrChk:
      #|         ldy ObjectOffset          ;get enemy buffer offset
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_enemy_gfx_handler()) { return; }"),
    content="false",
  )
}

///|
test "reviewed EnemyGfxHandler fingerprint is stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "EnemyGfxHandler" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(390, 19475, ea456aa396291511a0d552ad86dd35f507fbdce7cdb136792cef75142d29acce)",
      )
      return
    }
  }
  fail("EnemyGfxHandler was not lowered")
}
