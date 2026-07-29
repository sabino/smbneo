///|
test "reviewed enemy collision fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  let mut parent_found = false
  let mut loop_found = false
  let mut exit_found = false
  let mut box_offset_found = false
  let mut collision_core_found = false

  for subroutine in lowering.flat_subroutines {
    match subroutine.name {
      "EnemiesCollision" => {
        parent_found = true
        inspect(
          reviewed_flat_subroutine_fingerprint(subroutine),
          content="(19, 802, 115f4bbe901738cef2465b58e2fae2edca25664c0f4b6cb24f7e99652cf78608)",
        )
      }
      "ECLoop__sub" => {
        loop_found = true
        inspect(
          reviewed_flat_subroutine_fingerprint(subroutine),
          content="(50, 1951, bf17f7430a07b3868342a23f3012b2cb4d08f15533dea3ef8dec6e1240961d4d)",
        )
      }
      "ExitECRoutine__sub" => {
        exit_found = true
        inspect(
          reviewed_flat_subroutine_fingerprint(subroutine),
          content="(1, 48, 21c18c720803c362e169821aa4bf892756a9a83ea9d90cc3452ee72f6ad07b66)",
        )
      }
      "GetEnemyBoundBoxOfs" => {
        box_offset_found = true
        inspect(
          reviewed_flat_subroutine_fingerprint(subroutine),
          content="(2, 101, d125603a5f78556dfc01f924f9ea0ffc6394bd624d78f002df4b3dfc327db2e3)",
        )
      }
      "SprObjectCollisionCore" => {
        collision_core_found = true
        inspect(
          reviewed_flat_subroutine_fingerprint(subroutine),
          content="(50, 2193, 47604e934f85083753b365240adf36bc6883afb325fb214b794a9126f45ebf02)",
        )
      }
      _ => ()
    }
  }

  inspect(parent_found, content="true")
  inspect(loop_found, content="true")
  inspect(exit_found, content="true")
  inspect(box_offset_found, content="true")
  inspect(collision_core_found, content="true")
}

///|
test "reviewed enemy collision chain selects the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let code_c = transpile_test_source(source).files[4].contents

  inspect(
    code_c.contains("if (smb_core_fast_enemies_collision()) { return; }"),
    content="true",
  )
  // The ordinary translated body remains immediately after the hook and is
  // the exact fallback whenever runtime preflight rejects a state.
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemies_collision()) { return; }\n  #endif\n  lda_zp(FrameCounter);",
    ),
    content="true",
  )
}

///|
test "mutated enemy collision parent retains generic source emission" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|        lda Enemy_ID,x
      #|        cmp #$15                    ;if enemy object => $15, branch to leave
      #|        bcs ExitECRoutine__sub
    ),
    new=(
      #|        lda Enemy_ID,x
      #|        cmp #$14                    ;review guard must reject this shape
      #|        bcs ExitECRoutine__sub
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_enemies_collision()) { return; }"),
    content="false",
  )
  inspect(
    code_c.contains("void EnemiesCollision(void) {\n  lda_zp(FrameCounter);"),
    content="true",
  )
  inspect(code_c.contains("cmp_imm(0x14);"), content="true")
}

///|
test "mutated enemy collision dependencies omit the fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let loop_old =
    #|ECLoop__sub:
    #|ECLoopBody:
    #|        stx $01                     ;save enemy object buffer offset for second enemy here
  let loop_new =
    #|ECLoop__sub:
    #|ECLoopBody:
    #|        stx $02                     ;review guard must reject this loop
  let exit_old =
    #|ExitECRoutine__sub:
    #|      ldx ObjectOffset ;get enemy object buffer offset
  let exit_new =
    #|ExitECRoutine__sub:
    #|      ldy ObjectOffset ;review guard must reject this exit
  let box_offset_old =
    #|GetEnemyBoundBoxOfs:
    #|      lda ObjectOffset         ;get enemy object buffer offset
  let box_offset_new =
    #|GetEnemyBoundBoxOfs:
    #|      lda FrameCounter         ;review guard must reject this offset helper
  let collision_core_old =
    #|SprObjectCollisionCore:
    #|      sty $06      ;save contents of Y here
  let collision_core_new =
    #|SprObjectCollisionCore:
    #|      sty $05      ;review guard must reject this geometry helper
  let mutations = [
    (loop_old, loop_new),
    (exit_old, exit_new),
    (box_offset_old, box_offset_new),
    (collision_core_old, collision_core_new),
  ]

  for mutation in mutations {
    let (old, new) = mutation
    let mutated = source.replace(old~, new~)
    inspect(mutated == source, content="false")
    let code_c = transpile_test_source(mutated).files[4].contents
    inspect(
      code_c.contains("if (smb_core_fast_enemies_collision()) { return; }"),
      content="false",
    )
    inspect(
      code_c.contains("void EnemiesCollision(void) {\n  lda_zp(FrameCounter);"),
      content="true",
    )
  }
}

///|
test "reviewed enemy collision parent falls back without dependencies" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  let missing_dependencies = @lower.AsmLowering::new([])
  let emitter = Emitter::new()
  let mut found = false

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "EnemiesCollision" {
      found = true
      emitter.emit_flat_subroutine(subroutine, missing_dependencies)
    }
  }

  let code_c = emitter.output.to_string()
  inspect(found, content="true")
  inspect(
    code_c.contains("if (smb_core_fast_enemies_collision()) { return; }"),
    content="false",
  )
  inspect(code_c.contains("lda_zp(FrameCounter);"), content="true")
}
