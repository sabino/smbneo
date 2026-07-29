///|
test "reviewed TopScoreCheck shape selects the score-chain specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed score-chain specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint16_t difference = (uint16_t)player_digit - (uint16_t)top_digit - (local_carry ? 0u : 1u);",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint8_t *const player_score = &ram[PlayerScoreDisplay + initial_x - 5u];",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "const uint16_t high_difference = (uint16_t)player_score[0] - (uint16_t)top_score[0] - (lower_digits_borrow ? 1u : 0u);",
    ),
    content="true",
  )
}

///|
test "unreviewed TopScoreCheck shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "TopScoreCheck",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed score-chain specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(code_c.contains("void TopScoreCheck(void) {\n}"), content="true")
}
