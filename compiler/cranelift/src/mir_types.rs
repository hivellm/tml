/// MIR data model — mirrors the C++ `mir::Module` hierarchy.
/// Used as the deserialization target for the binary MIR format.

// Primitive types (matches C++ PrimitiveType enum values exactly)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PrimitiveType {
    Unit = 0,
    Bool = 1,
    I8 = 2,
    I16 = 3,
    I32 = 4,
    I64 = 5,
    I128 = 6,
    U8 = 7,
    U16 = 8,
    U32 = 9,
    U64 = 10,
    U128 = 11,
    F32 = 12,
    F64 = 13,
    Ptr = 14,
    Str = 15,
}

impl PrimitiveType {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Unit),
            1 => Some(Self::Bool),
            2 => Some(Self::I8),
            3 => Some(Self::I16),
            4 => Some(Self::I32),
            5 => Some(Self::I64),
            6 => Some(Self::I128),
            7 => Some(Self::U8),
            8 => Some(Self::U16),
            9 => Some(Self::U32),
            10 => Some(Self::U64),
            11 => Some(Self::U128),
            12 => Some(Self::F32),
            13 => Some(Self::F64),
            14 => Some(Self::Ptr),
            15 => Some(Self::Str),
            _ => None,
        }
    }

    pub fn is_signed(self) -> bool {
        matches!(
            self,
            Self::I8 | Self::I16 | Self::I32 | Self::I64 | Self::I128
        )
    }

    pub fn is_float(self) -> bool {
        matches!(self, Self::F32 | Self::F64)
    }

    pub fn bit_width(self) -> u32 {
        match self {
            Self::Unit => 0,
            Self::Bool => 8,
            Self::I8 | Self::U8 => 8,
            Self::I16 | Self::U16 => 16,
            Self::I32 | Self::U32 => 32,
            Self::I64 | Self::U64 => 64,
            Self::I128 | Self::U128 => 128,
            Self::F32 => 32,
            Self::F64 => 64,
            Self::Ptr | Self::Str => 64,
        }
    }
}

// Type system
#[derive(Debug, Clone)]
pub enum MirType {
    Primitive(PrimitiveType),
    Pointer {
        is_mut: bool,
        pointee: Box<MirType>,
    },
    Array {
        size: u64,
        element: Box<MirType>,
    },
    Slice {
        element: Box<MirType>,
    },
    Tuple {
        elements: Vec<MirType>,
    },
    Struct {
        name: String,
        type_args: Vec<MirType>,
    },
    Enum {
        name: String,
        type_args: Vec<MirType>,
    },
    Function {
        params: Vec<MirType>,
        return_type: Box<MirType>,
    },
}

impl MirType {
    pub fn is_unit(&self) -> bool {
        matches!(self, MirType::Primitive(PrimitiveType::Unit))
    }

    pub fn is_float(&self) -> bool {
        matches!(
            self,
            MirType::Primitive(PrimitiveType::F32) | MirType::Primitive(PrimitiveType::F64)
        )
    }

    pub fn is_pointer(&self) -> bool {
        matches!(
            self,
            MirType::Pointer { .. }
                | MirType::Primitive(PrimitiveType::Ptr)
                | MirType::Primitive(PrimitiveType::Str)
        )
    }
}

pub type ValueId = u32;

#[derive(Debug, Clone, Copy)]
pub struct Value {
    pub id: ValueId,
}

// Binary operations
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum BinOp {
    Add = 0,
    Sub = 1,
    Mul = 2,
    Div = 3,
    Mod = 4,
    Eq = 5,
    Ne = 6,
    Lt = 7,
    Le = 8,
    Gt = 9,
    Ge = 10,
    And = 11,
    Or = 12,
    BitAnd = 13,
    BitOr = 14,
    BitXor = 15,
    Shl = 16,
    Shr = 17,
}

impl BinOp {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Add),
            1 => Some(Self::Sub),
            2 => Some(Self::Mul),
            3 => Some(Self::Div),
            4 => Some(Self::Mod),
            5 => Some(Self::Eq),
            6 => Some(Self::Ne),
            7 => Some(Self::Lt),
            8 => Some(Self::Le),
            9 => Some(Self::Gt),
            10 => Some(Self::Ge),
            11 => Some(Self::And),
            12 => Some(Self::Or),
            13 => Some(Self::BitAnd),
            14 => Some(Self::BitOr),
            15 => Some(Self::BitXor),
            16 => Some(Self::Shl),
            17 => Some(Self::Shr),
            _ => None,
        }
    }

    pub fn is_comparison(self) -> bool {
        matches!(
            self,
            Self::Eq | Self::Ne | Self::Lt | Self::Le | Self::Gt | Self::Ge
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum UnaryOp {
    Neg = 0,
    Not = 1,
    BitNot = 2,
}

impl UnaryOp {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Neg),
            1 => Some(Self::Not),
            2 => Some(Self::BitNot),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum CastKind {
    Bitcast = 0,
    Trunc = 1,
    ZExt = 2,
    SExt = 3,
    FPTrunc = 4,
    FPExt = 5,
    FPToSI = 6,
    FPToUI = 7,
    SIToFP = 8,
    UIToFP = 9,
    PtrToInt = 10,
    IntToPtr = 11,
}

impl CastKind {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Bitcast),
            1 => Some(Self::Trunc),
            2 => Some(Self::ZExt),
            3 => Some(Self::SExt),
            4 => Some(Self::FPTrunc),
            5 => Some(Self::FPExt),
            6 => Some(Self::FPToSI),
            7 => Some(Self::FPToUI),
            8 => Some(Self::SIToFP),
            9 => Some(Self::UIToFP),
            10 => Some(Self::PtrToInt),
            11 => Some(Self::IntToPtr),
            _ => None,
        }
    }
}

// Constants
#[derive(Debug, Clone)]
pub enum Constant {
    Int {
        value: i64,
        bit_width: u8,
        is_signed: bool,
    },
    Float {
        value: f64,
        is_f64: bool,
    },
    Bool(bool),
    String(String),
    Unit,
}

// Instructions
#[derive(Debug, Clone)]
pub enum Instruction {
    Binary {
        op: BinOp,
        left: Value,
        right: Value,
    },
    Unary {
        op: UnaryOp,
        operand: Value,
    },
    Load {
        ptr: Value,
    },
    Store {
        ptr: Value,
        value: Value,
    },
    Alloca {
        name: String,
        alloc_type: MirType,
    },
    Gep {
        base: Value,
        indices: Vec<Value>,
    },
    ExtractValue {
        aggregate: Value,
        indices: Vec<u32>,
    },
    InsertValue {
        aggregate: Value,
        value: Value,
        indices: Vec<u32>,
    },
    Call {
        func_name: String,
        args: Vec<Value>,
        return_type: MirType,
    },
    MethodCall {
        receiver: Value,
        method_name: String,
        args: Vec<Value>,
        return_type: MirType,
    },
    Cast {
        kind: CastKind,
        operand: Value,
        target_type: MirType,
    },
    Phi {
        incoming: Vec<(Value, u32)>,
    },
    Constant(Constant),
    Select {
        condition: Value,
        true_val: Value,
        false_val: Value,
    },
    StructInit {
        struct_name: String,
        fields: Vec<Value>,
    },
    EnumInit {
        enum_name: String,
        variant_name: String,
        payload: Vec<Value>,
    },
    TupleInit {
        elements: Vec<Value>,
    },
    ArrayInit {
        element_type: MirType,
        elements: Vec<Value>,
    },
    Await {
        poll_value: Value,
        poll_type: MirType,
        result_type: MirType,
        suspension_id: u32,
    },
    ClosureInit {
        func_name: String,
        captures: Vec<(String, Value)>,
        cap_types: Vec<(String, MirType)>,
        func_type: MirType,
        result_type: MirType,
    },
}

#[derive(Debug, Clone)]
pub struct InstructionData {
    pub result: ValueId,
    pub inst: Instruction,
}

// Terminators
#[derive(Debug, Clone)]
pub enum Terminator {
    Return { value: Option<Value> },
    Branch { target: u32 },
    CondBranch { condition: Value, true_block: u32, false_block: u32 },
    Switch { discriminant: Value, cases: Vec<(i64, u32)>, default_block: u32 },
    Unreachable,
}

// Basic block
#[derive(Debug, Clone)]
pub struct BasicBlock {
    pub id: u32,
    pub name: String,
    pub predecessors: Vec<u32>,
    pub instructions: Vec<InstructionData>,
    pub terminator: Option<Terminator>,
}

// Function
#[derive(Debug, Clone)]
pub struct FunctionParam {
    pub name: String,
    pub ty: MirType,
    pub value_id: ValueId,
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub is_public: bool,
    pub params: Vec<FunctionParam>,
    pub return_type: MirType,
    pub blocks: Vec<BasicBlock>,
    pub next_value_id: u32,
    pub next_block_id: u32,
}

// Struct and enum definitions
#[derive(Debug, Clone)]
pub struct StructField {
    pub name: String,
    pub ty: MirType,
}

#[derive(Debug, Clone)]
pub struct StructDef {
    pub name: String,
    pub type_params: Vec<String>,
    pub fields: Vec<StructField>,
}

#[derive(Debug, Clone)]
pub struct EnumVariant {
    pub name: String,
    pub payload_types: Vec<MirType>,
}

#[derive(Debug, Clone)]
pub struct EnumDef {
    pub name: String,
    pub type_params: Vec<String>,
    pub variants: Vec<EnumVariant>,
}

// Module
#[derive(Debug, Clone)]
pub struct Module {
    pub name: String,
    pub structs: Vec<StructDef>,
    pub enums: Vec<EnumDef>,
    pub functions: Vec<Function>,
    pub constants: Vec<(String, Constant)>,
}
