///|
test "reviewed MoveObjectHorizontally shape selects the motion specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed horizontal-motion specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint8_t speed_address = (uint8_t)(SprObject_X_Speed + object_offset);",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint8_t pixel_delta = (uint8_t)((high_nibble ^ 8u) - 8u);",
    ),
    content="true",
  )
}

///|
test "unreviewed MoveObjectHorizontally shape uses generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "MoveObjectHorizontally",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed horizontal-motion specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(
    code_c.contains("void MoveObjectHorizontally(void) {\n}"),
    content="true",
  )
}
