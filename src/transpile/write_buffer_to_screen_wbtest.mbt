///|
test "reviewed WriteBufferToScreen shape selects the guarded batch hook" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed VRAM-buffer batch hook; rejected runs use the original loop.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "ppu_write_buffer_run(source_pointer, run_length, carry_flag ? 1u : 0u)",
    ),
    content="true",
  )
  inspect(code_c.contains("OutputToVRAMComplete:"), content="true")
  inspect(code_c.contains("ppu_write_data(a);"), content="true")
}

///|
test "mutated WriteBufferToScreen shape retains generic byte loop" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let mutated = source.replace(
    old=(
      #|               dex                       ;done writing?
      #|               bne OutputToVRAM
    ),
    new=(
      #|               inx                       ;done writing?
      #|               bne OutputToVRAM
    ),
  )

  inspect(mutated == source, content="false")
  let transpiler = transpile_test_source(mutated)
  let code_c = transpiler.files[4].contents
  inspect(
    code_c.contains(
      "Reviewed VRAM-buffer batch hook; rejected runs use the original loop.",
    ),
    content="false",
  )
  inspect(code_c.contains("inx(); // done writing?"), content="true")
}

///|
test "reviewed WriteBufferToScreen fingerprint is stable" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let lowering = @lower.AsmLowering::new(items)
  try! lowering.lower()

  for subroutine in lowering.flat_subroutines {
    if subroutine.name == "WriteBufferToScreen__sub" {
      inspect(
        reviewed_flat_subroutine_fingerprint(subroutine),
        content="(42, 1451, 1ab865f9531d4ba356bfc72ef79e637dd6b77096ae79e02628f364c5ac9eaffc)",
      )
      return
    }
  }
  fail("WriteBufferToScreen__sub was not lowered")
}
