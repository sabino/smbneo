///|
test "reviewed RenderAreaGraphics fingerprint is stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "RenderAreaGraphics" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(96, 4282, 08b5eacf08fd9a086b2544ed9b86601dce325ce1411bfd12e41d3c6c02b2393d)",
      )
      return
    }
  }
  fail("RenderAreaGraphics was not lowered")
}

///|
test "reviewed RenderAreaGraphics selects the guarded fast hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let code_c = transpile_test_source(source).files[4].contents

  inspect(
    code_c.contains("if (smb_core_fast_render_area_graphics()) { return; }"),
    content="true",
  )
}

///|
test "changed RenderAreaGraphics retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|            lda #$9a                     ;store length byte of 26 here with d7 set
      #|            sta VRAM_Buffer2+2,y         ;to increment by 32 (in columns)
    ),
    new=(
      #|            lda #$99                     ;store length byte of 25 here with d7 set
      #|            sta VRAM_Buffer2+2,y         ;to increment by 32 (in columns)
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_render_area_graphics()) { return; }"),
    content="false",
  )
}

///|
test "changed RenderAreaGraphics metatile table retains translated fallback" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|  .db <Palette0_MTiles, <Palette1_MTiles, <Palette2_MTiles, <Palette3_MTiles
    ),
    new=(
      #|  .db <Palette1_MTiles, <Palette1_MTiles, <Palette2_MTiles, <Palette3_MTiles
    ),
  )

  inspect(mutated == source, content="false")
  let code_c = transpile_test_source(mutated).files[4].contents
  inspect(
    code_c.contains("if (smb_core_fast_render_area_graphics()) { return; }"),
    content="false",
  )
}
