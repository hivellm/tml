TML_MODULE("codegen_x86")

//! MIR Codegen - Cast, PHI, Constant, Init, Atomic Instructions
//!
//! This file handles cast instructions, PHI nodes, constant materialization,
//! struct/tuple/array initialization, and atomic operations.
//!
//! Extracted from instructions.cpp to reduce file size.

#include "codegen/abi.hpp"
#include "codegen/mir_codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <variant>

namespace tml::codegen {

// ============================================================================
// Cast Instruction
// ============================================================================

void MirCodegen::emit_cast_inst(const mir::CastInst& i, const std::string& result_reg,
                                const mir::InstructionData& inst) {
    std::string operand = get_value_reg(i.operand);
    mir::MirTypePtr src_ptr = i.source_type ? i.source_type : i.operand.type;
    if (!src_ptr) {
        TML_LOG_WARN("codegen",
                     "[CG-I32] i32 fallback in CastInst — source_type and operand.type are null");
        src_ptr = mir::make_i32_type();
    }
    if (!i.target_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in CastInst — target_type is null");
    }
    mir::MirTypePtr tgt_ptr = i.target_type ? i.target_type : mir::make_i32_type();
    std::string src_type = mir_type_to_llvm(src_ptr);
    std::string tgt_type = mir_type_to_llvm(tgt_ptr);

    std::string operand_actual_type;
    auto cg_op_it = cg_values_.find(i.operand.id);
    if (cg_op_it != cg_values_.end()) {
        operand_actual_type = cg_op_it->second.llvm_type;
    } else if (i.operand.type) {
        operand_actual_type = mir_type_to_llvm(i.operand.type);
    }

    if (!operand_actual_type.empty() && operand_actual_type != src_type) {
        src_type = operand_actual_type;
    }

    // Enum → integer cast. The source MIR type is MirEnumType, which lowers to
    // `%struct.<Name>` = `{ i32 }` (payloadless) or `{ i32, [N x i8] }` (payload).
    // The user-level cast `k as I64` must become:
    //   (1) extract the i32 discriminant from field 0
    //   (2) zext / trunc / noop to the target integer width
    // Without this special-case, emit_cast falls through to the generic `bitcast
    // %struct.Kind %k to i64` path, which LLVM rejects (aggregate-to-integer
    // bitcast is invalid, and if the enum value arrived as a ptr parameter we
    // would also be mis-using %k as an aggregate SSA register).
    //
    // Two sub-cases for the enum operand representation:
    //   (a) SSA value of aggregate type (`%v = insertvalue %struct.Kind ...`)
    //       → `%disc = extractvalue %struct.Kind %v, 0`
    //   (b) Pointer to aggregate (`%k` after struct-by-pointer parameter lowering
    //       or after a prior spill) → GEP field 0 + `load i32`
    //
    // After extracting the i32 discriminant, apply an int-to-int resize to match
    // the target type width (zext when widening, trunc when narrowing, noop when
    // target is also i32).
    if (src_ptr && std::holds_alternative<mir::MirEnumType>(src_ptr->kind)) {
        bool tgt_is_int = (!tgt_type.empty() && tgt_type[0] == 'i' && tgt_type != "i1");
        if (tgt_is_int) {
            // Resolve the actual LLVM struct type for the enum. Prefer the
            // operand's recorded aggregate type so we match whatever insertvalue
            // / extractvalue chain produced it; otherwise fall back to the MIR
            // type mapping.
            std::string enum_struct_type;
            if (!operand_actual_type.empty() &&
                codegen::is_aggregate_llvm_type(operand_actual_type)) {
                enum_struct_type = operand_actual_type;
            } else {
                enum_struct_type = mir_type_to_llvm(src_ptr);
            }

            // Determine if the operand is a pointer (parameter-as-ptr case) or
            // a direct aggregate SSA value.
            bool operand_is_ptr = false;
            if (cg_op_it != cg_values_.end()) {
                operand_is_ptr = (cg_op_it->second.llvm_type == "ptr" ||
                                  cg_op_it->second.kind == CGValueKind::Address ||
                                  cg_op_it->second.kind == CGValueKind::FatPointer);
            } else if (operand_actual_type == "ptr") {
                operand_is_ptr = true;
            }

            // Extract the i32 discriminant.
            std::string disc_reg = "%disc" + std::to_string(temp_counter_++);
            if (operand_is_ptr) {
                // GEP field 0 of the enum struct, then load i32.
                std::string gep_reg = "%disc.gep" + std::to_string(temp_counter_++);
                emitln("    " + gep_reg + " = getelementptr inbounds " + enum_struct_type +
                       ", ptr " + operand + ", i32 0, i32 0");
                emitln("    " + disc_reg + " = load i32, ptr " + gep_reg + ", align 4");
            } else {
                // Aggregate SSA value — extractvalue field 0.
                emitln("    " + disc_reg + " = extractvalue " + enum_struct_type + " " + operand +
                       ", 0");
            }

            // Resize the i32 discriminant to the target integer width.
            int tgt_bits = 0;
            try {
                tgt_bits = std::stoi(tgt_type.substr(1));
            } catch (...) {
                tgt_bits = 32;
            }
            if (tgt_bits == 32) {
                // Same width as discriminant — alias the SSA register directly.
                value_regs_[inst.result] = disc_reg;
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::immediate(disc_reg, "i32", tgt_ptr);
                }
            } else if (tgt_bits > 32) {
                // Widen (i64, i128) — discriminants are always non-negative, so zext.
                emitln("    " + result_reg + " = zext i32 " + disc_reg + " to " + tgt_type);
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
                }
            } else {
                // Narrow (i8, i16) — trunc.
                emitln("    " + result_reg + " = trunc i32 " + disc_reg + " to " + tgt_type);
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
                }
            }
            return;
        }
    }

    // If casting an aggregate value to ptr, spill it first
    if (tgt_type == "ptr" && codegen::is_aggregate_llvm_type(operand_actual_type)) {
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + operand_actual_type);
        emitln("    store " + operand_actual_type + " " + operand + ", ptr " + spill_ptr);
        emitln("    " + result_reg + " = bitcast ptr " + spill_ptr + " to ptr");
        if (inst.result != mir::INVALID_VALUE) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, "ptr", tgt_ptr);
        }
    } else if (i.kind == mir::CastKind::Bitcast && codegen::is_aggregate_llvm_type(src_type) &&
               codegen::is_aggregate_llvm_type(tgt_type) && src_type != tgt_type) {
        // Class upcast: derived struct to base struct
        std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
        emitln("    " + spill_ptr + " = alloca " + src_type);
        emitln("    store " + src_type + " " + operand + ", ptr " + spill_ptr);
        emitln("    " + result_reg + " = load " + tgt_type + ", ptr " + spill_ptr);
        if (inst.result != mir::INVALID_VALUE) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
        }
    } else if ((src_type == "{ ptr, ptr }" || operand_actual_type == "{ ptr, ptr }") &&
               (tgt_type[0] == 'i' && tgt_type != "i1")) {
        // Function/closure fat pointer { fn_ptr, env_ptr } cast to integer:
        // Extract fn_ptr (element 0) then ptrtoint to target integer type.
        std::string extract_reg = "%extract" + std::to_string(temp_counter_++);
        emitln("    " + extract_reg + " = extractvalue { ptr, ptr } " + operand + ", 0");
        emitln("    " + result_reg + " = ptrtoint ptr " + extract_reg + " to " + tgt_type);
        if (inst.result != mir::INVALID_VALUE) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
        }
    } else if ((tgt_type == "{ ptr, ptr }" || tgt_type == "ptr") &&
               (src_type[0] == 'i' && src_type != "i1") && i.kind == mir::CastKind::IntToPtr) {
        // Integer to function pointer: inttoptr then wrap in fat pointer if needed.
        std::string ptr_reg = "%itp" + std::to_string(temp_counter_++);
        emitln("    " + ptr_reg + " = inttoptr " + src_type + " " + operand + " to ptr");
        if (tgt_type == "{ ptr, ptr }") {
            std::string fat1 = "%fat1." + std::to_string(temp_counter_++);
            emitln("    " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " + ptr_reg + ", 0");
            emitln("    " + result_reg + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");
        } else {
            // ptr to ptr — just alias, no bitcast needed
            value_regs_[inst.result] = ptr_reg;
            cg_values_[inst.result] = CGValue::immediate(ptr_reg, tgt_type, tgt_ptr);
            return;
        }
        if (inst.result != mir::INVALID_VALUE) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
        }
    } else if (src_type == tgt_type) {
        // Same type — no cast needed, just alias the register.
        // This happens when cg_values_ overrides src_type to match tgt_type
        // (e.g., constant 8 already coerced from i32 to i64 by a prior operation).
        value_regs_[inst.result] = operand;
        cg_values_[inst.result] = CGValue::immediate(operand, tgt_type, tgt_ptr);
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
        } else if (src_is_int && tgt_is_int && src_type != tgt_type) {
            // Safety net: auto-correct int-to-int casts based on width
            int src_bits = std::stoi(src_type.substr(1));
            int tgt_bits = std::stoi(tgt_type.substr(1));
            if (tgt_bits < src_bits) {
                cast_name = "trunc";
            } else if (tgt_bits > src_bits) {
                // Default to zext; sext would need signed info from MIR
                cast_name = (i.kind == mir::CastKind::SExt) ? "sext" : "zext";
            }
        } else if (src_type == "ptr" && tgt_is_int) {
            // Safety net: ptr-to-integer must use ptrtoint, not bitcast
            // (function pointers, raw pointers cast to I64/U64/etc.)
            cast_name = "ptrtoint";
        } else if (src_is_int && tgt_type == "ptr") {
            // Safety net: integer-to-ptr must use inttoptr, not bitcast
            cast_name = "inttoptr";
        }

        emitln("    " + result_reg + " = " + cast_name + " " + src_type + " " + operand + " to " +
               tgt_type);
        if (inst.result != mir::INVALID_VALUE) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, tgt_type, tgt_ptr);
        }
    }
}

// ============================================================================
// PHI Instruction
// ============================================================================

void MirCodegen::emit_phi_inst(const mir::PhiInst& i, const std::string& result_reg,
                               const mir::InstructionData& inst) {
    if (!i.result_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in PhiInst — result_type is null");
    }
    mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);

    if (i.incoming.empty()) {
        if (type_str == "{}") {
            // Empty struct (Unit) — no add instruction possible, just use undef
            emitln("    ; Unit phi with no incoming — treating as zeroinitializer");
            value_regs_[inst.result] = "zeroinitializer";
        } else {
            emitln("    " + result_reg + " = add " + type_str + " undef, 0");
        }
    } else {
        emit("    " + result_reg + " = phi " + type_str + " ");
        for (size_t j = 0; j < i.incoming.size(); ++j) {
            if (j > 0) {
                emit(", ");
            }
            std::string val = get_value_reg(i.incoming[j].first);
            uint32_t block_id = i.incoming[j].second;
            // Use the exit label for the predecessor block. Bounds check injection
            // can split a MIR block into multiple LLVM blocks (e.g., if.then1 ->
            // bc.panic.N + bc.ok.N). The phi must reference the actual LLVM block
            // that branches to this merge point (bc.ok.N), not the original entry
            // label (if.then1).
            std::string label;
            auto exit_it = block_exit_labels_.find(block_id);
            if (exit_it != block_exit_labels_.end()) {
                label = exit_it->second;
            } else {
                // Fall back to entry label
                auto label_it = block_labels_.find(block_id);
                if (label_it != block_labels_.end()) {
                    label = label_it->second;
                } else {
                    label = "MISSING_BLOCK_" + std::to_string(block_id);
                    TML_LOG_WARN("codegen", "[CODEGEN] PHI references block "
                                                << block_id << " which is not in block_labels_");
                }
            }
            emit("[ " + val + ", %" + label + " ]");
        }
        emitln();
    }

    if (inst.result != mir::INVALID_VALUE) {
        // For empty phi with zeroinitializer, the reg was overwritten in value_regs_
        std::string phi_reg =
            (i.incoming.empty() && type_str == "{}") ? std::string("zeroinitializer") : result_reg;
        cg_values_[inst.result] = CGValue::immediate(phi_reg, type_str, type_ptr);
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
                    std::string lit = std::to_string(c.value);
                    value_regs_[inst.result] = lit;
                    cg_values_[inst.result] = CGValue::immediate(lit, type_str, nullptr);
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
                    std::string lit = ss.str();
                    value_regs_[inst.result] = lit;
                    cg_values_[inst.result] = CGValue::immediate(lit, type_str, nullptr);
                }
                // No instruction emitted - the literal will be used directly
            } else if constexpr (std::is_same_v<C, mir::ConstBool>) {
                // OPTIMIZATION: Store literal value directly
                if (inst.result != mir::INVALID_VALUE) {
                    std::string lit = c.value ? "1" : "0";
                    value_regs_[inst.result] = lit;
                    cg_values_[inst.result] = CGValue::immediate(lit, "i1", nullptr);
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
                    cg_values_[inst.result] = CGValue::immediate(result_reg, "ptr", nullptr);
                    // Store string content for compile-time length optimization
                    value_string_contents_[inst.result] = c.value;
                }
            } else if constexpr (std::is_same_v<C, mir::ConstUnit>) {
                // Unit type — zero-sized. Map to zeroinitializer so that any
                // downstream use (insertvalue, store, phi) gets a valid operand.
                if (inst.result != mir::INVALID_VALUE) {
                    value_regs_[inst.result] = "zeroinitializer";
                    cg_values_[inst.result] = CGValue::zero_sized();
                }
            } else if constexpr (std::is_same_v<C, mir::ConstFuncRef>) {
                // Function reference — build a fat pointer { func_ptr, env_ptr } matching
                // the closure calling convention. env_ptr is null for plain function refs.
                // This allows indirect calls through local variables to use the same
                // emit_indirect_call path as closures.
                if (inst.result != mir::INVALID_VALUE) {
                    std::string func_ptr = "@" + quote_func_name(c.func_name);
                    std::string tmp = new_temp();
                    emitln("    " + tmp + " = insertvalue { ptr, ptr } undef, ptr " + func_ptr +
                           ", 0");
                    emitln("    " + result_reg + " = insertvalue { ptr, ptr } " + tmp +
                           ", ptr null, 1");
                    cg_values_[inst.result] =
                        CGValue::immediate(result_reg, "{ ptr, ptr }", nullptr);
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
        auto it = cg_values_.find(val_id);
        if (it != cg_values_.end()) {
            actual_type = it->second.llvm_type;
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
                    // Use zext for Bool (i1→i8) to avoid sign-extending true to -1
                    std::string ext_op = (actual_type == "i1") ? "zext" : "sext";
                    std::string ext_tmp = "%ext" + std::to_string(temp_counter_++);
                    emitln("    " + ext_tmp + " = " + ext_op + " " + actual_type + " " + field_val +
                           " to " + expected_type);
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
                    TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in StructInitInst (class) — "
                                            "field_ptr is null for field "
                                                << j);
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
                    TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in StructInitInst (value) — "
                                            "field_ptr is null for field "
                                                << j);
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
            // Empty struct init: produce zeroinitializer instead of an
            // incorrect `insertvalue ... i32 0, 0` which assumes field 0
            // is i32 (breaks for structs whose field 0 is ptr).
            // Zero-init alloca on stack and load it to get the SSA value.
            std::string tmp_alloca = "%zi_tmp" + std::to_string(temp_counter_++);
            emitln("    " + tmp_alloca + " = alloca " + struct_type);
            emitln("    store " + struct_type + " zeroinitializer, ptr " + tmp_alloca);
            emitln("    " + result_reg + " = load " + struct_type + ", ptr " + tmp_alloca);
        }
    }

    if (inst.result != mir::INVALID_VALUE) {
        cg_values_[inst.result] = CGValue::immediate(result_reg, struct_type, result_type);
    }
}

// ============================================================================
// Tuple Init Instruction
// ============================================================================

void MirCodegen::emit_tuple_init_inst(const mir::TupleInitInst& i, const std::string& result_reg) {
    if (!i.result_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in TupleInitInst — result_type is null");
    }
    mir::MirTypePtr tuple_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string tuple_type = mir_type_to_llvm(tuple_ptr);

    // Unit type (empty tuple) is "{}" (zero-sized). No alloca needed.
    if (tuple_type == "{}" && i.elements.empty()) {
        // Map the result register to zeroinitializer so downstream uses get a
        // valid operand. The result_reg was derived from the instruction's result
        // ID, so we overwrite it in value_regs_ by looking up the register name.
        // This is safe because `result_reg` = "%vN" where N is the result ID.
        // Extract the ID from result_reg ("%vN" -> N).
        if (result_reg.size() > 2 && result_reg[0] == '%' && result_reg[1] == 'v') {
            unsigned id = std::stoul(result_reg.substr(2));
            value_regs_[id] = "zeroinitializer";
            cg_values_[id] = CGValue::zero_sized();
        }
        return;
    }

    std::string alloc_reg = "%tmp" + std::to_string(temp_counter_++);
    emitln("    " + alloc_reg + " = alloca " + tuple_type);

    for (size_t j = 0; j < i.elements.size(); ++j) {
        std::string elem_val = get_value_reg(i.elements[j]);
        mir::MirTypePtr elem_ptr = (j < i.element_types.size() && i.element_types[j])
                                       ? i.element_types[j]
                                       : i.elements[j].type;
        if (!elem_ptr) {
            TML_LOG_WARN("codegen",
                         "[CG-I32] i32 fallback in TupleInitInst — elem_ptr is null for element "
                             << j);
            elem_ptr = mir::make_i32_type();
        }
        std::string elem_type = mir_type_to_llvm(elem_ptr);

        // If this element is an array type that was spilled to an alloca for
        // mutation (e.g., var arr: [U8; 4] with subsequent arr[i] = x), reload
        // from the spill alloca so the mutations are included in the tuple.
        if (!elem_type.empty() && elem_type[0] == '[') {
            auto cg_spill_it = cg_values_.find(i.elements[j].id);
            if (cg_spill_it != cg_values_.end() &&
                cg_spill_it->second.kind == CGValueKind::Address) {
                std::string reloaded = "%reload" + std::to_string(temp_counter_++);
                emitln("    " + reloaded + " = load " + elem_type + ", ptr " +
                       cg_spill_it->second.reg);
                elem_val = reloaded;
            }
        }

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
    if (!i.result_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in ArrayInitInst — result_type is null");
    }
    mir::MirTypePtr array_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string array_type = mir_type_to_llvm(array_ptr);
    if (!i.element_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in ArrayInitInst — element_type is null");
    }
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

        // If the element value's actual LLVM type differs from the target element type,
        // emit a truncation/extension so the insertvalue uses the correct type.
        std::string actual_elem_type = elem_type;
        if (i.elements[j].type) {
            std::string val_type = mir_type_to_llvm(i.elements[j].type);
            if (val_type != elem_type && (elem_type == "i8" || elem_type == "i16")) {
                // Truncate wider int to narrower element type
                std::string trunc_reg = "%trunc" + std::to_string(temp_counter_++);
                emitln("    " + trunc_reg + " = trunc " + val_type + " " + elem_val + " to " +
                       elem_type);
                elem_val = trunc_reg;
                actual_elem_type = elem_type;
            }
        }

        std::string next =
            (j == i.elements.size() - 1) ? result_reg : "%tmp" + std::to_string(temp_counter_++);
        emitln("    " + next + " = insertvalue " + array_type + " " + current + ", " +
               actual_elem_type + " " + elem_val + ", " + std::to_string(j));
        current = next;
    }
}

// ============================================================================
// Atomic Instructions
// ============================================================================

void MirCodegen::emit_atomic_load_inst(const mir::AtomicLoadInst& i, const std::string& result_reg,
                                       const mir::InstructionData& inst) {
    std::string ptr = get_value_reg(i.ptr);
    if (!i.result_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in AtomicLoadInst — result_type is null");
    }
    mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string ordering = atomic_ordering_to_llvm(i.ordering);
    emitln("    " + result_reg + " = load atomic " + type_str + ", ptr " + ptr + " " + ordering +
           ", align " + std::to_string(get_type_alignment(type_ptr)));
    if (inst.result != mir::INVALID_VALUE) {
        cg_values_[inst.result] = CGValue::immediate(result_reg, type_str, type_ptr);
    }
}

void MirCodegen::emit_atomic_store_inst(const mir::AtomicStoreInst& i) {
    std::string value = get_value_reg(i.value);
    std::string ptr = get_value_reg(i.ptr);
    mir::MirTypePtr type_ptr = i.value_type ? i.value_type : i.value.type;
    if (!type_ptr) {
        TML_LOG_WARN(
            "codegen",
            "[CG-I32] i32 fallback in AtomicStoreInst — value_type and value.type are null");
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
    if (!i.value_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in AtomicRMWInst — value_type is null");
    }
    mir::MirTypePtr type_ptr = i.value_type ? i.value_type : mir::make_i32_type();
    std::string type_str = mir_type_to_llvm(type_ptr);
    std::string ordering = atomic_ordering_to_llvm(i.ordering);
    std::string op = atomic_rmw_op_to_llvm(i.op);
    emitln("    " + result_reg + " = atomicrmw " + op + " ptr " + ptr + ", " + type_str + " " +
           value + " " + ordering);
    if (inst.result != mir::INVALID_VALUE) {
        cg_values_[inst.result] = CGValue::immediate(result_reg, type_str, type_ptr);
    }
}

void MirCodegen::emit_atomic_cmpxchg_inst(const mir::AtomicCmpXchgInst& i,
                                          const std::string& result_reg,
                                          const mir::InstructionData& inst) {
    std::string ptr = get_value_reg(i.ptr);
    std::string expected = get_value_reg(i.expected);
    std::string desired = get_value_reg(i.desired);
    if (!i.value_type) {
        TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in AtomicCmpXchgInst — value_type is null");
    }
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
        cg_values_[inst.result] = CGValue::immediate(result_reg, type_str, type_ptr);
    }
}

// emit_inline_int_to_string removed — was only used by V8-style Text optimizations

// ============================================================================
// Closure Initialization Instruction
// ============================================================================

void MirCodegen::emit_closure_init_inst(const mir::ClosureInitInst& i,
                                        const std::string& result_reg,
                                        const mir::InstructionData& inst) {
    // Closure initialization: create fat pointer { func_ptr, env_ptr }
    // First insertvalue: set function pointer to @closure_name
    std::string tmp1 = new_temp();
    emitln("    " + tmp1 + " = insertvalue { ptr, ptr } undef, ptr @" + i.func_name + ", 0");

    // Second insertvalue: set environment pointer (null for non-capturing closures)
    std::string env_ptr = "null";
    if (!i.captures.empty()) {
        // For capturing closures, would need to create environment struct
        // For now, just use null (non-capturing case)
        env_ptr = "null";
    }
    emitln("    " + result_reg + " = insertvalue { ptr, ptr } " + tmp1 + ", ptr " + env_ptr +
           ", 1");

    // Mark the result type as function type (fat pointer)
    // CRITICAL: This must be done so that when CallInst looks up the argument type,
    // it finds the { ptr, ptr } type in the cg_values_ map
    if (inst.result != mir::INVALID_VALUE) {
        cg_values_[inst.result] = CGValue::immediate(result_reg, "{ ptr, ptr }", nullptr);
    }
}

// ============================================================================
// Vtable Emission for dyn Dispatch
// ============================================================================

void MirCodegen::emit_vtables(const mir::Module& module) {
    // Build behavior_method_order_ from module behaviors
    for (const auto& behavior : module.behaviors) {
        if (behavior_method_order_.find(behavior.name) == behavior_method_order_.end()) {
            behavior_method_order_[behavior.name] = behavior.methods;
        }
    }

    // Build a function name lookup: maps mangled name -> true (for existence check)
    std::unordered_set<std::string> available_functions;
    for (const auto& func : module.functions) {
        available_functions.insert(func.name);
    }

    // Emit vtables for each impl block that has a behavior
    for (const auto& impl : module.impls) {
        if (impl.behavior_name.empty())
            continue;

        std::string vtable_name = "@vtable." + impl.type_name + "." + impl.behavior_name;

        // Skip if already emitted
        if (emitted_vtables_.count(vtable_name))
            continue;
        emitted_vtables_.insert(vtable_name);

        // Get method order for this behavior
        auto bmo_it = behavior_method_order_.find(impl.behavior_name);
        if (bmo_it == behavior_method_order_.end())
            continue;

        const auto& method_order = bmo_it->second;
        if (method_order.empty())
            continue;

        // Build vtable type: { ptr, ptr, ... } (one ptr per method)
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < method_order.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        bool all_found = true;
        for (size_t i = 0; i < method_order.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";

            // Look up the function name for this method
            auto mf_it = impl.method_functions.find(method_order[i]);
            if (mf_it != impl.method_functions.end()) {
                vtable_value += "ptr @" + quote_func_name(mf_it->second);
            } else {
                // Method not found in impl — this shouldn't happen for a valid program
                all_found = false;
                break;
            }
        }
        vtable_value += " }";

        if (!all_found)
            continue;

        // Emit the vtable global constant
        // Use linkonce_odr so each CGU can have a copy (linker deduplicates)
        std::string comdat_name = vtable_name.substr(1); // remove '@' prefix
        emitln();
        emitln("$" + comdat_name + " = comdat any");
        emitln(vtable_name + " = linkonce_odr constant " + vtable_type + " " + vtable_value +
               ", comdat");

        // Register vtable
        std::string key = impl.type_name + "::" + impl.behavior_name;
        vtables_[key] = vtable_name;
    }
}

// ============================================================================
// MakeDynObject Instruction (fat pointer construction for dyn dispatch)
// ============================================================================

void MirCodegen::emit_make_dyn_object_inst(const mir::MakeDynObjectInst& i,
                                           const std::string& result_reg,
                                           const mir::InstructionData& inst) {
    std::string data_ptr = get_value_reg(i.data_ptr);
    std::string vtable_name = "@vtable." + i.concrete_type + "." + i.behavior_name;

    // Emit the dyn type definition if not already emitted
    if (emitted_dyn_types_.find(i.behavior_name) == emitted_dyn_types_.end()) {
        emitted_dyn_types_.insert(i.behavior_name);
        // The dyn type is always { ptr, ptr }, no separate named type needed
    }

    // Register the vtable for later emission
    std::string vtable_key = i.concrete_type + "::" + i.behavior_name;
    vtables_[vtable_key] = vtable_name;

    // Construct the fat pointer: { data_ptr, vtable_ptr }
    std::string tmp1 = new_temp();
    emitln("    " + tmp1 + " = insertvalue { ptr, ptr } undef, ptr " + data_ptr + ", 0");
    emitln("    " + result_reg + " = insertvalue { ptr, ptr } " + tmp1 + ", ptr " + vtable_name +
           ", 1");

    // Track the type for dyn dispatch resolution
    if (inst.result != mir::INVALID_VALUE) {
        cg_values_[inst.result] = CGValue::immediate(result_reg, "{ ptr, ptr }", nullptr);
        value_dyn_behavior_[inst.result] = i.behavior_name;
    }
}

} // namespace tml::codegen
