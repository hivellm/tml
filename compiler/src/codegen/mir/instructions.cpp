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

#include "codegen/abi.hpp"
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
                if (!i.result_type) {
                    TML_LOG_WARN("codegen",
                                 "[CG-I32] i32 fallback in LoadInst — result_type is null");
                }
                mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                // Array loads need align 16 to match the alignment of array allocas.
                bool is_array_load = type_ptr && type_ptr->is_array();
                int align = is_array_load ? 16 : 0;
                emitter_.emit_load_to(result_reg, type_str, ptr, i.is_volatile, align);
                // Track the loaded value's type for method call receiver handling
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::immediate(result_reg, type_str, type_ptr);
                }

            } else if constexpr (std::is_same_v<T, mir::StoreInst>) {
                // NRVO: skip the sret store when the call result was already forwarded
                // directly to %sret by emit_sret_call with NRVO. Without this, we would
                // emit `store %struct.T %sret, ptr %sret` which is a no-op copy to itself.
                if (current_func_has_sret_ &&
                    current_func_sret_param_id_ != mir::INVALID_VALUE &&
                    i.ptr.id == current_func_sret_param_id_ &&
                    nrvo_call_results_.count(i.value.id) > 0) {
                    return; // Value is already in %sret via NRVO — skip redundant store
                }
                std::string value = get_value_reg(i.value);
                std::string ptr = get_value_reg(i.ptr);
                mir::MirTypePtr type_ptr = i.value_type ? i.value_type : i.value.type;
                if (!type_ptr) {
                    TML_LOG_WARN("codegen",
                                 "[CG-I32] i32 fallback in StoreInst — value_type is null");
                    type_ptr = mir::make_i32_type();
                }
                std::string type_str = mir_type_to_llvm(type_ptr);
                // Bool (i1) stored to i8 struct field: the value might be i1 but
                // the struct field expects i8. We check the value's registered LLVM
                // type OR its MIR type to detect Bool values that need promotion.
                if (type_str == "i8") {
                    bool needs_zext = false;
                    auto cg_it = cg_values_.find(i.value.id);
                    if (cg_it != cg_values_.end() && cg_it->second.llvm_type == "i1") {
                        needs_zext = true;
                    } else {
                        // Check the value's own MIR type
                        mir::MirTypePtr val_type = i.value.type;
                        if (val_type) {
                            if (auto* prim = std::get_if<mir::MirPrimitiveType>(&val_type->kind)) {
                                needs_zext = (prim->kind == mir::PrimitiveType::Bool);
                            }
                        }
                        // Check the StoreInst's value_type too (may differ from value.type)
                        if (!needs_zext && i.value_type) {
                            if (auto* prim =
                                    std::get_if<mir::MirPrimitiveType>(&i.value_type->kind)) {
                                needs_zext = (prim->kind == mir::PrimitiveType::Bool);
                            }
                        }
                    }
                    if (needs_zext) {
                        std::string ext = "%zext_store" + std::to_string(temp_counter_++);
                        emitln("    " + ext + " = zext i1 " + value + " to i8");
                        value = ext;
                    }
                }
                // Array stores need align 16 to match the alignment of array allocas
                // and prevent LLVM backend crashes with SIMD aggregate stores.
                bool is_array_store = type_ptr && type_ptr->is_array();
                int align = is_array_store ? 16 : 0;
                // emit_store handles unit type "{}" skip internally.
                emitter_.emit_store(type_str, value, ptr, i.is_volatile, align);

            } else if constexpr (std::is_same_v<T, mir::AllocaInst>) {
                if (!i.alloc_type) {
                    TML_LOG_WARN("codegen",
                                 "[CG-I32] i32 fallback in AllocaInst — alloc_type is null");
                }
                mir::MirTypePtr type_ptr = i.alloc_type ? i.alloc_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);
                emit_comment("ALLOCA: result_id=" + std::to_string(inst.result) +
                             " reg=" + result_reg + " type=" + type_str);
                // Array allocas need explicit alignment to prevent LLVM backend crashes
                // when storing/loading aggregate values (SIMD instructions require alignment).
                bool is_array_alloc = type_ptr && type_ptr->is_array();
                int align = is_array_alloc ? 16 : 8;
                emitter_.emit_alloca_to(result_reg, type_str, align);
                // If zero_init is set, emit a zeroinitializer store immediately after the
                // alloca. This avoids a separate large aggregate SSA store instruction
                // (e.g., 'store [100 x i32] %v1, ptr %v2') that crashes LLVM's x86
                // backend for large arrays (SelectionDAG can't handle 400-byte aggregates).
                if (i.zero_init && is_array_alloc) {
                    emitter_.emit_store(type_str, "zeroinitializer", result_reg, false, 16);
                }
                // Track alloca as pointer type for method call receiver handling
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::address(result_reg, type_str, type_ptr);
                }

            } else if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                std::string base = get_value_reg(i.base);
                if (!i.base_type) {
                    TML_LOG_WARN("codegen",
                                 "[CG-I32] i32 fallback in GetElementPtrInst — base_type is null");
                }
                mir::MirTypePtr type_ptr = i.base_type ? i.base_type : mir::make_i32_type();
                std::string type_str = mir_type_to_llvm(type_ptr);

                // GEP requires a pointer base operand. If the base is a non-pointer
                // value (e.g., an array from insertvalue chain or a load of an
                // aggregate), spill it to a temp alloca and use the alloca pointer.
                //
                // Strategy: check cg_values_ first (most precise), then the MIR
                // Value's type annotation, then infer from the GEP's own base_type.
                // If the base is NOT positively known to be a pointer, and the GEP
                // pointee type is an aggregate (array/struct), assume spill is needed.
                bool needs_spill = false;
                std::string spill_type;
                auto base_cg_it = cg_values_.find(i.base.id);
                if (base_cg_it != cg_values_.end()) {
                    // Tracked: spill if not a pointer
                    if (base_cg_it->second.llvm_type != "ptr" &&
                        base_cg_it->second.kind != CGValueKind::Address) {
                        needs_spill = true;
                        spill_type = base_cg_it->second.llvm_type;
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
                    emitter_.emit_alloca_to(spill_reg, spill_type, 8);
                    emitter_.emit_store(spill_type, base, spill_reg);
                    // Track the spill so later reads of this value ID (e.g., in
                    // TupleInit) reload from the alloca and pick up any mutations.
                    cg_values_[i.base.id] = CGValue::address(spill_reg, spill_type);
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

                // Build typed index vector for emitter.
                // For array types [N x T], LLVM GEP requires two indices:
                //   index 0: dereference the pointer-to-array (always 0)
                //   index N: select element N within the array
                // With only one index, the GEP steps over entire arrays (N * sizeof(array)),
                // causing out-of-bounds reads/writes for any non-zero index.
                std::vector<std::pair<std::string, std::string>> gep_indices;
                if (!type_str.empty() && type_str[0] == '[') {
                    gep_indices.emplace_back("i64", "0");
                }
                for (const auto& idx : i.indices) {
                    mir::MirTypePtr idx_type_ptr = idx.type;
                    std::string idx_type = idx_type_ptr ? mir_type_to_llvm(idx_type_ptr) : "i64";
                    gep_indices.emplace_back(idx_type, get_value_reg(idx));
                }
                emitter_.emit_gep_to(result_reg, type_str, base, gep_indices);
                // GEP result is always a pointer
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] = CGValue::address(result_reg, type_str, type_ptr);
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
                    TML_LOG_WARN("codegen", "[CG-I32] i32 fallback in SelectInst — result_type and "
                                            "true_val.type are null");
                    type_ptr = mir::make_i32_type();
                }
                std::string type_str = mir_type_to_llvm(type_ptr);
                emitln("    " + result_reg + " = select i1 " + cond + ", " + type_str + " " +
                       true_val + ", " + type_str + " " + false_val);

            } else if constexpr (std::is_same_v<T, mir::StructInitInst>) {
                emit_struct_init_inst(i, result_reg, result_type, inst);

            } else if constexpr (std::is_same_v<T, mir::EnumInitInst>) {
                // Initialize enum: { tag, payload }
                //
                // The enum LLVM layout (see emit_enum_def) is either:
                //   { i32 }                       — payloadless enums
                //   { i32, [max_payload_size x i8] } — enums with any payload variant
                //
                // Payloadless variants only need to set the discriminant (field 0).
                // We emit a single insertvalue directly into result_reg — historically
                // this was split into a %tmp helper followed by `%result = %tmp`,
                // which is NOT valid LLVM IR (bare SSA aliases have no opcode and
                // are rejected by the IR parser). The bug surfaced on any MIR-path
                // compile constructing enum values (phase0p C1 reproduction).
                //
                // For variants with payload, we must splat each payload scalar into
                // the byte array at the correct offset. LLVM `insertvalue` cannot
                // inject a non-i8 scalar into a `[N x i8]` directly, so we materialize
                // the aggregate on the stack, store the discriminant and each payload
                // field at its ABI offset, and load the final aggregate. The stack
                // slot is always in the entry block so mem2reg can still promote it.
                std::string enum_type;
                if (result_type) {
                    enum_type = mir_type_to_llvm(result_type);
                } else {
                    // RC7 workaround: replace unresolved typevar suffixes
                    enum_type = "%struct." + replace_typevar_suffixes(i.enum_name);
                }

                if (i.payload.empty()) {
                    // Payloadless: single insertvalue, directly into result_reg.
                    emitln("    " + result_reg + " = insertvalue " + enum_type + " undef, i32 " +
                           std::to_string(i.variant_index) + ", 0");
                } else {
                    // Payload variant: materialize on stack so we can memcpy-style
                    // store each scalar at offset 0 (tag), +4 (payload[0]), etc.
                    // Entry-block alloca => mem2reg can promote this to SSA later.
                    std::string slot = "%tmp" + std::to_string(temp_counter_++);
                    emitln("    " + slot + " = alloca " + enum_type + ", align 8");
                    // Store the discriminant into field 0.
                    std::string tag_ptr = "%tmp" + std::to_string(temp_counter_++);
                    emitln("    " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                           slot + ", i32 0, i32 0");
                    emitln("    store i32 " + std::to_string(i.variant_index) + ", ptr " + tag_ptr +
                           ", align 4");
                    // Store each payload field into the byte array starting at offset 0.
                    // The payload array lives in field 1. We re-GEP from its base for
                    // each element. Offsets accumulate by type size as measured in
                    // emit_enum_def (i8/bool=1, i16=2, i32/f32=4, i64/f64/ptr/str=8).
                    std::string payload_base = "%tmp" + std::to_string(temp_counter_++);
                    emitln("    " + payload_base + " = getelementptr inbounds " + enum_type +
                           ", ptr " + slot + ", i32 0, i32 1, i32 0");
                    size_t byte_offset = 0;
                    for (size_t pi = 0; pi < i.payload.size(); ++pi) {
                        const auto& pval = i.payload[pi];
                        const auto& ptype =
                            pi < i.payload_types.size() ? i.payload_types[pi] : pval.type;
                        std::string elem_type_str = ptype ? mir_type_to_llvm(ptype) : "i64";
                        size_t elem_size = 8; // default pointer/i64
                        size_t elem_align = 8;
                        if (ptype) {
                            if (ptype->is_integer() || ptype->is_float()) {
                                elem_size = ptype->bit_width() / 8;
                                elem_align = elem_size;
                            } else if (ptype->is_bool()) {
                                elem_size = 1;
                                elem_align = 1;
                            } else if (std::holds_alternative<mir::MirPointerType>(ptype->kind)) {
                                elem_size = 8;
                                elem_align = 8;
                            }
                        }
                        std::string pval_reg = get_value_reg(pval);
                        std::string slot_ptr = payload_base;
                        if (byte_offset != 0) {
                            slot_ptr = "%tmp" + std::to_string(temp_counter_++);
                            emitln("    " + slot_ptr + " = getelementptr inbounds i8, ptr " +
                                   payload_base + ", i64 " + std::to_string(byte_offset));
                        }
                        emitln("    store " + elem_type_str + " " + pval_reg + ", ptr " + slot_ptr +
                               ", align " + std::to_string(elem_align));
                        byte_offset += elem_size;
                    }
                    // Load the completed aggregate into the SSA result register.
                    emitln("    " + result_reg + " = load " + enum_type + ", ptr " + slot +
                           ", align 8");
                }

            } else if constexpr (std::is_same_v<T, mir::TupleInitInst>) {
                emit_tuple_init_inst(i, result_reg);
                // Track tuple type so GEP can detect non-pointer bases
                if (inst.result != mir::INVALID_VALUE && i.result_type) {
                    std::string tup_type = mir_type_to_llvm(i.result_type);
                    cg_values_[inst.result] =
                        CGValue::immediate(result_reg, tup_type, i.result_type);
                }

            } else if constexpr (std::is_same_v<T, mir::ArrayInitInst>) {
                emit_array_init_inst(i, result_reg);
                // Track array type so GEP can detect non-pointer bases
                if (inst.result != mir::INVALID_VALUE) {
                    std::string arr_type_str;
                    if (i.result_type) {
                        arr_type_str = mir_type_to_llvm(i.result_type);
                    } else if (i.element_type && !i.elements.empty()) {
                        // Fallback: compute array type from element type + count
                        std::string elem = mir_type_to_llvm(i.element_type);
                        arr_type_str = "[" + std::to_string(i.elements.size()) + " x " + elem + "]";
                    }
                    if (!arr_type_str.empty()) {
                        cg_values_[inst.result] =
                            CGValue::immediate(result_reg, arr_type_str, i.result_type);
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

            } else if constexpr (std::is_same_v<T, mir::MakeDynObjectInst>) {
                emit_make_dyn_object_inst(i, result_reg, inst);

            } else if constexpr (std::is_same_v<T, mir::AwaitInst>) {
                // Await in MIR: in the synchronous model, the awaited expression
                // has already been called and returned Poll[T]. Extract the Ready
                // payload (field 1) from the Poll struct.
                std::string poll_val = get_value_reg(i.poll_value);
                // Determine the inner type from the AwaitInst's result_type
                std::string inner_type;
                if (i.result_type) {
                    inner_type = mir_type_to_llvm(i.result_type);
                }
                if (inner_type.empty()) {
                    emit("  ; ERROR: AwaitInst has no result_type — cannot resolve inner type");
                    emit("  unreachable");
                    return;
                }
                // Determine the Poll struct type from the poll_value's type
                std::string poll_type;
                auto poll_cg_it = cg_values_.find(i.poll_value.id);
                if (poll_cg_it != cg_values_.end()) {
                    poll_type = poll_cg_it->second.llvm_type;
                } else if (i.poll_value.type) {
                    poll_type = mir_type_to_llvm(i.poll_value.type);
                }
                if (poll_type.empty()) {
                    poll_type = "%struct.Poll__I32"; // fallback
                }
                // Spill the Poll value to memory, GEP to field 1, load inner value
                std::string spill = "%await_spill" + std::to_string(temp_counter_++);
                emitln("    " + spill + " = alloca " + poll_type + ", align 8");
                emitln("    store " + poll_type + " " + poll_val + ", ptr " + spill);
                std::string field_ptr = "%await_fld" + std::to_string(temp_counter_++);
                emitln("    " + field_ptr + " = getelementptr inbounds " + poll_type + ", ptr " +
                       spill + ", i32 0, i32 1");
                emitln("    " + result_reg + " = load " + inner_type + ", ptr " + field_ptr);
                if (inst.result != mir::INVALID_VALUE) {
                    cg_values_[inst.result] =
                        CGValue::immediate(result_reg, inner_type, i.result_type);
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

    // First check cg_values_ for actual runtime type (important for intrinsic results)
    auto left_cg = cg_values_.find(i.left.id);
    auto right_cg = cg_values_.find(i.right.id);
    if (left_cg != cg_values_.end() && !left_cg->second.llvm_type.empty()) {
        type_str = left_cg->second.llvm_type;
    } else if (right_cg != cg_values_.end() && !right_cg->second.llvm_type.empty()) {
        type_str = right_cg->second.llvm_type;
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
            TML_LOG_WARN("codegen",
                         "[CG-I32] i32 fallback in BinaryInst — all type sources are null");
            type_ptr = mir::make_i32_type();
        }
        type_str = mir_type_to_llvm(type_ptr);
    }

    bool is_float = (type_str == "double" || type_str == "float");
    bool is_signed = type_ptr ? type_ptr->is_signed() : true;

    // Get operand types from cg_values_ first, then MIR types
    auto get_operand_type = [this](const mir::Value& v) -> std::string {
        auto it = cg_values_.find(v.id);
        if (it != cg_values_.end() && !it->second.llvm_type.empty()) {
            return it->second.llvm_type;
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
        emitln("    " + left_alloca + " = alloca " + type_str + ", align 8");
        emitln("    store " + type_str + " " + left + ", ptr " + left_alloca);

        std::string right_alloca = new_temp();
        emitln("    " + right_alloca + " = alloca " + type_str + ", align 8");
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
            cg_values_[inst.result] = CGValue::immediate(result_reg, "i1", nullptr);
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
            cg_values_[inst.result] = CGValue::immediate(result_reg, "i1", nullptr);
        }
    } else {
        // Special case: string concatenation when adding two pointers (strings)
        // Use str_concat_opt for O(1) amortized complexity
        if (type_str == "ptr" && i.op == mir::BinOp::Add) {
            emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr " + left + ", ptr " +
                   right + ")");
            if (inst.result != mir::INVALID_VALUE) {
                cg_values_[inst.result] = CGValue::immediate(result_reg, "ptr", nullptr);
            }
        } else {
            std::string op_name = get_binop_name(i.op, is_float, is_signed);
            emitln("    " + result_reg + " = " + op_name + " " + type_str + " " + left + ", " +
                   right);
            // Store result type for subsequent operations
            if (inst.result != mir::INVALID_VALUE) {
                cg_values_[inst.result] = CGValue::immediate(result_reg, type_str, result_type);
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
        TML_LOG_WARN("codegen",
                     "[CG-I32] i32 fallback in UnaryInst — result_type and operand.type are null");
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
    auto agg_cg_it = cg_values_.find(i.aggregate.id);
    bool agg_is_ptr =
        (agg_cg_it != cg_values_.end() &&
         (agg_cg_it->second.llvm_type == "ptr" || agg_cg_it->second.kind == CGValueKind::Address));
    if (agg_is_ptr && codegen::is_aggregate_llvm_type(agg_type)) {
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
        std::string ev_type_str;
        mir::MirTypePtr ev_mir_type;
        if (i.result_type) {
            ev_type_str = mir_type_to_llvm(i.result_type);
            ev_mir_type = i.result_type;
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
                ev_type_str = mir_type_to_llvm(field_type);
                ev_mir_type = field_type;
            }
        }
        if (!ev_type_str.empty()) {
            cg_values_[inst.result] = CGValue::immediate(result_reg, ev_type_str, ev_mir_type);
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

    // Get actual type - first try MIR type, then stored type from cg_values_
    std::string actual_type;
    if (i.value.type) {
        actual_type = mir_type_to_llvm(i.value.type);
    } else {
        // Look up from cg_values_ (for constants and other values)
        auto it = cg_values_.find(i.value.id);
        if (it != cg_values_.end()) {
            actual_type = it->second.llvm_type;
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

} // namespace tml::codegen
