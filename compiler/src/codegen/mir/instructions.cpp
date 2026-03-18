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
                // Unit type maps to "void" but LLVM doesn't allow `load void`.
                // Use "{}" (empty struct, zero-sized) as the data representation.
                if (type_str == "void") {
                    type_str = "{}";
                }
                std::string volatile_kw = i.is_volatile ? "volatile " : "";
                // Array loads need align 16 to match the alignment of array allocas.
                bool is_array_load = type_ptr && type_ptr->is_array();
                if (is_array_load) {
                    emitln("    " + result_reg + " = load " + volatile_kw + type_str + ", ptr " +
                           ptr + ", align 16");
                } else {
                    emitln("    " + result_reg + " = load " + volatile_kw + type_str + ", ptr " +
                           ptr);
                }
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
                // Unit type maps to "void" but LLVM doesn't allow `store void`.
                // Skip the store entirely for void/unit — it's a zero-sized type
                // with no data to write.
                if (type_str == "void") {
                    emitln("    ; skip store of void (unit type)");
                } else {
                    std::string volatile_kw = i.is_volatile ? "volatile " : "";
                    // Array stores need align 16 to match the alignment of array allocas
                    // and prevent LLVM backend crashes with SIMD aggregate stores.
                    bool is_array_store = type_ptr && type_ptr->is_array();
                    if (is_array_store) {
                        emitln("    store " + volatile_kw + type_str + " " + value + ", ptr " +
                               ptr + ", align 16");
                    } else {
                        emitln("    store " + volatile_kw + type_str + " " + value + ", ptr " +
                               ptr);
                    }
                }

            } else if constexpr (std::is_same_v<T, mir::AllocaInst>) {
                mir::MirTypePtr type_ptr = i.alloc_type ? i.alloc_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                // Unit type maps to "void" but LLVM doesn't allow `alloca void`.
                // Use "{}" (empty struct, zero-sized) as the data representation.
                if (type_str == "void") {
                    type_str = "{}";
                }
                emitln("    ; ALLOCA: result_id=" + std::to_string(inst.result) +
                       " reg=" + result_reg + " type=" + type_str);
                // Array allocas need explicit alignment to prevent LLVM backend crashes
                // when storing/loading aggregate values (SIMD instructions require alignment).
                bool is_array_alloc = type_ptr && type_ptr->is_array();
                if (is_array_alloc) {
                    emitln("    " + result_reg + " = alloca " + type_str + ", align 16");
                } else {
                    emitln("    " + result_reg + " = alloca " + type_str);
                }
                // If zero_init is set, emit a zeroinitializer store immediately after the
                // alloca. This avoids a separate large aggregate SSA store instruction
                // (e.g., 'store [100 x i32] %v1, ptr %v2') that crashes LLVM's x86
                // backend for large arrays (SelectionDAG can't handle 400-byte aggregates).
                if (i.zero_init && is_array_alloc) {
                    emitln("    store " + type_str + " zeroinitializer, ptr " + result_reg +
                           ", align 16");
                }
                // Track alloca as pointer type for method call receiver handling
                if (inst.result != mir::INVALID_VALUE) {
                    value_types_[inst.result] = "ptr";
                }

            } else if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                std::string base = get_value_reg(i.base);
                mir::MirTypePtr type_ptr = i.base_type ? i.base_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);

                // GEP requires a pointer base operand. If the base is a non-pointer
                // value (e.g., an array from insertvalue chain or a load of an
                // aggregate), spill it to a temp alloca and use the alloca pointer.
                //
                // Strategy: check value_types_ first (most precise), then the MIR
                // Value's type annotation, then infer from the GEP's own base_type.
                // If the base is NOT positively known to be a pointer, and the GEP
                // pointee type is an aggregate (array/struct), assume spill is needed.
                bool needs_spill = false;
                std::string spill_type;
                auto base_type_it = value_types_.find(i.base.id);
                if (base_type_it != value_types_.end()) {
                    // Tracked: spill if not a pointer
                    if (base_type_it->second != "ptr") {
                        needs_spill = true;
                        spill_type = base_type_it->second;
                    }
                } else {
                    // Not tracked. Check MIR Value type annotation first.
                    bool resolved = false;
                    if (i.base.type) {
                        std::string base_mir_type = mir_type_to_llvm(i.base.type);
                        if (base_mir_type == "ptr") {
                            resolved = true; // Known pointer, no spill needed
                        } else if (base_mir_type != "i1" && base_mir_type != "i8" &&
                                   base_mir_type != "i16" && base_mir_type != "i32" &&
                                   base_mir_type != "i64" && base_mir_type != "float" &&
                                   base_mir_type != "double") {
                            needs_spill = true;
                            spill_type = base_mir_type;
                            resolved = true;
                        }
                    }
                    // Last resort: if base is untracked and GEP pointee type is an
                    // aggregate (starts with '[' for arrays or '{' for structs), the
                    // base MUST be a pointer to that type. If it isn't, spill using
                    // the pointee type as the value type.
                    if (!resolved && !type_str.empty() &&
                        (type_str[0] == '[' || type_str[0] == '{')) {
                        needs_spill = true;
                        spill_type = type_str;
                    }
                }
                if (needs_spill) {
                    std::string spill_reg = "%arr_spill" + std::to_string(temp_counter_++);
                    emitln("    " + spill_reg + " = alloca " + spill_type);
                    emitln("    store " + spill_type + " " + base + ", ptr " + spill_reg);
                    // Track the spill so later reads of this value ID (e.g., in
                    // TupleInit) reload from the alloca and pick up any mutations.
                    value_spill_allocas_[i.base.id] = spill_reg;
                    base = spill_reg;
                }

                // Emit bounds check if needed (for array indexing with known size)
                if (i.needs_bounds_check && i.known_array_size >= 0 && !i.indices.empty()) {
                    std::string idx_val = get_value_reg(i.indices[0]);
                    std::string size_str = std::to_string(i.known_array_size);
                    // Use bounds_check_counter_ (not temp_counter_) so that the pre-scan
                    // in emit_function() can predict bc.ok.N labels without simulating
                    // all temp_counter_ uses. This enables correct phi predecessor labels.
                    std::string label_id = std::to_string(bounds_check_counter_++);

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

                    // Update exit label: phi nodes must reference bc.ok.N, not the
                    // original block name, since the branch split the block.
                    block_exit_labels_[current_block_id_] = ok_label;
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
                // For array types [N x T], LLVM GEP requires two indices:
                //   index 0: dereference the pointer-to-array (always 0)
                //   index N: select element N within the array
                // With only one index, the GEP steps over entire arrays (N * sizeof(array)),
                // causing out-of-bounds reads/writes for any non-zero index.
                if (!type_str.empty() && type_str[0] == '[') {
                    emit(", i64 0");
                }
                for (const auto& idx : i.indices) {
                    mir::MirTypePtr idx_type_ptr = idx.type;
                    std::string idx_type = idx_type_ptr ? mir_type_to_llvm(idx_type_ptr) : "i64";
                    emit(", " + idx_type + " " + get_value_reg(idx));
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
                // Initialize enum: { tag, payload }
                // Use result type to get properly mangled enum type name
                std::string enum_type;
                if (result_type) {
                    enum_type = mir_type_to_llvm(result_type);
                } else {
                    enum_type = "%struct." + i.enum_name;
                }
                // Insert tag
                std::string with_tag = "%tmp" + std::to_string(temp_counter_++);
                emitln("    " + with_tag + " = insertvalue " + enum_type + " undef, i32 " +
                       std::to_string(i.variant_index) + ", 0");
                // For simplicity, we're not handling payload here yet
                emitln("    " + result_reg + " = " + with_tag);

            } else if constexpr (std::is_same_v<T, mir::TupleInitInst>) {
                emit_tuple_init_inst(i, result_reg);
                // Track tuple type so GEP can detect non-pointer bases
                if (inst.result != mir::INVALID_VALUE && i.result_type) {
                    std::string tup_type = mir_type_to_llvm(i.result_type);
                    // Unit/empty tuple: use "{}" instead of "void" for data tracking
                    if (tup_type == "void")
                        tup_type = "{}";
                    value_types_[inst.result] = tup_type;
                }

            } else if constexpr (std::is_same_v<T, mir::ArrayInitInst>) {
                emit_array_init_inst(i, result_reg);
                // Track array type so GEP can detect non-pointer bases
                if (inst.result != mir::INVALID_VALUE) {
                    if (i.result_type) {
                        value_types_[inst.result] = mir_type_to_llvm(i.result_type);
                    } else if (i.element_type && !i.elements.empty()) {
                        // Fallback: compute array type from element type + count
                        std::string elem = mir_type_to_llvm(i.element_type);
                        value_types_[inst.result] =
                            "[" + std::to_string(i.elements.size()) + " x " + elem + "]";
                    }
                }

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

            } else if constexpr (std::is_same_v<T, mir::ClosureInitInst>) {
                emit_closure_init_inst(i, result_reg, inst);
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

    // Check if the operand type is a tuple/aggregate: "{ i32, i32, ... }"
    bool is_tuple_type =
        type_str.size() > 4 && type_str.starts_with("{ ") && type_str.ends_with(" }");

    if (is_comparison && is_tuple_type && (i.op == mir::BinOp::Eq || i.op == mir::BinOp::Ne)) {
        // Tuple element-by-element comparison.
        // LLVM icmp/fcmp do not support aggregate types, so we must spill
        // both operands to allocas and compare each element individually.

        // Parse element types from "{ i32, i64, double }" -> ["i32","i64","double"]
        std::vector<std::string> elem_types;
        std::string inner = type_str.substr(2, type_str.size() - 4); // strip "{ " and " }"
        size_t pos = 0;
        while (pos < inner.size()) {
            size_t comma = inner.find(", ", pos);
            if (comma == std::string::npos) {
                elem_types.push_back(inner.substr(pos));
                break;
            }
            elem_types.push_back(inner.substr(pos, comma - pos));
            pos = comma + 2;
        }

        // Spill both tuple values to allocas so we can GEP into elements
        std::string left_alloca = new_temp();
        emitln("    " + left_alloca + " = alloca " + type_str);
        emitln("    store " + type_str + " " + left + ", ptr " + left_alloca);

        std::string right_alloca = new_temp();
        emitln("    " + right_alloca + " = alloca " + type_str);
        emitln("    store " + type_str + " " + right + ", ptr " + right_alloca);

        // Compare each element, ANDing results together
        std::string running = "1"; // start with true (i1 literal)
        for (size_t idx = 0; idx < elem_types.size(); ++idx) {
            const std::string& et = elem_types[idx];
            std::string idx_str = std::to_string(idx);

            std::string lp = new_temp();
            emitln("    " + lp + " = getelementptr inbounds " + type_str + ", ptr " + left_alloca +
                   ", i32 0, i32 " + idx_str);
            std::string lv = new_temp();
            emitln("    " + lv + " = load " + et + ", ptr " + lp);

            std::string rp = new_temp();
            emitln("    " + rp + " = getelementptr inbounds " + type_str + ", ptr " + right_alloca +
                   ", i32 0, i32 " + idx_str);
            std::string rv = new_temp();
            emitln("    " + rv + " = load " + et + ", ptr " + rp);

            std::string cmp = new_temp();
            if (et == "double" || et == "float") {
                emitln("    " + cmp + " = fcmp oeq " + et + " " + lv + ", " + rv);
            } else {
                emitln("    " + cmp + " = icmp eq " + et + " " + lv + ", " + rv);
            }

            std::string combined = new_temp();
            emitln("    " + combined + " = and i1 " + running + ", " + cmp);
            running = combined;
        }

        // For Ne, invert the equality result
        if (i.op == mir::BinOp::Ne) {
            std::string neg = new_temp();
            emitln("    " + neg + " = xor i1 " + running + ", 1");
            // Alias result_reg to neg
            emitln("    " + result_reg + " = and i1 " + neg + ", 1"); // identity to bind result_reg
        } else {
            emitln("    " + result_reg + " = and i1 " + running +
                   ", 1"); // identity to bind result_reg
        }

        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = "i1";
        }
    } else if (is_comparison) {
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

    // Check if the aggregate is actually a pointer (e.g., 'this'/'self' parameter
    // whose type was changed from struct to ptr in emit_function).
    // In that case, use GEP+load instead of extractvalue.
    auto vt_it = value_types_.find(i.aggregate.id);
    bool agg_is_ptr = (vt_it != value_types_.end() && vt_it->second == "ptr");
    if (agg_is_ptr && (agg_type.starts_with("%struct.") || agg_type.starts_with("%enum.") ||
                       agg_type.starts_with("%class.") || agg_type.starts_with("%union."))) {
        // Aggregate is a pointer to a struct — emit GEP + load instead of extractvalue
        std::string gep_reg = new_temp();
        emit("    " + gep_reg + " = getelementptr inbounds " + agg_type + ", ptr " + agg);
        emit(", i32 0");
        for (auto idx : i.indices) {
            emit(", i32 " + std::to_string(idx));
        }
        emitln();
        // Determine field type for the load from the instruction's result type
        std::string field_type_str;
        if (i.result_type) {
            field_type_str = mir_type_to_llvm(i.result_type);
        } else if (type_ptr && !i.indices.empty()) {
            // Try to compute from aggregate tuple type
            if (auto* tuple = std::get_if<mir::MirTupleType>(&type_ptr->kind)) {
                size_t idx = i.indices[0];
                if (idx < tuple->elements.size()) {
                    field_type_str = mir_type_to_llvm(tuple->elements[idx]);
                }
            } else if (auto* arr = std::get_if<mir::MirArrayType>(&type_ptr->kind)) {
                field_type_str = mir_type_to_llvm(arr->element);
            }
            if (field_type_str.empty()) {
                field_type_str = "i64"; // Fallback for struct fields
            }
        } else {
            field_type_str = "i64";
        }
        emitln("    " + result_reg + " = load " + field_type_str + ", ptr " + gep_reg);
    } else {
        // Normal case: aggregate is a value, use extractvalue directly
        emit("    " + result_reg + " = extractvalue " + agg_type + " " + agg);
        for (auto idx : i.indices) {
            emit(", " + std::to_string(idx));
        }
        emitln();
    }

    // Store result type for subsequent operations (needed for GEP spill detection)
    if (inst.result != mir::INVALID_VALUE) {
        if (i.result_type) {
            value_types_[inst.result] = mir_type_to_llvm(i.result_type);
        } else if (type_ptr && !i.indices.empty()) {
            // Fallback: compute result type from aggregate type + extraction index.
            // For tuples: extractvalue { [4 x i8], i64 } %v, 0 → [4 x i8]
            // For arrays: extractvalue [4 x i8] %v, 0 → i8
            mir::MirTypePtr field_type;
            if (auto* tuple = std::get_if<mir::MirTupleType>(&type_ptr->kind)) {
                size_t idx = i.indices[0];
                if (idx < tuple->elements.size()) {
                    field_type = tuple->elements[idx];
                }
            } else if (auto* arr = std::get_if<mir::MirArrayType>(&type_ptr->kind)) {
                field_type = arr->element;
            }
            if (field_type) {
                value_types_[inst.result] = mir_type_to_llvm(field_type);
            }
        }
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

    // ========================================================================
    // Inline array methods (devirtualized from MethodCallInst)
    // When devirtualization converts array.len() to call "len"(array),
    // the first arg is the array value. Detect array type from value_types_.
    // ========================================================================
    if (!i.args.empty()) {
        std::string recv_vt;
        auto vt_it = value_types_.find(i.args[0].id);
        if (vt_it != value_types_.end())
            recv_vt = vt_it->second;
        if (recv_vt.size() > 2 && recv_vt[0] == '[') {
            size_t x_pos = recv_vt.find(" x ");
            if (x_pos != std::string::npos) {
                std::string n_str = recv_vt.substr(1, x_pos - 1);
                std::string elem_type = recv_vt.substr(x_pos + 3);
                if (!elem_type.empty() && elem_type.back() == ']')
                    elem_type.pop_back();
                int64_t arr_size = std::stoll(n_str);
                std::string receiver = get_value_reg(i.args[0]);

                std::string method_name = i.func_name;
                {
                    size_t lc = method_name.rfind("::");
                    if (lc != std::string::npos)
                        method_name = method_name.substr(lc + 2);
                }

                if (method_name == "len" && !result_reg.empty()) {
                    emitln("    " + result_reg + " = add i64 0, " + std::to_string(arr_size));
                    if (inst.result != mir::INVALID_VALUE)
                        value_types_[inst.result] = "i64";
                    return;
                }

                if (method_name == "hash" && !result_reg.empty()) {
                    std::string id = std::to_string(temp_counter_++);
                    std::string arr_ptr = "%arr_spill." + id;
                    emitln("    " + arr_ptr + " = alloca " + recv_vt);
                    emitln("    store " + recv_vt + " " + receiver + ", ptr " + arr_ptr);
                    // FNV-1a hash
                    std::string hash_reg = "%hash_init." + id;
                    emitln("    " + hash_reg + " = add i64 0, -3750763034362895579");
                    for (int64_t j = 0; j < arr_size; ++j) {
                        std::string idx = std::to_string(j);
                        std::string ep = "%arr_ep." + id + "." + idx;
                        std::string ev = "%arr_ev." + id + "." + idx;
                        emitln("    " + ep + " = getelementptr inbounds " + recv_vt + ", ptr " +
                               arr_ptr + ", i32 0, i32 " + idx);
                        emitln("    " + ev + " = load " + elem_type + ", ptr " + ep);
                        std::string e64 = "%arr_e64." + id + "." + idx;
                        if (elem_type == "i64") {
                            e64 = ev;
                        } else if (elem_type == "i32") {
                            emitln("    " + e64 + " = sext i32 " + ev + " to i64");
                        } else if (elem_type == "i16") {
                            emitln("    " + e64 + " = sext i16 " + ev + " to i64");
                        } else if (elem_type == "i8") {
                            emitln("    " + e64 + " = sext i8 " + ev + " to i64");
                        } else {
                            emitln("    " + e64 + " = ptrtoint " + elem_type + " " + ev +
                                   " to i64");
                        }
                        std::string xr = "%arr_hx." + id + "." + idx;
                        std::string mr = "%arr_hm." + id + "." + idx;
                        emitln("    " + xr + " = xor i64 " + hash_reg + ", " + e64);
                        emitln("    " + mr + " = mul i64 " + xr + ", 1099511628211");
                        hash_reg = mr;
                    }
                    emitln("    " + result_reg + " = add i64 0, " + hash_reg);
                    if (inst.result != mir::INVALID_VALUE)
                        value_types_[inst.result] = "i64";
                    return;
                }

                if (method_name == "eq" && !result_reg.empty() && i.args.size() >= 2) {
                    std::string id = std::to_string(temp_counter_++);
                    std::string other = get_value_reg(i.args[1]);
                    std::string a_ptr = "%eq_a." + id;
                    std::string b_ptr = "%eq_b." + id;
                    emitln("    " + a_ptr + " = alloca " + recv_vt);
                    emitln("    store " + recv_vt + " " + receiver + ", ptr " + a_ptr);
                    emitln("    " + b_ptr + " = alloca " + recv_vt);
                    emitln("    %eq_bval." + id + " = load " + recv_vt + ", ptr " + other);
                    emitln("    store " + recv_vt + " %eq_bval." + id + ", ptr " + b_ptr);
                    std::string acc = "%eq_init." + id;
                    emitln("    " + acc + " = add i1 0, 1");
                    for (int64_t j = 0; j < arr_size; ++j) {
                        std::string idx = std::to_string(j);
                        std::string ap = "%eq_ap." + id + "." + idx;
                        std::string bp = "%eq_bp." + id + "." + idx;
                        std::string av = "%eq_av." + id + "." + idx;
                        std::string bv = "%eq_bv." + id + "." + idx;
                        emitln("    " + ap + " = getelementptr inbounds " + recv_vt + ", ptr " +
                               a_ptr + ", i32 0, i32 " + idx);
                        emitln("    " + bp + " = getelementptr inbounds " + recv_vt + ", ptr " +
                               b_ptr + ", i32 0, i32 " + idx);
                        emitln("    " + av + " = load " + elem_type + ", ptr " + ap);
                        emitln("    " + bv + " = load " + elem_type + ", ptr " + bp);
                        std::string cmp = "%eq_cmp." + id + "." + idx;
                        emitln("    " + cmp + " = icmp eq " + elem_type + " " + av + ", " + bv);
                        std::string new_acc = "%eq_acc." + id + "." + idx;
                        emitln("    " + new_acc + " = and i1 " + acc + ", " + cmp);
                        acc = new_acc;
                    }
                    emitln("    " + result_reg + " = zext i1 " + acc + " to i1");
                    if (inst.result != mir::INVALID_VALUE)
                        value_types_[inst.result] = "i1";
                    return;
                }
            }
        }
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
    // Memory intrinsics: ptr_write, ptr_read, ptr_offset, mem_free,
    // copy_nonoverlapping — lowered to LLVM store/load/GEP/call.
    // These are TML's core::intrinsics functions that the legacy codegen
    // handles in builtins/intrinsics.cpp but MIR codegen was missing.
    // ========================================================================

    // ptr_write[T](ptr, val) -> Unit  — store val to ptr
    if (base_name == "ptr_write" && i.args.size() >= 2) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);
        std::string val_arg = get_value_reg(i.args[1]);

        // Determine the element type from the value argument
        std::string elem_type = "i32"; // default
        auto val_vt = value_types_.find(i.args[1].id);
        if (val_vt != value_types_.end() && !val_vt->second.empty()) {
            elem_type = val_vt->second;
        } else if (i.args[1].type) {
            elem_type = mir_type_to_llvm(i.args[1].type);
        }
        if (i.arg_types.size() >= 2 && i.arg_types[1]) {
            std::string declared = mir_type_to_llvm(i.arg_types[1]);
            if (declared != "void" && declared != "i32") {
                elem_type = declared;
            }
        }

        // If ptr_arg is an integer (e.g., I64 address), convert to ptr
        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.pw." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    store " + elem_type + " " + val_arg + ", ptr " + ptr_reg);
        return;
    }

    // ptr_read[T](ptr) -> T  — load T from ptr
    if (base_name == "ptr_read" && i.args.size() >= 1) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);

        // Determine the element type. Priority:
        // 1) Pointee type from pointer argument's MIR type (most reliable for generic [T])
        // 2) Return type from MIR CallInst (may be I32 default from type checker)
        // 3) Fallback i32
        std::string elem_type = "i32";

        // Check pointer argument's pointee type
        mir::MirTypePtr arg_type =
            (i.arg_types.size() >= 1 && i.arg_types[0]) ? i.arg_types[0] : i.args[0].type;
        if (arg_type) {
            if (auto* pt = std::get_if<mir::MirPointerType>(&arg_type->kind)) {
                if (pt->pointee) {
                    std::string pointee = mir_type_to_llvm(pt->pointee);
                    if (pointee != "void" && pointee != "{}")
                        elem_type = pointee;
                }
            }
        }

        // Fall back to return type if pointee didn't resolve
        if (elem_type == "i32" && i.return_type) {
            std::string rt = mir_type_to_llvm(i.return_type);
            if (rt != "void" && rt != "i32")
                elem_type = rt;
        }

        // If ptr_arg is an integer, convert to ptr
        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.pr." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    " + result_reg + " = load " + elem_type + ", ptr " + ptr_reg);
        if (inst.result != mir::INVALID_VALUE)
            value_types_[inst.result] = elem_type;
        return;
    }

    // ptr_read_volatile[T](ptr) -> T / volatile_read
    if ((base_name == "ptr_read_volatile" || base_name == "volatile_read") && i.args.size() >= 1) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);

        std::string elem_type = "i32";
        mir::MirTypePtr arg_type =
            (i.arg_types.size() >= 1 && i.arg_types[0]) ? i.arg_types[0] : i.args[0].type;
        if (arg_type) {
            if (auto* pt = std::get_if<mir::MirPointerType>(&arg_type->kind)) {
                if (pt->pointee) {
                    std::string pointee = mir_type_to_llvm(pt->pointee);
                    if (pointee != "void" && pointee != "{}")
                        elem_type = pointee;
                }
            }
        }
        if (elem_type == "i32" && i.return_type) {
            std::string rt = mir_type_to_llvm(i.return_type);
            if (rt != "void" && rt != "i32")
                elem_type = rt;
        }

        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.prv." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    " + result_reg + " = load volatile " + elem_type + ", ptr " + ptr_reg);
        if (inst.result != mir::INVALID_VALUE)
            value_types_[inst.result] = elem_type;
        return;
    }

    // ptr_write_volatile[T](ptr, val) / volatile_write
    if ((base_name == "ptr_write_volatile" || base_name == "volatile_write") &&
        i.args.size() >= 2) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);
        std::string val_arg = get_value_reg(i.args[1]);

        std::string elem_type = "i32";
        auto val_vt = value_types_.find(i.args[1].id);
        if (val_vt != value_types_.end() && !val_vt->second.empty()) {
            elem_type = val_vt->second;
        } else if (i.args[1].type) {
            elem_type = mir_type_to_llvm(i.args[1].type);
        }

        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.pwv." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    store volatile " + elem_type + " " + val_arg + ", ptr " + ptr_reg);
        return;
    }

    // ptr_read_unaligned[T](ptr) -> T
    if (base_name == "ptr_read_unaligned" && i.args.size() >= 1) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);

        std::string elem_type = "i32";
        mir::MirTypePtr arg_type =
            (i.arg_types.size() >= 1 && i.arg_types[0]) ? i.arg_types[0] : i.args[0].type;
        if (arg_type) {
            if (auto* pt = std::get_if<mir::MirPointerType>(&arg_type->kind)) {
                if (pt->pointee) {
                    std::string pointee = mir_type_to_llvm(pt->pointee);
                    if (pointee != "void" && pointee != "{}")
                        elem_type = pointee;
                }
            }
        }
        if (elem_type == "i32" && i.return_type) {
            std::string rt = mir_type_to_llvm(i.return_type);
            if (rt != "void" && rt != "i32")
                elem_type = rt;
        }

        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.pru." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    " + result_reg + " = load " + elem_type + ", ptr " + ptr_reg + ", align 1");
        if (inst.result != mir::INVALID_VALUE)
            value_types_[inst.result] = elem_type;
        return;
    }

    // ptr_write_unaligned[T](ptr, val)
    if (base_name == "ptr_write_unaligned" && i.args.size() >= 2) {
        std::string id = std::to_string(temp_counter_++);
        std::string ptr_arg = get_value_reg(i.args[0]);
        std::string val_arg = get_value_reg(i.args[1]);

        std::string elem_type = "i32";
        auto val_vt = value_types_.find(i.args[1].id);
        if (val_vt != value_types_.end() && !val_vt->second.empty()) {
            elem_type = val_vt->second;
        } else if (i.args[1].type) {
            elem_type = mir_type_to_llvm(i.args[1].type);
        }

        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string conv = "%itp.pwu." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    store " + elem_type + " " + val_arg + ", ptr " + ptr_reg + ", align 1");
        return;
    }

    // ptr_offset[T](ptr, offset) -> *T  — GEP-based pointer arithmetic
    if (base_name == "ptr_offset" && i.args.size() >= 2) {
        std::string ptr_arg = get_value_reg(i.args[0]);
        std::string offset_arg = get_value_reg(i.args[1]);

        // Determine element type for GEP stride
        std::string elem_type = "i8"; // default byte-stride
        if (i.return_type) {
            if (auto* pt = std::get_if<mir::MirPointerType>(&i.return_type->kind)) {
                if (pt->pointee)
                    elem_type = mir_type_to_llvm(pt->pointee);
            }
        }

        std::string ptr_reg = ptr_arg;
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i' && ptr_type_str != "i1") {
            std::string id = std::to_string(temp_counter_++);
            std::string conv = "%itp.po." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            ptr_reg = conv;
        }

        emitln("    " + result_reg + " = getelementptr " + elem_type + ", ptr " + ptr_reg +
               ", i64 " + offset_arg);
        if (inst.result != mir::INVALID_VALUE)
            value_types_[inst.result] = "ptr";
        return;
    }

    // mem_free(ptr) -> Unit
    if (base_name == "mem_free" && i.args.size() >= 1) {
        std::string ptr_arg = get_value_reg(i.args[0]);

        // If the argument is an integer, convert to ptr
        auto ptr_vt = value_types_.find(i.args[0].id);
        std::string ptr_type_str;
        if (ptr_vt != value_types_.end())
            ptr_type_str = ptr_vt->second;
        else if (i.args[0].type)
            ptr_type_str = mir_type_to_llvm(i.args[0].type);

        if (ptr_type_str == "ptr") {
            emitln("    call void @mem_free(ptr " + ptr_arg + ")");
        } else if (ptr_type_str.size() > 0 && ptr_type_str[0] == 'i') {
            std::string id = std::to_string(temp_counter_++);
            std::string conv = "%itp.mf." + id;
            emitln("    " + conv + " = inttoptr " + ptr_type_str + " " + ptr_arg + " to ptr");
            emitln("    call void @mem_free(ptr " + conv + ")");
        } else {
            emitln("    call void @mem_free(ptr " + ptr_arg + ")");
        }
        return;
    }

    // copy_nonoverlapping(src, dst, count) -> Unit
    if (base_name == "copy_nonoverlapping" && i.args.size() >= 3) {
        std::string src = get_value_reg(i.args[0]);
        std::string dst = get_value_reg(i.args[1]);
        std::string count = get_value_reg(i.args[2]);

        // Ensure both pointers are ptr type
        auto ensure_ptr = [&](const mir::Value& v, std::string& reg) {
            auto vt = value_types_.find(v.id);
            std::string vtype;
            if (vt != value_types_.end())
                vtype = vt->second;
            else if (v.type)
                vtype = mir_type_to_llvm(v.type);
            if (vtype.size() > 0 && vtype[0] == 'i' && vtype != "i1") {
                std::string id = std::to_string(temp_counter_++);
                std::string conv = "%itp.cn." + id;
                emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
                reg = conv;
            }
        };
        ensure_ptr(i.args[0], src);
        ensure_ptr(i.args[1], dst);

        emitln("    call void @llvm.memcpy.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
               count + ", i1 false)");
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

    // ========================================================================
    // Handle bare "to_string" / "debug_string" calls on primitive types
    // These come from devirtualized/resolved method calls where func_name
    // lost the type prefix. Detect receiver type from value_types_.
    // ========================================================================
    if ((i.func_name == "to_string" || i.func_name == "debug_string") && !i.args.empty() &&
        !result_reg.empty()) {
        std::string arg_vt;
        auto avt = value_types_.find(i.args[0].id);
        if (avt != value_types_.end())
            arg_vt = avt->second;
        std::string arg_reg = get_value_reg(i.args[0]);

        // Map LLVM type to mangled TML to_string function name
        // e.g., i64 -> tml_N4core3I649to_stringE
        struct TypeMapping {
            const char* llvm_type;
            const char* mangled_name;
        };
        static const TypeMapping mappings[] = {
            {"i8", "tml_N4core2I89to_stringE"},      {"i16", "tml_N4core3I169to_stringE"},
            {"i32", "tml_N4core3I329to_stringE"},    {"i64", "tml_N4core3I649to_stringE"},
            {"i128", "tml_N4core4I1289to_stringE"},  {"float", "tml_N4core3F329to_stringE"},
            {"double", "tml_N4core3F649to_stringE"},
        };

        for (const auto& m : mappings) {
            if (arg_vt == m.llvm_type) {
                emitln("    " + result_reg + " = call ptr @" + std::string(m.mangled_name) + "(" +
                       arg_vt + " " + arg_reg + ")");
                if (inst.result != mir::INVALID_VALUE)
                    value_types_[inst.result] = "ptr";
                return;
            }
        }
    }

    // Check if the THIR builder marked this as an indirect call through a local variable
    // or parameter holding a function pointer. The callee field carries the MIR Value
    // whose LLVM register contains the { ptr, ptr } fat pointer.
    if (i.callee.has_value()) {
        auto callee_type = i.callee_func_type ? i.callee_func_type : i.callee->type;
        emit_indirect_call(i, i.func_name, i.callee->id, callee_type, result_reg, inst);
        return;
    }

    // Check if this is an indirect call via a function pointer parameter
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

    // Look up declared parameter types from function signature
    std::vector<mir::MirTypePtr> const* declared_param_types = nullptr;
    auto fpt_it = func_param_types_.find(func_name);
    if (fpt_it != func_param_types_.end()) {
        declared_param_types = &fpt_it->second;
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

        // Check if this argument needs array-to-slice coercion.
        // When the declared parameter is *[T] (pointer to slice) and the actual
        // argument is a fixed-size array [N x T], we need to:
        // 1. Spill the array to the stack
        // 2. Build a fat pointer { ptr, i64 } with array address and length
        // 3. Spill the fat pointer to the stack
        // 4. Pass the fat pointer's address as ptr
        bool did_array_to_slice = false;
        if (declared_param_types && j < declared_param_types->size()) {
            auto& param_type = (*declared_param_types)[j];
            if (param_type) {
                auto* ptr_type = std::get_if<mir::MirPointerType>(&param_type->kind);
                if (ptr_type && ptr_type->pointee) {
                    auto* slice_type = std::get_if<mir::MirSliceType>(&ptr_type->pointee->kind);
                    if (slice_type && actual_type.size() > 2 && actual_type[0] == '[' &&
                        actual_type[1] != '0') {
                        // Extract array size from "[N x T]"
                        size_t array_size = 0;
                        auto space_pos = actual_type.find(' ');
                        if (space_pos != std::string::npos) {
                            array_size = std::stoull(actual_type.substr(1, space_pos - 1));
                        }

                        std::string id = std::to_string(temp_counter_++);
                        std::string elem_type = mir_type_to_llvm(slice_type->element);

                        // 1. Alloca for the array and store value
                        std::string arr_ptr = "%arr_spill." + id;
                        emitln("    " + arr_ptr + " = alloca " + actual_type + ", align 16");
                        emitln("    store " + actual_type + " " + arg + ", ptr " + arr_ptr +
                               ", align 16");

                        // 2. Build fat pointer { ptr, i64 } on the stack
                        std::string fat_ptr = "%fat_ptr." + id;
                        emitln("    " + fat_ptr + " = alloca { ptr, i64 }, align 8");
                        // Store data pointer (array address)
                        std::string data_field = "%fat_data." + id;
                        emitln("    " + data_field +
                               " = getelementptr inbounds { ptr, i64 }, ptr " + fat_ptr +
                               ", i32 0, i32 0");
                        emitln("    store ptr " + arr_ptr + ", ptr " + data_field);
                        // Store length
                        std::string len_field = "%fat_len." + id;
                        emitln("    " + len_field + " = getelementptr inbounds { ptr, i64 }, ptr " +
                               fat_ptr + ", i32 0, i32 1");
                        emitln("    store i64 " + std::to_string(array_size) + ", ptr " +
                               len_field);

                        // 3. Pass the fat pointer address as ptr
                        arg = fat_ptr;
                        arg_type = "ptr";
                        did_array_to_slice = true;
                    }
                }
            }
        }

        if (!did_array_to_slice) {
            // If the actual argument is a struct value but the function expects ptr,
            // spill to memory. This happens for method calls where the receiver is
            // passed by value but the method signature has `this: ref This`.
            bool is_struct_value = actual_type.find("%struct.") == 0;
            bool expects_ptr = false;
            if (declared_param_types && j < declared_param_types->size()) {
                auto& param_type = (*declared_param_types)[j];
                if (param_type) {
                    // MirPointerType: typed pointer (make_pointer_type)
                    // MirPrimitiveType{Ptr}: raw opaque pointer (make_ptr_type)
                    expects_ptr = std::holds_alternative<mir::MirPointerType>(param_type->kind);
                    if (!expects_ptr) {
                        if (auto* prim = std::get_if<mir::MirPrimitiveType>(&param_type->kind)) {
                            expects_ptr = (prim->kind == mir::PrimitiveType::Ptr);
                        }
                    }
                }
            }
            if (!expects_ptr) {
                expects_ptr = declared_type == "ptr";
            }

            if (is_struct_value && expects_ptr) {
                // Spill struct value to memory so we can pass a pointer
                std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
                emitln("    " + spill_ptr + " = alloca " + actual_type);
                emitln("    store " + actual_type + " " + arg + ", ptr " + spill_ptr);
                arg = spill_ptr;
                arg_type = "ptr";
            } else if (is_struct_value) {
                arg_type = actual_type;
            } else if ((declared_type == "void" || declared_type == "i32") &&
                       !actual_type.empty() && actual_type != declared_type) {
                arg_type = actual_type;
            }
        }

        // Named function → fat pointer conversion: when a named function (global @name)
        // is passed to a parameter expecting { ptr, ptr } (function type), wrap it.
        if (declared_type == "{ ptr, ptr }" && arg.size() > 0 && arg[0] == '@') {
            std::string fat1 = new_temp();
            std::string fat2 = new_temp();
            emitln("    " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " + arg + ", 0");
            emitln("    " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");
            arg = fat2;
            arg_type = "{ ptr, ptr }";
        }

        // Unit type maps to "void" but LLVM doesn't allow void as a call argument.
        // Use "{}" (empty struct, zero-sized) as the data representation.
        if (arg_type == "void") {
            arg_type = "{}";
        }
        processed_args.push_back(arg_type + " " + arg);
    }

    // Dispatch assert_eq to the correct variant based on argument types
    std::string resolved_func_name = func_name;
    if (func_name == "assert_eq" && !processed_args.empty()) {
        // Check first argument type to dispatch
        auto& first_arg = processed_args[0];
        if (first_arg.find("ptr ") == 0) {
            resolved_func_name = "assert_eq_str";
        } else if (first_arg.find("i1 ") == 0) {
            // Bool (i1) — zero-extend to i32 and use assert_eq_i32
            resolved_func_name = "assert_eq_i32";
            for (auto& arg : processed_args) {
                if (arg.find("i1 ") == 0) {
                    std::string val = arg.substr(3);
                    std::string ext_reg = new_temp();
                    emit("  " + ext_reg + " = zext i1 " + val + " to i32");
                    arg = "i32 " + ext_reg;
                }
            }
        } else if (first_arg.find("i32 ") == 0) {
            resolved_func_name = "assert_eq_i32";
        }
        // i64 stays as assert_eq (default)
    }

    // Check if calling an sret function
    auto sret_it = sret_functions_.find(resolved_func_name);
    if (sret_it != sret_functions_.end()) {
        emit_sret_call(resolved_func_name, sret_it->second, processed_args, result_reg, inst);
    } else {
        emit_normal_call(i, resolved_func_name, processed_args, result_reg, inst);
    }
}

void MirCodegen::emit_indirect_call(const mir::CallInst& i, const std::string& param_name,
                                    mir::ValueId value_id, const mir::MirTypePtr& func_type,
                                    const std::string& result_reg,
                                    const mir::InstructionData& inst) {
    // Function types are represented as fat pointers: { func_ptr, env_ptr }
    // where env_ptr is null for non-capturing closures / plain function pointers.
    // We must extract both components and branch on env_ptr nullness.

    // Resolve the LLVM register holding the fat pointer.
    // For local variables (callee field set), use value_regs_ lookup.
    // For parameters (param_info_ path), use "%" + param_name.
    std::string fat_ptr;
    auto reg_it = value_regs_.find(value_id);
    if (reg_it != value_regs_.end() && !reg_it->second.empty()) {
        fat_ptr = reg_it->second;
    } else {
        fat_ptr = "%" + param_name;
    }

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

    // Extract function pointer and environment pointer from the fat pointer
    std::string fn_ptr = new_temp();
    std::string env_ptr = new_temp();
    emitln("    " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 0");
    emitln("    " + env_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 1");

    // Pre-compute argument values and types (used in both branches)
    std::vector<std::string> arg_vals;
    std::vector<std::string> arg_types;
    for (size_t j = 0; j < i.args.size(); ++j) {
        arg_vals.push_back(get_value_reg(i.args[j]));
        arg_types.push_back(j < param_types.size() ? param_types[j] : "i64");
    }

    // Check if env is null to determine calling convention
    std::string is_null = new_temp();
    emitln("    " + is_null + " = icmp eq ptr " + env_ptr + ", null");

    std::string id = std::to_string(temp_counter_++);
    std::string label_thin = "fp_thin" + id;
    std::string label_fat = "fp_fat" + id;
    std::string label_merge = "fp_merge" + id;

    emitln("    br i1 " + is_null + ", label %" + label_thin + ", label %" + label_fat);

    // Thin call path (no env — plain function pointer or non-capturing closure)
    emitln(label_thin + ":");
    std::string thin_args;
    for (size_t j = 0; j < arg_vals.size(); ++j) {
        if (j > 0)
            thin_args += ", ";
        thin_args += arg_types[j] + " " + arg_vals[j];
    }
    std::string thin_result;
    if (ret_type == "void") {
        emitln("    call void " + fn_ptr + "(" + thin_args + ")");
    } else {
        thin_result = new_temp();
        emitln("    " + thin_result + " = call " + ret_type + " " + fn_ptr + "(" + thin_args + ")");
    }
    emitln("    br label %" + label_merge);

    // Fat call path (env non-null — capturing closure, env as first arg)
    emitln(label_fat + ":");
    std::string fat_args = "ptr " + env_ptr;
    for (size_t j = 0; j < arg_vals.size(); ++j) {
        fat_args += ", ";
        fat_args += arg_types[j] + " " + arg_vals[j];
    }
    std::string fat_result;
    if (ret_type == "void") {
        emitln("    call void " + fn_ptr + "(" + fat_args + ")");
    } else {
        fat_result = new_temp();
        emitln("    " + fat_result + " = call " + ret_type + " " + fn_ptr + "(" + fat_args + ")");
    }
    emitln("    br label %" + label_merge);

    // Merge block
    emitln(label_merge + ":");
    if (ret_type != "void") {
        emitln("    " + result_reg + " = phi " + ret_type + " [ " + thin_result + ", %" +
               label_thin + " ], [ " + fat_result + ", %" + label_fat + " ]");
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

    emit("    call void @" + quote_func_name(func_name) + "(ptr sret(" + orig_ret_type + ") " +
         sret_slot);
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

    // Unit type is represented as "{}" in MIR but "void" in LLVM function signatures.
    // Calling a void function with "call {} @func()" is invalid LLVM IR and causes
    // stack corruption at runtime. Normalize "{}" to "void" for call instructions.
    std::string call_ret_type = (ret_type == "{}") ? "void" : ret_type;

    if (call_ret_type != "void" && !result_reg.empty()) {
        emit("    " + result_reg + " = ");
    } else {
        emit("    ");
    }
    emit("call " + call_ret_type + " @" + quote_func_name(func_name) + "(");
    for (size_t j = 0; j < processed_args.size(); ++j) {
        if (j > 0) {
            emit(", ");
        }
        emit(processed_args[j]);
    }
    emitln(")");

    if (inst.result != mir::INVALID_VALUE && call_ret_type != "void") {
        value_types_[inst.result] = call_ret_type;
    }
}

} // namespace tml::codegen
