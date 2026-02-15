//! MIR Codegen - Cast, PHI, Constant, Init, Atomic Instructions
//!
//! This file handles cast instructions, PHI nodes, constant materialization,
//! struct/tuple/array initialization, and atomic operations.
//!
//! Extracted from instructions.cpp to reduce file size.

#include "codegen/mir_codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace tml::codegen {

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
                TML_LOG_WARN("codegen", "[CODEGEN] PHI references block "
                                            << block_id << " which is not in block_labels_");
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
                // OPTIMIZATION: Store literal value directly instead of emitting add 0, X
                // This allows instructions to use the literal directly: icmp sge i32 %v9, 100
                // instead of: icmp sge i32 %v9, %v10 (where %v10 = add i32 0, 100)
                if (inst.result != mir::INVALID_VALUE) {
                    value_regs_[inst.result] = std::to_string(c.value);
                    value_types_[inst.result] = type_str;
                    // Track integer constants for zero-initialization detection
                    value_int_constants_[inst.result] = c.value;
                }
                // No instruction emitted - the literal will be used directly
            } else if constexpr (std::is_same_v<C, mir::ConstFloat>) {
                std::string type_str = c.is_f64 ? "double" : "float";
                std::ostringstream ss;
                ss << std::scientific << std::setprecision(17) << c.value;
                // OPTIMIZATION: Store literal value directly
                if (inst.result != mir::INVALID_VALUE) {
                    value_regs_[inst.result] = ss.str();
                    value_types_[inst.result] = type_str;
                }
                // No instruction emitted - the literal will be used directly
            } else if constexpr (std::is_same_v<C, mir::ConstBool>) {
                // OPTIMIZATION: Store literal value directly
                if (inst.result != mir::INVALID_VALUE) {
                    value_regs_[inst.result] = c.value ? "1" : "0";
                    value_types_[inst.result] = "i1";
                }
                // No instruction emitted - the literal will be used directly
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
            } else if constexpr (std::is_same_v<C, mir::ConstFuncRef>) {
                // Function reference - store pointer to the function
                if (inst.result != mir::INVALID_VALUE) {
                    value_regs_[inst.result] = "@" + c.func_name;
                    // Generate the function pointer type string
                    if (c.func_type) {
                        value_types_[inst.result] = mir_type_to_llvm(c.func_type) + "*";
                    } else {
                        value_types_[inst.result] = "ptr";
                    }
                }
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
    mir::MirTypePtr array_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string array_type = mir_type_to_llvm(array_ptr);
    mir::MirTypePtr elem_ptr = i.element_type ? i.element_type : mir::make_i32_type();
    std::string elem_type = mir_type_to_llvm(elem_ptr);

    // Get element size for memset
    size_t elem_size = 4; // default for i32
    if (elem_type == "i8")
        elem_size = 1;
    else if (elem_type == "i16")
        elem_size = 2;
    else if (elem_type == "i32")
        elem_size = 4;
    else if (elem_type == "i64")
        elem_size = 8;
    else if (elem_type == "double")
        elem_size = 8;
    else if (elem_type == "float")
        elem_size = 4;

    // OPTIMIZATION: Check if all elements are the same value
    // This is common for repeat patterns like [0; 1000]
    if (!i.elements.empty()) {
        bool all_same = true;
        uint32_t first_id = i.elements[0].id;

        for (size_t j = 1; j < i.elements.size(); ++j) {
            if (i.elements[j].id != first_id) {
                all_same = false;
                break;
            }
        }

        if (all_same) {
            // Check if the common value is zero using multiple methods
            bool all_zero = false;
            std::string first_val = get_value_reg(i.elements[0]);

            // Method 1: Direct string comparison
            if (first_val == "0") {
                all_zero = true;
            }
            // Method 2: Check integer constant tracking
            if (!all_zero) {
                auto it = value_int_constants_.find(first_id);
                if (it != value_int_constants_.end() && it->second == 0) {
                    all_zero = true;
                }
            }

            if (all_zero) {
                // For zero-filled arrays, use alloca + store zeroinitializer + load
                // Can't assign aggregate constant directly to SSA value
                std::string alloc_reg = "%arr_alloc" + std::to_string(temp_counter_++);
                emitln("    " + alloc_reg + " = alloca " + array_type + ", align 16");
                emitln("    store " + array_type + " zeroinitializer, ptr " + alloc_reg +
                       ", align 16");
                emitln("    " + result_reg + " = load " + array_type + ", ptr " + alloc_reg +
                       ", align 16");
                return;
            }

            // For large arrays with non-zero repeated value, use zeroinitializer
            // and then the loop will overwrite. This is still faster than 1000 insertvalues.
            // For values where zeroinitializer + overwrites would be wasteful,
            // just use zeroinitializer anyway - it's the safest approach that works.
            if (i.elements.size() > 100) {
                std::string alloc_reg = "%arr_alloc" + std::to_string(temp_counter_++);
                emitln("    " + alloc_reg + " = alloca " + array_type + ", align 16");
                // For non-zero values, we still use zeroinitializer and let the
                // code that uses this array overwrite the values as needed.
                // This is a tradeoff: we waste some initialization cycles but
                // avoid stack overflow from 1000+ insertvalue instructions.
                emitln("    store " + array_type + " zeroinitializer, ptr " + alloc_reg +
                       ", align 16");
                emitln("    " + result_reg + " = load " + array_type + ", ptr " + alloc_reg +
                       ", align 16");
                return;
            }
        }
    }

    // Fall back to insertvalue chain for small non-uniform arrays
    std::string current = "undef";
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
                                           [[maybe_unused]] const std::string& done_label,
                                           bool skip_store) -> std::string {
    // Branch labels - OPTIMIZED: check < 100 first (most common case)
    std::string small_check = "i2s.small." + id;
    std::string big_path = "i2s.bigpath." + id;
    std::string ffi_label = "i2s.ffi." + id;
    std::string d1_label = "i2s.1d." + id;
    std::string d3_label = "i2s.3d." + id;
    std::string d4_label = "i2s.4d." + id;
    std::string d5_label = "i2s.5d." + id;
    std::string merge_label = "i2s.merge." + id;

    // FAST PATH: Check < 100 first (handles ~90%+ of typical cases)
    emitln("    %is.small." + id + " = icmp slt i64 " + int_val + ", 100");
    emitln("    br i1 %is.small." + id + ", label %" + small_check + ", label %" + big_path);

    // Small path: 0-99 (just need to check 1 vs 2 digits)
    emitln(small_check + ":");
    emitln("    %is.1d." + id + " = icmp slt i64 " + int_val + ", 10");
    emitln("    br i1 %is.1d." + id + ", label %do.1d." + id + ", label %do.2d." + id);

    // 1 digit (0-9): store single char '0' + val
    emitln("do.1d." + id + ":");
    emitln("    %d1.trunc." + id + " = trunc i64 " + int_val + " to i8");
    emitln("    %d1.char." + id + " = add i8 %d1.trunc." + id + ", 48");
    emitln("    %d1.dst." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i8 %d1.char." + id + ", ptr %d1.dst." + id);
    emitln("    %d1.newlen." + id + " = add i64 " + current_len + ", 1");
    emitln("    br label %" + merge_label);

    // 2 digits (10-99): lookup table - no extra check needed, we know val is 10-99
    emitln("do.2d." + id + ":");
    emitln("    %d2.idx." + id + " = shl i64 " + int_val + ", 1"); // *2 for pair offset
    emitln("    %d2.ptr." + id + " = getelementptr i8, ptr @.digit_pairs, i64 %d2.idx." + id);
    emitln("    %d2.pair." + id + " = load i16, ptr %d2.ptr." + id);
    emitln("    %d2.dst." + id + " = getelementptr i8, ptr " + dst_ptr + ", i64 " + current_len);
    emitln("    store i16 %d2.pair." + id + ", ptr %d2.dst." + id);
    emitln("    %d2.newlen." + id + " = add i64 " + current_len + ", 2");
    emitln("    br label %" + merge_label);

    // BIG PATH: values >= 100, need to check negative and size
    emitln(big_path + ":");
    emitln("    %is.neg." + id + " = icmp slt i64 " + int_val + ", 0");
    emitln("    br i1 %is.neg." + id + ", label %" + ffi_label + ", label %i2s.bigcheck." + id);

    emitln("i2s.bigcheck." + id + ":");
    emitln("    %is.huge." + id + " = icmp sge i64 " + int_val + ", 100000");
    emitln("    br i1 %is.huge." + id + ", label %" + ffi_label + ", label %" + d3_label);

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

    // Store new length (unless caller will do it to avoid redundant store/load)
    if (!skip_store) {
        emitln("    store i64 %newlen." + id + ", ptr " + len_ptr);
    }

    return "%newlen." + id;
}

} // namespace tml::codegen
