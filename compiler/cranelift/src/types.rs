/// Type mapping: MIR types → Cranelift types
///
/// Cranelift types are sign-agnostic integers (I8, I16, I32, I64, I128)
/// and floats (F32, F64). Aggregate types are lowered to memory with
/// explicit load/store at computed offsets.

use cranelift_codegen::ir::types;
use cranelift_codegen::ir::Type as CraneliftType;

use crate::mir_types::{MirType, PrimitiveType};

/// Pointer type for the target (always 64-bit for now).
pub const POINTER_TYPE: CraneliftType = types::I64;

/// Map a MIR primitive type to a Cranelift type.
/// Returns None for Unit (void).
pub fn primitive_to_cranelift(prim: PrimitiveType) -> Option<CraneliftType> {
    match prim {
        PrimitiveType::Unit => None,
        PrimitiveType::Bool => Some(types::I8),
        PrimitiveType::I8 | PrimitiveType::U8 => Some(types::I8),
        PrimitiveType::I16 | PrimitiveType::U16 => Some(types::I16),
        PrimitiveType::I32 | PrimitiveType::U32 => Some(types::I32),
        PrimitiveType::I64 | PrimitiveType::U64 => Some(types::I64),
        PrimitiveType::I128 | PrimitiveType::U128 => Some(types::I128),
        PrimitiveType::F32 => Some(types::F32),
        PrimitiveType::F64 => Some(types::F64),
        PrimitiveType::Ptr | PrimitiveType::Str => Some(POINTER_TYPE),
    }
}

/// Map a MIR type to a Cranelift type.
/// Returns None for void/unit types and aggregate types that live in memory.
pub fn mir_type_to_cranelift(ty: &MirType) -> Option<CraneliftType> {
    match ty {
        MirType::Primitive(prim) => primitive_to_cranelift(*prim),
        MirType::Pointer { .. } => Some(POINTER_TYPE),
        MirType::Slice { .. } => Some(POINTER_TYPE), // fat pointer represented as ptr
        // Aggregates are memory-resident, returned as pointer
        MirType::Struct { .. } | MirType::Enum { .. } | MirType::Tuple { .. } | MirType::Array { .. } => {
            Some(POINTER_TYPE)
        }
        MirType::Function { .. } => Some(POINTER_TYPE), // function pointer
    }
}

/// Compute the size in bytes of a MIR type.
pub fn type_size(ty: &MirType) -> u32 {
    match ty {
        MirType::Primitive(prim) => match prim {
            PrimitiveType::Unit => 0,
            PrimitiveType::Bool => 1,
            PrimitiveType::I8 | PrimitiveType::U8 => 1,
            PrimitiveType::I16 | PrimitiveType::U16 => 2,
            PrimitiveType::I32 | PrimitiveType::U32 => 4,
            PrimitiveType::I64 | PrimitiveType::U64 => 8,
            PrimitiveType::I128 | PrimitiveType::U128 => 16,
            PrimitiveType::F32 => 4,
            PrimitiveType::F64 => 8,
            PrimitiveType::Ptr | PrimitiveType::Str => 8,
        },
        MirType::Pointer { .. } => 8,
        MirType::Slice { .. } => 16, // ptr + len
        MirType::Function { .. } => 8,
        MirType::Array { size, element } => {
            let elem_size = type_size(element);
            (elem_size * (*size as u32)).max(1)
        }
        MirType::Tuple { elements } => {
            let mut offset = 0u32;
            for elem in elements {
                let align = type_alignment(elem);
                offset = align_to(offset, align);
                offset += type_size(elem);
            }
            let max_align = elements.iter().map(type_alignment).max().unwrap_or(1);
            align_to(offset, max_align)
        }
        MirType::Struct { .. } | MirType::Enum { .. } => {
            // Without full struct layout info, use pointer size as fallback.
            // Real struct sizes are computed from StructDef fields.
            8
        }
    }
}

/// Compute alignment of a MIR type.
pub fn type_alignment(ty: &MirType) -> u32 {
    match ty {
        MirType::Primitive(prim) => match prim {
            PrimitiveType::Unit => 1,
            PrimitiveType::Bool => 1,
            PrimitiveType::I8 | PrimitiveType::U8 => 1,
            PrimitiveType::I16 | PrimitiveType::U16 => 2,
            PrimitiveType::I32 | PrimitiveType::U32 => 4,
            PrimitiveType::I64 | PrimitiveType::U64 => 8,
            PrimitiveType::I128 | PrimitiveType::U128 => 16,
            PrimitiveType::F32 => 4,
            PrimitiveType::F64 => 8,
            PrimitiveType::Ptr | PrimitiveType::Str => 8,
        },
        MirType::Pointer { .. } | MirType::Slice { .. } | MirType::Function { .. } => 8,
        MirType::Array { element, .. } => type_alignment(element),
        MirType::Tuple { elements } => elements.iter().map(type_alignment).max().unwrap_or(1),
        MirType::Struct { .. } | MirType::Enum { .. } => 8,
    }
}

/// Compute field offsets for a struct given its field types.
pub fn compute_struct_layout(field_types: &[&MirType]) -> (Vec<u32>, u32) {
    let mut offsets = Vec::with_capacity(field_types.len());
    let mut offset = 0u32;
    let mut max_align = 1u32;

    for &field_ty in field_types {
        let align = type_alignment(field_ty);
        max_align = max_align.max(align);
        offset = align_to(offset, align);
        offsets.push(offset);
        offset += type_size(field_ty);
    }

    let total_size = align_to(offset, max_align);
    (offsets, total_size)
}

fn align_to(value: u32, alignment: u32) -> u32 {
    if alignment == 0 {
        return value;
    }
    (value + alignment - 1) & !(alignment - 1)
}
