//! # MIR Binary Writer
//!
//! This file writes MIR modules to a compact binary format.
//!
//! ## Binary Format Structure
//!
//! ```text
//! Header:
//!   magic: u32 (0x4D495220 = "MIR ")
//!   version_major: u16
//!   version_minor: u16
//!
//! Module:
//!   name: string
//!   structs: [StructDef...]
//!   enums: [EnumDef...]
//!   functions: [Function...]
//!   constants: [(name, Constant)...]
//! ```
//!
//! ## String Encoding
//!
//! Strings are length-prefixed: `u32 length` + `bytes[length]`
//!
//! ## Type Encoding
//!
//! Types are tagged with a TypeTag byte followed by type-specific data.
//!
//! ## Advantages
//!
//! - Compact representation (smaller than text)
//! - Fast to read/write (no parsing)
//! - Stable format for incremental compilation cache

#include "serializer_internal.hpp"

namespace tml::mir {

// ============================================================================
// MirBinaryWriter Implementation
// ============================================================================

MirBinaryWriter::MirBinaryWriter(std::ostream& out) : out_(out) {}

void MirBinaryWriter::write_header() {
    write_u32(MIR_MAGIC);
    write_u16(MIR_VERSION_MAJOR);
    write_u16(MIR_VERSION_MINOR);
}

void MirBinaryWriter::write_u8(uint8_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 1);
}

void MirBinaryWriter::write_u16(uint16_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 2);
}

void MirBinaryWriter::write_u32(uint32_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 4);
}

void MirBinaryWriter::write_u64(uint64_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void MirBinaryWriter::write_i64(int64_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void MirBinaryWriter::write_f64(double value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void MirBinaryWriter::write_string(const std::string& str) {
    write_u32(static_cast<uint32_t>(str.size()));
    out_.write(str.data(), static_cast<std::streamsize>(str.size()));
}

void MirBinaryWriter::write_type(const MirTypePtr& type) {
    if (!type) {
        write_u8(static_cast<uint8_t>(TypeTag::Primitive));
        write_u8(static_cast<uint8_t>(PrimitiveType::Unit));
        return;
    }

    std::visit(
        [this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, MirPrimitiveType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Primitive));
                write_u8(static_cast<uint8_t>(t.kind));
            } else if constexpr (std::is_same_v<T, MirPointerType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Pointer));
                write_u8(t.is_mut ? 1 : 0);
                write_type(t.pointee);
            } else if constexpr (std::is_same_v<T, MirArrayType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Array));
                write_u64(t.size);
                write_type(t.element);
            } else if constexpr (std::is_same_v<T, MirSliceType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Slice));
                write_type(t.element);
            } else if constexpr (std::is_same_v<T, MirTupleType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Tuple));
                write_u32(static_cast<uint32_t>(t.elements.size()));
                for (const auto& elem : t.elements) {
                    write_type(elem);
                }
            } else if constexpr (std::is_same_v<T, MirStructType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Struct));
                write_string(t.name);
                write_u32(static_cast<uint32_t>(t.type_args.size()));
                for (const auto& arg : t.type_args) {
                    write_type(arg);
                }
            } else if constexpr (std::is_same_v<T, MirEnumType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Enum));
                write_string(t.name);
                write_u32(static_cast<uint32_t>(t.type_args.size()));
                for (const auto& arg : t.type_args) {
                    write_type(arg);
                }
            } else if constexpr (std::is_same_v<T, MirFunctionType>) {
                write_u8(static_cast<uint8_t>(TypeTag::Function));
                write_u32(static_cast<uint32_t>(t.params.size()));
                for (const auto& param : t.params) {
                    write_type(param);
                }
                write_type(t.return_type);
            }
        },
        type->kind);
}

void MirBinaryWriter::write_value(const Value& value) {
    write_u32(value.id);
}

void MirBinaryWriter::write_instruction(const InstructionData& inst) {
    write_u32(inst.result);

    std::visit(
        [this](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Binary));
                write_u8(static_cast<uint8_t>(i.op));
                write_value(i.left);
                write_value(i.right);
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Unary));
                write_u8(static_cast<uint8_t>(i.op));
                write_value(i.operand);
            } else if constexpr (std::is_same_v<T, LoadInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Load));
                write_value(i.ptr);
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Store));
                write_value(i.ptr);
                write_value(i.value);
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Alloca));
                write_string(i.name);
                write_type(i.alloc_type);
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Gep));
                write_value(i.base);
                write_u32(static_cast<uint32_t>(i.indices.size()));
                for (const auto& idx : i.indices) {
                    write_value(idx);
                }
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                write_u8(static_cast<uint8_t>(InstTag::ExtractValue));
                write_value(i.aggregate);
                write_u32(static_cast<uint32_t>(i.indices.size()));
                for (auto idx : i.indices) {
                    write_u32(idx);
                }
            } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                write_u8(static_cast<uint8_t>(InstTag::InsertValue));
                write_value(i.aggregate);
                write_value(i.value);
                write_u32(static_cast<uint32_t>(i.indices.size()));
                for (auto idx : i.indices) {
                    write_u32(idx);
                }
            } else if constexpr (std::is_same_v<T, CallInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Call));
                write_string(i.func_name);
                write_u32(static_cast<uint32_t>(i.args.size()));
                for (const auto& arg : i.args) {
                    write_value(arg);
                }
                write_type(i.return_type);
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                write_u8(static_cast<uint8_t>(InstTag::MethodCall));
                write_value(i.receiver);
                write_string(i.method_name);
                write_u32(static_cast<uint32_t>(i.args.size()));
                for (const auto& arg : i.args) {
                    write_value(arg);
                }
                write_type(i.return_type);
            } else if constexpr (std::is_same_v<T, CastInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Cast));
                write_u8(static_cast<uint8_t>(i.kind));
                write_value(i.operand);
                write_type(i.target_type);
            } else if constexpr (std::is_same_v<T, PhiInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Phi));
                write_u32(static_cast<uint32_t>(i.incoming.size()));
                for (const auto& [val, block] : i.incoming) {
                    write_value(val);
                    write_u32(block);
                }
            } else if constexpr (std::is_same_v<T, ConstantInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Constant));
                std::visit(
                    [this](const auto& c) {
                        using C = std::decay_t<decltype(c)>;
                        if constexpr (std::is_same_v<C, ConstInt>) {
                            write_u8(static_cast<uint8_t>(ConstTag::Int));
                            write_i64(c.value);
                            write_u8(static_cast<uint8_t>(c.bit_width));
                            write_u8(c.is_signed ? 1 : 0);
                        } else if constexpr (std::is_same_v<C, ConstFloat>) {
                            write_u8(static_cast<uint8_t>(ConstTag::Float));
                            write_f64(c.value);
                            write_u8(c.is_f64 ? 1 : 0);
                        } else if constexpr (std::is_same_v<C, ConstBool>) {
                            write_u8(static_cast<uint8_t>(ConstTag::Bool));
                            write_u8(c.value ? 1 : 0);
                        } else if constexpr (std::is_same_v<C, ConstString>) {
                            write_u8(static_cast<uint8_t>(ConstTag::String));
                            write_string(c.value);
                        } else if constexpr (std::is_same_v<C, ConstUnit>) {
                            write_u8(static_cast<uint8_t>(ConstTag::Unit));
                        }
                    },
                    i.value);
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                write_u8(static_cast<uint8_t>(InstTag::Select));
                write_value(i.condition);
                write_value(i.true_val);
                write_value(i.false_val);
            } else if constexpr (std::is_same_v<T, StructInitInst>) {
                write_u8(static_cast<uint8_t>(InstTag::StructInit));
                write_string(i.struct_name);
                write_u32(static_cast<uint32_t>(i.fields.size()));
                for (const auto& field : i.fields) {
                    write_value(field);
                }
            } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                write_u8(static_cast<uint8_t>(InstTag::EnumInit));
                write_string(i.enum_name);
                write_string(i.variant_name);
                write_u32(static_cast<uint32_t>(i.payload.size()));
                for (const auto& p : i.payload) {
                    write_value(p);
                }
            } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                write_u8(static_cast<uint8_t>(InstTag::TupleInit));
                write_u32(static_cast<uint32_t>(i.elements.size()));
                for (const auto& elem : i.elements) {
                    write_value(elem);
                }
            } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                write_u8(static_cast<uint8_t>(InstTag::ArrayInit));
                write_type(i.element_type);
                write_u32(static_cast<uint32_t>(i.elements.size()));
                for (const auto& elem : i.elements) {
                    write_value(elem);
                }
            }
        },
        inst.inst);
}

void MirBinaryWriter::write_terminator(const Terminator& term) {
    std::visit(
        [this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, ReturnTerm>) {
                write_u8(static_cast<uint8_t>(TermTag::Return));
                write_u8(t.value.has_value() ? 1 : 0);
                if (t.value.has_value()) {
                    write_value(*t.value);
                }
            } else if constexpr (std::is_same_v<T, BranchTerm>) {
                write_u8(static_cast<uint8_t>(TermTag::Branch));
                write_u32(t.target);
            } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                write_u8(static_cast<uint8_t>(TermTag::CondBranch));
                write_value(t.condition);
                write_u32(t.true_block);
                write_u32(t.false_block);
            } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                write_u8(static_cast<uint8_t>(TermTag::Switch));
                write_value(t.discriminant);
                write_u32(static_cast<uint32_t>(t.cases.size()));
                for (const auto& [val, block] : t.cases) {
                    write_i64(val);
                    write_u32(block);
                }
                write_u32(t.default_block);
            } else if constexpr (std::is_same_v<T, UnreachableTerm>) {
                write_u8(static_cast<uint8_t>(TermTag::Unreachable));
            }
        },
        term);
}

void MirBinaryWriter::write_block(const BasicBlock& block) {
    write_u32(block.id);
    write_string(block.name);

    // Predecessors
    write_u32(static_cast<uint32_t>(block.predecessors.size()));
    for (auto pred : block.predecessors) {
        write_u32(pred);
    }

    // Instructions
    write_u32(static_cast<uint32_t>(block.instructions.size()));
    for (const auto& inst : block.instructions) {
        write_instruction(inst);
    }

    // Terminator
    write_u8(block.terminator.has_value() ? 1 : 0);
    if (block.terminator.has_value()) {
        write_terminator(*block.terminator);
    }
}

void MirBinaryWriter::write_function(const Function& func) {
    write_string(func.name);
    write_u8(func.is_public ? 1 : 0);

    // Parameters
    write_u32(static_cast<uint32_t>(func.params.size()));
    for (const auto& param : func.params) {
        write_string(param.name);
        write_type(param.type);
        write_u32(param.value_id);
    }

    // Return type
    write_type(func.return_type);

    // Blocks
    write_u32(static_cast<uint32_t>(func.blocks.size()));
    for (const auto& block : func.blocks) {
        write_block(block);
    }

    // Counters
    write_u32(func.next_value_id);
    write_u32(func.next_block_id);
}

void MirBinaryWriter::write_struct(const StructDef& s) {
    write_string(s.name);

    write_u32(static_cast<uint32_t>(s.type_params.size()));
    for (const auto& param : s.type_params) {
        write_string(param);
    }

    write_u32(static_cast<uint32_t>(s.fields.size()));
    for (const auto& field : s.fields) {
        write_string(field.name);
        write_type(field.type);
    }
}

void MirBinaryWriter::write_enum(const EnumDef& e) {
    write_string(e.name);

    write_u32(static_cast<uint32_t>(e.type_params.size()));
    for (const auto& param : e.type_params) {
        write_string(param);
    }

    write_u32(static_cast<uint32_t>(e.variants.size()));
    for (const auto& v : e.variants) {
        write_string(v.name);
        write_u32(static_cast<uint32_t>(v.payload_types.size()));
        for (const auto& t : v.payload_types) {
            write_type(t);
        }
    }
}

void MirBinaryWriter::write_module(const Module& module) {
    write_header();
    write_string(module.name);

    // Structs
    write_u32(static_cast<uint32_t>(module.structs.size()));
    for (const auto& s : module.structs) {
        write_struct(s);
    }

    // Enums
    write_u32(static_cast<uint32_t>(module.enums.size()));
    for (const auto& e : module.enums) {
        write_enum(e);
    }

    // Functions
    write_u32(static_cast<uint32_t>(module.functions.size()));
    for (const auto& f : module.functions) {
        write_function(f);
    }

    // Constants
    write_u32(static_cast<uint32_t>(module.constants.size()));
    for (const auto& [name, value] : module.constants) {
        write_string(name);
        std::visit(
            [this](const auto& c) {
                using C = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<C, ConstInt>) {
                    write_u8(static_cast<uint8_t>(ConstTag::Int));
                    write_i64(c.value);
                    write_u8(static_cast<uint8_t>(c.bit_width));
                    write_u8(c.is_signed ? 1 : 0);
                } else if constexpr (std::is_same_v<C, ConstFloat>) {
                    write_u8(static_cast<uint8_t>(ConstTag::Float));
                    write_f64(c.value);
                    write_u8(c.is_f64 ? 1 : 0);
                } else if constexpr (std::is_same_v<C, ConstBool>) {
                    write_u8(static_cast<uint8_t>(ConstTag::Bool));
                    write_u8(c.value ? 1 : 0);
                } else if constexpr (std::is_same_v<C, ConstString>) {
                    write_u8(static_cast<uint8_t>(ConstTag::String));
                    write_string(c.value);
                } else if constexpr (std::is_same_v<C, ConstUnit>) {
                    write_u8(static_cast<uint8_t>(ConstTag::Unit));
                }
            },
            value);
    }
}

} // namespace tml::mir
