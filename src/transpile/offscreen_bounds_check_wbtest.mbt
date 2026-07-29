///|
test "reviewed OffscreenBoundsCheck and erase leaf select guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "if (smb_core_fast_offscreen_bounds_check()) { return; }",
    ),
    content="true",
  )
}

///|
test "mutated OffscreenBoundsCheck shape omits guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|ExtendLB: sbc #$48
    ),
    new=(
      #|ExtendLB: sbc #$47
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_offscreen_bounds_check()) { return; }",
    ),
    content="false",
  )
}

///|
test "mutated EraseEnemyObject dependency omits guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|EraseEnemyObject:
      #|      lda #$00                 ;clear all enemy object variables
      #|      sta Enemy_Flag,x
    ),
    new=(
      #|EraseEnemyObject:
      #|      lda #$00                 ;clear all enemy object variables
      #|      sta Enemy_Y_MoveForce,x
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_offscreen_bounds_check()) { return; }",
    ),
    content="false",
  )
}

///|
test "reviewed OffscreenBoundsCheck and erase fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  let mut found_bounds = false
  let mut found_erase = false

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "OffscreenBoundsCheck" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(47, 2124, 43d6a841106e9f64fa858df9f60c3eaaecf7b8417a2714e5ef90009f0e579c24)",
      )
      found_bounds = true
    }
    if subroutine.name == "EraseEnemyObject" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(9, 464, ba9a7785c5e66cf654038a209fe0122077eb5653225d965be6c159d35a708cef)",
      )
      found_erase = true
    }
  }
  inspect(found_bounds, content="true")
  inspect(found_erase, content="true")
}
