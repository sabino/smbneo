
suberror PipelineError {
  IOError(String)
  LexerError(@parse.LexerError)
  ParserError(@parse.ParserError)
  LoweringError(@lower.LoweringError)
} derive(Show)

fn read_rom() -> String raise PipelineError {
  try @fs.read_file_to_string("src/smb.asm") catch {
    err => raise IOError(@debug.to_string(err))
  } noraise {
    assembly => assembly
  }
}

fn tokenize() -> Array[@parse.Token] raise PipelineError {
  let assembly = read_rom()
  let lexer = @parse.Lexer::new(assembly)

  try lexer.tokenize() catch {
    err => raise LexerError(err)
  } noraise {
    tokens => tokens
  }
}

fn parse() -> Array[@parse.SourceItem] raise PipelineError {
  let tokens = tokenize()
  let parser = @parse.Parser::new(tokens)

  try parser.parse() catch {
    err => raise ParserError(err)
  } noraise {
    instructions => instructions
  }
}

fn lower() -> @lower.AsmLowering raise PipelineError {
  let instructions = parse()
  let lowering = @lower.AsmLowering::new(instructions)
  try lowering.lower() catch {
    err => raise LoweringError(err)
  } noraise {
    _ => lowering
  }
}

fn transpile() -> @transpile.Transpiler raise PipelineError {
  let low = lower()
  @transpile.Transpiler::transpile(low)
}

let output_dir: String = "codegen"
let lib_output_dir: String = "\{output_dir}/lib"

fn run() -> Unit raise Error {
  if not(@fs.path_exists(output_dir)) {
    @fs.create_dir(output_dir)
  }

  if not(@fs.path_exists(lib_output_dir)) {
    @fs.create_dir(lib_output_dir)
  }

  try transpile() catch {
      err => {
          println(err)
      }
  } noraise {
      tr => {
          for file in tr.files {
              file.write(dir=if file.is_lib { lib_output_dir } else { output_dir })
          }
      }
  }
}

fn main {
  try run() catch {
    err => {
      println("Error: \{Show::to_string(err)}")
    }
  }
}
