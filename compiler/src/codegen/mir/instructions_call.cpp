TML_MODULE("codegen_x86")

//! MIR Codegen — Call Instruction Emission
//!
//! This file contains all call-related instruction emission for the MIR-based
//! code generator. It was split from instructions.cpp to keep file sizes
//! manageable.
//!
//! ## Methods
//!
//! | Method                    | Purpose                                              |
//! |---------------------------|------------------------------------------------------|
//! | emit_call_inst            | Main dispatch for CallInst — direct/indirect/builtin |
//! | emit_indirect_call        | Fat-pointer (closure) indirect call emission         |
//! | emit_llvm_intrinsic_call  | @llvm.* math intrinsic calls (sqrt, sin, pow, …)     |
//! | emit_sret_call            | sret-convention struct-return calls                  |
//! | emit_normal_call          | Plain direct call emission                           |

#include "codegen/mir_codegen.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace tml::codegen {

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
                    emitln("    " + arr_ptr + " = alloca " + recv_vt + ", align 8");
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
                    emitln("    " + a_ptr + " = alloca " + recv_vt + ", align 8");
                    emitln("    store " + recv_vt + " " + receiver + ", ptr " + a_ptr);
                    emitln("    " + b_ptr + " = alloca " + recv_vt + ", align 8");
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

        // Determine the element type. Priority:
        // 1) Generic type argument [T] from call (most reliable)
        // 2) value_types_ of the value argument
        // 3) MIR type of the value argument
        // 4) Declared arg_types
        // 5) Fallback i32
        std::string elem_type = "i32"; // default

        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }

        if (elem_type == "i32") {
            auto val_vt = value_types_.find(i.args[1].id);
            if (val_vt != value_types_.end() && !val_vt->second.empty()) {
                elem_type = val_vt->second;
            } else if (i.args[1].type) {
                elem_type = mir_type_to_llvm(i.args[1].type);
            }
        }
        if (elem_type == "i32" && i.arg_types.size() >= 2 && i.arg_types[1]) {
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
        // 1) Generic type argument [T] from call (most reliable for struct types)
        // 2) Pointee type from pointer argument's MIR type
        // 3) Return type from MIR CallInst (may be I32 default from type checker)
        // 4) Fallback i32
        std::string elem_type = "i32";

        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }

        // Check pointer argument's pointee type
        if (elem_type == "i32") {
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
        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }
        if (elem_type == "i32") {
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
        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }
        if (elem_type == "i32") {
            auto val_vt = value_types_.find(i.args[1].id);
            if (val_vt != value_types_.end() && !val_vt->second.empty()) {
                elem_type = val_vt->second;
            } else if (i.args[1].type) {
                elem_type = mir_type_to_llvm(i.args[1].type);
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
        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }
        if (elem_type == "i32") {
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
        // Highest priority: explicit generic type argument [T]
        if (!i.type_args.empty() && i.type_args[0]) {
            std::string ta = mir_type_to_llvm(i.type_args[0]);
            if (ta != "void" && ta != "i32") {
                elem_type = ta;
            }
        }
        if (elem_type == "i32") {
            auto val_vt = value_types_.find(i.args[1].id);
            if (val_vt != value_types_.end() && !val_vt->second.empty()) {
                elem_type = val_vt->second;
            } else if (i.args[1].type) {
                elem_type = mir_type_to_llvm(i.args[1].type);
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

    // memcpy(dst, src, size) / mem_copy(dst, src, size) — non-overlapping copy (byte count)
    if ((base_name == "memcpy" || base_name == "mem_copy") && i.args.size() >= 3) {
        std::string dst = get_value_reg(i.args[0]);
        std::string src = get_value_reg(i.args[1]);
        std::string size = get_value_reg(i.args[2]);

        auto ensure_ptr_local = [&](const mir::Value& v, std::string& reg) {
            auto vt = value_types_.find(v.id);
            std::string vtype;
            if (vt != value_types_.end())
                vtype = vt->second;
            else if (v.type)
                vtype = mir_type_to_llvm(v.type);
            if (vtype.size() > 0 && vtype[0] == 'i' && vtype != "i1") {
                std::string id = std::to_string(temp_counter_++);
                std::string conv = "%itp.mc." + id;
                emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
                reg = conv;
            }
        };
        ensure_ptr_local(i.args[0], dst);
        ensure_ptr_local(i.args[1], src);

        // Promote i32 size to i64
        auto sz_vt = value_types_.find(i.args[2].id);
        std::string sz_type = sz_vt != value_types_.end() ? sz_vt->second
                              : i.args[2].type            ? mir_type_to_llvm(i.args[2].type)
                                                          : "i64";
        if (sz_type == "i32") {
            std::string ext = "%ext.mc." + std::to_string(temp_counter_++);
            emitln("    " + ext + " = sext i32 " + size + " to i64");
            size = ext;
        }

        emitln("    call void @llvm.memcpy.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
               size + ", i1 false)");
        return;
    }

    // memmove(dst, src, size) / mem_move(dst, src, size) / copy(src, dst, count)
    // Overlapping-safe memory copy; lowered to @llvm.memmove.p0.p0.i64.
    if ((base_name == "memmove" || base_name == "mem_move" || base_name == "copy") &&
        i.args.size() >= 3) {
        // copy(src, dst, count) uses (src, dst, count); the others use (dst, src, size)
        bool src_first = (base_name == "copy");
        std::string first = get_value_reg(i.args[0]);
        std::string second = get_value_reg(i.args[1]);
        std::string size = get_value_reg(i.args[2]);
        std::string dst = src_first ? second : first;
        std::string src = src_first ? first : second;
        const mir::Value& dst_v = src_first ? i.args[1] : i.args[0];
        const mir::Value& src_v = src_first ? i.args[0] : i.args[1];

        auto ensure_ptr_local = [&](const mir::Value& v, std::string& reg) {
            auto vt = value_types_.find(v.id);
            std::string vtype;
            if (vt != value_types_.end())
                vtype = vt->second;
            else if (v.type)
                vtype = mir_type_to_llvm(v.type);
            if (vtype.size() > 0 && vtype[0] == 'i' && vtype != "i1") {
                std::string id = std::to_string(temp_counter_++);
                std::string conv = "%itp.mm." + id;
                emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
                reg = conv;
            }
        };
        ensure_ptr_local(dst_v, dst);
        ensure_ptr_local(src_v, src);

        // Promote i32 size/count to i64
        auto sz_vt = value_types_.find(i.args[2].id);
        std::string sz_type = sz_vt != value_types_.end() ? sz_vt->second
                              : i.args[2].type            ? mir_type_to_llvm(i.args[2].type)
                                                          : "i64";
        if (sz_type == "i32") {
            std::string ext = "%ext.mm." + std::to_string(temp_counter_++);
            emitln("    " + ext + " = sext i32 " + size + " to i64");
            size = ext;
        }

        emitln("    call void @llvm.memmove.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " +
               size + ", i1 false)");
        return;
    }

    // memset(dst, value, size) / mem_set(dst, value, size) / mem_zero(dst, size)
    // write_bytes(dst, val, count) — fills count*sizeof(T) bytes
    // Lowered to @llvm.memset.p0.i64.
    if ((base_name == "memset" || base_name == "mem_set" || base_name == "mem_zero" ||
         base_name == "write_bytes") &&
        i.args.size() >= 2) {
        std::string dst = get_value_reg(i.args[0]);

        auto ensure_ptr_local = [&](const mir::Value& v, std::string& reg) {
            auto vt = value_types_.find(v.id);
            std::string vtype;
            if (vt != value_types_.end())
                vtype = vt->second;
            else if (v.type)
                vtype = mir_type_to_llvm(v.type);
            if (vtype.size() > 0 && vtype[0] == 'i' && vtype != "i1") {
                std::string id = std::to_string(temp_counter_++);
                std::string conv = "%itp.ms." + id;
                emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
                reg = conv;
            }
        };
        ensure_ptr_local(i.args[0], dst);

        // mem_zero(dst, size): two-arg form, fill byte is 0
        if (base_name == "mem_zero" && i.args.size() >= 2) {
            std::string size = get_value_reg(i.args[1]);
            auto sz_vt = value_types_.find(i.args[1].id);
            std::string sz_type = sz_vt != value_types_.end() ? sz_vt->second
                                  : i.args[1].type            ? mir_type_to_llvm(i.args[1].type)
                                                              : "i64";
            if (sz_type == "i32") {
                std::string ext = "%ext.mz." + std::to_string(temp_counter_++);
                emitln("    " + ext + " = sext i32 " + size + " to i64");
                size = ext;
            }
            emitln("    call void @llvm.memset.p0.i64(ptr " + dst + ", i8 0, i64 " + size +
                   ", i1 false)");
            return;
        }

        // Three-arg form: memset(dst, val, size) / mem_set(dst, val, size) / write_bytes(dst, val,
        // count)
        if (i.args.size() >= 3) {
            std::string val = get_value_reg(i.args[1]);
            std::string size = get_value_reg(i.args[2]);

            // Truncate fill byte to i8 if needed
            auto val_vt = value_types_.find(i.args[1].id);
            std::string val_type = val_vt != value_types_.end() ? val_vt->second
                                   : i.args[1].type             ? mir_type_to_llvm(i.args[1].type)
                                                                : "i32";
            if (val_type != "i8") {
                std::string trunc = "%trunc.ms." + std::to_string(temp_counter_++);
                emitln("    " + trunc + " = trunc " + val_type + " " + val + " to i8");
                val = trunc;
            }

            // Promote size/count to i64
            auto sz_vt = value_types_.find(i.args[2].id);
            std::string sz_type = sz_vt != value_types_.end() ? sz_vt->second
                                  : i.args[2].type            ? mir_type_to_llvm(i.args[2].type)
                                                              : "i64";
            if (sz_type == "i32") {
                std::string ext = "%ext.ms." + std::to_string(temp_counter_++);
                emitln("    " + ext + " = sext i32 " + size + " to i64");
                size = ext;
            }

            emitln("    call void @llvm.memset.p0.i64(ptr " + dst + ", i8 " + val + ", i64 " +
                   size + ", i1 false)");
            return;
        }

        return; // malformed call — skip
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

    // Dispatch println/print to type-specific runtime functions.
    // The type checker treats println/print as polymorphic builtins that accept any type,
    // but the C runtime has separate functions: print_i32, print_i64, print_f64, etc.
    // Without this dispatch, `println(42)` emits `call void @println(i32 42)` which
    // passes an integer to a function expecting a pointer, causing a segfault.
    if ((resolved_func_name == "println" || resolved_func_name == "print") &&
        !processed_args.empty()) {
        auto& arg = processed_args[0];
        std::string type_specific_func;
        bool needs_newline = (resolved_func_name == "println");

        if (arg.find("i64 ") == 0) {
            type_specific_func = "print_i64";
        } else if (arg.find("i32 ") == 0) {
            type_specific_func = "print_i32";
        } else if (arg.find("i16 ") == 0 || arg.find("i8 ") == 0) {
            // Extend to i32 for printing
            type_specific_func = "print_i32";
            std::string val = arg.substr(arg.find(' ') + 1);
            std::string orig_type = arg.substr(0, arg.find(' '));
            std::string ext_reg = new_temp();
            emitln("    " + ext_reg + " = sext " + orig_type + " " + val + " to i32");
            arg = "i32 " + ext_reg;
        } else if (arg.find("double ") == 0) {
            type_specific_func = "print_f64";
        } else if (arg.find("float ") == 0) {
            type_specific_func = "print_f64";
            std::string val = arg.substr(6);
            std::string ext_reg = new_temp();
            emitln("    " + ext_reg + " = fpext float " + val + " to double");
            arg = "double " + ext_reg;
        } else if (arg.find("i1 ") == 0) {
            type_specific_func = "print_bool";
            std::string val = arg.substr(3);
            std::string ext_reg = new_temp();
            emitln("    " + ext_reg + " = zext i1 " + val + " to i32");
            arg = "i32 " + ext_reg;
        }

        if (!type_specific_func.empty()) {
            // Emit type-specific print call
            emitln("    call void @" + type_specific_func + "(" + arg + ")");
            if (needs_newline) {
                emitln("    call void @println(ptr null)");
            }
            return;
        }
        // Fall through for ptr args (strings) — use println/print directly
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

    // Determine if the return type requires sret convention.
    // The SretConversionPass converts functions returning named struct types
    // (and tuples with >2 elements) to use an sret pointer parameter.
    // Indirect calls through fn pointers must match this convention.
    bool needs_sret = false;
    if (mir_func_type.return_type) {
        if (std::holds_alternative<mir::MirStructType>(mir_func_type.return_type->kind)) {
            needs_sret = true;
        } else if (auto* tuple = std::get_if<mir::MirTupleType>(&mir_func_type.return_type->kind)) {
            needs_sret = tuple->elements.size() > 2;
        }
    }

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

    // If sret is needed, allocate a single sret slot before branching.
    // Both thin and fat paths write to this slot; only one executes at runtime.
    std::string sret_slot;
    if (needs_sret) {
        sret_slot = "%sret.slot." + std::to_string(spill_counter_++);
        emitln("    " + sret_slot + " = alloca " + ret_type + ", align 8");
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
    if (needs_sret) {
        // sret pointer is the first argument
        thin_args = "ptr sret(" + ret_type + ") " + sret_slot;
        for (size_t j = 0; j < arg_vals.size(); ++j) {
            thin_args += ", ";
            thin_args += arg_types[j] + " " + arg_vals[j];
        }
    } else {
        for (size_t j = 0; j < arg_vals.size(); ++j) {
            if (j > 0)
                thin_args += ", ";
            thin_args += arg_types[j] + " " + arg_vals[j];
        }
    }
    std::string thin_result;
    if (needs_sret || ret_type == "void") {
        emitln("    call void " + fn_ptr + "(" + thin_args + ")");
    } else {
        thin_result = new_temp();
        emitln("    " + thin_result + " = call " + ret_type + " " + fn_ptr + "(" + thin_args + ")");
    }
    emitln("    br label %" + label_merge);

    // Fat call path (env non-null — capturing closure, env as first arg)
    emitln(label_fat + ":");
    std::string fat_args;
    if (needs_sret) {
        // sret pointer first, then env pointer, then user args
        fat_args = "ptr sret(" + ret_type + ") " + sret_slot + ", ptr " + env_ptr;
        for (size_t j = 0; j < arg_vals.size(); ++j) {
            fat_args += ", ";
            fat_args += arg_types[j] + " " + arg_vals[j];
        }
    } else {
        fat_args = "ptr " + env_ptr;
        for (size_t j = 0; j < arg_vals.size(); ++j) {
            fat_args += ", ";
            fat_args += arg_types[j] + " " + arg_vals[j];
        }
    }
    std::string fat_result;
    if (needs_sret || ret_type == "void") {
        emitln("    call void " + fn_ptr + "(" + fat_args + ")");
    } else {
        fat_result = new_temp();
        emitln("    " + fat_result + " = call " + ret_type + " " + fn_ptr + "(" + fat_args + ")");
    }
    emitln("    br label %" + label_merge);

    // Merge block
    emitln(label_merge + ":");
    if (needs_sret) {
        // Load the result from the shared sret slot
        emitln("    " + result_reg + " = load " + ret_type + ", ptr " + sret_slot + ", align 8");
        if (inst.result != mir::INVALID_VALUE) {
            value_types_[inst.result] = ret_type;
        }
    } else if (ret_type != "void") {
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
