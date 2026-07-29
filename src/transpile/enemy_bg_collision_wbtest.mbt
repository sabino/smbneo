///|
test "reviewed enemy background collision shapes select the policy hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="true",
  )
}

///|
test "mutated enemy background collision shape omits the policy hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|EnemyToBGCollisionDet:
      #|      lda Enemy_State,x        ;check enemy state for d6 set
    ),
    new=(
      #|EnemyToBGCollisionDet:
      #|      lda Enemy_ID,x           ;review guard must reject this shape
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
}

///|
test "enemy background collision policy requires its exact folded leaf" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|SubtEnemyYPos:
      #|      lda Enemy_Y_Position,x  ;add 62 pixels to enemy object's
    ),
    new=(
      #|SubtEnemyYPos:
      #|      lda Enemy_State,x       ;review guard must reject this leaf
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
}

///|
test "reviewed collision parent still falls back without its leaf" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  let missing_dependency = @lower.AsmLowering::new([])
  let emitter = Emitter::new()
  let mut found = false

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "EnemyToBGCollisionDet" {
      found = true
      emitter.emit_flat_subroutine(subroutine, missing_dependency)
    }
  }
  let code_c = emitter.output.to_string()
  inspect(found, content="true")
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
  inspect(code_c.contains("lda_zpx(Enemy_State);"), content="true")
}

///|
test "reviewed enemy background collision fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  let mut parent_found = false
  let mut leaf_found = false

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "EnemyToBGCollisionDet" {
      parent_found = true
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(53, 2210, e76dff395348d7a40a928826c673bcf4974722ff28d377022712acc99f40159c)",
      )
    }
    if subroutine.name == "SubtEnemyYPos" {
      leaf_found = true
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(4, 172, 2666ec0c211c034685c9adc7f5bf61a909d6854b57b0171ad51b09fe42c3c9a0)",
      )
    }
  }
  inspect(parent_found, content="true")
  inspect(leaf_found, content="true")
}
