///|
test "jump-engine emission preserves the assembly ASL carry side effect" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])

  emitter.emit_item(
    @lower.JumpEngine(jump_table=["TargetZero", "TargetOne"]),
    lowering,
  )
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "carry_flag = (a & 0x80u) != 0;\nswitch (a) {",
    ),
    content="true",
  )
}
