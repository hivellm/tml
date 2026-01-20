//! MIR Codegen Instruction Emission
//!
//! This file contains instruction emission for the MIR-based code generator.
//! The emit_instruction method handles all MIR instruction types and generates
//! corresponding LLVM IR.
//!
//! ## Instruction Categories
//!
//! | Category     | Instructions                                          |
//! |--------------|-------------------------------------------------------|
//! | Arithmetic   | BinaryInst, UnaryInst                                 |
//! | Memory       | LoadInst, StoreInst, AllocaInst, GetElementPtrInst    |
//! | Aggregate    | ExtractValueInst, InsertValueInst, StructInitInst     |
//! | Control      | CallInst, MethodCallInst, SelectInst, PhiInst         |
//! | Type         | CastInst                                              |
//! | Constants    | ConstantInst                                          |
//! | Collections  | TupleInitInst, ArrayInitInst, EnumInitInst            |
//! | Atomic       | AtomicLoadInst, AtomicStoreInst, AtomicRMWInst, etc.  |

#include "codegen/mir_codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace tml::codegen {

void MirCodegen::emit_instruction(const mir::InstructionData& inst) {
    std::string result_reg;
    if (inst.result != mir::INVALID_VALUE) {
        result_reg = "%v" + std::to_string(inst.result);
        value_regs_[inst.result] = result_reg;
    }

    // Capture result type for struct init handling (class types need allocation)
    mir::MirTypePtr result_type = inst.type;

    std::visit(
        [this, &result_reg, &result_type, &inst](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, mir::BinaryInst>) {
                emit_binary_inst(i, result_reg, result_type, inst);

            } else if constexpr (std::is_same_v<T, mir::UnaryInst>) {
                emit_unary_inst(i, result_reg);

            } else if constexpr (std::is_same_v<T, mir::LoadInst>) {
                std::string ptr = get_value_reg(i.ptr);
                mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    " + result_reg + " = load " + type_str + ", ptr " + ptr);

            } else if constexpr (std::is_same_v<T, mir::StoreInst>) {
                std::string value = get_value_reg(i.value);
                std::string ptr = get_value_reg(i.ptr);
                mir::MirTypePtr type_ptr = i.value_type ? i.value_type : i.value.type;
                if (!type_ptr) {
                    type_ptr = mir::make_i32_type();
                }
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    store " + type_str + " " + value + ", ptr " + ptr);

            } else if constexpr (std::is_same_v<T, mir::AllocaInst>) {
                mir::MirTypePtr type_ptr = i.alloc_type ? i.alloc_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    " + result_reg + " = alloca " + type_str);

            } else if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                std::string base = get_value_reg(i.base);
                mir::MirTypePtr type_ptr = i.base_type ? i.base_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                emit("    " + result_reg + " = getelementptr " + type_str + ", ptr " + base);
                for (const auto& idx : i.indices) {
                    emit(", i32 " + get_value_reg(idx));
                }
                emitln();

            } else if constexpr (std::is_same_v<T, mir::ExtractValueInst>) {
                emit_extract_value_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::InsertValueInst>) {
                emit_insert_value_inst(i, result_reg);

            } else if constexpr (std::is_same_v<T, mir::CallInst>) {
                emit_call_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::MethodCallInst>) {
                emit_method_call_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::CastInst>) {
                emit_cast_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::PhiInst>) {
                emit_phi_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::ConstantInst>) {
                emit_constant_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::SelectInst>) {
                std::string cond = get_value_reg(i.condition);
                std::string true_val = get_value_reg(i.true_val);
                std::string false_val = get_value_reg(i.false_val);
                mir::MirTypePtr type_ptr = i.result_type ? i.result_type : i.true_val.type;
                if (!type_ptr) {
                    type_ptr = mir::make_i32_type();
                }
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    " + result_reg + " = select i1 " + cond + ", " + type_str + " " +
                       true_val + ", " + type_str + " " + false_val);

            } else if constexpr (std::is_same_v<T, mir::StructInitInst>) {
                emit_struct_init_inst(i, result_reg, result_type, inst);

            } else if constexpr (std::is_same_v<T, mir::EnumInitInst>) {
                // Initialize enum: { tag, payload }
                std::string enum_type = "%enum." + i.enum_name;
                // Insert tag
                std::string with_tag = "%tmp" + std::to_string(temp_counter_++);
                emitln("    " + with_tag + " = insertvalue " + enum_type + " undef, i32 " +
                       std::to_string(i.variant_index) + ", 0");
                // For simplicity, we're not handling payload here yet
                emitln("    " + result_reg + " = " + with_tag);

            } else if constexpr (std::is_same_v<T, mir::TupleInitInst>) {
                emit_tuple_init_inst(i, result_reg);

            } else if constexpr (std::is_same_v<T, mir::ArrayInitInst>) {
                emit_array_init_inst(i, result_reg);

            } else if constexpr (std::is_same_v<T, mir::AtomicLoadInst>) {
                emit_atomic_load_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::AtomicStoreInst>) {
                emit_atomic_store_inst(i);

            } else if constexpr (std::is_same_v<T, mir::AtomicRMWInst>) {
                emit_atomic_rmw_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::AtomicCmpXchgInst>) {
                emit_atomic_cmpxchg_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::FenceInst>) {
                std::string ordering = atomic_ordering_to_llvm(i.ordering);
                if (i.single_thread) {
                    emitln("    fence syncscope(\"singlethread\") " + ordering);
                } else {
                    emitln("    fence " + ordering);
                }
            }
        },
        inst.inst);
}

// ============================================================================
// Binary Instruction
// ============================================================================

void MirCodegen::emit_binary_inst(const mir::BinaryInst& i, const std::string& result_reg,
                                  const mir::MirTypePtr& result_type,
                                  const mir::InstructionData& inst) {
    std::string left = get_value_reg(i.left);
    std::string right = get_value_reg(i.right);

    // Check if it's a comparison
    bool is_comparison = (i.op >= mir::BinOp::Eq && i.op <= mir::BinOp::Ge);

    // For comparisons, always use the operand's type (not result_type which is bool)
    // For other operations, prefer InstructionData's type, then BinaryInst's result_type
    mir::MirTypePtr type_ptr;
    std::string type_str;

    // First check value_types_ for actual runtime type (important for intrinsic results)
    auto left_it = value_types_.find(i.left.id);
    auto right_it = value_types_.find(i.right.id);
    if (left_it != value_types_.end() && !left_it->second.empty()) {
        type_str = left_it->second;
    } else if (right_it != value_types_.end() && !right_it->second.empty()) {
        type_str = right_it->second;
    }

    if (type_str.empty()) {
        if (is_comparison) {
            // Comparison uses operand types - prefer left.type
            type_ptr = i.left.type ? i.left.type : i.right.type;
        } else {
            // Prefer InstructionData's type (result_type captured from inst.type),
            // then BinaryInst's result_type, then operand types
            type_ptr = result_type ? result_type : i.result_type;
            if (!type_ptr) {
                type_ptr = i.left.type ? i.left.type : i.right.type;
            }
        }
        if (!type_ptr) {
            // Fallback to i32 if no type info
            type_ptr = mir::make_i32_type();
        }
        type_str = mir_type_to_llvm(type_ptr);
    }

    bool is_float = (type_str == "double" || type_str == "float");
    bool is_signed = type_ptr ? type_ptr->is_signed() : true;

    // Get operand types from value_types_ first, then MIR types
    auto get_operand_type = [this](const mir::Value& v) -> std::string {
        auto it = value_types_.find(v.id);
        if (it != value_types_.end() && !it->second.empty()) {
            return it->second;
        }
        if (v.type) {
            return mir_type_to_llvm(v.type);
        }
        return "";
    };

    std::string left_type = get_operand_type(i.left);
    std::string right_type = get_operand_type(i.right);

    // Helper to coerce operand to target type if needed
    auto coerce_operand = [this, &type_str, is_signed](std::string& operand,
                                                       const std::string& operand_type_str) {
        if (operand_type_str.empty() || operand_type_str == type_str)
            return;

        // Check for integer type widening
        bool is_int_target = type_str[0] == 'i' && type_str.find("x") == std::string::npos;
        bool is_int_operand =
            operand_type_str[0] == 'i' && operand_type_str.find("x") == std::string::npos;
        if (is_int_target && is_int_operand) {
            int target_bits = std::stoi(type_str.substr(1));
            int operand_bits = std::stoi(operand_type_str.substr(1));
            if (target_bits > operand_bits) {
                std::string ext_tmp = "%ext" + std::to_string(temp_counter_++);
                std::string ext_op = is_signed ? "sext" : "zext";
                emitln("    " + ext_tmp + " = " + ext_op + " " + operand_type_str + " " + operand +
                       " to " + type_str);
                operand = ext_tmp;
            } else if (target_bits < operand_bits) {
                std::string trunc_tmp = "%trunc" + std::to_string(temp_counter_++);
                emitln("    " + trunc_tmp + " = trunc " + operand_type_str + " " + operand +
                       " to " + type_str);
                operand = trunc_tmp;
            }
        }
    };

    // Coerce operands if their types don't match the operation type
    coerce_operand(left, left_type);
    coerce_operand(right, right_type);

    if (is_comparison) {
        std::string pred = get_cmp_predicate(i.op, is_float, is_signed);
        if (is_float) {
            emitln("    " + result_reg + " = fcmp " + pred + " " + type_str + " " + left + ", " +
                   right);
        } else {
            emitln("    " + result_reg + " = icmp " + pred + " " + type_str + " " + left + ", " +
                   right);
        }
        // Comparison results are always i1 (bool)
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "i1";
        }
    } else {
        // Special case: string concatenation when adding two pointers (strings)
        if (type_str == "ptr" && i.op == mir::BinOp::Add) {
            emitln("    " + result_reg + " = call ptr @str_concat(ptr " + left + ", ptr " + right +
                   ")");
            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "ptr";
            }
        } else {
            std::string op_name = get_binop_name(i.op, is_float, is_signed);
            emitln("    " + result_reg + " = " + op_name + " " + type_str + " " + left + ", " +
                   right);
            // Store result type for subsequent operations
            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = type_str;
            }
        }
    }
}

// ============================================================================
// Unary Instruction
// ============================================================================

void MirCodegen::emit_unary_inst(const mir::UnaryInst& i, const std::string& result_reg) {
    std::string operand = get_value_reg(i.operand);

    // Use result_type if available, otherwise use operand's type
    mir::MirTypePtr type_ptr = i.result_type ? i.result_type : i.operand.type;
    if (!type_ptr) {
        type_ptr = mir::make_i32_type();
    }
    std::string type_str = mir_type_to_llvm(type_ptr);

    switch (i.op) {
    case mir::UnaryOp::Neg:
        if (type_ptr->is_float()) {
            emitln("    " + result_reg + " = fneg " + type_str + " " + operand);
        } else {
            emitln("    " + result_reg + " = sub " + type_str + " 0, " + operand);
        }
        break;
    case mir::UnaryOp::Not:
        emitln("    " + result_reg + " = xor i1 " + operand + ", true");
        break;
    case mir::UnaryOp::BitNot:
        emitln("    " + result_reg + " = xor " + type_str + " " + operand + ", -1");
        break;
    }
}

// ============================================================================
// Extract Value Instruction
// ============================================================================

void MirCodegen::emit_extract_value_inst(const mir::ExtractValueInst& i,
                                         const std::string& result_reg,
                                         const mir::InstructionData& inst) {
    // Use LLVM's native extractvalue instruction for direct field access.
    // This is much more efficient than alloca+gep+load and enables better optimization.
    std::string agg = get_value_reg(i.aggregate);
    mir::MirTypePtr type_ptr = i.aggregate_type ? i.aggregate_type : i.aggregate.type;
    std::string agg_type = mir_type_to_llvm(type_ptr);

    // Emit: %result = extractvalue <agg_type> <agg>, <idx1>, <idx2>, ...
    emit("    " + result_reg + " = extractvalue " + agg_type + " " + agg);
    for (auto idx : i.indices) {
        emit(", " + std::to_string(idx));
    }
    emitln();

    // Store result type for subsequent operations
    if (i.result_type && inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = mir_type_to_llvm(i.result_type);
    }
}

// ============================================================================
// Insert Value Instruction
// ============================================================================

void MirCodegen::emit_insert_value_inst(const mir::InsertValueInst& i,
                                        const std::string& result_reg) {
    std::string agg = get_value_reg(i.aggregate);
    std::string val = get_value_reg(i.value);
    mir::MirTypePtr agg_ptr = i.aggregate_type ? i.aggregate_type : i.aggregate.type;
    mir::MirTypePtr expected_ptr = i.value_type; // Expected type from struct field
    std::string agg_type = mir_type_to_llvm(agg_ptr);

    // Get expected type string
    std::string expected_type = expected_ptr ? mir_type_to_llvm(expected_ptr) : "";

    // Get actual type - first try MIR type, then stored type from value_types_
    std::string actual_type;
    if (i.value.type) {
        actual_type = mir_type_to_llvm(i.value.type);
    } else {
        // Look up from value_types_ (for constants and other values)
        auto it = value_types_.find(i.value.id);
        if (it != value_types_.end()) {
            actual_type = it->second;
        }
    }

    // Use expected type for the insertvalue instruction
    std::string val_type = !expected_type.empty() ? expected_type : actual_type;

    // Check for integer type width mismatch and insert cast if needed
    if (!expected_type.empty() && !actual_type.empty() && expected_type != actual_type) {
        // Both are integer types - need to cast
        bool is_int_expected =
            expected_type[0] == 'i' && expected_type.find("x") == std::string::npos;
        bool is_int_actual = actual_type[0] == 'i' && actual_type.find("x") == std::string::npos;
        if (is_int_expected && is_int_actual) {
            int expected_bits = std::stoi(expected_type.substr(1));
            int actual_bits = std::stoi(actual_type.substr(1));
            if (expected_bits > actual_bits) {
                // Need to extend
                std::string ext_tmp = "%ext" + std::to_string(temp_counter_++);
                emitln("    " + ext_tmp + " = sext " + actual_type + " " + val + " to " +
                       expected_type);
                val = ext_tmp;
            } else if (expected_bits < actual_bits) {
                // Need to truncate
                std::string trunc_tmp = "%trunc" + std::to_string(temp_counter_++);
                emitln("    " + trunc_tmp + " = trunc " + actual_type + " " + val + " to " +
                       expected_type);
                val = trunc_tmp;
            }
        }
    }

    emit("    " + result_reg + " = insertvalue " + agg_type + " " + agg + ", " + val_type + " " +
         val);
    for (auto idx : i.indices) {
        emit(", " + std::to_string(idx));
    }
    emitln();
}

// ============================================================================
// Call Instruction
// ============================================================================

void MirCodegen::emit_call_inst(const mir::CallInst& i, const std::string& result_reg,
                                const mir::InstructionData& inst) {
    // Skip ALL drop_ calls - they are no-ops for trivially destructible types
    if (i.func_name.rfind("drop_", 0) == 0) {
        return; // Skip all drops - they're no-ops
    }

    // Handle LLVM intrinsics (sqrt, sin, cos, etc.)
    std::string base_name = i.func_name;
    size_t last_colon = base_name.rfind("::");
    if (last_colon != std::string::npos) {
        base_name = base_name.substr(last_colon + 2);
    }

    // Check for math intrinsics that map to @llvm.* calls
    static const std::unordered_set<std::string> llvm_intrinsics = {
        "sqrt",  "sin",   "cos", "log",  "exp",    "pow",    "floor",   "ceil",
        "round", "trunc", "fma", "fabs", "minnum", "maxnum", "copysign"};

    if (llvm_intrinsics.count(base_name) > 0 && !i.args.empty()) {
        emit_llvm_intrinsic_call(i, base_name, result_reg, inst);
        return;
    }

    // Sanitize function name: replace :: with __ for LLVM compatibility
    std::string func_name = i.func_name;
    size_t pos = 0;
    while ((pos = func_name.find("::", pos)) != std::string::npos) {
        func_name.replace(pos, 2, "__");
        pos += 2;
    }

    // Pre-process arguments
    std::vector<std::string> processed_args;
    for (size_t j = 0; j < i.args.size(); ++j) {
        std::string arg = get_value_reg(i.args[j]);

        std::string actual_type;
        auto vt_it = value_types_.find(i.args[j].id);
        if (vt_it != value_types_.end()) {
            actual_type = vt_it->second;
        } else if (i.args[j].type) {
            actual_type = mir_type_to_llvm(i.args[j].type);
        }

        mir::MirTypePtr arg_ptr =
            (j < i.arg_types.size() && i.arg_types[j]) ? i.arg_types[j] : i.args[j].type;
        if (!arg_ptr) {
            arg_ptr = mir::make_i32_type();
        }
        std::string declared_type = mir_type_to_llvm(arg_ptr);

        std::string arg_type = declared_type;
        if (actual_type.find("%struct.") == 0) {
            arg_type = actual_type;
        } else if ((declared_type == "void" || declared_type == "i32") && !actual_type.empty() &&
                   actual_type != declared_type) {
            arg_type = actual_type;
        }

        processed_args.push_back(arg_type + " " + arg);
    }

    // Check if calling an sret function
    auto sret_it = sret_functions_.find(func_name);
    if (sret_it != sret_functions_.end()) {
        emit_sret_call(func_name, sret_it->second, processed_args, result_reg, inst);
    } else {
        emit_normal_call(i, func_name, processed_args, result_reg, inst);
    }
}

void MirCodegen::emit_llvm_intrinsic_call(const mir::CallInst& i, const std::string& base_name,
                                          const std::string& result_reg,
                                          const mir::InstructionData& inst) {
    std::string arg = get_value_reg(i.args[0]);
    std::string arg_type;
    if (i.args[0].type) {
        arg_type = mir_type_to_llvm(i.args[0].type);
    }
    if (arg_type.empty()) {
        auto it = value_types_.find(i.args[0].id);
        if (it != value_types_.end()) {
            arg_type = it->second;
        }
    }
    if (arg_type.empty()) {
        arg_type = "double";
    }

    std::string llvm_name = "@llvm." + base_name + "." + arg_type;
    if (!result_reg.empty()) {
        emit("    " + result_reg + " = ");
    } else {
        emit("    ");
    }

    if (base_name == "pow" || base_name == "minnum" || base_name == "maxnum" ||
        base_name == "copysign") {
        std::string arg2 = i.args.size() > 1 ? get_value_reg(i.args[1]) : arg;
        emitln("call " + arg_type + " " + llvm_name + "(" + arg_type + " " + arg + ", " + arg_type +
               " " + arg2 + ")");
    } else if (base_name == "fma") {
        std::string arg2 = i.args.size() > 1 ? get_value_reg(i.args[1]) : arg;
        std::string arg3 = i.args.size() > 2 ? get_value_reg(i.args[2]) : arg;
        emitln("call " + arg_type + " " + llvm_name + "(" + arg_type + " " + arg + ", " + arg_type +
               " " + arg2 + ", " + arg_type + " " + arg3 + ")");
    } else {
        emitln("call " + arg_type + " " + llvm_name + "(" + arg_type + " " + arg + ")");
    }

    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = arg_type;
    }
}

void MirCodegen::emit_sret_call(const std::string& func_name, const std::string& orig_ret_type,
                                const std::vector<std::string>& processed_args,
                                const std::string& result_reg, const mir::InstructionData& inst) {
    std::string sret_slot = "%sret.slot." + std::to_string(spill_counter_++);
    emitln("    " + sret_slot + " = alloca " + orig_ret_type + ", align 8");

    emit("    call void @" + func_name + "(ptr sret(" + orig_ret_type + ") " + sret_slot);
    for (const auto& arg : processed_args) {
        emit(", " + arg);
    }
    emitln(")");

    if (!result_reg.empty()) {
        emitln("    " + result_reg + " = load " + orig_ret_type + ", ptr " + sret_slot +
               ", align 8");
        value_types_[inst.result] = orig_ret_type;
    }
}

void MirCodegen::emit_normal_call(const mir::CallInst& i, const std::string& func_name,
                                  const std::vector<std::string>& processed_args,
                                  const std::string& result_reg, const mir::InstructionData& inst) {
    mir::MirTypePtr ret_ptr = i.return_type;
    if (!ret_ptr && inst.result != mir::INVALID_VALUE) {
        ret_ptr = mir::make_ptr_type();
    } else if (!ret_ptr) {
        ret_ptr = mir::make_unit_type();
    }
    std::string ret_type = mir_type_to_llvm(ret_ptr);

    if (ret_type != "void" && !result_reg.empty()) {
        emit("    " + result_reg + " = ");
    } else {
        emit("    ");
    }
    emit("call " + ret_type + " @" + func_name + "(");
    for (size_t j = 0; j < processed_args.size(); ++j) {
        if (j > 0) {
            emit(", ");
        }
        emit(processed_args[j]);
    }
    emitln(")");

    if (inst.result != mir::INVALID_VALUE && ret_type != "void") {
        value_types_[inst.result] = ret_type;
    }
}

// ============================================================================
// Method Call Instruction
// ============================================================================

void MirCodegen::emit_method_call_inst(const mir::MethodCallInst& i, const std::string& result_reg,
                                       const mir::InstructionData& inst) {
    mir::MirTypePtr ret_ptr = i.return_type;
    if (!ret_ptr && inst.result != mir::INVALID_VALUE) {
        ret_ptr = mir::make_ptr_type();
    } else if (!ret_ptr) {
        ret_ptr = mir::make_unit_type();
    }
    if (i.method_name == "to_string" && !result_reg.empty()) {
        ret_ptr = mir::make_ptr_type();
    }
    std::string ret_type = mir_type_to_llvm(ret_ptr);

    std::string recv_type = i.receiver_type.empty() ? "Unknown" : i.receiver_type;
    std::string receiver = get_value_reg(i.receiver);
    std::string receiver_actual_type;
    if (i.receiver.type) {
        receiver_actual_type = mir_type_to_llvm(i.receiver.type);
    }
    if (receiver_actual_type.empty() || receiver_actual_type == "ptr") {
        auto it = value_types_.find(i.receiver.id);
        if (it != value_types_.end()) {
            receiver_actual_type = it->second;
        }
    }

    static const std::unordered_set<std::string> primitive_tml_types = {
        "I8",  "I16", "I32",  "I64", "I128", "U8",   "U16",
        "U32", "U64", "U128", "F32", "F64",  "Bool", "Char"};
    bool is_primitive_tml = primitive_tml_types.count(recv_type) > 0;
    bool is_struct_type = receiver_actual_type.find("%struct.") == 0;

    std::string receiver_type_for_call = receiver_actual_type;

    if (is_primitive_tml) {
        receiver_type_for_call = receiver_actual_type;
    } else if (is_struct_type) {
        receiver_type_for_call = receiver_actual_type;
    } else if (receiver_actual_type == "ptr" || receiver_actual_type.empty()) {
        receiver_type_for_call = "ptr";
    } else {
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + receiver_actual_type);
        emitln("    store " + receiver_actual_type + " " + receiver + ", ptr " + spill_ptr);
        receiver = spill_ptr;
        receiver_type_for_call = "ptr";
    }

    if (ret_type != "void" && !result_reg.empty()) {
        emit("    " + result_reg + " = ");
    } else {
        emit("    ");
    }

    std::string func_name;
    if (is_primitive_tml) {
        std::string lower_type = recv_type;
        for (auto& c : lower_type)
            c = static_cast<char>(std::tolower(c));
        func_name = lower_type + "_" + i.method_name;
    } else {
        func_name = recv_type + "__" + i.method_name;
    }

    emit("call " + ret_type + " @" + func_name + "(");
    emit(receiver_type_for_call + " " + receiver);
    for (size_t j = 0; j < i.args.size(); ++j) {
        emit(", ");
        mir::MirTypePtr arg_ptr =
            (j < i.arg_types.size() && i.arg_types[j]) ? i.arg_types[j] : i.args[j].type;
        if (!arg_ptr) {
            arg_ptr = mir::make_i32_type();
        }
        std::string arg_type = mir_type_to_llvm(arg_ptr);
        std::string arg = get_value_reg(i.args[j]);
        emit(arg_type + " " + arg);
    }
    emitln(")");

    if (inst.result != mir::INVALID_VALUE && ret_type != "void") {
        value_types_[inst.result] = ret_type;
    }
}

// ============================================================================
// Cast Instruction
// ============================================================================

void MirCodegen::emit_cast_inst(const mir::CastInst& i, const std::string& result_reg,
                                const mir::InstructionData& inst) {
    std::string operand = get_value_reg(i.operand);
    mir::MirTypePtr src_ptr = i.source_type ? i.source_type : i.operand.type;
    if (!src_ptr) {
        src_ptr = mir::make_i32_type();
    }
    mir::MirTypePtr tgt_ptr = i.target_type ? i.target_type : mir::make_i32_type();
    std::string src_type = mir_type_to_llvm(src_ptr);
    std::string tgt_type = mir_type_to_llvm(tgt_ptr);

    std::string operand_actual_type;
    auto vt_it = value_types_.find(i.operand.id);
    if (vt_it != value_types_.end()) {
        operand_actual_type = vt_it->second;
    } else if (i.operand.type) {
        operand_actual_type = mir_type_to_llvm(i.operand.type);
    }

    if (!operand_actual_type.empty() && operand_actual_type != src_type) {
        src_type = operand_actual_type;
    }

    // If casting a struct value to ptr, spill it first
    if (tgt_type == "ptr" && operand_actual_type.find("%struct.") == 0) {
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + operand_actual_type);
        emitln("    store " + operand_actual_type + " " + operand + ", ptr " + spill_ptr);
        emitln("    " + result_reg + " = bitcast ptr " + spill_ptr + " to ptr");
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "ptr";
        }
    } else if (i.kind == mir::CastKind::Bitcast && src_type.find("%struct.") == 0 &&
               tgt_type.find("%struct.") == 0 && src_type != tgt_type) {
        // Class upcast: derived struct to base struct
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + src_type);
        emitln("    store " + src_type + " " + operand + ", ptr " + spill_ptr);
        emitln("    " + result_reg + " = load " + tgt_type + ", ptr " + spill_ptr);
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = tgt_type;
        }
    } else {
        static const char* cast_names[] = {"bitcast", "trunc",  "zext",     "sext",
                                           "fptrunc", "fpext",  "fptosi",   "fptoui",
                                           "sitofp",  "uitofp", "ptrtoint", "inttoptr"};
        std::string cast_name = cast_names[static_cast<int>(i.kind)];

        bool src_is_float = (src_type == "double" || src_type == "float");
        bool tgt_is_float = (tgt_type == "double" || tgt_type == "float");
        bool src_is_int = (src_type[0] == 'i' && src_type != "i1");
        bool tgt_is_int = (tgt_type[0] == 'i' && tgt_type != "i1");

        if (src_is_float && tgt_is_int) {
            cast_name = "fptosi";
        } else if (src_is_int && tgt_is_float) {
            cast_name = "sitofp";
        } else if (src_is_float && tgt_is_float) {
            if (src_type == "float" && tgt_type == "double") {
                cast_name = "fpext";
            } else if (src_type == "double" && tgt_type == "float") {
                cast_name = "fptrunc";
            }
        }

        emitln("    " + result_reg + " = " + cast_name + " " + src_type + " " + operand + " to " +
               tgt_type);
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = tgt_type;
        }
    }
}

// ============================================================================
// PHI Instruction
// ============================================================================

void MirCodegen::emit_phi_inst(const mir::PhiInst& i, const std::string& result_reg,
                               const mir::InstructionData& inst) {
    mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);

    if (i.incoming.empty()) {
        emitln("    " + result_reg + " = add " + type_str + " undef, 0");
    } else {
        emit("    " + result_reg + " = phi " + type_str + " ");
        for (size_t j = 0; j < i.incoming.size(); ++j) {
            if (j > 0) {
                emit(", ");
            }
            std::string val = get_value_reg(i.incoming[j].first);
            uint32_t block_id = i.incoming[j].second;
            auto label_it = block_labels_.find(block_id);
            std::string label;
            if (label_it != block_labels_.end()) {
                label = label_it->second;
            } else {
                label = "MISSING_BLOCK_" + std::to_string(block_id);
                std::cerr << "[CODEGEN] PHI references block " << block_id
                          << " which is not in block_labels_\n";
            }
            emit("[ " + val + ", %" + label + " ]");
        }
        emitln();
    }

    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = type_str;
    }
}

// ============================================================================
// Constant Instruction
// ============================================================================

void MirCodegen::emit_constant_inst(const mir::ConstantInst& i, const std::string& result_reg,
                                    const mir::InstructionData& inst) {
    std::visit(
        [this, &result_reg, &inst](const auto& c) {
            using C = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<C, mir::ConstInt>) {
                std::string type_str = "i" + std::to_string(c.bit_width);
                emitln("    " + result_reg + " = add " + type_str + " 0, " +
                       std::to_string(c.value));
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = type_str;
                }
            } else if constexpr (std::is_same_v<C, mir::ConstFloat>) {
                std::string type_str = c.is_f64 ? "double" : "float";
                std::ostringstream ss;
                ss << std::scientific << std::setprecision(17) << c.value;
                emitln("    " + result_reg + " = fadd " + type_str + " 0.0, " + ss.str());
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = type_str;
                }
            } else if constexpr (std::is_same_v<C, mir::ConstBool>) {
                emitln("    " + result_reg + " = add i1 0, " + std::string(c.value ? "1" : "0"));
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = "i1";
                }
            } else if constexpr (std::is_same_v<C, mir::ConstString>) {
                auto it = string_constants_.find(c.value);
                if (it != string_constants_.end()) {
                    emitln("    " + result_reg + " = bitcast ptr " + it->second + " to ptr");
                } else {
                    emitln("    " + result_reg + " = bitcast ptr null to ptr");
                }
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = "ptr";
                }
            } else if constexpr (std::is_same_v<C, mir::ConstUnit>) {
                // Unit type - no value needed
            }
        },
        i.value);
}

// ============================================================================
// Struct Init Instruction
// ============================================================================

void MirCodegen::emit_struct_init_inst(const mir::StructInitInst& i, const std::string& result_reg,
                                       const mir::MirTypePtr& result_type,
                                       const mir::InstructionData& inst) {
    std::string struct_type = "%struct." + i.struct_name;

    emitln("    ; STRUCTINIT: " + i.struct_name + " result=" + result_reg);

    bool is_class_type =
        result_type && std::holds_alternative<mir::MirPointerType>(result_type->kind);

    if (options_.emit_comments) {
        std::string type_info = result_type ? "has_type" : "null_type";
        if (result_type) {
            if (std::holds_alternative<mir::MirPointerType>(result_type->kind)) {
                type_info += "_ptr";
            } else if (std::holds_alternative<mir::MirStructType>(result_type->kind)) {
                type_info += "_struct";
            } else {
                type_info += "_other";
            }
        }
        emitln("    ; StructInit " + i.struct_name +
               " is_class=" + (is_class_type ? "true" : "false") + " type=" + type_info);
    }

    // Helper lambda to coerce integer types if needed
    auto coerce_int_type = [this](std::string& field_val, const std::string& expected_type,
                                  mir::ValueId val_id, mir::MirTypePtr /* actual_type_ptr */) {
        std::string actual_type;
        auto it = value_types_.find(val_id);
        if (it != value_types_.end()) {
            actual_type = it->second;
        }

        if (!actual_type.empty() && actual_type != expected_type) {
            bool is_int_expected = !expected_type.empty() && expected_type[0] == 'i' &&
                                   expected_type.find("x") == std::string::npos;
            bool is_int_actual =
                actual_type[0] == 'i' && actual_type.find("x") == std::string::npos;
            if (is_int_expected && is_int_actual) {
                int expected_bits = std::stoi(expected_type.substr(1));
                int actual_bits = std::stoi(actual_type.substr(1));
                if (expected_bits > actual_bits) {
                    std::string ext_tmp = "%ext" + std::to_string(temp_counter_++);
                    emitln("    " + ext_tmp + " = sext " + actual_type + " " + field_val + " to " +
                           expected_type);
                    field_val = ext_tmp;
                } else if (expected_bits < actual_bits) {
                    std::string trunc_tmp = "%trunc" + std::to_string(temp_counter_++);
                    emitln("    " + trunc_tmp + " = trunc " + actual_type + " " + field_val +
                           " to " + expected_type);
                    field_val = trunc_tmp;
                }
            }
        }
    };

    auto struct_it = struct_field_types_.find(i.struct_name);
    const std::vector<std::string>* expected_field_types = nullptr;
    if (struct_it != struct_field_types_.end()) {
        expected_field_types = &struct_it->second;
    }

    if (is_class_type) {
        // For class types: use alloca pattern (need to return pointer)
        std::string alloc_reg = "%tmp" + std::to_string(temp_counter_++);
        emitln("    " + alloc_reg + " = alloca " + struct_type);

        for (size_t j = 0; j < i.fields.size(); ++j) {
            std::string field_val = get_value_reg(i.fields[j]);

            std::string field_type;
            if (expected_field_types && j < expected_field_types->size()) {
                field_type = (*expected_field_types)[j];
            } else {
                mir::MirTypePtr field_ptr = (j < i.field_types.size() && i.field_types[j])
                                                ? i.field_types[j]
                                                : i.fields[j].type;
                if (!field_ptr) {
                    field_ptr = mir::make_i32_type();
                }
                field_type = mir_type_to_llvm(field_ptr);
            }

            coerce_int_type(field_val, field_type, i.fields[j].id, i.fields[j].type);

            std::string field_ptr_reg = "%gep" + std::to_string(temp_counter_++);
            emitln("    " + field_ptr_reg + " = getelementptr inbounds " + struct_type + ", ptr " +
                   alloc_reg + ", i32 0, i32 " + std::to_string(j));
            emitln("    store " + field_type + " " + field_val + ", ptr " + field_ptr_reg);
        }
        emitln("    " + result_reg + " = bitcast ptr " + alloc_reg + " to ptr");
    } else {
        // For non-class types: use insertvalue chain (much more efficient!)
        std::string current_val = "undef";

        for (size_t j = 0; j < i.fields.size(); ++j) {
            std::string field_val = get_value_reg(i.fields[j]);

            std::string field_type;
            if (expected_field_types && j < expected_field_types->size()) {
                field_type = (*expected_field_types)[j];
            } else {
                mir::MirTypePtr field_ptr = (j < i.field_types.size() && i.field_types[j])
                                                ? i.field_types[j]
                                                : i.fields[j].type;
                if (!field_ptr) {
                    field_ptr = mir::make_i32_type();
                }
                field_type = mir_type_to_llvm(field_ptr);
            }

            coerce_int_type(field_val, field_type, i.fields[j].id, i.fields[j].type);

            std::string next_reg = (j == i.fields.size() - 1)
                                       ? result_reg
                                       : ("%insert" + std::to_string(temp_counter_++));
            emitln("    " + next_reg + " = insertvalue " + struct_type + " " + current_val + ", " +
                   field_type + " " + field_val + ", " + std::to_string(j));
            current_val = next_reg;
        }

        if (i.fields.empty()) {
            emitln("    " + result_reg + " = insertvalue " + struct_type + " undef, i32 0, 0");
        }
    }

    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = struct_type;
    }
}

// ============================================================================
// Tuple Init Instruction
// ============================================================================

void MirCodegen::emit_tuple_init_inst(const mir::TupleInitInst& i, const std::string& result_reg) {
    mir::MirTypePtr tuple_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string tuple_type = mir_type_to_llvm(tuple_ptr);

    std::string alloc_reg = "%tmp" + std::to_string(temp_counter_++);
    emitln("    " + alloc_reg + " = alloca " + tuple_type);

    for (size_t j = 0; j < i.elements.size(); ++j) {
        std::string elem_val = get_value_reg(i.elements[j]);
        mir::MirTypePtr elem_ptr = (j < i.element_types.size() && i.element_types[j])
                                       ? i.element_types[j]
                                       : i.elements[j].type;
        if (!elem_ptr) {
            elem_ptr = mir::make_i32_type();
        }
        std::string elem_type = mir_type_to_llvm(elem_ptr);

        std::string elem_ptr_reg = "%gep" + std::to_string(temp_counter_++);
        emitln("    " + elem_ptr_reg + " = getelementptr inbounds " + tuple_type + ", ptr " +
               alloc_reg + ", i32 0, i32 " + std::to_string(j));
        emitln("    store " + elem_type + " " + elem_val + ", ptr " + elem_ptr_reg);
    }

    emitln("    " + result_reg + " = load " + tuple_type + ", ptr " + alloc_reg);
}

// ============================================================================
// Array Init Instruction
// ============================================================================

void MirCodegen::emit_array_init_inst(const mir::ArrayInitInst& i, const std::string& result_reg) {
    std::string current = "undef";
    mir::MirTypePtr array_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string array_type = mir_type_to_llvm(array_ptr);
    mir::MirTypePtr elem_ptr = i.element_type ? i.element_type : mir::make_i32_type();
    std::string elem_type = mir_type_to_llvm(elem_ptr);

    for (size_t j = 0; j < i.elements.size(); ++j) {
        std::string elem_val = get_value_reg(i.elements[j]);
        std::string next =
            (j == i.elements.size() - 1) ? result_reg : "%tmp" + std::to_string(temp_counter_++);
        emitln("    " + next + " = insertvalue " + array_type + " " + current + ", " + elem_type +
               " " + elem_val + ", " + std::to_string(j));
        current = next;
    }
}

// ============================================================================
// Atomic Instructions
// ============================================================================

void MirCodegen::emit_atomic_load_inst(const mir::AtomicLoadInst& i, const std::string& result_reg,
                                       const mir::InstructionData& inst) {
    std::string ptr = get_value_reg(i.ptr);
    mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string ordering = atomic_ordering_to_llvm(i.ordering);
    emitln("    " + result_reg + " = load atomic " + type_str + ", ptr " + ptr + " " + ordering +
           ", align " + std::to_string(get_type_alignment(type_ptr)));
    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = type_str;
    }
}

void MirCodegen::emit_atomic_store_inst(const mir::AtomicStoreInst& i) {
    std::string value = get_value_reg(i.value);
    std::string ptr = get_value_reg(i.ptr);
    mir::MirTypePtr type_ptr = i.value_type ? i.value_type : i.value.type;
    if (!type_ptr) {
        type_ptr = mir::make_i32_type();
    }
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string ordering = atomic_ordering_to_llvm(i.ordering);
    emitln("    store atomic " + type_str + " " + value + ", ptr " + ptr + " " + ordering +
           ", align " + std::to_string(get_type_alignment(type_ptr)));
}

void MirCodegen::emit_atomic_rmw_inst(const mir::AtomicRMWInst& i, const std::string& result_reg,
                                      const mir::InstructionData& inst) {
    std::string ptr = get_value_reg(i.ptr);
    std::string value = get_value_reg(i.value);
    mir::MirTypePtr type_ptr = i.value_type ? i.value_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string ordering = atomic_ordering_to_llvm(i.ordering);
    std::string op = atomic_rmw_op_to_llvm(i.op);
    emitln("    " + result_reg + " = atomicrmw " + op + " ptr " + ptr + ", " + type_str + " " +
           value + " " + ordering);
    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = type_str;
    }
}

void MirCodegen::emit_atomic_cmpxchg_inst(const mir::AtomicCmpXchgInst& i,
                                          const std::string& result_reg,
                                          const mir::InstructionData& inst) {
    std::string ptr = get_value_reg(i.ptr);
    std::string expected = get_value_reg(i.expected);
    std::string desired = get_value_reg(i.desired);
    mir::MirTypePtr type_ptr = i.value_type ? i.value_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string success_ord = atomic_ordering_to_llvm(i.success_ordering);
    std::string failure_ord = atomic_ordering_to_llvm(i.failure_ordering);
    std::string weak_str = i.weak ? " weak" : "";

    std::string cmpxchg_result = "%cmpxchg" + std::to_string(temp_counter_++);
    emitln("    " + cmpxchg_result + " = cmpxchg" + weak_str + " ptr " + ptr + ", " + type_str +
           " " + expected + ", " + type_str + " " + desired + " " + success_ord + " " +
           failure_ord);
    emitln("    " + result_reg + " = extractvalue { " + type_str + ", i1 } " + cmpxchg_result +
           ", 0");
    if (inst.result != mir::INVALID_VALUE) {
        value_types_[inst.result] = type_str;
    }
}

} // namespace tml::codegen
