///|
test "reviewed relative enemy position and helpers select the folded specialization" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let transpiler = transpile_test_source(source)
  let code_c = transpiler.files[4].contents

  inspect(
    code_c.contains(
      "Reviewed enemy-relative-position specialization; guarded by exact folded helper shapes.",
    ),
    content="true",
  )
  inspect(
    code_c.contains("const uint8_t object_index = (uint8_t)(initial_x + 1u);"),
    content="true",
  )
  inspect(
    code_c.contains(
      "object_x = ram[(uint8_t)(SprObject_X_Position + object_index)];",
    ),
    content="true",
  )
}

///|
test "relative enemy position requires both reviewed folded helpers" {
  let source = try! @fs.read_file_to_string("src/smb.asm")
  let tokens = try! @parse.Lexer::new(source).tokenize()
  let items = try! @parse.Parser::new(tokens).parse()
  let complete = @lower.AsmLowering::new(items)
  try! complete.lower()
  let dependencies_missing = @lower.AsmLowering::new([])
  let emitter = Emitter::new()

  for subroutine in complete.flat_subroutines {
    if subroutine.name == "RelativeEnemyPosition" {
      emitter.emit_flat_subroutine(subroutine, dependencies_missing)
    }
  }
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains("Reviewed enemy-relative-position specialization"),
    content="false",
  )
  inspect(code_c.contains("VariableObjOfsRelPos();"), content="true")
}

///|
test "changed relative enemy position shape falls back to generic emission" {
  let emitter = Emitter::new()
  let lowering = @lower.AsmLowering::new([])
  let unreviewed : @lower.AsmFlatSubroutine = {
    name: "RelativeEnemyPosition",
    items: [],
  }

  emitter.emit_flat_subroutine(unreviewed, lowering)
  let code_c = emitter.output.to_string()

  inspect(
    code_c.contains("Reviewed enemy-relative-position specialization"),
    content="false",
  )
  inspect(
    code_c.contains("void RelativeEnemyPosition(void) {\n}"),
    content="true",
  )
}
