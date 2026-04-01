TML_MODULE("codegen_x86")

//! # LLVM IR Generator - SIMD Vector Intrinsics
//!
//! This file implements the generic SIMD vector intrinsics (simd_load, simd_store,
//! simd_extract, simd_insert, simd_splat, simd_load_ptr, simd_bitmask).
//! Split from intrinsics_slice_simd.cpp for maintainability.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles generic SIMD vector intrinsics (simd_* prefix).
/// Called from try_gen_intrinsic_slice_simd() after the non-SIMD sections.
///
/// Parameters follow the same convention as try_gen_intrinsic:
///   intrinsic_name — base name with module prefix stripped (e.g. "simd_load")
///   fn_name        — the raw qualified function name passed by the caller
///   call           — the parsed call expression including arguments and callee
auto LLVMIRGen::try_gen_simd_vector_intrinsic(const std::string& intrinsic_name,
                                              const std::string& fn_name,
                                              const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Helper lambda: resolve SIMD type name from the first generic type argument [V]
    auto resolve_simd_from_generics = [&](int arg_index =
                                              0) -> std::pair<std::string, const SimdTypeInfo*> {
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics &&
                static_cast<int>(path_expr.generics->args.size()) > arg_index) {
                const auto& type_arg = path_expr.generics->args[arg_index];
                if (type_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*type_arg.as_type(), current_type_subs_);
                    if (resolved && resolved->is<types::NamedType>()) {
                        const auto& name = resolved->as<types::NamedType>().name;
                        auto it = simd_types_.find(name);
                        if (it != simd_types_.end()) {
                            return {name, &it->second};
                        }
                    }
                }
            }
        }
        return {"", nullptr};
    };

    // simd_load[V](ptr: ref V) -> V
    // Loads the entire @simd struct as a raw LLVM vector value.
    if (intrinsic_name == "simd_load") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                std::string result = fresh_reg();
                emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr);
                last_expr_type_ = vec_type;
                return result;
            }

            // Fallback: try to infer from argument's semantic type
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                const types::Type* inner_type = nullptr;
                if (arg_type->is<types::RefType>()) {
                    inner_type = arg_type->as<types::RefType>().inner.get();
                }
                if (inner_type && inner_type->is<types::NamedType>()) {
                    const auto& sname = inner_type->as<types::NamedType>().name;
                    auto it = simd_types_.find(sname);
                    if (it != simd_types_.end()) {
                        std::string vec_type = simd_vec_type_str(it->second);
                        std::string result = fresh_reg();
                        emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr);
                        last_expr_type_ = vec_type;
                        return result;
                    }
                }
            }
        }
        return "0";
    }

    // simd_store[V](ptr: mut ref V, val: V)
    // Stores a raw LLVM vector value back to a @simd struct.
    if (intrinsic_name == "simd_store") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;

            // If val_type is already a vector type string, use it directly
            if (val_type.starts_with("<")) {
                emit_line("  store " + val_type + " " + val + ", ptr " + ptr);
            } else {
                // Resolve from generics
                auto [name, info] = resolve_simd_from_generics();
                if (info) {
                    std::string vec_type = simd_vec_type_str(*info);
                    emit_line("  store " + vec_type + " " + val + ", ptr " + ptr);
                }
            }
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // simd_extract[V, T](vec: V, idx: I32) -> T
    // Extracts a single element from a SIMD vector by lane index.
    if (intrinsic_name == "simd_extract") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_; // Should be "<N x T>" from simd_load
            std::string idx = gen_expr(*call.args[1]);

            // Parse element type from vector type string "<N x T>" -> "T"
            std::string elem_type = "i32"; // Default
            if (vec_type.starts_with("<")) {
                auto x_pos = vec_type.find(" x ");
                if (x_pos != std::string::npos) {
                    elem_type = vec_type.substr(x_pos + 3);
                    if (elem_type.back() == '>') {
                        elem_type.pop_back();
                    }
                }
            }

            std::string result = fresh_reg();
            emit_line("  " + result + " = extractelement " + vec_type + " " + vec + ", i32 " + idx);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // simd_insert[V, T](vec: V, elem: T, idx: I32) -> V
    // Inserts a single element into a SIMD vector at lane index.
    if (intrinsic_name == "simd_insert") {
        if (call.args.size() >= 3) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_; // "<N x T>"
            std::string elem = gen_expr(*call.args[1]);
            std::string elem_type = last_expr_type_;
            std::string idx = gen_expr(*call.args[2]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = insertelement " + vec_type + " " + vec + ", " +
                      elem_type + " " + elem + ", i32 " + idx);
            last_expr_type_ = vec_type;
            return result;
        }
        return "0";
    }

    // simd_splat[V, T](val: T) -> V
    // Broadcasts a scalar value to all lanes of a SIMD vector.
    if (intrinsic_name == "simd_splat") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                int n = info->lane_count;
                // Efficient broadcast: insertelement at 0 + shufflevector zeroinitializer
                // Generates 2 instructions instead of N insertelements.
                // LLVM lowers this to a single broadcast instruction (e.g., VPBROADCASTB on x86).
                std::string insert_reg = fresh_reg();
                emit_line("  " + insert_reg + " = insertelement " + vec_type + " poison, " +
                          val_type + " " + val + ", i64 0");
                // Build zeroinitializer mask: <0, 0, 0, ...> (all lanes read from index 0)
                std::string mask = "<";
                for (int i = 0; i < n; ++i) {
                    if (i > 0)
                        mask += ", ";
                    mask += "i32 0";
                }
                mask += ">";
                std::string result = fresh_reg();
                emit_line("  " + result + " = shufflevector " + vec_type + " " + insert_reg + ", " +
                          vec_type + " poison, <" + std::to_string(n) + " x i32> " + mask);
                last_expr_type_ = vec_type;
                return result;
            }
        }
        return "0";
    }

    // simd_load_ptr[V](ptr: *Unit) -> V
    // Loads a SIMD vector from a raw pointer (unaligned, align 1).
    if (intrinsic_name == "simd_load_ptr") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                std::string result = fresh_reg();
                emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr + ", align 1");
                last_expr_type_ = vec_type;
                return result;
            }
        }
        return "0";
    }

    // simd_bitmask(mask: <N x i1>) -> I32
    // Converts a vector boolean mask to an integer bitmask.
    // Bit i of the result is 1 if lane i of the mask is true.
    if (intrinsic_name == "simd_bitmask") {
        if (!call.args.empty()) {
            std::string mask = gen_expr(*call.args[0]);
            std::string mask_type = last_expr_type_; // e.g. "<16 x i1>"

            // Extract N from "<N x i1>"
            if (mask_type.starts_with("<")) {
                auto space_pos = mask_type.find(' ');
                std::string n_str = mask_type.substr(1, space_pos - 1);
                int n = std::stoi(n_str);

                std::string int_type = "i" + std::to_string(n);
                std::string cast_reg = fresh_reg();
                emit_line("  " + cast_reg + " = bitcast " + mask_type + " " + mask + " to " +
                          int_type);

                std::string result = fresh_reg();
                if (n < 32) {
                    emit_line("  " + result + " = zext " + int_type + " " + cast_reg + " to i32");
                } else if (n > 32) {
                    emit_line("  " + result + " = trunc " + int_type + " " + cast_reg + " to i32");
                } else {
                    result = cast_reg;
                }
                last_expr_type_ = "i32";
                return result;
            }
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
