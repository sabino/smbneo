
pub(all) struct Lexer {
  input: String
  mut index: Int
}

pub suberror LexerError {
  UnexpectedChar(char~: Char, index~: Int)
  InvalidConstant(lexeme~: String, base~: ConstantBase)
} derive(Show)

fn is_whitespace(c: Char) -> Bool {
  match c {
    ' ' => true
    '\t' => true
    '\n' => true
    '\r' => true
    _ => false
  }
}

fn is_decimal_digit(c: Char) -> Bool {
  c >= '0' && c <= '9'
}

fn is_hex_digit(c: Char) -> Bool {
  is_decimal_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
}

fn is_valid_digit(char: Char, base~: ConstantBase) -> Bool {
  match base {
    Binary => char == '0' || char == '1'
    Decimal => is_decimal_digit(char)
    Hexadecimal => is_hex_digit(char)
  }
}

fn is_alpha(c: Char) -> Bool {
  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
}

fn is_alphanumeric(c: Char) -> Bool {
  is_alpha(c) || is_decimal_digit(c)
}

pub fn Lexer::new(input: String) -> Lexer {
  { input, index: 0 }
}

fn Lexer::peek(self: Lexer) -> Char {
  self.input[self.index].to_char().unwrap()
}

fn Lexer::matches(self: Lexer, char: Char) -> Bool {
  self.peek() == char
}

fn Lexer::consumes(self: Lexer, char: Char) -> Bool {
  if self.matches(char) {
    self.index += 1
    true
  } else {
    false
  }
}

fn Lexer::skip_whitespaces(self: Lexer) -> Unit {
  while self.index < self.input.length() && is_whitespace(self.peek()) {
    self.index += 1
  }
}

fn Lexer::parse_identifier(self: Lexer) -> String {
  let identifier = StringBuilder::new()
  
  for i in self.index..<self.input.length() {
    let char = self.input[i].to_char().unwrap()

    if is_alphanumeric(char) {
      identifier.write_char(char)
      self.advance()
    } else {
      break
    }
  }

  identifier.to_string()
}

fn Lexer::parse_constant(self: Lexer, base~: ConstantBase) -> LexerResult {
  let lexeme = StringBuilder::new()

  for i in self.index..<self.input.length() {
    let char = self.input[i].to_char().unwrap()

    if is_valid_digit(char, base~) {
      lexeme.write_char(char)
      self.advance()
    } else {
      break
    }
  }

  let lexeme = lexeme.to_string()

  let value = match (lexeme, base) {
    ("0b", Hexadecimal) => 11 // https://github.com/moonbitlang/core/issues/2006
    _ => {
      try @string.parse_int(lexeme, base=base.to_int()) catch {
        _ => return Grr(InvalidConstant(lexeme~, base~))
      } noraise {
        value => value
      }
    }
  }

  let size = match base {
    Binary => {
      if lexeme.length() <= 8 {
        Byte
      } else {
        Word
      }
    }
    Decimal => {
      if lexeme.length() <= 3 && value <= 0xFF {
        Byte
      } else {
        Word
      }
    }
    Hexadecimal => {
      if lexeme.length() <= 2 {
        Byte
      } else {
        Word
      }
    }
  }

  Yay(Constant(base~, value~, size~))
}

fn parse_opcode(str: String) -> OpCode? {
  match str[0] {
    'a' => {
      match str {
        "adc" => Some(ADC)
        "and" => Some(AND)
        "asl" => Some(ASL)
        _ => None
      }
    }
    'b' => {
      match str {
        "bcc" => Some(BCC)
        "bcs" => Some(BCS)
        "beq" => Some(BEQ)
        "bit" => Some(BIT)
        "bmi" => Some(BMI)
        "bne" => Some(BNE)
        "bpl" => Some(BPL)
        "brk" => Some(BRK)
        "bvc" => Some(BVC)
        "bvs" => Some(BVS)
        _ => None
      }
    }
    'c' => {
      match str {
        "clc" => Some(CLC)
        "cld" => Some(CLD)
        "cli" => Some(CLI)
        "clv" => Some(CLV)
        "cmp" => Some(CMP)
        "cpx" => Some(CPX)
        "cpy" => Some(CPY)
        _ => None
      }
    }
    'd' => {
      match str {
        "dec" => Some(DEC)
        "dex" => Some(DEX)
        "dey" => Some(DEY)
        _ => None
      }
    }
    'e' => {
      match str {
        "eor" => Some(EOR)
        _ => None
      }
    }
    'i' => {
      match str {
        "inc" => Some(INC)
        "inx" => Some(INX)
        "iny" => Some(INY)
        _ => None
      }
    }
    'j' => {
      match str {
        "jmp" => Some(JMP)
        "jsr" => Some(JSR)
        _ => None
      }
    }
    'l' => {
      match str {
        "lda" => Some(LDA)
        "ldx" => Some(LDX)
        "ldy" => Some(LDY)
        "lsr" => Some(LSR)
        _ => None
      }
    }
    'n' => {
      match str {
        "nop" => Some(NOP)
        _ => None
      }
    }
    'o' => {
      match str {
        "ora" => Some(ORA)
        _ => None
      }
    }
    'p' => {
      match str {
        "pha" => Some(PHA)
        "php" => Some(PHP)
        "pla" => Some(PLA)
        "plp" => Some(PLP)
        _ => None
      }
    }
    'r' => {
      match str {
        "rol" => Some(ROL)
        "ror" => Some(ROR)
        "rti" => Some(RTI)
        "rts" => Some(RTS)
        _ => None
      }
    }
    's' => {
      match str {
        "sbc" => Some(SBC)
        "sec" => Some(SEC)
        "sed" => Some(SED)
        "sei" => Some(SEI)
        "sta" => Some(STA)
        "stx" => Some(STX)
        "sty" => Some(STY)
        _ => None
      }
    }
    't' => {
      match str {
        "tax" => Some(TAX)
        "tay" => Some(TAY)
        "tsx" => Some(TSX)
        "txa" => Some(TXA)
        "txs" => Some(TXS)
        "tya" => Some(TYA)
        _ => None
      }
    }
    _ => None
  }
}

fn Lexer::advance(self: Lexer) -> Unit {
  self.index += 1
}

priv enum LexerResult {
  Yay(Token)
  Grr(LexerError)
  EOF
}

fn Lexer::parse_comment(self: Lexer) -> String {
  let comment = StringBuilder::new()

  for i in self.index..<self.input.length() {
    let char = self.input[i].to_char().unwrap()

    if char == '\n' {
      break
    }

    comment.write_char(char)
    self.advance()
  }

  comment.to_string()
}

fn Lexer::next(self: Lexer) -> LexerResult {
  self.skip_whitespaces()

  if self.index >= self.input.length() {
    return EOF
  }

  let char = self.peek()

  match char {
    ',' => { self.advance(); Yay(Comma) }
    '#' => { self.advance(); Yay(Hash) }
    '+' => { self.advance(); Yay(Plus) }
    '-' => { self.advance(); Yay(Minus) }
    '(' => { self.advance(); Yay(LeftParen) }
    ')' => { self.advance(); Yay(RightParen) }
    '=' => { self.advance(); Yay(Equal) }
    '<' => { self.advance(); Yay(LeftChevron) }
    '>' => { self.advance(); Yay(RightChevron) }
    '.' => {
      self.advance()
      let directive = self.parse_identifier()
      Yay(Directive(directive))
    }
    ';' => {
      let mut i = self.index - 1
      let mut is_inline = false

      while i >= 0 && self.input[i] != '\n' {
        if not(is_whitespace(self.input[i].to_char().unwrap())) {
          is_inline = true
          break
        }

        i -= 1
      }

      self.advance()
      let comment = self.parse_comment()
      Yay(Comment(comment~, is_inline~))
    }
    '%' => {
      self.advance()
      self.parse_constant(base=Binary)
    }
    '$' => {
      self.advance()
      self.parse_constant(base=Hexadecimal)
    }
    _ => {
      if is_alpha(char) {
        let identifier = self.parse_identifier()

        if self.consumes(':') {
          return Yay(Label(identifier))
        }

        match parse_opcode(identifier) {
          Some(opcode) => Yay(OpCode(opcode))
          None => Yay(Identifier(identifier))
        }
      } else if is_decimal_digit(char) {
        self.parse_constant(base=Decimal)
      } else {
        Grr(UnexpectedChar(char~, index=self.index))
      }
    }
  }
}

pub fn Lexer::tokenize(self: Lexer) -> Array[Token] raise LexerError {
  let tokens = []

  while true {
    match self.next() {
      Yay(token) => tokens.push(token)
      Grr(error) => raise error
      EOF => break
    }
  }

  tokens
}
