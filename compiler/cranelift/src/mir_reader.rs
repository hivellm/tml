/// MIR Binary Format Reader
///
/// Deserializes the TML MIR binary format produced by the C++ `MirBinaryWriter`.
/// Format: little-endian, length-prefixed strings, tagged types/instructions.

use crate::error::{BridgeError, BridgeResult};
use crate::mir_types::*;

const MIR_MAGIC: u32 = 0x544D4952; // "TMIR"
const MIR_VERSION_MAJOR: u16 = 1;

pub struct MirBinaryReader<'a> {
    data: &'a [u8],
    pos: usize,
}

impl<'a> MirBinaryReader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Self { data, pos: 0 }
    }

    pub fn read_module(&mut self) -> BridgeResult<Module> {
        self.verify_header()?;

        let name = self.read_string()?;

        // Structs
        let struct_count = self.read_u32()? as usize;
        let mut structs = Vec::with_capacity(struct_count);
        for _ in 0..struct_count {
            structs.push(self.read_struct_def()?);
        }

        // Enums
        let enum_count = self.read_u32()? as usize;
        let mut enums = Vec::with_capacity(enum_count);
        for _ in 0..enum_count {
            enums.push(self.read_enum_def()?);
        }

        // Functions
        let func_count = self.read_u32()? as usize;
        let mut functions = Vec::with_capacity(func_count);
        for _ in 0..func_count {
            functions.push(self.read_function()?);
        }

        // Constants
        let const_count = self.read_u32()? as usize;
        let mut constants = Vec::with_capacity(const_count);
        for _ in 0..const_count {
            let cname = self.read_string()?;
            let cval = self.read_constant_value()?;
            constants.push((cname, cval));
        }

        Ok(Module {
            name,
            structs,
            enums,
            functions,
            constants,
        })
    }

    fn verify_header(&mut self) -> BridgeResult<()> {
        let magic = self.read_u32()?;
        if magic != MIR_MAGIC {
            return Err(BridgeError::MirDeserialize(format!(
                "invalid magic: expected 0x{:08X}, got 0x{:08X}",
                MIR_MAGIC, magic
            )));
        }
        let major = self.read_u16()?;
        let _minor = self.read_u16()?;
        if major != MIR_VERSION_MAJOR {
            return Err(BridgeError::MirDeserialize(format!(
                "version mismatch: expected major {}, got {}",
                MIR_VERSION_MAJOR, major
            )));
        }
        Ok(())
    }

    // Primitive readers
    fn read_u8(&mut self) -> BridgeResult<u8> {
        if self.pos >= self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading u8".into()));
        }
        let v = self.data[self.pos];
        self.pos += 1;
        Ok(v)
    }

    fn read_u16(&mut self) -> BridgeResult<u16> {
        if self.pos + 2 > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading u16".into()));
        }
        let v = u16::from_le_bytes([self.data[self.pos], self.data[self.pos + 1]]);
        self.pos += 2;
        Ok(v)
    }

    fn read_u32(&mut self) -> BridgeResult<u32> {
        if self.pos + 4 > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading u32".into()));
        }
        let v = u32::from_le_bytes([
            self.data[self.pos],
            self.data[self.pos + 1],
            self.data[self.pos + 2],
            self.data[self.pos + 3],
        ]);
        self.pos += 4;
        Ok(v)
    }

    fn read_u64(&mut self) -> BridgeResult<u64> {
        if self.pos + 8 > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading u64".into()));
        }
        let bytes: [u8; 8] = self.data[self.pos..self.pos + 8].try_into().unwrap();
        self.pos += 8;
        Ok(u64::from_le_bytes(bytes))
    }

    fn read_i64(&mut self) -> BridgeResult<i64> {
        if self.pos + 8 > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading i64".into()));
        }
        let bytes: [u8; 8] = self.data[self.pos..self.pos + 8].try_into().unwrap();
        self.pos += 8;
        Ok(i64::from_le_bytes(bytes))
    }

    fn read_f64(&mut self) -> BridgeResult<f64> {
        if self.pos + 8 > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading f64".into()));
        }
        let bytes: [u8; 8] = self.data[self.pos..self.pos + 8].try_into().unwrap();
        self.pos += 8;
        Ok(f64::from_le_bytes(bytes))
    }

    fn read_string(&mut self) -> BridgeResult<String> {
        let len = self.read_u32()? as usize;
        if self.pos + len > self.data.len() {
            return Err(BridgeError::MirDeserialize("unexpected EOF reading string".into()));
        }
        let s = String::from_utf8_lossy(&self.data[self.pos..self.pos + len]).into_owned();
        self.pos += len;
        Ok(s)
    }

    fn read_value(&mut self) -> BridgeResult<Value> {
        let id = self.read_u32()?;
        Ok(Value { id })
    }

    // Type reader
    fn read_type(&mut self) -> BridgeResult<MirType> {
        let tag = self.read_u8()?;
        match tag {
            0 => {
                // Primitive
                let kind = self.read_u8()?;
                let prim = PrimitiveType::from_u8(kind).ok_or_else(|| {
                    BridgeError::MirDeserialize(format!("unknown primitive type: {}", kind))
                })?;
                Ok(MirType::Primitive(prim))
            }
            1 => {
                // Pointer
                let is_mut = self.read_u8()? != 0;
                let pointee = self.read_type()?;
                Ok(MirType::Pointer {
                    is_mut,
                    pointee: Box::new(pointee),
                })
            }
            2 => {
                // Array
                let size = self.read_u64()?;
                let element = self.read_type()?;
                Ok(MirType::Array {
                    size,
                    element: Box::new(element),
                })
            }
            3 => {
                // Slice
                let element = self.read_type()?;
                Ok(MirType::Slice {
                    element: Box::new(element),
                })
            }
            4 => {
                // Tuple
                let count = self.read_u32()? as usize;
                let mut elements = Vec::with_capacity(count);
                for _ in 0..count {
                    elements.push(self.read_type()?);
                }
                Ok(MirType::Tuple { elements })
            }
            5 => {
                // Struct
                let name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut type_args = Vec::with_capacity(count);
                for _ in 0..count {
                    type_args.push(self.read_type()?);
                }
                Ok(MirType::Struct { name, type_args })
            }
            6 => {
                // Enum
                let name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut type_args = Vec::with_capacity(count);
                for _ in 0..count {
                    type_args.push(self.read_type()?);
                }
                Ok(MirType::Enum { name, type_args })
            }
            7 => {
                // Function
                let param_count = self.read_u32()? as usize;
                let mut params = Vec::with_capacity(param_count);
                for _ in 0..param_count {
                    params.push(self.read_type()?);
                }
                let return_type = self.read_type()?;
                Ok(MirType::Function {
                    params,
                    return_type: Box::new(return_type),
                })
            }
            _ => Err(BridgeError::MirDeserialize(format!(
                "unknown type tag: {}",
                tag
            ))),
        }
    }

    // Constant value reader (for module-level constants)
    fn read_constant_value(&mut self) -> BridgeResult<Constant> {
        let tag = self.read_u8()?;
        match tag {
            0 => {
                // Int
                let value = self.read_i64()?;
                let bit_width = self.read_u8()?;
                let is_signed = self.read_u8()? != 0;
                Ok(Constant::Int {
                    value,
                    bit_width,
                    is_signed,
                })
            }
            1 => {
                // Float
                let value = self.read_f64()?;
                let is_f64 = self.read_u8()? != 0;
                Ok(Constant::Float { value, is_f64 })
            }
            2 => {
                // Bool
                let value = self.read_u8()? != 0;
                Ok(Constant::Bool(value))
            }
            3 => {
                // String
                let value = self.read_string()?;
                Ok(Constant::String(value))
            }
            4 => {
                // Unit
                Ok(Constant::Unit)
            }
            _ => Err(BridgeError::MirDeserialize(format!(
                "unknown constant tag: {}",
                tag
            ))),
        }
    }

    // Instruction reader
    fn read_instruction(&mut self) -> BridgeResult<InstructionData> {
        let result = self.read_u32()?;
        let tag = self.read_u8()?;

        let inst = match tag {
            0 => {
                // Binary
                let op = BinOp::from_u8(self.read_u8()?).ok_or_else(|| {
                    BridgeError::MirDeserialize("unknown binary op".into())
                })?;
                let left = self.read_value()?;
                let right = self.read_value()?;
                Instruction::Binary { op, left, right }
            }
            1 => {
                // Unary
                let op = UnaryOp::from_u8(self.read_u8()?).ok_or_else(|| {
                    BridgeError::MirDeserialize("unknown unary op".into())
                })?;
                let operand = self.read_value()?;
                Instruction::Unary { op, operand }
            }
            2 => {
                // Load
                let ptr = self.read_value()?;
                Instruction::Load { ptr }
            }
            3 => {
                // Store
                let ptr = self.read_value()?;
                let value = self.read_value()?;
                Instruction::Store { ptr, value }
            }
            4 => {
                // Alloca
                let name = self.read_string()?;
                let alloc_type = self.read_type()?;
                Instruction::Alloca { name, alloc_type }
            }
            5 => {
                // Gep
                let base = self.read_value()?;
                let count = self.read_u32()? as usize;
                let mut indices = Vec::with_capacity(count);
                for _ in 0..count {
                    indices.push(self.read_value()?);
                }
                Instruction::Gep { base, indices }
            }
            6 => {
                // ExtractValue
                let aggregate = self.read_value()?;
                let count = self.read_u32()? as usize;
                let mut indices = Vec::with_capacity(count);
                for _ in 0..count {
                    indices.push(self.read_u32()?);
                }
                Instruction::ExtractValue { aggregate, indices }
            }
            7 => {
                // InsertValue
                let aggregate = self.read_value()?;
                let value = self.read_value()?;
                let count = self.read_u32()? as usize;
                let mut indices = Vec::with_capacity(count);
                for _ in 0..count {
                    indices.push(self.read_u32()?);
                }
                Instruction::InsertValue {
                    aggregate,
                    value,
                    indices,
                }
            }
            8 => {
                // Call
                let func_name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut args = Vec::with_capacity(count);
                for _ in 0..count {
                    args.push(self.read_value()?);
                }
                let return_type = self.read_type()?;
                Instruction::Call {
                    func_name,
                    args,
                    return_type,
                }
            }
            9 => {
                // MethodCall
                let receiver = self.read_value()?;
                let method_name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut args = Vec::with_capacity(count);
                for _ in 0..count {
                    args.push(self.read_value()?);
                }
                let return_type = self.read_type()?;
                Instruction::MethodCall {
                    receiver,
                    method_name,
                    args,
                    return_type,
                }
            }
            10 => {
                // Cast
                let kind = CastKind::from_u8(self.read_u8()?).ok_or_else(|| {
                    BridgeError::MirDeserialize("unknown cast kind".into())
                })?;
                let operand = self.read_value()?;
                let target_type = self.read_type()?;
                Instruction::Cast {
                    kind,
                    operand,
                    target_type,
                }
            }
            11 => {
                // Phi
                let count = self.read_u32()? as usize;
                let mut incoming = Vec::with_capacity(count);
                for _ in 0..count {
                    let val = self.read_value()?;
                    let block = self.read_u32()?;
                    incoming.push((val, block));
                }
                Instruction::Phi { incoming }
            }
            12 => {
                // Constant
                let cval = self.read_constant_value()?;
                Instruction::Constant(cval)
            }
            13 => {
                // Select
                let condition = self.read_value()?;
                let true_val = self.read_value()?;
                let false_val = self.read_value()?;
                Instruction::Select {
                    condition,
                    true_val,
                    false_val,
                }
            }
            14 => {
                // StructInit
                let struct_name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut fields = Vec::with_capacity(count);
                for _ in 0..count {
                    fields.push(self.read_value()?);
                }
                Instruction::StructInit {
                    struct_name,
                    fields,
                }
            }
            15 => {
                // EnumInit
                let enum_name = self.read_string()?;
                let variant_name = self.read_string()?;
                let count = self.read_u32()? as usize;
                let mut payload = Vec::with_capacity(count);
                for _ in 0..count {
                    payload.push(self.read_value()?);
                }
                Instruction::EnumInit {
                    enum_name,
                    variant_name,
                    payload,
                }
            }
            16 => {
                // TupleInit
                let count = self.read_u32()? as usize;
                let mut elements = Vec::with_capacity(count);
                for _ in 0..count {
                    elements.push(self.read_value()?);
                }
                Instruction::TupleInit { elements }
            }
            17 => {
                // ArrayInit
                let element_type = self.read_type()?;
                let count = self.read_u32()? as usize;
                let mut elements = Vec::with_capacity(count);
                for _ in 0..count {
                    elements.push(self.read_value()?);
                }
                Instruction::ArrayInit {
                    element_type,
                    elements,
                }
            }
            18 => {
                // Await
                let poll_value = self.read_value()?;
                let poll_type = self.read_type()?;
                let result_type = self.read_type()?;
                let suspension_id = self.read_u32()?;
                Instruction::Await {
                    poll_value,
                    poll_type,
                    result_type,
                    suspension_id,
                }
            }
            19 => {
                // ClosureInit
                let func_name = self.read_string()?;
                let cap_count = self.read_u32()? as usize;
                let mut captures = Vec::with_capacity(cap_count);
                for _ in 0..cap_count {
                    let cname = self.read_string()?;
                    let cval = self.read_value()?;
                    captures.push((cname, cval));
                }
                let mut cap_types = Vec::with_capacity(cap_count);
                for _ in 0..cap_count {
                    let tname = self.read_string()?;
                    let ttype = self.read_type()?;
                    cap_types.push((tname, ttype));
                }
                let func_type = self.read_type()?;
                let result_type = self.read_type()?;
                Instruction::ClosureInit {
                    func_name,
                    captures,
                    cap_types,
                    func_type,
                    result_type,
                }
            }
            _ => {
                return Err(BridgeError::MirDeserialize(format!(
                    "unknown instruction tag: {}",
                    tag
                )));
            }
        };

        Ok(InstructionData { result, inst })
    }

    // Terminator reader
    fn read_terminator(&mut self) -> BridgeResult<Terminator> {
        let tag = self.read_u8()?;
        match tag {
            0 => {
                // Return
                let has_value = self.read_u8()? != 0;
                let value = if has_value {
                    Some(self.read_value()?)
                } else {
                    None
                };
                Ok(Terminator::Return { value })
            }
            1 => {
                // Branch
                let target = self.read_u32()?;
                Ok(Terminator::Branch { target })
            }
            2 => {
                // CondBranch
                let condition = self.read_value()?;
                let true_block = self.read_u32()?;
                let false_block = self.read_u32()?;
                Ok(Terminator::CondBranch {
                    condition,
                    true_block,
                    false_block,
                })
            }
            3 => {
                // Switch
                let discriminant = self.read_value()?;
                let count = self.read_u32()? as usize;
                let mut cases = Vec::with_capacity(count);
                for _ in 0..count {
                    let val = self.read_i64()?;
                    let block = self.read_u32()?;
                    cases.push((val, block));
                }
                let default_block = self.read_u32()?;
                Ok(Terminator::Switch {
                    discriminant,
                    cases,
                    default_block,
                })
            }
            4 => {
                // Unreachable
                Ok(Terminator::Unreachable)
            }
            _ => Err(BridgeError::MirDeserialize(format!(
                "unknown terminator tag: {}",
                tag
            ))),
        }
    }

    // Block reader
    fn read_block(&mut self) -> BridgeResult<BasicBlock> {
        let id = self.read_u32()?;
        let name = self.read_string()?;

        let pred_count = self.read_u32()? as usize;
        let mut predecessors = Vec::with_capacity(pred_count);
        for _ in 0..pred_count {
            predecessors.push(self.read_u32()?);
        }

        let inst_count = self.read_u32()? as usize;
        let mut instructions = Vec::with_capacity(inst_count);
        for _ in 0..inst_count {
            instructions.push(self.read_instruction()?);
        }

        let has_term = self.read_u8()? != 0;
        let terminator = if has_term {
            Some(self.read_terminator()?)
        } else {
            None
        };

        Ok(BasicBlock {
            id,
            name,
            predecessors,
            instructions,
            terminator,
        })
    }

    // Function reader
    fn read_function(&mut self) -> BridgeResult<Function> {
        let name = self.read_string()?;
        let is_public = self.read_u8()? != 0;

        let param_count = self.read_u32()? as usize;
        let mut params = Vec::with_capacity(param_count);
        for _ in 0..param_count {
            let pname = self.read_string()?;
            let pty = self.read_type()?;
            let pval = self.read_u32()?;
            params.push(FunctionParam {
                name: pname,
                ty: pty,
                value_id: pval,
            });
        }

        let return_type = self.read_type()?;

        let block_count = self.read_u32()? as usize;
        let mut blocks = Vec::with_capacity(block_count);
        for _ in 0..block_count {
            blocks.push(self.read_block()?);
        }

        let next_value_id = self.read_u32()?;
        let next_block_id = self.read_u32()?;

        Ok(Function {
            name,
            is_public,
            params,
            return_type,
            blocks,
            next_value_id,
            next_block_id,
        })
    }

    fn read_struct_def(&mut self) -> BridgeResult<StructDef> {
        let name = self.read_string()?;
        let tp_count = self.read_u32()? as usize;
        let mut type_params = Vec::with_capacity(tp_count);
        for _ in 0..tp_count {
            type_params.push(self.read_string()?);
        }
        let field_count = self.read_u32()? as usize;
        let mut fields = Vec::with_capacity(field_count);
        for _ in 0..field_count {
            let fname = self.read_string()?;
            let ftype = self.read_type()?;
            fields.push(StructField {
                name: fname,
                ty: ftype,
            });
        }
        Ok(StructDef {
            name,
            type_params,
            fields,
        })
    }

    fn read_enum_def(&mut self) -> BridgeResult<EnumDef> {
        let name = self.read_string()?;
        let tp_count = self.read_u32()? as usize;
        let mut type_params = Vec::with_capacity(tp_count);
        for _ in 0..tp_count {
            type_params.push(self.read_string()?);
        }
        let var_count = self.read_u32()? as usize;
        let mut variants = Vec::with_capacity(var_count);
        for _ in 0..var_count {
            let vname = self.read_string()?;
            let pt_count = self.read_u32()? as usize;
            let mut payload_types = Vec::with_capacity(pt_count);
            for _ in 0..pt_count {
                payload_types.push(self.read_type()?);
            }
            variants.push(EnumVariant {
                name: vname,
                payload_types,
            });
        }
        Ok(EnumDef {
            name,
            type_params,
            variants,
        })
    }
}
