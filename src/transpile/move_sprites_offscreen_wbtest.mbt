///|
test "reviewed MoveSpritesOffscreenSkip shape selects the OAM-clear specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed OAM-clear specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "sprite_y[local_y] = 0xf8u;",
    ),
    content="true",
  )
}

///|
test "unreviewed MoveSpritesOffscreenSkip shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "MoveSpritesOffscreenSkip",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed OAM-clear specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(
    code_c.contains("void MoveSpritesOffscreenSkip(void) {\n}"),
    content="true",
  )
}
