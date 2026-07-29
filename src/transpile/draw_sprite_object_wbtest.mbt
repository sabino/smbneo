///|
test "reviewed DrawSpriteObject shape selects the sprite-row specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed sprite-row specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains("const bool horizontal_flip = (ram[0x3] & 0x02u) != 0u;"),
    content="true",
  )
  inspect(
    code_c.contains("ram[Sprite_Tilenumber + right_sprite] = right_tile;"),
    content="true",
  )
}

///|
test "unreviewed DrawSpriteObject shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "DrawSpriteObject",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed sprite-row specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(code_c.contains("void DrawSpriteObject(void) {\n}"), content="true")
}
