///|
test "reviewed large-platform draw fingerprints are stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()
  let found = []

  for subroutine in lowering.flat_subroutines {
    match subroutine.name {
      "DrawLargePlatform"
      | "SixSpriteStacker"
      | "DumpSixSpr"
      | "DumpFourSpr"
      | "DumpThreeSpr"
      | "DumpTwoSpr"
      | "MoveSixSpritesOffscreen"
      | "GetXOffscreenBits"
      | "DividePDiff" =>
        found.push(
          (subroutine.name, reviewed_flat_subroutine_fingerprint(subroutine)),
        )
      _ => ()
    }
  }
  inspect(
    found,
    content="[(SixSpriteStacker, (12, 331, 145a001162a4e864f49548bfad2a7b8e7cff0950ad23fa10386294743610527e)), (MoveSixSpritesOffscreen, (2, 95, 0db34e6d8b9e5ea87703e42d38d9c476fb0a292c82e94b5e421d49c15f92d170)), (DumpSixSpr, (3, 270, c11b7e41aca5fc496efbe4c2a93750ee9b79c22df9c1b9d8d597efb6e1c4ba32)), (DumpFourSpr, (2, 157, adf79f43f70729f6968981fecff4198811bfc7724a9753f1c1bd448d862fbb36)), (DumpThreeSpr, (2, 154, 86fc3691f14a1473837f11288aa4c1587a3b4e1f28df3df4a1e00ddbae95754e)), (DumpTwoSpr, (2, 162, 43d5bd22124cdd28fae8a1c6561817bac9fcdffb07f2dc33a40a1a2299be45e3)), (DrawLargePlatform, (81, 3274, a6cee1a7c99d9894f322fe7cf12b6a008387fd960dbd57c73e1737766a30c688)), (GetXOffscreenBits, (25, 1161, 036387c1bbe23218adc7118f6976358d46af54c8078ce01e8e34e4d6eaf87823)), (DividePDiff, (12, 404, 11ce08547ce94defbe9e23f7392c7591364fc78c66fb7bef25ac222baae2f28f))]",
  )
}

///|
test "reviewed large-platform draw selects the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains("if (smb_core_fast_draw_large_platform()) { return; }"),
    content="true",
  )
}

///|
test "changed large-platform draw retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|DrawLargePlatform:
      #|      ldy Enemy_SprDataOffset,x   ;get OAM data offset
    ),
    new=(
      #|DrawLargePlatform:
      #|      lda Enemy_SprDataOffset,x   ;get OAM data offset
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_draw_large_platform()) { return; }"),
    content="false",
  )
}

///|
test "changed large-platform stacker retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|SixSpriteStacker:
      #|       ldx #$06           ;do six sprites
    ),
    new=(
      #|SixSpriteStacker:
      #|       ldx #$05           ;do six sprites
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_draw_large_platform()) { return; }"),
    content="false",
  )
}

///|
test "changed large-platform dump leaf retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|DumpSixSpr:
      #|      sta Sprite_Data+20,y      ;dump A contents
    ),
    new=(
      #|DumpSixSpr:
      #|      sta Sprite_Data+19,y      ;dump A contents
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_draw_large_platform()) { return; }"),
    content="false",
  )
}

///|
test "changed horizontal offscreen dependency retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|GetXOffscreenBits:
      #|          stx $04                     ;save position in buffer to here
    ),
    new=(
      #|GetXOffscreenBits:
      #|          sty $04                     ;save position in buffer to here
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_draw_large_platform()) { return; }"),
    content="false",
  )
}
