// MIR Serialization Implementation

#include "mir/mir_serialize.hpp"

#include <cstring>
#include <fstream>
#include <sstream>

namespace tml::mir {

// ============================================================================
// Binary Format Type Tags
// ============================================================================

enum class TypeTag : uint8_t {
    Primitive = 0,
    Pointer = 1,
    Array = 2,
    Slice = 3,
    Tuple = 4,
    Struct = 5,
    Enum = 6,
    Function = 7,
};

enum class InstTag : uint8_t {
    Binary = 0,
    Unary = 1,
    Load = 2,
    Store = 3,
    Alloca = 4,
    Gep = 5,
    ExtractValue = 6,
    InsertValue = 7,
    Call = 8,
    MethodCall = 9,
    Cast = 10,
    Phi = 11,
    Constant = 12,
    Select = 13,
    StructInit = 14,
    EnumInit = 15,
    TupleInit = 16,
    ArrayInit = 17,
};

enum class TermTag : uint8_t {
    Return = 0,
    Branch = 1,
    CondBranch = 2,
    Switch = 3,
    Unreachable = 4,
};

enum class ConstTag : uint8_t {
    Int = 0,
    Float = 1,
    Bool = 2,
    String = 3,
    Unit = 4,
};

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

// ============================================================================
// MirBinaryReader Implementation
// ============================================================================

MirBinaryReader::MirBinaryReader(std::istream& in) : in_(in) {}

void MirBinaryReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = msg;
}

auto MirBinaryReader::read_u8() -> uint8_t {
    uint8_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 1);
    return value;
}

auto MirBinaryReader::read_u16() -> uint16_t {
    uint16_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 2);
    return value;
}

auto MirBinaryReader::read_u32() -> uint32_t {
    uint32_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 4);
    return value;
}

auto MirBinaryReader::read_u64() -> uint64_t {
    uint64_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto MirBinaryReader::read_i64() -> int64_t {
    int64_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto MirBinaryReader::read_f64() -> double {
    double value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    return value;
}

auto MirBinaryReader::read_string() -> std::string {
    uint32_t len = read_u32();
    std::string str(len, '\0');
    in_.read(str.data(), len);
    return str;
}

auto MirBinaryReader::verify_header() -> bool {
    uint32_t magic = read_u32();
    if (magic != MIR_MAGIC) {
        set_error("Invalid MIR magic number");
        return false;
    }

    uint16_t major = read_u16();
    uint16_t minor = read_u16();
    (void)minor; // Minor version differences are OK

    if (major != MIR_VERSION_MAJOR) {
        set_error("Unsupported MIR version");
        return false;
    }

    return true;
}

auto MirBinaryReader::read_type() -> MirTypePtr {
    auto tag = static_cast<TypeTag>(read_u8());

    switch (tag) {
    case TypeTag::Primitive: {
        auto kind = static_cast<PrimitiveType>(read_u8());
        auto type = std::make_shared<MirType>();
        type->kind = MirPrimitiveType{kind};
        return type;
    }
    case TypeTag::Pointer: {
        bool is_mut = read_u8() != 0;
        auto pointee = read_type();
        return make_pointer_type(pointee, is_mut);
    }
    case TypeTag::Array: {
        size_t size = read_u64();
        auto elem = read_type();
        return make_array_type(elem, size);
    }
    case TypeTag::Slice: {
        auto elem = read_type();
        auto type = std::make_shared<MirType>();
        type->kind = MirSliceType{elem};
        return type;
    }
    case TypeTag::Tuple: {
        uint32_t count = read_u32();
        std::vector<MirTypePtr> elements;
        for (uint32_t i = 0; i < count; ++i) {
            elements.push_back(read_type());
        }
        return make_tuple_type(std::move(elements));
    }
    case TypeTag::Struct: {
        std::string name = read_string();
        uint32_t count = read_u32();
        std::vector<MirTypePtr> type_args;
        for (uint32_t i = 0; i < count; ++i) {
            type_args.push_back(read_type());
        }
        return make_struct_type(name, std::move(type_args));
    }
    case TypeTag::Enum: {
        std::string name = read_string();
        uint32_t count = read_u32();
        std::vector<MirTypePtr> type_args;
        for (uint32_t i = 0; i < count; ++i) {
            type_args.push_back(read_type());
        }
        return make_enum_type(name, std::move(type_args));
    }
    case TypeTag::Function: {
        uint32_t param_count = read_u32();
        std::vector<MirTypePtr> params;
        for (uint32_t i = 0; i < param_count; ++i) {
            params.push_back(read_type());
        }
        auto ret = read_type();
        auto type = std::make_shared<MirType>();
        type->kind = MirFunctionType{std::move(params), ret};
        return type;
    }
    }

    return make_unit_type();
}

auto MirBinaryReader::read_value() -> Value {
    return Value{read_u32()};
}

auto MirBinaryReader::read_instruction() -> InstructionData {
    InstructionData data;
    data.result = read_u32();

    auto tag = static_cast<InstTag>(read_u8());

    switch (tag) {
    case InstTag::Binary: {
        BinaryInst inst;
        inst.op = static_cast<BinOp>(read_u8());
        inst.left = read_value();
        inst.right = read_value();
        data.inst = inst;
        break;
    }
    case InstTag::Unary: {
        UnaryInst inst;
        inst.op = static_cast<UnaryOp>(read_u8());
        inst.operand = read_value();
        data.inst = inst;
        break;
    }
    case InstTag::Load: {
        LoadInst inst;
        inst.ptr = read_value();
        data.inst = inst;
        break;
    }
    case InstTag::Store: {
        StoreInst inst;
        inst.ptr = read_value();
        inst.value = read_value();
        data.inst = inst;
        break;
    }
    case InstTag::Alloca: {
        AllocaInst inst;
        inst.name = read_string();
        inst.alloc_type = read_type();
        data.inst = inst;
        break;
    }
    case InstTag::Gep: {
        GetElementPtrInst inst;
        inst.base = read_value();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.indices.push_back(read_value());
        }
        data.inst = inst;
        break;
    }
    case InstTag::ExtractValue: {
        ExtractValueInst inst;
        inst.aggregate = read_value();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.indices.push_back(read_u32());
        }
        data.inst = inst;
        break;
    }
    case InstTag::InsertValue: {
        InsertValueInst inst;
        inst.aggregate = read_value();
        inst.value = read_value();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.indices.push_back(read_u32());
        }
        data.inst = inst;
        break;
    }
    case InstTag::Call: {
        CallInst inst;
        inst.func_name = read_string();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.args.push_back(read_value());
        }
        inst.return_type = read_type();
        data.inst = inst;
        break;
    }
    case InstTag::MethodCall: {
        MethodCallInst inst;
        inst.receiver = read_value();
        inst.method_name = read_string();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.args.push_back(read_value());
        }
        inst.return_type = read_type();
        data.inst = inst;
        break;
    }
    case InstTag::Cast: {
        CastInst inst;
        inst.kind = static_cast<CastKind>(read_u8());
        inst.operand = read_value();
        inst.target_type = read_type();
        data.inst = inst;
        break;
    }
    case InstTag::Phi: {
        PhiInst inst;
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            Value val = read_value();
            uint32_t block = read_u32();
            inst.incoming.emplace_back(val, block);
        }
        data.inst = inst;
        break;
    }
    case InstTag::Constant: {
        ConstantInst inst;
        auto const_tag = static_cast<ConstTag>(read_u8());
        switch (const_tag) {
        case ConstTag::Int: {
            ConstInt c;
            c.value = read_i64();
            c.bit_width = read_u8();
            c.is_signed = read_u8() != 0;
            inst.value = c;
            break;
        }
        case ConstTag::Float: {
            ConstFloat c;
            c.value = read_f64();
            c.is_f64 = read_u8() != 0;
            inst.value = c;
            break;
        }
        case ConstTag::Bool: {
            ConstBool c;
            c.value = read_u8() != 0;
            inst.value = c;
            break;
        }
        case ConstTag::String: {
            ConstString c;
            c.value = read_string();
            inst.value = c;
            break;
        }
        case ConstTag::Unit:
            inst.value = ConstUnit{};
            break;
        }
        data.inst = inst;
        break;
    }
    case InstTag::Select: {
        SelectInst inst;
        inst.condition = read_value();
        inst.true_val = read_value();
        inst.false_val = read_value();
        data.inst = inst;
        break;
    }
    case InstTag::StructInit: {
        StructInitInst inst;
        inst.struct_name = read_string();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.fields.push_back(read_value());
        }
        data.inst = inst;
        break;
    }
    case InstTag::EnumInit: {
        EnumInitInst inst;
        inst.enum_name = read_string();
        inst.variant_name = read_string();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.payload.push_back(read_value());
        }
        data.inst = inst;
        break;
    }
    case InstTag::TupleInit: {
        TupleInitInst inst;
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.elements.push_back(read_value());
        }
        data.inst = inst;
        break;
    }
    case InstTag::ArrayInit: {
        ArrayInitInst inst;
        inst.element_type = read_type();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            inst.elements.push_back(read_value());
        }
        data.inst = inst;
        break;
    }
    }

    return data;
}

auto MirBinaryReader::read_terminator() -> Terminator {
    auto tag = static_cast<TermTag>(read_u8());

    switch (tag) {
    case TermTag::Return: {
        ReturnTerm term;
        if (read_u8() != 0) {
            term.value = read_value();
        }
        return term;
    }
    case TermTag::Branch: {
        BranchTerm term;
        term.target = read_u32();
        return term;
    }
    case TermTag::CondBranch: {
        CondBranchTerm term;
        term.condition = read_value();
        term.true_block = read_u32();
        term.false_block = read_u32();
        return term;
    }
    case TermTag::Switch: {
        SwitchTerm term;
        term.discriminant = read_value();
        uint32_t count = read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            int64_t val = read_i64();
            uint32_t block = read_u32();
            term.cases.emplace_back(val, block);
        }
        term.default_block = read_u32();
        return term;
    }
    case TermTag::Unreachable:
        return UnreachableTerm{};
    }

    return UnreachableTerm{};
}

auto MirBinaryReader::read_block() -> BasicBlock {
    BasicBlock block;
    block.id = read_u32();
    block.name = read_string();

    uint32_t pred_count = read_u32();
    for (uint32_t i = 0; i < pred_count; ++i) {
        block.predecessors.push_back(read_u32());
    }

    uint32_t inst_count = read_u32();
    for (uint32_t i = 0; i < inst_count; ++i) {
        block.instructions.push_back(read_instruction());
    }

    if (read_u8() != 0) {
        block.terminator = read_terminator();
    }

    return block;
}

auto MirBinaryReader::read_function() -> Function {
    Function func;
    func.name = read_string();
    func.is_public = read_u8() != 0;

    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count; ++i) {
        FunctionParam param;
        param.name = read_string();
        param.type = read_type();
        param.value_id = read_u32();
        func.params.push_back(param);
    }

    func.return_type = read_type();

    uint32_t block_count = read_u32();
    for (uint32_t i = 0; i < block_count; ++i) {
        func.blocks.push_back(read_block());
    }

    func.next_value_id = read_u32();
    func.next_block_id = read_u32();

    return func;
}

auto MirBinaryReader::read_struct() -> StructDef {
    StructDef s;
    s.name = read_string();

    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count; ++i) {
        s.type_params.push_back(read_string());
    }

    uint32_t field_count = read_u32();
    for (uint32_t i = 0; i < field_count; ++i) {
        StructField field;
        field.name = read_string();
        field.type = read_type();
        s.fields.push_back(field);
    }

    return s;
}

auto MirBinaryReader::read_enum() -> EnumDef {
    EnumDef e;
    e.name = read_string();

    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count; ++i) {
        e.type_params.push_back(read_string());
    }

    uint32_t variant_count = read_u32();
    for (uint32_t i = 0; i < variant_count; ++i) {
        EnumVariant v;
        v.name = read_string();
        uint32_t payload_count = read_u32();
        for (uint32_t j = 0; j < payload_count; ++j) {
            v.payload_types.push_back(read_type());
        }
        e.variants.push_back(v);
    }

    return e;
}

auto MirBinaryReader::read_module() -> Module {
    Module module;

    if (!verify_header()) {
        return module;
    }

    module.name = read_string();

    uint32_t struct_count = read_u32();
    for (uint32_t i = 0; i < struct_count; ++i) {
        module.structs.push_back(read_struct());
    }

    uint32_t enum_count = read_u32();
    for (uint32_t i = 0; i < enum_count; ++i) {
        module.enums.push_back(read_enum());
    }

    uint32_t func_count = read_u32();
    for (uint32_t i = 0; i < func_count; ++i) {
        module.functions.push_back(read_function());
    }

    uint32_t const_count = read_u32();
    for (uint32_t i = 0; i < const_count; ++i) {
        std::string name = read_string();
        auto const_tag = static_cast<ConstTag>(read_u8());
        Constant value;

        switch (const_tag) {
        case ConstTag::Int: {
            ConstInt c;
            c.value = read_i64();
            c.bit_width = read_u8();
            c.is_signed = read_u8() != 0;
            value = c;
            break;
        }
        case ConstTag::Float: {
            ConstFloat c;
            c.value = read_f64();
            c.is_f64 = read_u8() != 0;
            value = c;
            break;
        }
        case ConstTag::Bool: {
            ConstBool c;
            c.value = read_u8() != 0;
            value = c;
            break;
        }
        case ConstTag::String: {
            ConstString c;
            c.value = read_string();
            value = c;
            break;
        }
        case ConstTag::Unit:
            value = ConstUnit{};
            break;
        }

        module.constants[name] = value;
    }

    return module;
}

// ============================================================================
// MirTextWriter Implementation
// ============================================================================

MirTextWriter::MirTextWriter(std::ostream& out, SerializeOptions options)
    : out_(out), options_(options) {}

void MirTextWriter::write_module(const Module& module) {
    MirPrinter printer(!options_.compact);
    out_ << printer.print_module(module);
}

// ============================================================================
// MirTextReader Implementation (Placeholder)
// ============================================================================

MirTextReader::MirTextReader(std::istream& in) : in_(in) {}

void MirTextReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = "Line " + std::to_string(line_num_) + ": " + msg;
}

auto MirTextReader::next_line() -> bool {
    if (!std::getline(in_, current_line_)) {
        return false;
    }
    ++line_num_;
    pos_ = 0;
    return true;
}

void MirTextReader::skip_whitespace() {
    while (pos_ < current_line_.size() && std::isspace(current_line_[pos_])) {
        ++pos_;
    }
}

auto MirTextReader::peek_char() -> char {
    if (pos_ >= current_line_.size())
        return '\0';
    return current_line_[pos_];
}

auto MirTextReader::read_char() -> char {
    if (pos_ >= current_line_.size())
        return '\0';
    return current_line_[pos_++];
}

auto MirTextReader::read_identifier() -> std::string {
    skip_whitespace();
    std::string result;
    while (pos_ < current_line_.size() &&
           (std::isalnum(current_line_[pos_]) || current_line_[pos_] == '_')) {
        result += current_line_[pos_++];
    }
    return result;
}

auto MirTextReader::read_number() -> int64_t {
    skip_whitespace();
    std::string num_str;
    bool negative = false;

    if (peek_char() == '-') {
        negative = true;
        ++pos_;
    }

    while (pos_ < current_line_.size() && std::isdigit(current_line_[pos_])) {
        num_str += current_line_[pos_++];
    }

    if (num_str.empty())
        return 0;

    int64_t value = std::stoll(num_str);
    return negative ? -value : value;
}

auto MirTextReader::read_string_literal() -> std::string {
    skip_whitespace();
    if (peek_char() != '"')
        return "";
    ++pos_;

    std::string result;
    while (pos_ < current_line_.size() && current_line_[pos_] != '"') {
        if (current_line_[pos_] == '\\' && pos_ + 1 < current_line_.size()) {
            ++pos_;
            switch (current_line_[pos_]) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += current_line_[pos_];
            }
        } else {
            result += current_line_[pos_];
        }
        ++pos_;
    }

    if (peek_char() == '"')
        ++pos_;

    return result;
}

auto MirTextReader::expect(char c) -> bool {
    skip_whitespace();
    if (peek_char() == c) {
        ++pos_;
        return true;
    }
    return false;
}

auto MirTextReader::expect(const std::string& s) -> bool {
    skip_whitespace();
    if (current_line_.substr(pos_, s.size()) == s) {
        pos_ += s.size();
        return true;
    }
    return false;
}

auto MirTextReader::read_type() -> MirTypePtr {
    skip_whitespace();
    std::string type_name = read_identifier();

    // Primitive types
    if (type_name == "i8")
        return make_i8_type();
    if (type_name == "i16")
        return make_i16_type();
    if (type_name == "i32")
        return make_i32_type();
    if (type_name == "i64")
        return make_i64_type();
    if (type_name == "f32")
        return make_f32_type();
    if (type_name == "f64")
        return make_f64_type();
    if (type_name == "bool")
        return make_bool_type();
    if (type_name == "unit" || type_name == "()")
        return make_unit_type();
    if (type_name == "str")
        return make_str_type();
    if (type_name == "ptr")
        return make_ptr_type();

    // Pointer type: *T or *mut T
    if (type_name.empty() && peek_char() == '*') {
        ++pos_;
        bool is_mut = false;
        if (current_line_.substr(pos_, 3) == "mut") {
            is_mut = true;
            pos_ += 3;
        }
        auto pointee = read_type();
        return make_pointer_type(pointee, is_mut);
    }

    // Array type: [T; N]
    if (type_name.empty() && peek_char() == '[') {
        ++pos_;
        auto element = read_type();
        expect(';');
        auto size = static_cast<size_t>(read_number());
        expect(']');
        return make_array_type(element, size);
    }

    // Default to struct type
    return make_struct_type(type_name);
}

auto MirTextReader::read_value_ref() -> Value {
    skip_whitespace();
    Value v;
    v.type = make_i32_type(); // Default type

    if (peek_char() == '%') {
        ++pos_;
        v.id = static_cast<uint32_t>(read_number());
    } else {
        // Could be a constant
        v.id = INVALID_VALUE;
    }
    return v;
}

auto MirTextReader::read_function() -> Function {
    Function func;

    // Parse: func @name(params) -> return_type {
    skip_whitespace();
    if (!expect("func"))
        return func;

    skip_whitespace();
    if (peek_char() == '@')
        ++pos_;
    func.name = read_identifier();

    // Parse parameters
    if (expect('(')) {
        while (!expect(')') && pos_ < current_line_.size()) {
            skip_whitespace();
            if (peek_char() == '%')
                ++pos_;
            std::string param_name = read_identifier();
            if (expect(':')) {
                auto type = read_type();
                FunctionParam param;
                param.name = param_name;
                param.type = type;
                param.value_id = static_cast<uint32_t>(func.params.size());
                func.params.push_back(param);
            }
            expect(',');
        }
    }

    // Parse return type
    if (expect("->")) {
        func.return_type = read_type();
    } else {
        func.return_type = make_unit_type();
    }

    return func;
}

auto MirTextReader::read_block(Function& func) -> BasicBlock {
    BasicBlock block;

    // Parse: bb0: or entry:
    skip_whitespace();
    std::string label = read_identifier();

    // Remove trailing colon
    if (!label.empty() && label.back() == ':') {
        label.pop_back();
    } else if (peek_char() == ':') {
        ++pos_;
    }

    // Extract block ID from label (bb0 -> 0, bb1 -> 1, etc.)
    if (label.substr(0, 2) == "bb") {
        try {
            block.id = static_cast<uint32_t>(std::stoul(label.substr(2)));
        } catch (...) {
            block.id = func.next_block_id++;
        }
    } else {
        block.id = func.next_block_id++;
    }
    block.name = label;

    return block;
}

auto MirTextReader::read_instruction() -> InstructionData {
    InstructionData inst;
    inst.result = INVALID_VALUE;
    inst.type = make_unit_type();

    skip_whitespace();

    // Check for result assignment: %0 = ...
    if (peek_char() == '%') {
        ++pos_;
        inst.result = static_cast<uint32_t>(read_number());
        skip_whitespace();
        if (!expect('=')) {
            return inst;
        }
    }

    skip_whitespace();
    std::string opcode = read_identifier();

    // Parse different instruction types
    if (opcode == "add" || opcode == "sub" || opcode == "mul" || opcode == "div" ||
        opcode == "mod" || opcode == "eq" || opcode == "ne" || opcode == "lt" || opcode == "le" ||
        opcode == "gt" || opcode == "ge" || opcode == "and" || opcode == "or" || opcode == "xor" ||
        opcode == "shl" || opcode == "shr") {
        BinaryInst bin;
        if (opcode == "add")
            bin.op = BinOp::Add;
        else if (opcode == "sub")
            bin.op = BinOp::Sub;
        else if (opcode == "mul")
            bin.op = BinOp::Mul;
        else if (opcode == "div")
            bin.op = BinOp::Div;
        else if (opcode == "mod")
            bin.op = BinOp::Mod;
        else if (opcode == "eq")
            bin.op = BinOp::Eq;
        else if (opcode == "ne")
            bin.op = BinOp::Ne;
        else if (opcode == "lt")
            bin.op = BinOp::Lt;
        else if (opcode == "le")
            bin.op = BinOp::Le;
        else if (opcode == "gt")
            bin.op = BinOp::Gt;
        else if (opcode == "ge")
            bin.op = BinOp::Ge;
        else if (opcode == "and")
            bin.op = BinOp::And;
        else if (opcode == "or")
            bin.op = BinOp::Or;
        else if (opcode == "xor")
            bin.op = BinOp::BitXor;
        else if (opcode == "shl")
            bin.op = BinOp::Shl;
        else if (opcode == "shr")
            bin.op = BinOp::Shr;

        bin.left = read_value_ref();
        expect(',');
        bin.right = read_value_ref();
        inst.inst = bin;
    } else if (opcode == "neg" || opcode == "not" || opcode == "bitnot") {
        UnaryInst unary;
        if (opcode == "neg")
            unary.op = UnaryOp::Neg;
        else if (opcode == "not")
            unary.op = UnaryOp::Not;
        else
            unary.op = UnaryOp::BitNot;
        unary.operand = read_value_ref();
        inst.inst = unary;
    } else if (opcode == "load") {
        LoadInst load;
        load.ptr = read_value_ref();
        inst.inst = load;
    } else if (opcode == "store") {
        StoreInst store;
        store.value = read_value_ref();
        expect(',');
        store.ptr = read_value_ref();
        inst.inst = store;
    } else if (opcode == "alloca") {
        AllocaInst alloca_inst;
        alloca_inst.alloc_type = read_type();
        alloca_inst.name = "";
        inst.inst = alloca_inst;
    } else if (opcode == "call") {
        CallInst call;
        if (peek_char() == '@')
            ++pos_;
        call.func_name = read_identifier();
        if (expect('(')) {
            while (!expect(')') && pos_ < current_line_.size()) {
                call.args.push_back(read_value_ref());
                expect(',');
            }
        }
        call.return_type = make_unit_type();
        inst.inst = call;
    } else if (opcode == "const") {
        ConstantInst constant;
        skip_whitespace();
        if (peek_char() == '"') {
            constant.value = ConstString{read_string_literal()};
            inst.type = make_str_type();
        } else if (current_line_.substr(pos_, 4) == "true") {
            pos_ += 4;
            constant.value = ConstBool{true};
            inst.type = make_bool_type();
        } else if (current_line_.substr(pos_, 5) == "false") {
            pos_ += 5;
            constant.value = ConstBool{false};
            inst.type = make_bool_type();
        } else if (current_line_.substr(pos_, 4) == "unit") {
            pos_ += 4;
            constant.value = ConstUnit{};
            inst.type = make_unit_type();
        } else {
            int64_t val = read_number();
            constant.value = ConstInt{val, true, 32};
            inst.type = make_i32_type();
        }
        inst.inst = constant;
    } else if (opcode == "ret" || opcode == "return") {
        // Terminator, not regular instruction
        // Will be handled separately
    } else if (opcode == "br" || opcode == "branch") {
        // Terminator
    }

    return inst;
}

auto MirTextReader::read_terminator() -> std::optional<Terminator> {
    skip_whitespace();
    std::string opcode = read_identifier();

    if (opcode == "ret" || opcode == "return") {
        ReturnTerm ret;
        skip_whitespace();
        if (pos_ < current_line_.size() && peek_char() != '\0' && peek_char() != ';') {
            ret.value = read_value_ref();
        }
        return ret;
    } else if (opcode == "br" || opcode == "branch") {
        skip_whitespace();
        if (current_line_.substr(pos_, 2) == "if" ||
            (peek_char() == '%' || std::isdigit(peek_char()))) {
            // Conditional branch: br if %cond, bb1, bb2
            // or: br %cond, bb1, bb2
            if (current_line_.substr(pos_, 2) == "if") {
                pos_ += 2;
            }
            CondBranchTerm cond_term;
            cond_term.condition = read_value_ref();
            expect(',');
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                cond_term.true_block = static_cast<uint32_t>(read_number());
            }
            expect(',');
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                cond_term.false_block = static_cast<uint32_t>(read_number());
            }
            return cond_term;
        } else {
            // Unconditional branch: br bb1
            BranchTerm branch;
            skip_whitespace();
            if (current_line_.substr(pos_, 2) == "bb") {
                pos_ += 2;
                branch.target = static_cast<uint32_t>(read_number());
            }
            return branch;
        }
    } else if (opcode == "unreachable") {
        return UnreachableTerm{};
    }

    return std::nullopt;
}

auto MirTextReader::read_module() -> Module {
    Module module;

    // Parse module structure from text format
    // Format:
    // ; MIR Module: name
    // func @func_name(params) -> ret_type {
    //   bb0:
    //     %0 = ...
    //     ret %0
    // }

    Function* current_func = nullptr;
    BasicBlock* current_block = nullptr;
    bool in_function = false;

    while (next_line()) {
        skip_whitespace();

        // Skip empty lines and comments
        if (pos_ >= current_line_.size() || peek_char() == '\0') {
            continue;
        }

        if (peek_char() == ';') {
            // Comment line - check for module name
            if (current_line_.find("; MIR Module:") != std::string::npos) {
                pos_ = current_line_.find(":") + 1;
                skip_whitespace();
                module.name = read_identifier();
            }
            continue;
        }

        // Function definition
        if (current_line_.substr(pos_, 4) == "func") {
            auto func = read_function();
            module.functions.push_back(std::move(func));
            current_func = &module.functions.back();
            in_function = true;
            continue;
        }

        // End of function
        if (peek_char() == '}') {
            in_function = false;
            current_func = nullptr;
            current_block = nullptr;
            continue;
        }

        if (!in_function || !current_func) {
            continue;
        }

        // Block label
        if (std::isalpha(peek_char()) && current_line_.find(':') != std::string::npos) {
            auto block = read_block(*current_func);
            current_func->blocks.push_back(std::move(block));
            current_block = &current_func->blocks.back();
            continue;
        }

        // Instruction or terminator
        if (current_block) {
            // Check if it's a terminator
            std::string first_word;
            size_t saved_pos = pos_;
            if (peek_char() == '%') {
                // Skip result assignment
                while (pos_ < current_line_.size() && peek_char() != '=')
                    ++pos_;
                if (peek_char() == '=')
                    ++pos_;
                skip_whitespace();
            }
            first_word = read_identifier();
            pos_ = saved_pos; // restore

            if (first_word == "ret" || first_word == "return" || first_word == "br" ||
                first_word == "branch" || first_word == "unreachable") {
                auto term = read_terminator();
                if (term) {
                    current_block->terminator = *term;
                }
            } else {
                auto inst = read_instruction();
                if (inst.result != INVALID_VALUE ||
                    !std::holds_alternative<BinaryInst>(inst.inst) ||
                    std::get<BinaryInst>(inst.inst).op != BinOp::Add) {
                    current_block->instructions.push_back(std::move(inst));
                }
            }
        }
    }

    return module;
}

// ============================================================================
// Convenience Functions
// ============================================================================

auto serialize_binary(const Module& module) -> std::vector<uint8_t> {
    std::ostringstream oss(std::ios::binary);
    MirBinaryWriter writer(oss);
    writer.write_module(module);
    std::string data = oss.str();
    return std::vector<uint8_t>(data.begin(), data.end());
}

auto deserialize_binary(const std::vector<uint8_t>& data) -> Module {
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    MirBinaryReader reader(iss);
    return reader.read_module();
}

auto serialize_text(const Module& module, SerializeOptions options) -> std::string {
    std::ostringstream oss;
    MirTextWriter writer(oss, options);
    writer.write_module(module);
    return oss.str();
}

auto deserialize_text(const std::string& text) -> Module {
    std::istringstream iss(text);
    MirTextReader reader(iss);
    return reader.read_module();
}

auto write_mir_file(const Module& module, const std::string& path, bool binary) -> bool {
    std::ofstream file(path, binary ? std::ios::binary : std::ios::out);
    if (!file)
        return false;

    if (binary) {
        MirBinaryWriter writer(file);
        writer.write_module(module);
    } else {
        MirTextWriter writer(file);
        writer.write_module(module);
    }

    return true;
}

auto read_mir_file(const std::string& path) -> Module {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Module{};
    }

    // Check magic to determine format
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.seekg(0);

    if (magic == MIR_MAGIC) {
        MirBinaryReader reader(file);
        return reader.read_module();
    } else {
        // Try text format
        std::stringstream ss;
        ss << file.rdbuf();
        return deserialize_text(ss.str());
    }
}

} // namespace tml::mir
