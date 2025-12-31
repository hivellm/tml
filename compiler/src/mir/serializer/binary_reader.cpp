// MIR Binary Reader Implementation

#include "serializer_internal.hpp"

namespace tml::mir {

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

} // namespace tml::mir
