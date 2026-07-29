///|
test "reviewed ReadPortBits shape selects the serial-input specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed serial-input specialization; guarded by the exact lowered structure.",
    ),
    content="true",
  )
  inspect(
    code_c.contains(
      "serial_value = (uint8_t)(reversed | ((1u << port_index) - 1u));",
    ),
    content="true",
  )
  inspect(
    code_c.contains("ram[0x100u + sp] = serial_value;"),
    content="true",
  )
  inspect(
    code_c.contains(
      "Reviewed whole-read specialization; requires the exact ReadPortBits shape.",
    ),
    content="true",
  )
}

///|
test "unreviewed ReadPortBits shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "ReadPortBits",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed serial-input specialization; guarded by the exact lowered structure.",
    ),
    content="false",
  )
  inspect(code_c.contains("void ReadPortBits(void) {\n}"), content="true")
}

///|
test "unreviewed ReadJoypads shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "ReadJoypads",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains(
      "Reviewed whole-read specialization; requires the exact ReadPortBits shape.",
    ),
    content="false",
  )
  inspect(code_c.contains("void ReadJoypads(void) {\n}"), content="true")
}
