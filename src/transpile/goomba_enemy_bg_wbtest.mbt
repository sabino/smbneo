///|
test "reviewed Goomba ground-collision dependency fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    match subroutine.name {
      "ChkUnderEnemy" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(ChkUnderEnemy, (3, 156, 888f64bc941830ab801f963a5cf7a091fc279a1b85c5ed9ebfc03a74308a04ae))
          ),
        )
      "BlockBufferChk_Enemy" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(BlockBufferChk_Enemy, (7, 154, 3e228109841ed43cb76c3c82398e7f64a908f86891e77bdf22f46948db63a8db))
          ),
        )
      "BBChk_E" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(BBChk_E, (3, 151, de27535c535d8b44b0b1abf271539ec0e54681009e30e25e96af140a3a598215))
          ),
        )
      "BlockBufferCollision" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(BlockBufferCollision, (37, 1425, 035f74da3dfd3e14dfa2b4351f481586b27b94ca77cad693e7b1d6f457506876))
          ),
        )
      "GetBlockBufferAddr" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content="(GetBlockBufferAddr, (13, 424, e60681deea0a9db19321cc2abddcf5a8683b1f0e81e47d68b46440f044feb865))",
        )
      "ChkForNonSolids" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(ChkForNonSolids, (10, 440, 2254054f60506fecaee94cbba8daf6d819a0d73c72638314c6f1fc2528903b1d))
          ),
        )
      "LandEnemyProperly__sub" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content="(LandEnemyProperly__sub, (71, 2922, 735e18602f717697d1b11788ba010539fec088487982e1441cc865ff621a85b4))",
        )
      "ChkForRedKoopa" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content="(ChkForRedKoopa, (18, 642, 6231c43c7a8a4c87f5eeeaccb9d6c379308a51c390422f73d2fb5f9869b362bf))",
        )
      "DoEnemySideCheck" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=(
            #|(DoEnemySideCheck, (21, 845, bac217bb5f0237959dcef9fea01c2c9e43cfdf8fcd9c8ada011acbb2747a1135))
          ),
        )
      _ => ()
    }
  }
}

///|
test "changed Goomba side-check shape omits the collision policy hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|DoEnemySideCheck:
      #|          lda Enemy_Y_Position,x     ;if enemy within status bar, branch to leave
      #|          cmp #$20                   ;because there's nothing there that impedes movement
    ),
    new=(
      #|DoEnemySideCheck:
      #|          lda Enemy_Y_Position,x     ;if enemy within status bar, branch to leave
      #|          cmp #$21                   ;review guard must reject this shape
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
}

///|
test "changed Goomba block wrapper omits the collision policy hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|      clc        ;add 1 to X to run sub with enemy offset in mind
      #|      adc #$01
      #|      tax
    ),
    new=(
      #|      clc        ;changed wrapper must retain generic emission
      #|      adc #$02
      #|      tax
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
}

///|
test "changed non-solid tile policy omits the collision policy hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|ChkForNonSolids:
      #|       cmp #$26       ;blank metatile used for vines?
    ),
    new=(
      #|ChkForNonSolids:
      #|       cmp #$27       ;review guard must reject this tile policy
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains(
      "if (smb_core_fast_enemy_to_bg_collision_det()) { return; }",
    ),
    content="false",
  )
}
