TML_MODULE("codegen_x86")

//! # LLVM IR Generator - First Pass
//!
//! Implements `generate_first_pass()`, called from `generate()` before function
//! body codegen. This pass iterates module declarations to:
//!   - Register const values into global_constants_
//!   - Generate struct/union/enum/class/interface/namespace type declarations
//!   - Register impl blocks for vtable generation and collect associated constants
//!   - Register trait/behavior declarations for default method generation
//!   - Run pending generic instantiations collected during type registration
//!   - Emit dyn types for registered behaviors
//!   - Pre-register all local function signatures and return types

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

namespace tml::codegen {

// Static helpers copied from generate.cpp (needed for const extraction)
static std::string get_const_llvm_type(const parser::TypePtr& type) {
    if (!type)
        return "i64";
    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            const std::string& name = named.path.segments.back();
            if (name == "I8" || name == "U8")
                return "i8";
            if (name == "I16" || name == "U16")
                return "i16";
            if (name == "I32" || name == "U32")
                return "i32";
            if (name == "I64" || name == "U64")
                return "i64";
            if (name == "I128" || name == "U128")
                return "i128";
            if (name == "Bool")
                return "i1";
            if (name == "Isize" || name == "Usize")
                return "i64";
            if (name == "Str")
                return "ptr";
        }
    } else if (type->is<parser::TupleType>()) {
        const auto& tuple = type->as<parser::TupleType>();
        if (tuple.elements.empty())
            return "{}";
        std::string result = "{ ";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += get_const_llvm_type(tuple.elements[i]);
        }
        result += " }";
        return result;
    }
    return "i64";
}

static std::string try_extract_scalar_const(const parser::Expr* expr) {
    if (!expr)
        return "";
    if (expr->is<parser::CastExpr>()) {
        const auto& cast = expr->as<parser::CastExpr>();
        if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
            expr = cast.expr.get();
        } else if (cast.expr && cast.expr->is<parser::UnaryExpr>()) {
            const auto& unary = cast.expr->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                const auto& lit = unary.operand->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    int64_t int_val = static_cast<int64_t>(lit.token.int_value().value);
                    return std::to_string(-int_val);
                }
            }
            return "";
        } else {
            return "";
        }
    }
    if (expr->is<parser::UnaryExpr>()) {
        const auto& unary = expr->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
            const auto& lit = unary.operand->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                int64_t int_val = static_cast<int64_t>(lit.token.int_value().value);
                return std::to_string(-int_val);
            }
        }
        return "";
    }
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
            return std::to_string(lit.token.int_value().value);
        } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
            return lit.token.bool_value() ? "1" : "0";
        } else if (lit.token.kind == lexer::TokenKind::NullLiteral) {
            return "null";
        } else if (lit.token.kind == lexer::TokenKind::StringLiteral) {
            return "STR:" + std::string(lit.token.string_value().value);
        }
    }
    return "";
}

static std::string try_extract_const_value(const parser::Expr* expr, const parser::TypePtr& type,
                                           std::string& out_llvm_type) {
    if (!expr)
        return "";
    if (expr->is<parser::TupleExpr>()) {
        const auto& tuple = expr->as<parser::TupleExpr>();
        if (tuple.elements.empty()) {
            out_llvm_type = "{}";
            return "zeroinitializer";
        }
        std::vector<std::string> elem_types;
        if (type && type->is<parser::TupleType>()) {
            const auto& tuple_type = type->as<parser::TupleType>();
            for (const auto& et : tuple_type.elements) {
                elem_types.push_back(get_const_llvm_type(et));
            }
        }
        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const(tuple.elements[i].get());
            if (val.empty())
                return "";
            elem_values.push_back(val);
        }
        if (elem_types.size() != elem_values.size()) {
            elem_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i) {
                elem_types.push_back("i64");
            }
        }
        std::string llvm_type = "{ ";
        for (size_t i = 0; i < elem_types.size(); ++i) {
            if (i > 0)
                llvm_type += ", ";
            llvm_type += elem_types[i];
        }
        llvm_type += " }";
        out_llvm_type = llvm_type;
        std::string llvm_value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0)
                llvm_value += ", ";
            llvm_value += elem_types[i] + " " + elem_values[i];
        }
        llvm_value += " }";
        return llvm_value;
    }
    std::string scalar = try_extract_scalar_const(expr);
    if (!scalar.empty()) {
        out_llvm_type = get_const_llvm_type(type);
        return scalar;
    }
    return "";
}

/// First pass over module declarations: register const values, struct/enum/class types,
/// trait declarations, and pre-register local function signatures.
/// Called from generate() before function body codegen.
void LLVMIRGen::generate_first_pass(const parser::Module& module) {
    // First pass: collect const declarations and struct/enum declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            std::string llvm_type;
            std::string value =
                try_extract_const_value(const_decl.value.get(), const_decl.type, llvm_type);
            if (!value.empty()) {
                global_constants_[const_decl.name] = {value, llvm_type};
            }
        } else if (decl->is<parser::StructDecl>()) {
            gen_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::UnionDecl>()) {
            gen_union_decl(decl->as<parser::UnionDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            gen_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::ClassDecl>()) {
            gen_class_decl(decl->as<parser::ClassDecl>());
        } else if (decl->is<parser::InterfaceDecl>()) {
            gen_interface_decl(decl->as<parser::InterfaceDecl>());
        } else if (decl->is<parser::NamespaceDecl>()) {
            gen_namespace_decl(decl->as<parser::NamespaceDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Register impl block for vtable generation
            register_impl(&decl->as<parser::ImplDecl>());

            // Collect associated constants from impl block
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) { // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                for (const auto& const_decl : impl.constants) {
                    std::string qualified_name = type_name + "::" + const_decl.name;
                    std::string llvm_type;
                    std::string value =
                        try_extract_const_value(const_decl.value.get(), const_decl.type, llvm_type);
                    if (!value.empty()) {
                        global_constants_[qualified_name] = {value, llvm_type};
                    }
                }
            }
        } else if (decl->is<parser::TraitDecl>()) {
            // Register trait/behavior declaration for default implementations
            const auto& trait_decl = decl->as<parser::TraitDecl>();
            // Register by FQN when module context is known (avoids short-name collisions
            // between same-named traits in different modules, e.g. core::io::Write vs
            // core::fmt::Write)
            if (!current_module_name_.empty()) {
                std::string fqn = current_module_name_ + "::" + trait_decl.name;
                trait_decls_[fqn] = &trait_decl;
            }
            // Register by short name only if not already registered (first-write-wins
            // for short name preserves backward compat with callers that use short name)
            if (trait_decls_.find(trait_decl.name) == trait_decls_.end()) {
                trait_decls_[trait_decl.name] = &trait_decl;
            }
        }
    }

    // Generate any pending generic instantiations collected during first pass
    // This happens after structs/enums are registered but before function codegen
    {
        auto saved_lib = in_library_body_;
        in_library_body_ = true;
        generate_pending_instantiations();
        in_library_body_ = saved_lib;
    }

    // Emit dyn types for all registered behaviors before function generation
    // This must happen BEFORE saving output_ to ensure dyn types appear before functions
    for (const auto& [key, vtable_name] : vtables_) {
        // key is "TypeName::BehaviorName", extract behavior name
        size_t pos = key.find("::");
        if (pos != std::string::npos) {
            std::string behavior_name = key.substr(pos + 2);
            emit_dyn_type(behavior_name);
        }
    }

    // Emit dyn types from type_defs_buffer_ to output_ NOW, before saving
    // This ensures dyn types appear before imported module functions that use them
    std::string dyn_type_defs = type_defs_buffer_.str();
    if (!dyn_type_defs.empty()) {
        emit_line("; Dynamic dispatch types");
        output_ << dyn_type_defs;
        type_defs_buffer_.str(""); // Clear so we don't emit them twice later
    }

    // Pre-pass: register all local function signatures and return types.
    // This serves two purposes:
    // 1. Type inference: later functions can be used in earlier functions correctly
    // 2. Name priority: local functions overwrite library module functions with
    //    the same short name (e.g., a local `to_uppercase` takes priority over
    //    `core::str::to_uppercase` that was pre-registered during library Phase 1).
    //    This prevents library essential modules from shadowing local definitions.
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            // Skip generic functions - their return types depend on instantiation
            if (!func.generics.empty()) {
                continue;
            }
            // Pre-register function signature (name, params, return type)
            // so forward references resolve to the local function, not a
            // library function with the same name.
            if (!func.is_unsafe && func.body.has_value()) {
                pre_register_func(func);
            }
            if (func.return_type.has_value()) {
                types::TypePtr semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});
                if (semantic_ret) {
                    func_return_types_[func.name] = semantic_ret;
                }
            }
        }
    }
}

} // namespace tml::codegen
