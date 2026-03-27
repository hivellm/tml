TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Primitive Type Static Methods & Intrinsics
//!
//! This file handles:
//!
//! - **TypeId::of[T]()** inline expansion (FNV-1a hash of type name)
//! - **OOP reflection intrinsics**: is_abstract[T](), method_count[T](),
//!   method_name[T](i), base_class[T](), is_virtual[T](i), etc.
//! - **Primitive type static methods**: I32::default(), Bool::zero(),
//!   U64::max_value(), F64::from(x), etc.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

auto LLVMIRGen::gen_call_primitive_or_intrinsic(const parser::CallExpr& call,
                                                const std::string& fn_name)
    -> std::optional<std::string> {
    // ============ TypeId::of[T]() INLINE EXPANSION ============
    // TypeId::of[T]() wraps the type_id[T]() intrinsic. Since the generic function
    // cannot be monomorphized by the lazy library system, inline it here: compute
    // the FNV-1a hash of the type name and construct a TypeId struct directly.
    if (call.callee->is<parser::PathExpr>()) {
        const auto& pe = call.callee->as<parser::PathExpr>();
        if (pe.path.segments.size() == 2 && pe.path.segments[0] == "TypeId" &&
            pe.path.segments[1] == "of" && pe.generics.has_value() && !pe.generics->args.empty()) {
            // Extract type argument (e.g., I32 from TypeId::of[I32]())
            std::string type_name_for_hash = "unknown";
            const auto& first_arg = pe.generics->args[0];
            if (first_arg.is_type()) {
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                types::TypePtr type_arg =
                    resolve_parser_type_with_subs(*first_arg.as_type(), empty_subs);
                type_name_for_hash = mangle_type(type_arg);
            }
            // FNV-1a hash (same as type_id intrinsic in intrinsics.cpp)
            uint64_t hash = 14695981039346656037ULL;
            for (char c : type_name_for_hash) {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ULL;
            }
            // Construct TypeId { id: hash } inline
            // TypeId is defined as struct { id: I64 } -> %struct.TypeId = type { i64 }
            std::string result = fresh_reg();
            emit_line("  " + result + " = insertvalue %struct.TypeId zeroinitializer, i64 " +
                      std::to_string(hash) + ", 0");
            // Track coverage for the of function
            emit_coverage("TypeId::of");
            last_expr_type_ = "%struct.TypeId";
            return result;
        }
    }

    // ============ OOP REFLECTION INTRINSICS ============
    // Handle intrinsics like is_abstract[T](), method_count[T](), method_name[T](i), etc.
    // These are compile-time intrinsics that query ClassDef metadata.
    if (call.callee->is<parser::PathExpr>()) {
        const auto& pe = call.callee->as<parser::PathExpr>();
        if (pe.path.segments.size() == 1 && pe.generics.has_value() && !pe.generics->args.empty()) {
            const std::string& iname = pe.path.segments[0];

            // Extract the type name from the generic argument
            auto extract_type = [&]() -> std::string {
                const auto& first_arg = pe.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        return resolved->as<types::NamedType>().name;
                    }
                    // Fallback for class types (which resolve to ClassType, not NamedType)
                    std::string ts = types::type_to_string(resolved);
                    if (!ts.empty() && ts.find(' ') == std::string::npos &&
                        ts.find('[') == std::string::npos) {
                        return ts;
                    }
                }
                return "";
            };

            // Extract integer argument (for method_name[T](0))
            auto extract_index = [&]() -> size_t {
                if (!call.args.empty() && call.args[0]->is<parser::LiteralExpr>()) {
                    const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                    if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                        return static_cast<size_t>(
                            std::get<lexer::IntValue>(lit.token.value).value);
                    }
                }
                return 0;
            };

            if (iname == "is_abstract" || iname == "is_sealed") {
                std::string type_name = extract_type();
                bool result = false;
                if (!type_name.empty()) {
                    auto cd = env_.lookup_class(type_name);
                    if (cd.has_value()) {
                        result = (iname == "is_abstract") ? cd->is_abstract : cd->is_sealed;
                    }
                }
                last_expr_type_ = "i1";
                return result ? "true" : "false";
            }

            if (iname == "method_count") {
                std::string type_name = extract_type();
                size_t count = 0;
                if (!type_name.empty()) {
                    // Try TypeEnv first (most complete)
                    auto cd = env_.lookup_class(type_name);
                    if (cd.has_value()) {
                        count = cd->methods.size();
                    }
                    // Fallback: codegen's vtable layout (populated by gen_class_decl)
                    if (count == 0) {
                        auto vt_it = class_vtable_layout_.find(type_name);
                        if (vt_it != class_vtable_layout_.end()) {
                            count = vt_it->second.size();
                        }
                    }
                    // Fallback: codegen's class_meta_
                    if (count == 0) {
                        auto meta_it = class_meta_.find(type_name);
                        if (meta_it != class_meta_.end()) {
                            count = meta_it->second.method_count;
                        }
                    }
                    // Fallback: count from AST class methods directly
                    if (count == 0) {
                        auto class_fields_it = class_fields_.find(type_name);
                        if (class_fields_it != class_fields_.end()) {
                            // class_fields_ exists -> class was processed. Count methods
                            // by scanning the local pending_class_methods_ or similar.
                            // For now, emit a comment so we can debug
                            emit_line("  ; DEBUG method_count: type=" + type_name + " env_lookup=" +
                                      (cd.has_value() ? "yes" : "no") + " vtable=" +
                                      std::to_string(class_vtable_layout_.count(type_name)) +
                                      " meta=" + std::to_string(class_meta_.count(type_name)) +
                                      " fields=" + std::to_string(class_fields_it->second.size()));
                        }
                    }
                }
                last_expr_type_ = "i64";
                return std::to_string(count);
            }

            if (iname == "base_class" || iname == "method_name") {
                std::string type_name = extract_type();
                std::string str_val;
                if (!type_name.empty()) {
                    auto cd = env_.lookup_class(type_name);
                    if (cd.has_value()) {
                        if (iname == "base_class" && cd->base_class.has_value()) {
                            str_val = *cd->base_class;
                        } else if (iname == "method_name") {
                            size_t idx = extract_index();
                            if (idx < cd->methods.size()) {
                                str_val = cd->methods[idx].sig.name;
                            }
                        }
                    }
                    // Fallback: use codegen's class_meta_ for method names
                    if (str_val.empty() && iname == "method_name") {
                        auto meta_it = class_meta_.find(type_name);
                        if (meta_it != class_meta_.end()) {
                            size_t idx = extract_index();
                            if (idx < meta_it->second.method_names.size()) {
                                str_val = meta_it->second.method_names[idx];
                            }
                        }
                    }
                    // Fallback for base_class: check class_meta_
                    if (str_val.empty() && iname == "base_class") {
                        auto meta_it = class_meta_.find(type_name);
                        if (meta_it != class_meta_.end() && !meta_it->second.base_class.empty()) {
                            str_val = meta_it->second.base_class;
                        }
                    }
                }
                if (str_val.empty()) {
                    last_expr_type_ = "ptr";
                    return std::string("null");
                }
                last_expr_type_ = "ptr";
                return add_string_literal(str_val);
            }

            if (iname == "is_virtual" || iname == "is_override" || iname == "is_static_method") {
                std::string type_name = extract_type();
                size_t idx = extract_index();
                bool result = false;
                if (!type_name.empty()) {
                    auto cd = env_.lookup_class(type_name);
                    if (cd.has_value() && idx < cd->methods.size()) {
                        if (iname == "is_virtual") {
                            result = cd->methods[idx].is_virtual;
                        } else if (iname == "is_override") {
                            result = cd->methods[idx].is_override;
                        } else {
                            result = cd->methods[idx].is_static;
                        }
                    }
                    // Fallback: use class_meta_ for method flags
                    if (!cd.has_value() || cd->methods.empty()) {
                        auto meta_it = class_meta_.find(type_name);
                        if (meta_it != class_meta_.end() &&
                            idx < meta_it->second.method_names.size()) {
                            if (iname == "is_virtual") {
                                result = meta_it->second.method_is_virtual[idx];
                            } else if (iname == "is_override") {
                                result = meta_it->second.method_is_override[idx];
                            } else {
                                result = meta_it->second.method_is_static[idx];
                            }
                        }
                    }
                }
                last_expr_type_ = "i1";
                return result ? "true" : "false";
            }
            // ============ Interface Reflection Intrinsics ============

            if (iname == "interface_method_count") {
                std::string type_name = extract_type();
                size_t count = 0;
                if (!type_name.empty()) {
                    // Check interface_method_order_ (populated by gen_interface_decl)
                    auto it = interface_method_order_.find(type_name);
                    if (it != interface_method_order_.end()) {
                        count = it->second.size();
                    }
                    // Fallback: check TypeEnv interfaces
                    if (count == 0) {
                        auto idef = env_.lookup_interface(type_name);
                        if (idef.has_value()) {
                            count = idef->methods.size();
                        }
                    }
                    // Fallback: check behaviors (TML uses behavior as trait)
                    if (count == 0) {
                        auto bdef = env_.lookup_behavior(type_name);
                        if (bdef.has_value()) {
                            count = bdef->methods.size();
                        }
                    }
                }
                last_expr_type_ = "i64";
                return std::to_string(count);
            }

            if (iname == "interface_method_name") {
                std::string type_name = extract_type();
                size_t idx = extract_index();
                std::string name;
                if (!type_name.empty()) {
                    auto it = interface_method_order_.find(type_name);
                    if (it != interface_method_order_.end() && idx < it->second.size()) {
                        name = it->second[idx];
                    }
                    // Fallback: TypeEnv interface
                    if (name.empty()) {
                        auto idef = env_.lookup_interface(type_name);
                        if (idef.has_value() && idx < idef->methods.size()) {
                            name = idef->methods[idx].sig.name;
                        }
                    }
                    // Fallback: behaviors
                    if (name.empty()) {
                        auto bdef = env_.lookup_behavior(type_name);
                        if (bdef.has_value() && idx < bdef->methods.size()) {
                            name = bdef->methods[idx].name;
                        }
                    }
                }
                if (name.empty()) {
                    last_expr_type_ = "ptr";
                    return "null";
                }
                last_expr_type_ = "ptr";
                return add_string_literal(name);
            }
        }
    }

    // ============ PRIMITIVE TYPE STATIC METHODS ============
    // Handle Type::default() calls for primitive types
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>().path;
        if (path.segments.size() == 2) {
            std::string type_name = path.segments[0];
            const std::string& method = path.segments[1];

            // Substitute type parameter with concrete type (e.g., T -> I64)
            // This handles T::default() in generic contexts
            auto type_sub_it = current_type_subs_.find(type_name);
            if (type_sub_it != current_type_subs_.end()) {
                type_name = types::type_to_string(type_sub_it->second);
            } else if (!current_type_subs_.empty()) {
                // Handle behavior name used as static dispatch in generic impl.
                // e.g., Default::default() inside impl[T: Default] -> T::default()
                // e.g., Step::steps_between() inside impl[T: Step] -> I32::steps_between()
                // Use dynamic lookup instead of hardcoded set — any behavior qualifies.
                // Find the generic type param (not Self/This) whose concrete type
                // implements this behavior. Typically this is "T".
                if (env_.lookup_behavior(type_name).has_value()) {
                    for (const auto& [param, concrete] : current_type_subs_) {
                        if (param == "Self" || param == "This")
                            continue;
                        type_name = types::type_to_string(concrete);
                        // Note: fn_name is const ref, but we don't need to update it here
                        // because this section returns directly from the method handlers below.
                        break;
                    }
                }
            }

            bool is_primitive_type =
                type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                type_name == "Bool" || type_name == "Str";

            // Handle default(), zero(), one(), min_value(), max_value() for primitive types
            if (is_primitive_type && (method == "default" || method == "zero")) {
                // Track coverage
                emit_coverage(type_name + "::" + method);

                // Integer types: default/zero is 0
                if (type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                    type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                    type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                    type_name == "U128") {
                    std::string llvm_ty;
                    if (type_name == "I8" || type_name == "U8")
                        llvm_ty = "i8";
                    else if (type_name == "I16" || type_name == "U16")
                        llvm_ty = "i16";
                    else if (type_name == "I32" || type_name == "U32")
                        llvm_ty = "i32";
                    else if (type_name == "I64" || type_name == "U64")
                        llvm_ty = "i64";
                    else
                        llvm_ty = "i128";
                    last_expr_type_ = llvm_ty;
                    return std::string("0");
                }
                // Float types: default/zero is 0.0
                if (type_name == "F32") {
                    last_expr_type_ = "float";
                    return std::string("0.0");
                }
                if (type_name == "F64") {
                    last_expr_type_ = "double";
                    return std::string("0.0");
                }
                // Bool: default is false
                if (type_name == "Bool") {
                    last_expr_type_ = "i1";
                    return std::string("false");
                }
                // Str: default is empty string
                if (type_name == "Str") {
                    std::string empty_str = add_string_literal("");
                    last_expr_type_ = "ptr";
                    return empty_str;
                }
            }

            // Handle one() for primitive types
            if (is_primitive_type && method == "one") {
                emit_coverage(type_name + "::one");

                // Integer types: one is 1
                if (type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                    type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                    type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                    type_name == "U128") {
                    std::string llvm_ty;
                    if (type_name == "I8" || type_name == "U8")
                        llvm_ty = "i8";
                    else if (type_name == "I16" || type_name == "U16")
                        llvm_ty = "i16";
                    else if (type_name == "I32" || type_name == "U32")
                        llvm_ty = "i32";
                    else if (type_name == "I64" || type_name == "U64")
                        llvm_ty = "i64";
                    else
                        llvm_ty = "i128";
                    last_expr_type_ = llvm_ty;
                    return std::string("1");
                }
                // Float types: one is 1.0
                if (type_name == "F32") {
                    last_expr_type_ = "float";
                    return std::string("1.0");
                }
                if (type_name == "F64") {
                    last_expr_type_ = "double";
                    return std::string("1.0");
                }
            }

            // Handle min_value() for bounded types
            if (is_primitive_type && method == "min_value") {
                emit_coverage(type_name + "::min_value");

                if (type_name == "I8") {
                    last_expr_type_ = "i8";
                    return std::string("-128");
                }
                if (type_name == "I16") {
                    last_expr_type_ = "i16";
                    return std::string("-32768");
                }
                if (type_name == "I32") {
                    last_expr_type_ = "i32";
                    return std::string("-2147483648");
                }
                if (type_name == "I64") {
                    last_expr_type_ = "i64";
                    return std::string("-9223372036854775808");
                }
                if (type_name == "U8" || type_name == "U16" || type_name == "U32" ||
                    type_name == "U64" || type_name == "U128") {
                    std::string llvm_ty;
                    if (type_name == "U8")
                        llvm_ty = "i8";
                    else if (type_name == "U16")
                        llvm_ty = "i16";
                    else if (type_name == "U32")
                        llvm_ty = "i32";
                    else if (type_name == "U64")
                        llvm_ty = "i64";
                    else
                        llvm_ty = "i128";
                    last_expr_type_ = llvm_ty;
                    return std::string("0");
                }
            }

            // Handle max_value() for bounded types
            if (is_primitive_type && method == "max_value") {
                emit_coverage(type_name + "::max_value");

                if (type_name == "I8") {
                    last_expr_type_ = "i8";
                    return std::string("127");
                }
                if (type_name == "I16") {
                    last_expr_type_ = "i16";
                    return std::string("32767");
                }
                if (type_name == "I32") {
                    last_expr_type_ = "i32";
                    return std::string("2147483647");
                }
                if (type_name == "I64") {
                    last_expr_type_ = "i64";
                    return std::string("9223372036854775807");
                }
                if (type_name == "U8") {
                    last_expr_type_ = "i8";
                    return std::string("255");
                }
                if (type_name == "U16") {
                    last_expr_type_ = "i16";
                    return std::string("65535");
                }
                if (type_name == "U32") {
                    last_expr_type_ = "i32";
                    return std::string("4294967295");
                }
                if (type_name == "U64") {
                    last_expr_type_ = "i64";
                    return std::string("18446744073709551615");
                }
            }

            // Handle Type::from(value) calls for type conversion
            if (is_primitive_type && method == "from" && !call.args.empty()) {
                // Coverage: track which From conversion was used
                emit_coverage(type_name + "::from");

                // Generate the source value
                std::string src_val = gen_expr(*call.args[0]);
                std::string src_type = last_expr_type_;

                // Determine target LLVM type
                std::string target_ty;
                bool target_is_float = false;
                bool target_is_signed = true;
                if (type_name == "I8")
                    target_ty = "i8";
                else if (type_name == "I16")
                    target_ty = "i16";
                else if (type_name == "I32")
                    target_ty = "i32";
                else if (type_name == "I64")
                    target_ty = "i64";
                else if (type_name == "I128")
                    target_ty = "i128";
                else if (type_name == "U8") {
                    target_ty = "i8";
                    target_is_signed = false;
                } else if (type_name == "U16") {
                    target_ty = "i16";
                    target_is_signed = false;
                } else if (type_name == "U32") {
                    target_ty = "i32";
                    target_is_signed = false;
                } else if (type_name == "U64") {
                    target_ty = "i64";
                    target_is_signed = false;
                } else if (type_name == "U128") {
                    target_ty = "i128";
                    target_is_signed = false;
                } else if (type_name == "F32") {
                    target_ty = "float";
                    target_is_float = true;
                } else if (type_name == "F64") {
                    target_ty = "double";
                    target_is_float = true;
                }

                // Determine source type properties
                bool src_is_float = (src_type == "float" || src_type == "double");

                // If types are the same, just return the value
                if (src_type == target_ty) {
                    last_expr_type_ = target_ty;
                    return src_val;
                }

                std::string result = fresh_reg();

                // Get bit widths for integer types
                auto get_bit_width = [](const std::string& ty) -> int {
                    if (ty == "i8")
                        return 8;
                    if (ty == "i16")
                        return 16;
                    if (ty == "i32")
                        return 32;
                    if (ty == "i64")
                        return 64;
                    if (ty == "i128")
                        return 128;
                    if (ty == "float")
                        return 32;
                    if (ty == "double")
                        return 64;
                    return 0;
                };

                int src_width = get_bit_width(src_type);
                int target_width = get_bit_width(target_ty);

                if (src_is_float && target_is_float) {
                    // Float to float conversion
                    if (src_width < target_width) {
                        emit_line("  " + result + " = fpext " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = fptrunc " + src_type + " " + src_val +
                                  " to " + target_ty);
                    }
                } else if (src_is_float && !target_is_float) {
                    // Float to int conversion
                    if (target_is_signed) {
                        emit_line("  " + result + " = fptosi " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = fptoui " + src_type + " " + src_val + " to " +
                                  target_ty);
                    }
                } else if (!src_is_float && target_is_float) {
                    // Int to float conversion
                    // Use last_expr_is_unsigned_ to determine si/ui conversion
                    if (last_expr_is_unsigned_) {
                        emit_line("  " + result + " = uitofp " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = sitofp " + src_type + " " + src_val + " to " +
                                  target_ty);
                    }
                } else {
                    // Int to int conversion
                    if (src_width < target_width) {
                        // Extension - use sext for signed, zext for unsigned
                        // Use last_expr_is_unsigned_ which was set by gen_expr for the source
                        // Special case: i1 (Bool) is always unsigned - use zext to get 0 or 1
                        if (last_expr_is_unsigned_ || src_type == "i1") {
                            emit_line("  " + result + " = zext " + src_type + " " + src_val +
                                      " to " + target_ty);
                        } else {
                            emit_line("  " + result + " = sext " + src_type + " " + src_val +
                                      " to " + target_ty);
                        }
                    } else if (src_width > target_width) {
                        // Truncation
                        emit_line("  " + result + " = trunc " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        // Same width, just a bitcast (e.g., I32 to U32)
                        last_expr_type_ = target_ty;
                        return src_val;
                    }
                }

                last_expr_type_ = target_ty;
                return result;
            }
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
