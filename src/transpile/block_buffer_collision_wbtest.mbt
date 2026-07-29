///|
test "reviewed BlockBufferCollision shapes select the address specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed block-buffer specialization; guarded with its address leaf.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "block_column = (uint8_t)((((uint8_t)coordinate_sum & 1u) << 4) |",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "ram[0x3] = ram[(block_address + adjusted_y) & (RAM_SIZE - 1u)];",
    ),
    content="true",
  )
}

///|
test "direct block-buffer read requires the exact address table" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  inspect(has_reviewed_block_buffer_addr_table(complete), content="true")

  match complete.defines.get("Block_Buffer_2") {
    Some(value) => complete.defines["Block_Buffer_1"] = value
    None => abort("missing reviewed Block_Buffer_2 define")
  }
  inspect(has_reviewed_block_buffer_addr_table(complete), content="false")

  let mut found = false
  for subroutine in complete.flat_subroutines {
    if subroutine.name == "BlockBufferCollision" {
      found = true
      let emitter = Emitter::new()
      emitter.emit_flat_subroutine(subroutine, complete)
      let code_c = emitter.output.to_string()
      inspect(
        code_c.contains(
          "Reviewed block-buffer specialization; guarded with its address leaf.",
        ),
        content="false",
      )
      inspect(
        code_c.contains("pha(); // save contents of A to stack"),
        content="true",
      )
    }
  }
  inspect(found, content="true")
}

///|
test "unreviewed BlockBufferCollision shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "BlockBufferCollision",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed block-buffer specialization; guarded with its address leaf.",
    ),
    content="false",
  )
  inspect(
    code_c.contains("void BlockBufferCollision(void) {\n}"),
    content="true",
  )
}

///|
test "reviewed collision shape still falls back without its address leaf" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  let missing_dependency = @lower.AsmLowering::new([])
  let mut found = false

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "BlockBufferCollision" {
      found = true
      let emitter = Emitter::new()
      emitter.emit_flat_subroutine(subroutine, missing_dependency)
      let code_c = emitter.output.to_string()

      inspect(
        code_c.contains(
          "Reviewed block-buffer specialization; guarded with its address leaf.",
        ),
        content="false",
      )
      inspect(
        code_c.contains("pha(); // save contents of A to stack"),
        content="true",
      )
    }
  }
  inspect(found, content="true")
}
