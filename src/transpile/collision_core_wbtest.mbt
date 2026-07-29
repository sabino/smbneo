///|
test "reviewed collision core shape selects the leaf specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed leaf specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "local_a = ram[BoundingBox_UL_Corner + local_y];",
    ),
    content="true",
  )
}

///|
test "unreviewed collision core shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "SprObjectCollisionCore",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed leaf specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(
    code_c.contains("void SprObjectCollisionCore(void) {\n}"),
    content="true",
  )
}
