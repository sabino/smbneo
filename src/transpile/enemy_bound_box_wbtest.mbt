///|
test "reviewed enemy bounding-box fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    match subroutine.name {
      "GetEnemyBoundBox"
      | "GetMaskedOffScrBits"
      | "MoveBoundBoxOffscreen__sub"
      | "SetupEOffsetFBBox"
      | "CheckRightScreenBBox" =>
        inspect(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
          content=match subroutine.name {
            "GetEnemyBoundBox" =>
              "(GetEnemyBoundBox, (4, 207, 335d9f8f67b7757417e929a8488484b2362b0c8902f315e18f8e59b761c6e392))"
            "GetMaskedOffScrBits" =>
              "(GetMaskedOffScrBits, (17, 689, 0105b1108022612d66e6e8aba4b3f44c7ee03cfcfd564445fd62e71c9f233f63))"
            "SetupEOffsetFBBox" =>
              "(SetupEOffsetFBBox, (7, 240, d859442de1e36d4cf5c862c94334b27fcefd328623825c8967bd209ef0869157))"
            "MoveBoundBoxOffscreen__sub" =>
              "(MoveBoundBoxOffscreen__sub, (9, 534, 1ef53cbfdf10020f1bcbf8b6dfe0daee8a25672ea8f8074de763c84394a98013))"
            "CheckRightScreenBBox" =>
              "(CheckRightScreenBBox, (31, 1399, c2afa622f15ba87ca0705b8bbdf530c2113ab05c980ff8ec7308c58c39eb513c))"
            _ => "placeholder"
          },
        )
      _ => ()
    }
  }
}

///|
test "reviewed enemy bounding-box chain selects the direct specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed enemy bounding-box specialization; guarded by exact caller and folded-helper shapes.",
    ),
    content="true",
  )
  inspect(
    code_c.contains("GetEnemyBoundBoxAliasExact();"),
    content="true",
  )
  inspect(
    code_c.contains("if (object_offset > 5u)"),
    content="true",
  )
  inspect(
    code_c.contains("SetupEOffsetFBBox();"),
    content="true",
  )
  inspect(
    code_c.contains("ram[EnemyBoundingBoxCoord + 3u + box_offset] = a;"),
    content="true",
  )
}

///|
test "changed enemy mask constants retain generic source emission" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|GetEnemyBoundBox:
      #|      ldy #$48                 ;store bitmask here for now
    ),
    new=(
      #|GetEnemyBoundBox:
      #|      ldy #$49                 ;store bitmask here for now
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("Reviewed enemy bounding-box specialization"),
    content="false",
  )
  inspect(
    code_c.contains("void GetEnemyBoundBox(void) {\n  ldy_imm(0x49);"),
    content="true",
  )
}

///|
test "changed masked-offscreen helper retains generic source emission" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|        lda Enemy_X_Position,x      ;get enemy object position relative
      #|        sec                         ;to the left side of the screen
      #|        sbc ScreenLeft_X_Pos
    ),
    new=(
      #|        lda Enemy_X_Position,x      ;get enemy object position relative
      #|        clc                         ;to the left side of the screen
      #|        sbc ScreenLeft_X_Pos
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("Reviewed enemy bounding-box specialization"),
    content="false",
  )
}

///|
test "changed offscreen bounding-box leaf retains generic source emission" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|      sta EnemyBoundingBoxCoord+2,y
      #|      sta EnemyBoundingBoxCoord+3,y
    ),
    new=(
      #|      sta EnemyBoundingBoxCoord+2,y
      #|      sta EnemyBoundingBoxCoord+4,y
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("Reviewed enemy bounding-box specialization"),
    content="false",
  )
}
