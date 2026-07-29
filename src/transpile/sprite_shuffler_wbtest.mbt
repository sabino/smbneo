///|
test "reviewed SpriteShuffler shape selects the fixed-table specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed fixed-table specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "for (sprite_index = 15u; sprite_index-- != 0u;)",
    ),
    content="true",
  )
}

///|
test "unreviewed SpriteShuffler shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "SpriteShuffler",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed fixed-table specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(
    code_c.contains("void SpriteShuffler(void) {\n}"),
    content="true",
  )
}
