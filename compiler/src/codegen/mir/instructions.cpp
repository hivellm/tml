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

                // Emit bounds check if needed (for array indexing with known size)
                if (i.needs_bounds_check && i.known_array_size >= 0 && !i.indices.empty()) {
                    std::string idx_val = get_value_reg(i.indices[0]);
                    std::string size_str = std::to_string(i.known_array_size);
                    std::string label_id = std::to_string(temp_counter_++);

                    // Check index < 0 (signed comparison)
                    std::string below_zero = "%bc.below." + label_id;
                    emitln("    " + below_zero + " = icmp slt i32 " + idx_val + ", 0");

                    // Check index >= size
                    std::string above_max = "%bc.above." + label_id;
                    emitln("    " + above_max + " = icmp sge i32 " + idx_val + ", " + size_str);

                    // Combine checks
                    std::string oob = "%bc.oob." + label_id;
                    emitln("    " + oob + " = or i1 " + below_zero + ", " + above_max);

                    // Branch: out of bounds -> panic, in bounds -> continue
                    std::string panic_label = "bc.panic." + label_id;
                    std::string ok_label = "bc.ok." + label_id;
                    emitln("    br i1 " + oob + ", label %" + panic_label + ", label %" + ok_label);

                    // Panic block
                    emitln(panic_label + ":");
                    emitln("    call void @abort()");
                    emitln("    unreachable");

                    // OK block - continue with GEP
                    emitln(ok_label + ":");
                }

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
        // Use str_concat_opt for O(1) amortized complexity
        if (type_str == "ptr" && i.op == mir::BinOp::Add) {
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr " + left + ", ptr " +
                   right + ")");
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

    // Handle store_byte intrinsic: store_byte(ptr, offset, byte_val)
    // Optimized for tight loops - combines GEP and store in one intrinsic
    if (base_name == "store_byte" && i.args.size() >= 3) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr = get_value_reg(i.args[0]);
        std::string offset = get_value_reg(i.args[1]);
        std::string byte_val = get_value_reg(i.args[2]);

        // GEP to compute ptr + offset
        emitln("    %gep.sb." + id + " = getelementptr i8, ptr " + ptr + ", i64 " + offset);
        // Truncate i32 to i8
        emitln("    %trunc.sb." + id + " = trunc i32 " + byte_val + " to i8");
        // Store the byte
        emitln("    store i8 %trunc.sb." + id + ", ptr %gep.sb." + id);
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
    std::string recv_type = i.receiver_type.empty() ? "Unknown" : i.receiver_type;
    std::string receiver = get_value_reg(i.receiver);

    // V8-style optimization: Inline simple Text methods to avoid FFI overhead
    // This is critical for performance - each FFI call has ~10ns overhead
    // Uses select instruction for branchless code that LLVM can optimize well
    if (recv_type == "Text") {
        // Text struct layout (32 bytes total):
        // Heap mode (flags & 1 == 0):
        //   offset 0: ptr data
        //   offset 8: i64 len
        //   offset 16: i64 cap
        //   offset 24: i8 flags
        // SSO mode (flags & 1 == 1):
        //   offset 0-22: [23 x i8] data
        //   offset 23: i8 len
        //   offset 24: i8 flags

        if (i.method_name == "len" && !result_reg.empty()) {
            // Inline Text::len() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Load SSO len (offset 23, i8 -> i64)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    %sso_len_i8." + id + " = load i8, ptr %sso_len_ptr." + id);
            emitln("    %sso_len." + id + " = zext i8 %sso_len_i8." + id + " to i64");

            // Load heap len (offset 8, i64)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %heap_len." + id + " = load i64, ptr %heap_len_ptr." + id);

            // Branchless select: is_sso ? sso_len : heap_len
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i64 %sso_len." + id +
                   ", i64 %heap_len." + id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i64";
            }
            return;
        }

        if (i.method_name == "clear") {
            // Inline Text::clear() - branchless using conditional stores
            // For simplicity, we store to both locations (one will be ignored based on mode)
            // LLVM will optimize this if the mode is known
            std::string id = std::to_string(temp_counter_++);

            // Store 0 to SSO len location (offset 23)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    store i8 0, ptr %sso_len_ptr." + id);

            // Store 0 to heap len location (offset 8)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    store i64 0, ptr %heap_len_ptr." + id);

            return;
        }

        if (i.method_name == "is_empty" && !result_reg.empty()) {
            // Inline Text::is_empty() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Check SSO empty (offset 23)
            emitln("    %sso_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 23");
            emitln("    %sso_len." + id + " = load i8, ptr %sso_len_ptr." + id);
            emitln("    %sso_empty." + id + " = icmp eq i8 %sso_len." + id + ", 0");

            // Check heap empty (offset 8)
            emitln("    %heap_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %heap_len." + id + " = load i64, ptr %heap_len_ptr." + id);
            emitln("    %heap_empty." + id + " = icmp eq i64 %heap_len." + id + ", 0");

            // Branchless select
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i1 %sso_empty." + id +
                   ", i1 %heap_empty." + id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i1";
            }
            return;
        }

        if (i.method_name == "capacity" && !result_reg.empty()) {
            // Inline Text::capacity() using branchless select
            std::string id = std::to_string(temp_counter_++);

            // Load flags and check SSO bit
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_sso." + id + " = trunc i8 %flags." + id + " to i1");

            // Load heap capacity (offset 16)
            emitln("    %heap_cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %heap_cap." + id + " = load i64, ptr %heap_cap_ptr." + id);

            // Branchless select: is_sso ? 23 : heap_cap
            emitln("    " + result_reg + " = select i1 %is_sso." + id + ", i64 23, i64 %heap_cap." +
                   id);

            if (inst.result != mir::INVALID_VALUE) {
                value_types_[inst.result] = "i64";
            }
            return;
        }

        if (i.method_name == "push" && i.args.size() == 1) {
            // Inline Text::push() with fast path for heap mode
            // This is critical - push() is called millions of times in tight loops
            // Fast path: heap mode with space available -> direct store
            // Slow path: SSO mode or need realloc -> call FFI
            std::string id = std::to_string(temp_counter_++);
            std::string byte_val = get_value_reg(i.args[0]);

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %push_heap." + id + ", label %push_slow." +
                   id);

            // Heap fast path: check capacity and store directly
            emitln("  push_heap." + id + ":");
            // Load data ptr, len, cap
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);
            // Check if has space
            emitln("    %has_space." + id + " = icmp ult i64 %len." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %push_fast." + id +
                   ", label %push_slow." + id);

            // Fast store path
            emitln("  push_fast." + id + ":");
            // Truncate i32 byte to i8
            emitln("    %byte_i8." + id + " = trunc i32 " + byte_val + " to i8");
            // Store byte at data[len]
            emitln("    %store_ptr." + id + " = getelementptr i8, ptr %data_ptr." + id +
                   ", i64 %len." + id);
            emitln("    store i8 %byte_i8." + id + ", ptr %store_ptr." + id);
            // Increment len
            emitln("    %new_len." + id + " = add i64 %len." + id + ", 1");
            emitln("    store i64 %new_len." + id + ", ptr %len_ptr." + id);
            emitln("    br label %push_done." + id);

            // Slow path: SSO mode or needs realloc - call FFI
            emitln("  push_slow." + id + ":");
            emitln("    call void @tml_text_push(ptr " + receiver + ", i32 " + byte_val + ")");
            emitln("    br label %push_done." + id);

            emitln("  push_done." + id + ":");
            return;
        }

        if (i.method_name == "push_str" && i.args.size() == 1) {
            // Inline Text::push_str() with fast path for heap mode
            // push_str takes a Str argument, we need to:
            // 1. Get string ptr and len
            // 2. Check heap mode with sufficient capacity
            // 3. memcpy and update len (fast path) or call FFI (slow path)
            std::string id = std::to_string(temp_counter_++);
            std::string str_arg = get_value_reg(i.args[0]);

            // Check if argument is a constant string (compile-time length)
            auto const_it = value_string_contents_.find(i.args[0].id);
            if (const_it != value_string_contents_.end()) {
                // Constant string - use compile-time length (no FFI call!)
                size_t const_len = const_it->second.size();
                emitln("    %str_len." + id + " = add i64 0, " + std::to_string(const_len));
            } else {
                // Non-constant string - call @str_len at runtime
                emitln("    %str_len_i32." + id + " = call i32 @str_len(ptr " + str_arg + ")");
                emitln("    %str_len." + id + " = zext i32 %str_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pstr_heap." + id + ", label %pstr_slow." +
                   id);

            // Heap path: check capacity
            emitln("  pstr_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Check if len + str_len <= cap
            emitln("    %new_len." + id + " = add i64 %len." + id + ", %str_len." + id);
            emitln("    %has_space." + id + " = icmp ule i64 %new_len." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %pstr_fast." + id +
                   ", label %pstr_slow." + id);

            // Fast memcpy path
            emitln("  pstr_fast." + id + ":");
            // dst = data + len
            emitln("    %dst." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            // memcpy(dst, str, str_len)
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst." + id + ", ptr " + str_arg +
                   ", i64 %str_len." + id + ", i1 false)");
            // Update len
            emitln("    store i64 %new_len." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pstr_done." + id);

            // Slow path: SSO mode or needs realloc - call FFI
            emitln("  pstr_slow." + id + ":");
            emitln("    call void @tml_text_push_str_len(ptr " + receiver + ", ptr " + str_arg +
                   ", i64 %str_len." + id + ")");
            emitln("    br label %pstr_done." + id);

            emitln("  pstr_done." + id + ":");
            return;
        }

        if (i.method_name == "push_i64" && i.args.size() == 1) {
            // Inline Text::push_i64() with fully inline fast path for small non-negative integers
            // Uses lookup table for direct conversion, avoiding FFI for values 0-9999
            std::string id = std::to_string(temp_counter_++);
            std::string int_val = get_value_reg(i.args[0]);

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pi64_heap." + id + ", label %pi64_slow." +
                   id);

            // Heap path: check capacity and value range for inline conversion
            emitln("  pi64_heap." + id + ":");
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            // Check if value is in range [0, 9999] for inline conversion
            emitln("    %is_small." + id + " = icmp ult i64 " + int_val + ", 10000");
            emitln("    %is_non_neg." + id + " = icmp sge i64 " + int_val + ", 0");
            emitln("    %can_inline." + id + " = and i1 %is_small." + id + ", %is_non_neg." + id);
            // Need at most 5 bytes for values 0-9999
            emitln("    %needed." + id + " = add i64 %len." + id + ", 5");
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    %do_inline." + id + " = and i1 %can_inline." + id + ", %has_space." + id);
            emitln("    br i1 %do_inline." + id + ", label %pi64_inline." + id +
                   ", label %pi64_ffi." + id);

            // Inline fast path: direct conversion using lookup table
            emitln("  pi64_inline." + id + ":");
            // Get destination pointer
            emitln("    %dst." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            // Truncate to i32 for easier arithmetic
            emitln("    %n32." + id + " = trunc i64 " + int_val + " to i32");

            // Check digit count and branch accordingly
            emitln("    %lt10." + id + " = icmp ult i32 %n32." + id + ", 10");
            emitln("    br i1 %lt10." + id + ", label %pi64_1d." + id + ", label %pi64_ge10." + id);

            // 1 digit: n < 10
            emitln("  pi64_1d." + id + ":");
            emitln("    %d1." + id + " = add i32 %n32." + id + ", 48"); // '0' = 48
            emitln("    %d1_8." + id + " = trunc i32 %d1." + id + " to i8");
            emitln("    store i8 %d1_8." + id + ", ptr %dst." + id);
            emitln("    %newlen1." + id + " = add i64 %len." + id + ", 1");
            emitln("    store i64 %newlen1." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // >= 10: check if < 100
            emitln("  pi64_ge10." + id + ":");
            emitln("    %lt100." + id + " = icmp ult i32 %n32." + id + ", 100");
            emitln("    br i1 %lt100." + id + ", label %pi64_2d." + id + ", label %pi64_ge100." +
                   id);

            // 2 digits: 10 <= n < 100, use lookup table
            emitln("  pi64_2d." + id + ":");
            emitln("    %idx2." + id + " = mul i32 %n32." + id + ", 2");
            emitln("    %idx2_64." + id + " = zext i32 %idx2." + id + " to i64");
            emitln("    %pair2_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx2_64." + id);
            emitln("    %pair2." + id + " = load i16, ptr %pair2_ptr." + id);
            emitln("    store i16 %pair2." + id + ", ptr %dst." + id);
            emitln("    %newlen2." + id + " = add i64 %len." + id + ", 2");
            emitln("    store i64 %newlen2." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // >= 100: check if < 1000
            emitln("  pi64_ge100." + id + ":");
            emitln("    %lt1000." + id + " = icmp ult i32 %n32." + id + ", 1000");
            emitln("    br i1 %lt1000." + id + ", label %pi64_3d." + id + ", label %pi64_4d." + id);

            // 3 digits: 100 <= n < 1000
            emitln("  pi64_3d." + id + ":");
            emitln("    %q3." + id + " = udiv i32 %n32." + id + ", 100"); // First digit
            emitln("    %r3." + id + " = urem i32 %n32." + id + ", 100"); // Last 2 digits
            // Write first digit
            emitln("    %d3_first." + id + " = add i32 %q3." + id + ", 48");
            emitln("    %d3_first_8." + id + " = trunc i32 %d3_first." + id + " to i8");
            emitln("    store i8 %d3_first_8." + id + ", ptr %dst." + id);
            // Write last 2 digits from lookup
            emitln("    %idx3." + id + " = mul i32 %r3." + id + ", 2");
            emitln("    %idx3_64." + id + " = zext i32 %idx3." + id + " to i64");
            emitln("    %pair3_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx3_64." + id);
            emitln("    %pair3." + id + " = load i16, ptr %pair3_ptr." + id);
            emitln("    %dst3_1." + id + " = getelementptr i8, ptr %dst." + id + ", i64 1");
            emitln("    store i16 %pair3." + id + ", ptr %dst3_1." + id);
            emitln("    %newlen3." + id + " = add i64 %len." + id + ", 3");
            emitln("    store i64 %newlen3." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // 4 digits: 1000 <= n < 10000
            emitln("  pi64_4d." + id + ":");
            emitln("    %q4." + id + " = udiv i32 %n32." + id + ", 100"); // First 2 digits
            emitln("    %r4." + id + " = urem i32 %n32." + id + ", 100"); // Last 2 digits
            // Write first 2 digits from lookup
            emitln("    %idx4a." + id + " = mul i32 %q4." + id + ", 2");
            emitln("    %idx4a_64." + id + " = zext i32 %idx4a." + id + " to i64");
            emitln("    %pair4a_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx4a_64." + id);
            emitln("    %pair4a." + id + " = load i16, ptr %pair4a_ptr." + id);
            emitln("    store i16 %pair4a." + id + ", ptr %dst." + id);
            // Write last 2 digits from lookup
            emitln("    %idx4b." + id + " = mul i32 %r4." + id + ", 2");
            emitln("    %idx4b_64." + id + " = zext i32 %idx4b." + id + " to i64");
            emitln("    %pair4b_ptr." + id +
                   " = getelementptr [200 x i8], ptr @.digit_pairs, i64 0, i64 %idx4b_64." + id);
            emitln("    %pair4b." + id + " = load i16, ptr %pair4b_ptr." + id);
            emitln("    %dst4_2." + id + " = getelementptr i8, ptr %dst." + id + ", i64 2");
            emitln("    store i16 %pair4b." + id + ", ptr %dst4_2." + id);
            emitln("    %newlen4." + id + " = add i64 %len." + id + ", 4");
            emitln("    store i64 %newlen4." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pi64_done." + id);

            // FFI path: call unsafe version for large/negative values
            emitln("  pi64_ffi." + id + ":");
            // Check if we have capacity for FFI path (need 20 bytes max)
            emitln("    %needed_ffi." + id + " = add i64 %len." + id + ", 20");
            emitln("    %has_space_ffi." + id + " = icmp ule i64 %needed_ffi." + id + ", %cap." +
                   id);
            emitln("    br i1 %has_space_ffi." + id + ", label %pi64_ffi_fast." + id +
                   ", label %pi64_slow." + id);

            emitln("  pi64_ffi_fast." + id + ":");
            emitln("    %written." + id + " = call i64 @tml_text_push_i64_unsafe(ptr " + receiver +
                   ", i64 " + int_val + ")");
            emitln("    br label %pi64_done." + id);

            // Slow path: call regular FFI (handles reallocation)
            emitln("  pi64_slow." + id + ":");
            emitln("    call void @tml_text_push_i64(ptr " + receiver + ", i64 " + int_val + ")");
            emitln("    br label %pi64_done." + id);

            emitln("  pi64_done." + id + ":");
            return;
        }

        if (i.method_name == "push_formatted" && i.args.size() == 3) {
            // Inline Text::push_formatted(prefix, value, suffix) with fast path
            // Pattern: prefix_str + int + suffix_str
            std::string id = std::to_string(temp_counter_++);
            std::string prefix = get_value_reg(i.args[0]);
            std::string int_val = get_value_reg(i.args[1]);
            std::string suffix = get_value_reg(i.args[2]);

            // Check for constant strings (compile-time length - no FFI!)
            auto prefix_const = value_string_contents_.find(i.args[0].id);
            auto suffix_const = value_string_contents_.find(i.args[2].id);

            if (prefix_const != value_string_contents_.end()) {
                size_t len = prefix_const->second.size();
                emitln("    %prefix_len." + id + " = add i64 0, " + std::to_string(len));
            } else {
                emitln("    %prefix_len_i32." + id + " = call i32 @str_len(ptr " + prefix + ")");
                emitln("    %prefix_len." + id + " = zext i32 %prefix_len_i32." + id + " to i64");
            }

            if (suffix_const != value_string_contents_.end()) {
                size_t len = suffix_const->second.size();
                emitln("    %suffix_len." + id + " = add i64 0, " + std::to_string(len));
            } else {
                emitln("    %suffix_len_i32." + id + " = call i32 @str_len(ptr " + suffix + ")");
                emitln("    %suffix_len." + id + " = zext i32 %suffix_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %pfmt_heap." + id + ", label %pfmt_slow." +
                   id);

            // Heap path: check capacity for prefix + 20 (max int) + suffix
            emitln("  pfmt_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed space: len + prefix_len + 20 + suffix_len
            emitln("    %need1." + id + " = add i64 %len." + id + ", %prefix_len." + id);
            emitln("    %need2." + id + " = add i64 %need1." + id + ", 20");
            emitln("    %needed." + id + " = add i64 %need2." + id + ", %suffix_len." + id);
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %pfmt_fast." + id +
                   ", label %pfmt_slow." + id);

            // Fast path: memcpy prefix, inline int-to-string, memcpy suffix
            emitln("  pfmt_fast." + id + ":");
            // Copy prefix
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + prefix +
                   ", i64 %prefix_len." + id + ", i1 false)");
            // Update len after prefix
            emitln("    %len2." + id + " = add i64 %len." + id + ", %prefix_len." + id);
            emitln("    store i64 %len2." + id + ", ptr %len_ptr." + id);
            // Inline int-to-string conversion (creates multiple blocks, ends at merge)
            std::string int_id = id + ".i";
            emit_inline_int_to_string(int_id, int_val, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len2." + id, receiver, "pfmt_suffix." + id);
            emitln("    br label %pfmt_suffix." + id);
            // Continue with suffix after int conversion
            emitln("  pfmt_suffix." + id + ":");
            // Load updated len after int conversion
            emitln("    %len3." + id + " = load i64, ptr %len_ptr." + id);
            // Copy suffix
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len3." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + suffix +
                   ", i64 %suffix_len." + id + ", i1 false)");
            // Update final len
            emitln("    %len4." + id + " = add i64 %len3." + id + ", %suffix_len." + id);
            emitln("    store i64 %len4." + id + ", ptr %len_ptr." + id);
            emitln("    br label %pfmt_done." + id);

            // Slow path: call FFI
            emitln("  pfmt_slow." + id + ":");
            emitln("    call void @tml_text_push_formatted(ptr " + receiver + ", ptr " + prefix +
                   ", i64 %prefix_len." + id + ", i64 " + int_val + ", ptr " + suffix +
                   ", i64 %suffix_len." + id + ")");
            emitln("    br label %pfmt_done." + id);

            emitln("  pfmt_done." + id + ":");
            return;
        }

        if (i.method_name == "push_log" && i.args.size() == 7) {
            // Inline Text::push_log(s1, n1, s2, n2, s3, n3, s4) with fast path
            // Pattern: s1 + n1 + s2 + n2 + s3 + n3 + s4
            std::string id = std::to_string(temp_counter_++);
            std::string s1 = get_value_reg(i.args[0]);
            std::string n1 = get_value_reg(i.args[1]);
            std::string s2 = get_value_reg(i.args[2]);
            std::string n2 = get_value_reg(i.args[3]);
            std::string s3 = get_value_reg(i.args[4]);
            std::string n3 = get_value_reg(i.args[5]);
            std::string s4 = get_value_reg(i.args[6]);

            // Check for constant strings (compile-time length - no FFI!)
            auto s1_const = value_string_contents_.find(i.args[0].id);
            auto s2_const = value_string_contents_.find(i.args[2].id);
            auto s3_const = value_string_contents_.find(i.args[4].id);
            auto s4_const = value_string_contents_.find(i.args[6].id);

            // Get lengths for all 4 strings
            if (s1_const != value_string_contents_.end()) {
                emitln("    %s1_len." + id + " = add i64 0, " +
                       std::to_string(s1_const->second.size()));
            } else {
                emitln("    %s1_len_i32." + id + " = call i32 @str_len(ptr " + s1 + ")");
                emitln("    %s1_len." + id + " = zext i32 %s1_len_i32." + id + " to i64");
            }
            if (s2_const != value_string_contents_.end()) {
                emitln("    %s2_len." + id + " = add i64 0, " +
                       std::to_string(s2_const->second.size()));
            } else {
                emitln("    %s2_len_i32." + id + " = call i32 @str_len(ptr " + s2 + ")");
                emitln("    %s2_len." + id + " = zext i32 %s2_len_i32." + id + " to i64");
            }
            if (s3_const != value_string_contents_.end()) {
                emitln("    %s3_len." + id + " = add i64 0, " +
                       std::to_string(s3_const->second.size()));
            } else {
                emitln("    %s3_len_i32." + id + " = call i32 @str_len(ptr " + s3 + ")");
                emitln("    %s3_len." + id + " = zext i32 %s3_len_i32." + id + " to i64");
            }
            if (s4_const != value_string_contents_.end()) {
                emitln("    %s4_len." + id + " = add i64 0, " +
                       std::to_string(s4_const->second.size()));
            } else {
                emitln("    %s4_len_i32." + id + " = call i32 @str_len(ptr " + s4 + ")");
                emitln("    %s4_len." + id + " = zext i32 %s4_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %plog_heap." + id + ", label %plog_slow." +
                   id);

            // Heap path: check capacity for all strings + 60 (3 ints max 20 each)
            emitln("  plog_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed: len + s1 + s2 + s3 + s4 + 60
            emitln("    %need1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            emitln("    %need2." + id + " = add i64 %need1." + id + ", %s2_len." + id);
            emitln("    %need3." + id + " = add i64 %need2." + id + ", %s3_len." + id);
            emitln("    %need4." + id + " = add i64 %need3." + id + ", %s4_len." + id);
            emitln("    %needed." + id + " = add i64 %need4." + id + ", 60");
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %plog_fast." + id +
                   ", label %plog_slow." + id);

            // Fast path: inline all memcpy and int-to-string
            emitln("  plog_fast." + id + ":");

            // s1
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i1 false)");
            emitln("    %len1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            emitln("    store i64 %len1." + id + ", ptr %len_ptr." + id);

            // n1 (inline int-to-string)
            emit_inline_int_to_string(id + ".n1", n1, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len1." + id, receiver, "plog_s2." + id);
            emitln("    br label %plog_s2." + id);
            emitln("  plog_s2." + id + ":");
            emitln("    %len2." + id + " = load i64, ptr %len_ptr." + id);

            // s2
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len2." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + s2 +
                   ", i64 %s2_len." + id + ", i1 false)");
            emitln("    %len3." + id + " = add i64 %len2." + id + ", %s2_len." + id);
            emitln("    store i64 %len3." + id + ", ptr %len_ptr." + id);

            // n2 (inline int-to-string)
            emit_inline_int_to_string(id + ".n2", n2, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len3." + id, receiver, "plog_s3." + id);
            emitln("    br label %plog_s3." + id);
            emitln("  plog_s3." + id + ":");
            emitln("    %len4." + id + " = load i64, ptr %len_ptr." + id);

            // s3
            emitln("    %dst3." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len4." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst3." + id + ", ptr " + s3 +
                   ", i64 %s3_len." + id + ", i1 false)");
            emitln("    %len5." + id + " = add i64 %len4." + id + ", %s3_len." + id);
            emitln("    store i64 %len5." + id + ", ptr %len_ptr." + id);

            // n3 (inline int-to-string)
            emit_inline_int_to_string(id + ".n3", n3, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len5." + id, receiver, "plog_s4." + id);
            emitln("    br label %plog_s4." + id);
            emitln("  plog_s4." + id + ":");
            emitln("    %len6." + id + " = load i64, ptr %len_ptr." + id);

            // s4
            emitln("    %dst4." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len6." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst4." + id + ", ptr " + s4 +
                   ", i64 %s4_len." + id + ", i1 false)");
            emitln("    %len7." + id + " = add i64 %len6." + id + ", %s4_len." + id);
            emitln("    store i64 %len7." + id + ", ptr %len_ptr." + id);

            emitln("    br label %plog_done." + id);

            // Slow path: call FFI
            emitln("  plog_slow." + id + ":");
            emitln("    call void @tml_text_push_log(ptr " + receiver + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i64 " + n1 + ", ptr " + s2 + ", i64 %s2_len." + id +
                   ", i64 " + n2 + ", ptr " + s3 + ", i64 %s3_len." + id + ", i64 " + n3 +
                   ", ptr " + s4 + ", i64 %s4_len." + id + ")");
            emitln("    br label %plog_done." + id);

            emitln("  plog_done." + id + ":");
            return;
        }

        if (i.method_name == "push_path" && i.args.size() == 5) {
            // Inline Text::push_path(s1, n1, s2, n2, s3) with fast path
            // Pattern: s1 + n1 + s2 + n2 + s3
            std::string id = std::to_string(temp_counter_++);
            std::string s1 = get_value_reg(i.args[0]);
            std::string n1 = get_value_reg(i.args[1]);
            std::string s2 = get_value_reg(i.args[2]);
            std::string n2 = get_value_reg(i.args[3]);
            std::string s3 = get_value_reg(i.args[4]);

            // Check for constant strings (compile-time length - no FFI!)
            auto s1_const = value_string_contents_.find(i.args[0].id);
            auto s2_const = value_string_contents_.find(i.args[2].id);
            auto s3_const = value_string_contents_.find(i.args[4].id);

            // Get lengths for all 3 strings
            if (s1_const != value_string_contents_.end()) {
                emitln("    %s1_len." + id + " = add i64 0, " +
                       std::to_string(s1_const->second.size()));
            } else {
                emitln("    %s1_len_i32." + id + " = call i32 @str_len(ptr " + s1 + ")");
                emitln("    %s1_len." + id + " = zext i32 %s1_len_i32." + id + " to i64");
            }
            if (s2_const != value_string_contents_.end()) {
                emitln("    %s2_len." + id + " = add i64 0, " +
                       std::to_string(s2_const->second.size()));
            } else {
                emitln("    %s2_len_i32." + id + " = call i32 @str_len(ptr " + s2 + ")");
                emitln("    %s2_len." + id + " = zext i32 %s2_len_i32." + id + " to i64");
            }
            if (s3_const != value_string_contents_.end()) {
                emitln("    %s3_len." + id + " = add i64 0, " +
                       std::to_string(s3_const->second.size()));
            } else {
                emitln("    %s3_len_i32." + id + " = call i32 @str_len(ptr " + s3 + ")");
                emitln("    %s3_len." + id + " = zext i32 %s3_len_i32." + id + " to i64");
            }

            // Load flags and check if heap mode (flags == 0)
            emitln("    %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 24");
            emitln("    %flags." + id + " = load i8, ptr %flags_ptr." + id);
            emitln("    %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
            emitln("    br i1 %is_heap." + id + ", label %ppath_heap." + id +
                   ", label %ppath_slow." + id);

            // Heap path: check capacity for all strings + 40 (2 ints max 20 each)
            emitln("  ppath_heap." + id + ":");
            emitln("    %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 0");
            emitln("    %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
            emitln("    %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
            emitln("    %len." + id + " = load i64, ptr %len_ptr." + id);
            emitln("    %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 16");
            emitln("    %cap." + id + " = load i64, ptr %cap_ptr." + id);

            // Calculate needed: len + s1 + s2 + s3 + 40
            emitln("    %need1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            emitln("    %need2." + id + " = add i64 %need1." + id + ", %s2_len." + id);
            emitln("    %need3." + id + " = add i64 %need2." + id + ", %s3_len." + id);
            emitln("    %needed." + id + " = add i64 %need3." + id + ", 40");
            emitln("    %has_space." + id + " = icmp ule i64 %needed." + id + ", %cap." + id);
            emitln("    br i1 %has_space." + id + ", label %ppath_fast." + id +
                   ", label %ppath_slow." + id);

            // Fast path: inline all memcpy and int-to-string
            emitln("  ppath_fast." + id + ":");

            // s1
            emitln("    %dst1." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst1." + id + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i1 false)");
            emitln("    %len1." + id + " = add i64 %len." + id + ", %s1_len." + id);
            emitln("    store i64 %len1." + id + ", ptr %len_ptr." + id);

            // n1 (inline int-to-string)
            emit_inline_int_to_string(id + ".n1", n1, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len1." + id, receiver, "ppath_s2." + id);
            emitln("    br label %ppath_s2." + id);
            emitln("  ppath_s2." + id + ":");
            emitln("    %len2." + id + " = load i64, ptr %len_ptr." + id);

            // s2
            emitln("    %dst2." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len2." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst2." + id + ", ptr " + s2 +
                   ", i64 %s2_len." + id + ", i1 false)");
            emitln("    %len3." + id + " = add i64 %len2." + id + ", %s2_len." + id);
            emitln("    store i64 %len3." + id + ", ptr %len_ptr." + id);

            // n2 (inline int-to-string)
            emit_inline_int_to_string(id + ".n2", n2, "%data_ptr." + id, "%len_ptr." + id,
                                      "%len3." + id, receiver, "ppath_s3." + id);
            emitln("    br label %ppath_s3." + id);
            emitln("  ppath_s3." + id + ":");
            emitln("    %len4." + id + " = load i64, ptr %len_ptr." + id);

            // s3
            emitln("    %dst3." + id + " = getelementptr i8, ptr %data_ptr." + id + ", i64 %len4." +
                   id);
            emitln("    call void @llvm.memcpy.p0.p0.i64(ptr %dst3." + id + ", ptr " + s3 +
                   ", i64 %s3_len." + id + ", i1 false)");
            emitln("    %len5." + id + " = add i64 %len4." + id + ", %s3_len." + id);
            emitln("    store i64 %len5." + id + ", ptr %len_ptr." + id);

            emitln("    br label %ppath_done." + id);

            // Slow path: call FFI
            emitln("  ppath_slow." + id + ":");
            emitln("    call void @tml_text_push_path(ptr " + receiver + ", ptr " + s1 +
                   ", i64 %s1_len." + id + ", i64 " + n1 + ", ptr " + s2 + ", i64 %s2_len." + id +
                   ", i64 " + n2 + ", ptr " + s3 + ", i64 %s3_len." + id + ")");
            emitln("    br label %ppath_done." + id);

            emitln("  ppath_done." + id + ":");
            return;
        }
    }

    // Normal method call path (non-inlined)
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
                    // Store string content for compile-time length optimization
                    value_string_contents_[inst.result] = c.value;
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

// Helper: emit inline int-to-string conversion
// Returns the LLVM register containing the new length after conversion
// Only handles non-negative values < 100000, falls back to FFI for others
auto MirCodegen::emit_inline_int_to_string(const std::string& id, const std::string& int_val,
                                           const std::string& dst_ptr, const std::string& len_ptr,
                                           const std::string& current_len,
                                           const std::string& receiver,
                                           [[maybe_unused]] const std::string& done_label)
    -> std::string {
    // Branch labels
    std::string big_check = "i2s.big." + id;
    std::string ffi_label = "i2s.ffi." + id;
    std::string d1_label = "i2s.1d." + id;
    std::string d2_label = "i2s.2d." + id;
    std::string d3_label = "i2s.3d." + id;
    std::string d4_label = "i2s.4d." + id;
    std::string d5_label = "i2s.5d." + id;
    std::string merge_label = "i2s.merge." + id;

    // Check negative
    emitln("    %is.neg." + id + " = icmp slt i64 " + int_val + ", 0");
    emitln("    br i1 %is.neg." + id + ", label %" + ffi_label + ", label %" + big_check);

    // Check >= 100000 (supports 5 digits now)
    emitln(big_check + ":");
    emitln("    %is.big." + id + " = icmp sge i64 " + int_val + ", 100000");
    emitln("    br i1 %is.big." + id + ", label %" + ffi_label + ", label %" + d1_label);

    // Check 1 digit (0-9)
    emitln(d1_label + ":");
    emitln("    %is.1d." + id + " = icmp slt i64 " + int_val + ", 10");
    emitln("    br i1 %is.1d." + id + ", label %do.1d." + id + ", label %" + d2_label);

    // 1 digit: store single char '0' + val
    emitln("do.1d." + id + ":");
    emitln("    %d1.trunc." + id + " = trunc i64 " + int_val + " to i8");
    emitln("    %d1.char." + id + " = add i8 %d1.trunc." + id + ", 48");
    emitln("    %d1.dst." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i8 %d1.char." + id + ", ptr %d1.dst." + id);
    emitln("    %d1.newlen." + id + " = add i64 " + current_len + ", 1");
    emitln("    br label %" + merge_label);

    // Check 2 digits (10-99)
    emitln(d2_label + ":");
    emitln("    %is.2d." + id + " = icmp slt i64 " + int_val + ", 100");
    emitln("    br i1 %is.2d." + id + ", label %do.2d." + id + ", label %" + d3_label);

    // 2 digits: lookup table
    emitln("do.2d." + id + ":");
    emitln("    %d2.idx." + id + " = shl i64 " + int_val + ", 1"); // *2 for pair offset
    emitln("    %d2.ptr." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d2.idx." + id);
    emitln("    %d2.pair." + id + " = load i16, ptr %d2.ptr." + id);
    emitln("    %d2.dst." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i16 %d2.pair." + id + ", ptr %d2.dst." + id);
    emitln("    %d2.newlen." + id + " = add i64 " + current_len + ", 2");
    emitln("    br label %" + merge_label);

    // Check 3 digits (100-999)
    emitln(d3_label + ":");
    emitln("    %is.3d." + id + " = icmp slt i64 " + int_val + ", 1000");
    emitln("    br i1 %is.3d." + id + ", label %do.3d." + id + ", label %" + d4_label);

    // 3 digits: first digit + lookup for last 2
    emitln("do.3d." + id + ":");
    emitln("    %d3.hi." + id + " = sdiv i64 " + int_val + ", 100"); // first digit
    emitln("    %d3.lo." + id + " = srem i64 " + int_val + ", 100"); // last 2 digits
    emitln("    %d3.hi8." + id + " = trunc i64 %d3.hi." + id + " to i8");
    emitln("    %d3.char." + id + " = add i8 %d3.hi8." + id + ", 48");
    emitln("    %d3.dst0." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i8 %d3.char." + id + ", ptr %d3.dst0." + id);
    emitln("    %d3.off1." + id + " = add i64 " + current_len + ", 1");
    emitln("    %d3.idx." + id + " = shl i64 %d3.lo." + id + ", 1");
    emitln("    %d3.ptr." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d3.idx." + id);
    emitln("    %d3.pair." + id + " = load i16, ptr %d3.ptr." + id);
    emitln("    %d3.dst1." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 %d3.off1." + id);
    emitln("    store i16 %d3.pair." + id + ", ptr %d3.dst1." + id);
    emitln("    %d3.newlen." + id + " = add i64 " + current_len + ", 3");
    emitln("    br label %" + merge_label);

    // Check 4 digits (1000-9999)
    emitln(d4_label + ":");
    emitln("    %is.4d." + id + " = icmp slt i64 " + int_val + ", 10000");
    emitln("    br i1 %is.4d." + id + ", label %do.4d." + id + ", label %" + d5_label);

    // 4 digits (1000-9999): 2x lookup table
    emitln("do.4d." + id + ":");
    emitln("    %d4.hi." + id + " = sdiv i64 " + int_val + ", 100"); // first 2 digits
    emitln("    %d4.lo." + id + " = srem i64 " + int_val + ", 100"); // last 2 digits
    emitln("    %d4.idx.hi." + id + " = shl i64 %d4.hi." + id + ", 1");
    emitln("    %d4.ptr.hi." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d4.idx.hi." + id);
    emitln("    %d4.pair.hi." + id + " = load i16, ptr %d4.ptr.hi." + id);
    emitln("    %d4.dst0." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i16 %d4.pair.hi." + id + ", ptr %d4.dst0." + id);
    emitln("    %d4.off2." + id + " = add i64 " + current_len + ", 2");
    emitln("    %d4.idx.lo." + id + " = shl i64 %d4.lo." + id + ", 1");
    emitln("    %d4.ptr.lo." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d4.idx.lo." + id);
    emitln("    %d4.pair.lo." + id + " = load i16, ptr %d4.ptr.lo." + id);
    emitln("    %d4.dst2." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 %d4.off2." + id);
    emitln("    store i16 %d4.pair.lo." + id + ", ptr %d4.dst2." + id);
    emitln("    %d4.newlen." + id + " = add i64 " + current_len + ", 4");
    emitln("    br label %" + merge_label);

    // 5 digits (10000-99999): first digit + 2x lookup table
    emitln(d5_label + ":");
    emitln("    %d5.hi." + id + " = sdiv i64 " + int_val + ", 10000");   // first digit
    emitln("    %d5.rest." + id + " = srem i64 " + int_val + ", 10000"); // last 4 digits
    emitln("    %d5.hi8." + id + " = trunc i64 %d5.hi." + id + " to i8");
    emitln("    %d5.char." + id + " = add i8 %d5.hi8." + id + ", 48");
    emitln("    %d5.dst0." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i8 %d5.char." + id + ", ptr %d5.dst0." + id);
    // Process remaining 4 digits using two lookups
    emitln("    %d5.mid." + id + " = sdiv i64 %d5.rest." + id + ", 100"); // digits 2-3
    emitln("    %d5.lo." + id + " = srem i64 %d5.rest." + id + ", 100");  // digits 4-5
    emitln("    %d5.off1." + id + " = add i64 " + current_len + ", 1");
    emitln("    %d5.idx.mid." + id + " = shl i64 %d5.mid." + id + ", 1");
    emitln("    %d5.ptr.mid." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d5.idx.mid." +
           id);
    emitln("    %d5.pair.mid." + id + " = load i16, ptr %d5.ptr.mid." + id);
    emitln("    %d5.dst1." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 %d5.off1." + id);
    emitln("    store i16 %d5.pair.mid." + id + ", ptr %d5.dst1." + id);
    emitln("    %d5.off3." + id + " = add i64 " + current_len + ", 3");
    emitln("    %d5.idx.lo." + id + " = shl i64 %d5.lo." + id + ", 1");
    emitln("    %d5.ptr.lo." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d5.idx.lo." + id);
    emitln("    %d5.pair.lo." + id + " = load i16, ptr %d5.ptr.lo." + id);
    emitln("    %d5.dst3." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 %d5.off3." + id);
    emitln("    store i16 %d5.pair.lo." + id + ", ptr %d5.dst3." + id);
    emitln("    %d5.newlen." + id + " = add i64 " + current_len + ", 5");
    emitln("    br label %" + merge_label);

    // FFI fallback for negative or values >= 100000
    emitln(ffi_label + ":");
    emitln("    call void @tml_text_push_i64_unsafe(ptr " + receiver + ", i64 " + int_val + ")");
    emitln("    %ffi.len.ptr." + id + " = getelementptr i8, ptr " + receiver + ", i64 8");
    emitln("    %ffi.newlen." + id + " = load i64, ptr %ffi.len.ptr." + id);
    emitln("    br label %" + merge_label);

    // Merge block with phi for new length
    emitln(merge_label + ":");
    emitln("    %newlen." + id + " = phi i64 [ %d1.newlen." + id + ", %do.1d." + id + " ], " +
           "[ %d2.newlen." + id + ", %do.2d." + id + " ], " + "[ %d3.newlen." + id + ", %do.3d." +
           id + " ], " + "[ %d4.newlen." + id + ", %do.4d." + id + " ], " + "[ %d5.newlen." + id +
           ", %" + d5_label + " ], " + "[ %ffi.newlen." + id + ", %" + ffi_label + " ]");

    // Store new length
    emitln("    store i64 %newlen." + id + ", ptr " + len_ptr);

    return "%newlen." + id;
}

} // namespace tml::codegen
