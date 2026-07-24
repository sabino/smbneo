
pub(all) enum Type {
  ConstTy(@parse.ConstantSize)
  ArrayTy(@parse.ConstantSize)
  LabelTy
} derive (Show, Eq)

pub(all) enum TypedExpr {
  Const(
    base~: @parse.ConstantBase,
    value~: Int,
    size~: @parse.ConstantSize,
    ty~: Type,
  )
  Ident(name~: String, ty~: Type)
  Unary(op~: @parse.UnaryOp, expr~: TypedExpr, ty~: Type)
  Binary(op~: @parse.BinaryOp, lhs~: TypedExpr, rhs~: TypedExpr, ty~: Type)
} derive (Show)

pub(all) enum AsmTableLine {
  Data(data~: Array[TypedExpr], comment~: Ref[String])
  RomFallthrough(data~: Array[TypedExpr], comment~: Ref[String])
  Comment(String)
} derive (Show)

pub(all) struct AsmTable {
  name: String
  offset: Int
  lines: Array[AsmTableLine]
  // Logical bytes declared by .db/.dw and addressable as table contents.
  size: Int
  // Packed bytes, including original-ROM fall-through guards.
  storage_size: Int
  elem_size: @parse.ConstantSize
  items_count: Int
} derive (Show)

pub(all) enum AsmAddressingMode {
  Accumulator
  Implied
  Immediate(TypedExpr)
  Relative(TypedExpr)
  ZeroPage(TypedExpr)
  ZeroPageX(TypedExpr)
  ZeroPageY(TypedExpr)
  Absolute(TypedExpr)
  AbsoluteX(TypedExpr)
  AbsoluteY(TypedExpr)
  Indirect(TypedExpr)
  IndexedIndirect(TypedExpr)
  IndirectIndexed(TypedExpr)
} derive (Show)

pub(all) enum AsmItem {
  Label(String)
  Inst(op~: @parse.OpCode, mode~: AsmAddressingMode, comment~: String?)
  JumpEngine(jump_table~: Array[String])
  If(cond~: @parse.OpCode, then~: Array[AsmItem], otherwise~: Array[AsmItem]?, comment~: String?)
  Comment(String)
} derive (Show)

pub(all) struct BasicBlockLink {
  bb: BasicBlock
  is_fallthrough: Bool
} derive (Show)

pub(all) struct BasicBlock {
  id: Int
  mut name: String?
  items: Array[AsmItem]
  predecessors: Array[BasicBlockLink]
  successors: Array[BasicBlockLink]
  mut is_subroutine_entry: Bool
  mut subroutine: AsmSubroutine?
} derive (Show)

pub(all) struct AsmSubroutine {
  name: String
  blocks: Array[BasicBlock]
  entry: BasicBlock
  mut exit: BasicBlock
  internal_labels: Set[String]
} derive (Show)

// subroutines with a flat array of items instead of basic blocks
pub(all) struct AsmFlatSubroutine {
  name: String
  items: Array[AsmItem]
} derive (Show)

pub fn TypedExpr::ty(self: TypedExpr) -> Type {
  match self {
    Const(base=_, value=_, size=_, ty~) => ty
    Ident(name=_, ty~) => ty
    Unary(op=_, expr=_, ty~) => ty
    Binary(op=_, lhs=_, rhs=_, ty~) => ty
  }
}

pub fn AsmAddressingMode::suffix(self: AsmAddressingMode) -> String {
  match self {
    Accumulator => "_acc"
    Implied => ""
    Immediate(_) => "_imm"
    Relative(_) => "_rel"
    ZeroPage(_) => "_zp"
    ZeroPageX(_) => "_zpx"
    ZeroPageY(_) => "_zpy"
    Absolute(_) => "_abs"
    AbsoluteX(_) => "_absx"
    AbsoluteY(_) => "_absy"
    Indirect(_) => "_ind"
    IndexedIndirect(_) => "_indx"
    IndirectIndexed(_) => "_indy"
  }
}
