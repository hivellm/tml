TML_MODULE("codegen_x86")

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
                std::string volatile_kw = i.is_volatile ? "volatile " : "";
                emitln("    " + result_reg + " = load " + volatile_kw + type_str + ", ptr " + ptr);
                // Track the loaded value's type for method call receiver handling
                value_types_[inst.result] = type_str;

            } else if constexpr (std::is_same_v<T, mir::StoreInst>) {
                std::string value = get_value_reg(i.value);
                std::string ptr = get_value_reg(i.ptr);
                mir::MirTypePtr type_ptr = i.value_type ? i.value_type : i.value.type;
                if (!type_ptr) {
                    type_ptr = mir::make_i32_type();
                }
                std::string type_str = mir_type_to_llvm(type_ptr);
                std::string volatile_kw = i.is_volatile ? "volatile " : "";
                emitln("    store " + volatile_kw + type_str + " " + value + ", ptr " + ptr);

            } else if constexpr (std::is_same_v<T, mir::AllocaInst>) {
                mir::MirTypePtr type_ptr = i.alloc_type ? i.alloc_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    " + result_reg + " = alloca " + type_str);
                // Track alloca as pointer type for method call receiver handling
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = "ptr";
                }

            } else if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                std::string base = get_value_reg(i.base);
                mir::MirTypePtr type_ptr = i.base_type ? i.base_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);

                // Emit bounds check if needed (for array indexing with known size)
                if (i.needs_bounds_check && i.known_array_size >= 0 && !i.indices.empty()) {
                    std::string idx_val = get_value_reg(i.indices[0]);
                    std::string size_str = std::to_string(i.known_array_size);
                    std::string label_id = std::to_string(temp_counter_++);

                    // Get the actual type of the index (might be i32 or i64)
                    mir::MirTypePtr idx_type_ptr = i.indices[0].type;
                    std::string idx_type = idx_type_ptr ? mir_type_to_llvm(idx_type_ptr) : "i32";

                    // Check index < 0 (signed comparison)
                    std::string below_zero = "%bc.below." + label_id;
                    emitln("    " + below_zero + " = icmp slt " + idx_type + " " + idx_val + ", 0");

                    // Check index >= size
                    std::string above_max = "%bc.above." + label_id;
                    emitln("    " + above_max + " = icmp sge " + idx_type + " " + idx_val + ", " +
                           size_str);

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
                // Emit @llvm.assume hints when BCE proved the access is safe
                // This helps LLVM with cross-function optimization and vectorization
                else if (!i.needs_bounds_check && i.known_array_size >= 0 && !i.indices.empty()) {
                    std::string idx_val = get_value_reg(i.indices[0]);
                    std::string size_str = std::to_string(i.known_array_size);
                    std::string label_id = std::to_string(temp_counter_++);

                    mir::MirTypePtr idx_type_ptr = i.indices[0].type;
                    std::string idx_type = idx_type_ptr ? mir_type_to_llvm(idx_type_ptr) : "i32";

                    // Emit assume: index >= 0
                    std::string nonneg_cmp = "%assume.nonneg." + label_id;
                    emitln("    " + nonneg_cmp + " = icmp sge " + idx_type + " " + idx_val + ", 0");
                    emitln("    call void @llvm.assume(i1 " + nonneg_cmp + ")");

                    // Emit assume: index < size
                    std::string bounded_cmp = "%assume.bounded." + label_id;
                    emitln("    " + bounded_cmp + " = icmp slt " + idx_type + " " + idx_val + ", " +
                           size_str);
                    emitln("    call void @llvm.assume(i1 " + bounded_cmp + ")");
                }

                emit("    " + result_reg + " = getelementptr inbounds " + type_str + ", ptr " +
                     base);
                for (const auto& idx : i.indices) {
                    emit(", i32 " + get_value_reg(idx));
                }
                emitln();
                // GEP result is always a pointer
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = "ptr";
                }

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
                // Initialize enum: { tag }
                // Note: Use %struct. prefix to be consistent with AST-based codegen
                // imported enum types are emitted in emit_type_defs via used_enum_types_
                std::string enum_type = "%struct." + i.enum_name;
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

    // Handle black_box intrinsics - prevent optimization
    if (base_name == "black_box" && i.args.size() == 1) {
        std::string arg = get_value_reg(i.args[0]);
        emitln("    " + result_reg + " = call i32 @black_box_i32(i32 " + arg + ")");
        value_regs_[inst.result] = result_reg;
        return;
    }
    if (base_name == "black_box_i64" && i.args.size() == 1) {
        std::string arg = get_value_reg(i.args[0]);
        emitln("    " + result_reg + " = call i64 @black_box_i64(i64 " + arg + ")");
        value_regs_[inst.result] = result_reg;
        return;
    }
    if (base_name == "black_box_f64" && i.args.size() == 1) {
        std::string arg = get_value_reg(i.args[0]);
        emitln("    " + result_reg + " = call double @black_box_f64(double " + arg + ")");
        value_regs_[inst.result] = result_reg;
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

    // ========================================================================
    // Inline primitive to_string / debug_string (Char, Str, Bool)
    // These may arrive as CallInst with func_name "Type::method" when the
    // MIR builder resolves behavior methods to qualified function names.
    // ========================================================================
    if (i.func_name == "Char::to_string" || i.func_name == "Char::debug_string" ||
        i.func_name == "Char__to_string" || i.func_name == "Char__debug_string") {
        std::string id = std::to_string(temp_counter_++);
        std::string receiver = i.args.empty() ? "0" : get_value_reg(i.args[0]);
        // Truncate i32 to i8 (ASCII)
        emitln("    %char_byte." + id + " = trunc i32 " + receiver + " to i8");
        // Allocate 2 bytes for single-char string + null
        emitln("    %char_buf." + id + " = call ptr @mem_alloc(i64 2)");
        emitln("    store i8 %char_byte." + id + ", ptr %char_buf." + id);
        emitln("    %char_p1." + id + " = getelementptr i8, ptr %char_buf." + id + ", i64 1");
        emitln("    store i8 0, ptr %char_p1." + id);
        if (i.func_name == "Char::debug_string" || i.func_name == "Char__debug_string") {
            emitln("    %sq_tmp." + id +
                   " = call ptr @str_concat_opt(ptr @.str.sq, ptr %char_buf." + id + ")");
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr %sq_tmp." + id +
                   ", ptr @.str.sq)");
        } else {
            emitln("    " + result_reg + " = bitcast ptr %char_buf." + id + " to ptr");
        }
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "ptr";
        }
        return;
    }
    if (i.func_name == "Str::to_string" || i.func_name == "Str::debug_string" ||
        i.func_name == "Str__to_string" || i.func_name == "Str__debug_string") {
        std::string receiver = i.args.empty() ? "null" : get_value_reg(i.args[0]);
        if (i.func_name == "Str::to_string" || i.func_name == "Str__to_string") {
            emitln("    " + result_reg + " = bitcast ptr " + receiver + " to ptr");
        } else {
            std::string id = std::to_string(temp_counter_++);
            emitln("    %dq_tmp." + id + " = call ptr @str_concat_opt(ptr @.str.dq, ptr " +
                   receiver + ")");
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr %dq_tmp." + id +
                   ", ptr @.str.dq)");
        }
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "ptr";
        }
        return;
    }

    // Check if this is an indirect call (function pointer parameter)
    auto param_it = param_info_.find(i.func_name);
    if (param_it != param_info_.end()) {
        auto& [value_id, param_type] = param_it->second;
        // Check if the parameter is a function type
        if (param_type && std::holds_alternative<mir::MirFunctionType>(param_type->kind)) {
            emit_indirect_call(i, param_it->first, value_id, param_type, result_reg, inst);
            return;
        }
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

        // For devirtualized method calls, the first arg is the receiver (this)
        // If it's a struct value but the function expects ptr, spill to memory
        bool is_devirt_receiver = i.devirt_info.has_value() && j == 0;
        bool is_struct_value = actual_type.find("%struct.") == 0;
        bool expects_ptr = declared_type == "ptr";

        if (is_devirt_receiver && is_struct_value && expects_ptr) {
            // Spill struct value to memory so we can pass a pointer
            std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
            emitln("    " + spill_ptr + " = alloca " + actual_type);
            emitln("    store " + actual_type + " " + arg + ", ptr " + spill_ptr);
            arg = spill_ptr;
            arg_type = "ptr";
        } else if (is_struct_value) {
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

void MirCodegen::emit_indirect_call(const mir::CallInst& i, const std::string& param_name,
                                    mir::ValueId value_id, const mir::MirTypePtr& func_type,
                                    const std::string& result_reg,
                                    const mir::InstructionData& inst) {
    (void)value_id; // May be used for future enhancements

    // Get the function pointer value (the parameter register)
    std::string func_ptr = "%" + param_name;

    // Extract function type info
    const auto& mir_func_type = std::get<mir::MirFunctionType>(func_type->kind);

    // Build parameter type list
    std::vector<std::string> param_types;
    for (const auto& pt : mir_func_type.params) {
        param_types.push_back(mir_type_to_llvm(pt));
    }

    // Get return type
    std::string ret_type =
        mir_func_type.return_type ? mir_type_to_llvm(mir_func_type.return_type) : "void";

    // Build argument list
    std::string args_str;
    for (size_t j = 0; j < i.args.size(); ++j) {
        if (j > 0)
            args_str += ", ";
        std::string arg = get_value_reg(i.args[j]);
        std::string arg_type = j < param_types.size() ? param_types[j] : "i64";
        args_str += arg_type + " " + arg;
    }

    // Emit the indirect call
    if (ret_type == "void") {
        emitln("    call void " + func_ptr + "(" + args_str + ")");
    } else {
        emitln("    " + result_reg + " = call " + ret_type + " " + func_ptr + "(" + args_str + ")");
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = ret_type;
        }
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

} // namespace tml::codegen
