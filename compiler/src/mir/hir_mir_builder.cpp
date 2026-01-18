//! # HIR to MIR Builder Implementation
//!
//! This file implements the main HirMirBuilder class that converts HIR to MIR.
//! Expression and statement building are split into separate files for
//! maintainability.
//!
//! ## File Organization
//!
//! - `hir_mir_builder.cpp` - Module/function building, type conversion (this file)
//! - `builder/hir_expr.cpp` - Expression lowering
//! - `builder/hir_stmt.cpp` - Statement lowering
//! - `builder/hir_pattern.cpp` - Pattern matching

#include "mir/hir_mir_builder.hpp"

#include <stdexcept>

namespace tml::mir {

// ============================================================================
// Constructor
// ============================================================================

HirMirBuilder::HirMirBuilder(const types::TypeEnv& env) : env_(env), module_{}, ctx_{} {}

// ============================================================================
// Main Entry Point
// ============================================================================

auto HirMirBuilder::build(const hir::HirModule& hir_module) -> Module {
    module_.name = hir_module.name;

    // Build all declarations
    build_declarations(hir_module);

    return std::move(module_);
}

// ============================================================================
// Declaration Building
// ============================================================================

void HirMirBuilder::build_declarations(const hir::HirModule& hir_module) {
    // Build structs first (may be referenced by functions)
    for (const auto& s : hir_module.structs) {
        build_struct(s);
    }

    // Build enums
    for (const auto& e : hir_module.enums) {
        build_enum(e);
    }

    // Build impl blocks (adds methods)
    for (const auto& impl : hir_module.impls) {
        build_impl(impl);
    }

    // Build standalone functions
    for (const auto& func : hir_module.functions) {
        build_function(func);
    }
}

void HirMirBuilder::build_struct(const hir::HirStruct& s) {
    StructDef mir_struct;
    mir_struct.name = s.mangled_name.empty() ? s.name : s.mangled_name;

    for (const auto& field : s.fields) {
        StructField mir_field;
        mir_field.name = field.name;
        mir_field.type = convert_type(field.type);
        mir_struct.fields.push_back(mir_field);
    }

    module_.structs.push_back(std::move(mir_struct));
}

void HirMirBuilder::build_enum(const hir::HirEnum& e) {
    EnumDef mir_enum;
    mir_enum.name = e.mangled_name.empty() ? e.name : e.mangled_name;

    for (const auto& variant : e.variants) {
        EnumVariant mir_variant;
        mir_variant.name = variant.name;

        for (const auto& payload_type : variant.payload_types) {
            mir_variant.payload_types.push_back(convert_type(payload_type));
        }

        mir_enum.variants.push_back(std::move(mir_variant));
    }

    module_.enums.push_back(std::move(mir_enum));
}

void HirMirBuilder::build_impl(const hir::HirImpl& impl) {
    // Build each method in the impl block
    for (const auto& method : impl.methods) {
        build_function(method);
    }
}

void HirMirBuilder::build_function(const hir::HirFunction& func) {
    Function mir_func;
    mir_func.name = func.mangled_name.empty() ? func.name : func.mangled_name;
    mir_func.return_type = convert_type(func.return_type);
    mir_func.is_public = func.is_public;
    mir_func.is_async = func.is_async;
    mir_func.attributes = func.attributes;

    // Create entry block
    mir_func.next_block_id = 0;
    mir_func.next_value_id = 0;
    auto entry_id = mir_func.create_block("entry");
    (void)entry_id; // Should be 0

    // Set up context
    ctx_.current_func = &mir_func;
    ctx_.current_block = 0;
    ctx_.variables.clear();
    ctx_.in_async_func = func.is_async;
    ctx_.next_suspension_id = 0;

    // Clear drop scopes and start fresh
    ctx_.drop_scopes.clear();
    ctx_.push_drop_scope();

    // Build parameters
    for (const auto& param : func.params) {
        FunctionParam mir_param;
        mir_param.name = param.name;
        mir_param.type = convert_type(param.type);
        mir_param.value_id = mir_func.fresh_value();

        mir_func.params.push_back(mir_param);

        // Register parameter as variable
        Value param_val{mir_param.value_id, mir_param.type};
        ctx_.variables[param.name] = param_val;
    }

    // Build function body if present
    if (func.body) {
        Value result = build_expr(*func.body);

        // If block doesn't end with terminator, emit return
        if (!is_terminated()) {
            // Emit drops before return
            emit_all_drops();

            if (mir_func.return_type->is_unit()) {
                emit_return(std::nullopt);
            } else {
                emit_return(result);
            }
        }
    } else {
        // Extern function - no body, just declaration
        // Clear blocks for extern functions
        mir_func.blocks.clear();
    }

    ctx_.pop_drop_scope();
    ctx_.current_func = nullptr;

    module_.functions.push_back(std::move(mir_func));
}

// ============================================================================
// Type Conversion
// ============================================================================

namespace {

// Helper to convert semantic type to MIR type
auto convert_type_impl(const types::TypePtr& type) -> MirTypePtr {
    if (!type) {
        return make_unit_type();
    }

    // Handle different type kinds
    if (auto* prim = std::get_if<types::PrimitiveType>(&type->kind)) {
        switch (prim->kind) {
        case types::PrimitiveKind::Unit:
            return make_unit_type();
        case types::PrimitiveKind::Bool:
            return make_bool_type();
        case types::PrimitiveKind::I8:
            return make_i8_type();
        case types::PrimitiveKind::I16:
            return make_i16_type();
        case types::PrimitiveKind::I32:
            return make_i32_type();
        case types::PrimitiveKind::I64:
            return make_i64_type();
        case types::PrimitiveKind::I128:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::I128}});
        case types::PrimitiveKind::U8:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U8}});
        case types::PrimitiveKind::U16:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U16}});
        case types::PrimitiveKind::U32:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U32}});
        case types::PrimitiveKind::U64:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U64}});
        case types::PrimitiveKind::U128:
            return std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U128}});
        case types::PrimitiveKind::F32:
            return make_f32_type();
        case types::PrimitiveKind::F64:
            return make_f64_type();
        case types::PrimitiveKind::Str:
            return make_str_type();
        case types::PrimitiveKind::Char:
            return std::make_shared<MirType>(
                MirType{MirPrimitiveType{PrimitiveType::U32}}); // Char as U32
        default:
            return make_unit_type();
        }
    }

    if (auto* ref = std::get_if<types::RefType>(&type->kind)) {
        auto pointee = convert_type_impl(ref->inner);
        return make_pointer_type(pointee, ref->is_mut);
    }

    if (auto* ptr = std::get_if<types::PtrType>(&type->kind)) {
        auto pointee = convert_type_impl(ptr->inner);
        return make_pointer_type(pointee, ptr->is_mut);
    }

    if (auto* arr = std::get_if<types::ArrayType>(&type->kind)) {
        auto element = convert_type_impl(arr->element);
        return make_array_type(element, arr->size);
    }

    if (auto* slice = std::get_if<types::SliceType>(&type->kind)) {
        auto element = convert_type_impl(slice->element);
        return std::make_shared<MirType>(MirType{MirSliceType{element}});
    }

    if (auto* tuple = std::get_if<types::TupleType>(&type->kind)) {
        std::vector<MirTypePtr> elements;
        for (const auto& elem : tuple->elements) {
            elements.push_back(convert_type_impl(elem));
        }
        return make_tuple_type(std::move(elements));
    }

    if (auto* named_type = std::get_if<types::NamedType>(&type->kind)) {
        std::vector<MirTypePtr> type_args;
        for (const auto& arg : named_type->type_args) {
            type_args.push_back(convert_type_impl(arg));
        }
        // For now, treat all named types as structs - enum vs struct is
        // distinguished at a higher level
        return make_struct_type(named_type->name, std::move(type_args));
    }

    if (auto* func_type = std::get_if<types::FuncType>(&type->kind)) {
        std::vector<MirTypePtr> params;
        for (const auto& param : func_type->params) {
            params.push_back(convert_type_impl(param));
        }
        auto ret = convert_type_impl(func_type->return_type);
        return std::make_shared<MirType>(MirType{MirFunctionType{std::move(params), ret}});
    }

    // Class types are returned as struct values (like regular structs)
    // When passed as parameters or stored, they may be passed by pointer
    if (auto* class_type = std::get_if<types::ClassType>(&type->kind)) {
        std::vector<MirTypePtr> type_args;
        for (const auto& arg : class_type->type_args) {
            type_args.push_back(convert_type_impl(arg));
        }
        return make_struct_type(class_type->name, std::move(type_args));
    }

    // Interface types are also passed as pointers (trait objects)
    if (auto* iface_type = std::get_if<types::InterfaceType>(&type->kind)) {
        return make_ptr_type();
    }

    // Default fallback
    return make_unit_type();
}

} // anonymous namespace

auto HirMirBuilder::convert_type(const hir::HirType& type) -> MirTypePtr {
    if (!type) {
        return make_unit_type();
    }
    // HirType is types::TypePtr, so delegate to helper
    return convert_type_impl(type);
}

// ============================================================================
// Helper Methods
// ============================================================================

auto HirMirBuilder::create_block(const std::string& name) -> uint32_t {
    if (!ctx_.current_func) {
        throw std::runtime_error("No current function in HirMirBuilder::create_block");
    }
    return ctx_.current_func->create_block(name);
}

void HirMirBuilder::switch_to_block(uint32_t block_id) {
    ctx_.current_block = block_id;
}

auto HirMirBuilder::is_terminated() const -> bool {
    if (!ctx_.current_func) {
        return true;
    }
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    return block && block->terminator.has_value();
}

auto HirMirBuilder::emit(Instruction inst, MirTypePtr type, SourceSpan span) -> Value {
    if (!ctx_.current_func) {
        throw std::runtime_error("No current function in HirMirBuilder::emit");
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block) {
        throw std::runtime_error("Invalid current block in HirMirBuilder::emit");
    }

    ValueId result_id = ctx_.current_func->fresh_value();
    Value result{result_id, type};

    InstructionData data;
    data.result = result_id;
    data.type = type;
    data.inst = std::move(inst);
    data.span = span;

    block->instructions.push_back(std::move(data));

    return result;
}

void HirMirBuilder::emit_void(Instruction inst, SourceSpan span) {
    if (!ctx_.current_func) {
        throw std::runtime_error("No current function in HirMirBuilder::emit_void");
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block) {
        throw std::runtime_error("Invalid current block in HirMirBuilder::emit_void");
    }

    InstructionData data;
    data.result = INVALID_VALUE;
    data.type = make_unit_type();
    data.inst = std::move(inst);
    data.span = span;

    block->instructions.push_back(std::move(data));
}

auto HirMirBuilder::emit_at_entry(Instruction inst, MirTypePtr type) -> Value {
    // Insert instruction at the start of the entry block.
    // This is used for allocas that need to dominate all uses (for LLVM's mem2reg).
    if (!ctx_.current_func) {
        throw std::runtime_error("No current function in HirMirBuilder::emit_at_entry");
    }

    auto& entry = ctx_.current_func->entry_block();

    ValueId result_id = ctx_.current_func->fresh_value();
    Value result{result_id, type};

    InstructionData data;
    data.result = result_id;
    data.type = type;
    data.inst = std::move(inst);

    // Insert at the beginning of the entry block
    entry.instructions.insert(entry.instructions.begin(), std::move(data));

    return result;
}

void HirMirBuilder::emit_return(std::optional<Value> value) {
    if (!ctx_.current_func) {
        return;
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value()) {
        return;
    }

    block->terminator = ReturnTerm{value};
}

void HirMirBuilder::emit_branch(uint32_t target) {
    if (!ctx_.current_func) {
        return;
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value()) {
        return;
    }

    block->terminator = BranchTerm{target};
    block->successors.push_back(target);

    auto* target_block = ctx_.current_func->get_block(target);
    if (target_block) {
        target_block->predecessors.push_back(ctx_.current_block);
    }
}

void HirMirBuilder::emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block) {
    if (!ctx_.current_func) {
        return;
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value()) {
        return;
    }

    block->terminator = CondBranchTerm{cond, true_block, false_block};
    block->successors.push_back(true_block);
    block->successors.push_back(false_block);

    auto* true_target = ctx_.current_func->get_block(true_block);
    if (true_target) {
        true_target->predecessors.push_back(ctx_.current_block);
    }

    auto* false_target = ctx_.current_func->get_block(false_block);
    if (false_target) {
        false_target->predecessors.push_back(ctx_.current_block);
    }
}

void HirMirBuilder::emit_unreachable() {
    if (!ctx_.current_func) {
        return;
    }

    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value()) {
        return;
    }

    block->terminator = UnreachableTerm{};
}

// ============================================================================
// Constants
// ============================================================================

auto HirMirBuilder::const_int(int64_t value, int bit_width, bool is_signed) -> Value {
    MirTypePtr type;
    if (is_signed) {
        switch (bit_width) {
        case 8:
            type = make_i8_type();
            break;
        case 16:
            type = make_i16_type();
            break;
        case 32:
            type = make_i32_type();
            break;
        case 64:
            type = make_i64_type();
            break;
        default:
            type = make_i32_type();
        }
    } else {
        switch (bit_width) {
        case 8:
            type = std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U8}});
            break;
        case 16:
            type = std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U16}});
            break;
        case 32:
            type = std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U32}});
            break;
        case 64:
            type = std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U64}});
            break;
        default:
            type = std::make_shared<MirType>(MirType{MirPrimitiveType{PrimitiveType::U32}});
        }
    }

    ConstantInst inst;
    inst.value = ConstInt{value, is_signed, bit_width};
    return emit(inst, type);
}

auto HirMirBuilder::const_float(double value, bool is_f64) -> Value {
    auto type = is_f64 ? make_f64_type() : make_f32_type();
    ConstantInst inst;
    inst.value = ConstFloat{value, is_f64};
    return emit(inst, type);
}

auto HirMirBuilder::const_bool(bool value) -> Value {
    ConstantInst inst;
    inst.value = ConstBool{value};
    return emit(inst, make_bool_type());
}

auto HirMirBuilder::const_string(const std::string& value) -> Value {
    ConstantInst inst;
    inst.value = ConstString{value};
    return emit(inst, make_str_type());
}

auto HirMirBuilder::const_unit() -> Value {
    ConstantInst inst;
    inst.value = ConstUnit{};
    return emit(inst, make_unit_type());
}

// ============================================================================
// Variable Management
// ============================================================================

auto HirMirBuilder::get_variable(const std::string& name) -> Value {
    auto it = ctx_.variables.find(name);
    if (it != ctx_.variables.end()) {
        return it->second;
    }
    // Variable not found - return invalid value
    return Value{INVALID_VALUE, make_unit_type()};
}

void HirMirBuilder::set_variable(const std::string& name, Value value) {
    ctx_.variables[name] = value;
}

// ============================================================================
// Operation Conversion
// ============================================================================

auto HirMirBuilder::convert_binop(hir::HirBinOp op) -> BinOp {
    switch (op) {
    case hir::HirBinOp::Add:
        return BinOp::Add;
    case hir::HirBinOp::Sub:
        return BinOp::Sub;
    case hir::HirBinOp::Mul:
        return BinOp::Mul;
    case hir::HirBinOp::Div:
        return BinOp::Div;
    case hir::HirBinOp::Mod:
        return BinOp::Mod;
    case hir::HirBinOp::Eq:
        return BinOp::Eq;
    case hir::HirBinOp::Ne:
        return BinOp::Ne;
    case hir::HirBinOp::Lt:
        return BinOp::Lt;
    case hir::HirBinOp::Le:
        return BinOp::Le;
    case hir::HirBinOp::Gt:
        return BinOp::Gt;
    case hir::HirBinOp::Ge:
        return BinOp::Ge;
    case hir::HirBinOp::And:
        return BinOp::And;
    case hir::HirBinOp::Or:
        return BinOp::Or;
    case hir::HirBinOp::BitAnd:
        return BinOp::BitAnd;
    case hir::HirBinOp::BitOr:
        return BinOp::BitOr;
    case hir::HirBinOp::BitXor:
        return BinOp::BitXor;
    case hir::HirBinOp::Shl:
        return BinOp::Shl;
    case hir::HirBinOp::Shr:
        return BinOp::Shr;
    default:
        return BinOp::Add; // Fallback
    }
}

auto HirMirBuilder::convert_compound_op(hir::HirCompoundOp op) -> BinOp {
    switch (op) {
    case hir::HirCompoundOp::Add:
        return BinOp::Add;
    case hir::HirCompoundOp::Sub:
        return BinOp::Sub;
    case hir::HirCompoundOp::Mul:
        return BinOp::Mul;
    case hir::HirCompoundOp::Div:
        return BinOp::Div;
    case hir::HirCompoundOp::Mod:
        return BinOp::Mod;
    case hir::HirCompoundOp::BitAnd:
        return BinOp::BitAnd;
    case hir::HirCompoundOp::BitOr:
        return BinOp::BitOr;
    case hir::HirCompoundOp::BitXor:
        return BinOp::BitXor;
    case hir::HirCompoundOp::Shl:
        return BinOp::Shl;
    case hir::HirCompoundOp::Shr:
        return BinOp::Shr;
    default:
        return BinOp::Add; // Fallback
    }
}

auto HirMirBuilder::is_comparison_op(hir::HirBinOp op) -> bool {
    switch (op) {
    case hir::HirBinOp::Eq:
    case hir::HirBinOp::Ne:
    case hir::HirBinOp::Lt:
    case hir::HirBinOp::Le:
    case hir::HirBinOp::Gt:
    case hir::HirBinOp::Ge:
        return true;
    default:
        return false;
    }
}

auto HirMirBuilder::convert_unaryop(hir::HirUnaryOp op) -> UnaryOp {
    switch (op) {
    case hir::HirUnaryOp::Neg:
        return UnaryOp::Neg;
    case hir::HirUnaryOp::Not:
        return UnaryOp::Not;
    case hir::HirUnaryOp::BitNot:
        return UnaryOp::BitNot;
    default:
        return UnaryOp::Neg; // Fallback
    }
}

// ============================================================================
// Drop Helpers
// ============================================================================

void HirMirBuilder::emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops) {
    for (const auto& drop : drops) {
        emit_drop_for_value(drop.value, drop.type, drop.type_name);
    }
}

void HirMirBuilder::emit_drop_for_value(Value value, const MirTypePtr& type,
                                        const std::string& type_name) {
    // Generate drop call: drop_TypeName(value)
    std::string drop_func = "drop_" + type_name;

    CallInst call;
    call.func_name = drop_func;
    call.args = {value};
    call.arg_types = {type};
    call.return_type = make_unit_type();

    emit_void(call);
}

void HirMirBuilder::emit_scope_drops() {
    auto drops = ctx_.get_drops_for_current_scope();
    emit_drop_calls(drops);
    // Mark these drops as emitted to avoid duplicate drops on return/break paths
    ctx_.mark_scope_dropped();
}

void HirMirBuilder::emit_all_drops() {
    auto drops = ctx_.get_all_drops();
    emit_drop_calls(drops);
    // Mark all drops as emitted
    ctx_.mark_all_dropped();
}

auto HirMirBuilder::get_type_name(const MirTypePtr& type) const -> std::string {
    if (!type) {
        return "Unit";
    }

    if (auto* prim = std::get_if<MirPrimitiveType>(&type->kind)) {
        switch (prim->kind) {
        case PrimitiveType::Unit:
            return "Unit";
        case PrimitiveType::Bool:
            return "Bool";
        case PrimitiveType::I8:
            return "I8";
        case PrimitiveType::I16:
            return "I16";
        case PrimitiveType::I32:
            return "I32";
        case PrimitiveType::I64:
            return "I64";
        case PrimitiveType::I128:
            return "I128";
        case PrimitiveType::U8:
            return "U8";
        case PrimitiveType::U16:
            return "U16";
        case PrimitiveType::U32:
            return "U32";
        case PrimitiveType::U64:
            return "U64";
        case PrimitiveType::U128:
            return "U128";
        case PrimitiveType::F32:
            return "F32";
        case PrimitiveType::F64:
            return "F64";
        case PrimitiveType::Ptr:
            return "Ptr";
        case PrimitiveType::Str:
            return "Str";
        }
    }

    if (auto* struct_type = std::get_if<MirStructType>(&type->kind)) {
        return struct_type->name;
    }

    if (auto* enum_type = std::get_if<MirEnumType>(&type->kind)) {
        return enum_type->name;
    }

    return "Unknown";
}

} // namespace tml::mir
