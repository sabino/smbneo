///|
test "reviewed BoundingBoxCore shape selects the direct geometry specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed bounding-box specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains("const uint8_t box_offset = (uint8_t)(object_offset << 2);"),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint8_t *const bounds = &data[BoundBoxCtrlData - 0x8000u + control_offset];",
    ),
    content="true",
  )
  inspect(code_c.contains("box[3] = (uint8_t)sum;"), content="true")
}

///|
test "unreviewed BoundingBoxCore shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "BoundingBoxCore",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed bounding-box specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(code_c.contains("void BoundingBoxCore(void) {\n}"), content="true")
}
