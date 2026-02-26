TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Main Entry Point
//!
//! This file implements the main `generate()` code generation entry point.
//!
//! Related files:
//! - generate_cache.cpp: GlobalASTCache and GlobalLibraryIRCache implementations
//! - generate_support.cpp: Loop metadata, lifetime intrinsics, print type inference,
//!   namespace support, and library state capture

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "version_generated.hpp"

#include <filesystem>
#include <iomanip>
#include <set>

namespace tml::codegen {

// Helper: Convert a parser::Type to a string for name mangling
// Used to extract behavior type parameters for impl method names
static std::string parser_type_to_string(const parser::Type& type) {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        std::string result = named.path.segments.empty() ? "" : named.path.segments.back();
        if (named.generics.has_value() && !named.generics->args.empty()) {
            result += "__";
            for (size_t i = 0; i < named.generics->args.size(); ++i) {
                if (i > 0)
                    result += "__";
                const auto& arg = named.generics->args[i];
                if (arg.is_type()) {
                    result += parser_type_to_string(*arg.as_type());
                }
            }
        }
        return result;
    } else if (type.is<parser::PtrType>()) {
        const auto& ptr = type.as<parser::PtrType>();
        std::string prefix = ptr.is_mut ? "mutptr_" : "ptr_";
        return prefix + parser_type_to_string(*ptr.inner);
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        std::string prefix = ref.is_mut ? "mutref_" : "ref_";
        return prefix + parser_type_to_string(*ref.inner);
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        return "Slice__" + parser_type_to_string(*slice.element);
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple = type.as<parser::TupleType>();
        std::string result = "Tuple";
        for (const auto& elem : tuple.elements) {
            result += "__" + parser_type_to_string(*elem);
        }
        return result;
    }
    return "";
}

// Helper: Get the LLVM type string for a constant's declared type
// For primitives like I32, I64, Bool, etc.
static std::string get_const_llvm_type(const parser::TypePtr& type) {
    if (!type)
        return "i64"; // Default fallback

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            const std::string& name = named.path.segments.back();
            // Map TML primitive types to LLVM types
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
    return "i64"; // Default for unknown types
}

/// Try to extract a compile-time constant scalar value from an expression.
/// Handles: LiteralExpr, CastExpr(LiteralExpr), UnaryExpr(-LiteralExpr),
/// and CastExpr(UnaryExpr(-LiteralExpr)).
/// Returns empty string if the expression is not a constant scalar.
static std::string try_extract_scalar_const(const parser::Expr* expr) {
    if (!expr)
        return "";

    // Unwrap cast expressions (e.g., "15 as U8")
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

    // Unary negation (e.g., -128)
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

    // Direct literal
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
            return std::to_string(lit.token.int_value().value);
        } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
            return lit.token.bool_value() ? "1" : "0";
        } else if (lit.token.kind == lexer::TokenKind::NullLiteral) {
            return "null";
        }
    }

    return "";
}

/// Try to extract a compile-time constant value (scalar or tuple) from an expression.
/// For tuples, returns the full LLVM aggregate constant (e.g., "{ i8 15, i8 1, i8 0 }").
/// For scalars, returns just the value (e.g., "42").
/// Also sets out_llvm_type to the corresponding LLVM type.
/// Returns empty string if the expression is not a compile-time constant.
static std::string try_extract_const_value(const parser::Expr* expr, const parser::TypePtr& type,
                                           std::string& out_llvm_type) {
    if (!expr)
        return "";

    // Handle tuple expressions
    if (expr->is<parser::TupleExpr>()) {
        const auto& tuple = expr->as<parser::TupleExpr>();
        if (tuple.elements.empty()) {
            out_llvm_type = "{}";
            return "zeroinitializer";
        }

        // Get element types from the declared type
        std::vector<std::string> elem_types;
        if (type && type->is<parser::TupleType>()) {
            const auto& tuple_type = type->as<parser::TupleType>();
            for (const auto& et : tuple_type.elements) {
                elem_types.push_back(get_const_llvm_type(et));
            }
        }

        // Extract each element value
        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const(tuple.elements[i].get());
            if (val.empty())
                return ""; // Non-constant element
            elem_values.push_back(val);
        }

        // If we don't have declared types, default each element to i64
        if (elem_types.size() != elem_values.size()) {
            elem_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i) {
                elem_types.push_back("i64");
            }
        }

        // Build LLVM type: { i8, i8, i8 }
        std::string llvm_type = "{ ";
        for (size_t i = 0; i < elem_types.size(); ++i) {
            if (i > 0)
                llvm_type += ", ";
            llvm_type += elem_types[i];
        }
        llvm_type += " }";
        out_llvm_type = llvm_type;

        // Build LLVM value: { i8 15, i8 1, i8 0 }
        std::string llvm_value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0)
                llvm_value += ", ";
            llvm_value += elem_types[i] + " " + elem_values[i];
        }
        llvm_value += " }";
        return llvm_value;
    }

    // Handle scalar expressions
    std::string scalar = try_extract_scalar_const(expr);
    if (!scalar.empty()) {
        out_llvm_type = get_const_llvm_type(type);
        return scalar;
    }

    return "";
}

auto LLVMIRGen::generate(const parser::Module& module)
    -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    type_defs_buffer_.str(""); // Clear type definitions buffer
    enum_drop_output_.str(""); // Clear enum drop function buffer
    generated_enum_drop_functions_.clear();
    string_literals_.clear();
    string_literal_dedup_.clear();
    current_type_subs_.clear(); // Clear type substitutions from previous compilation
    temp_counter_ = 0;
    label_counter_ = 0;

    // Register builtin enums
    // Ordering enum: Less=0, Equal=1, Greater=2
    enum_variants_["Ordering::Less"] = 0;
    enum_variants_["Ordering::Equal"] = 1;
    enum_variants_["Ordering::Greater"] = 2;

    // Register builtin generic enums: Maybe[T], Outcome[T, E]
    // These need to be stored in builtin_enum_decls_ to keep the AST alive
    {
        // Maybe[T] { Just(T), Nothing }
        auto maybe_decl = std::make_unique<parser::EnumDecl>();
        maybe_decl->name = "Maybe";
        maybe_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Just(T) variant
        parser::EnumVariant just_variant;
        just_variant.name = "Just";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> just_fields;
        just_fields.push_back(std::move(t_type));
        just_variant.tuple_fields = std::move(just_fields);
        maybe_decl->variants.push_back(std::move(just_variant));

        // Nothing variant
        parser::EnumVariant nothing_variant;
        nothing_variant.name = "Nothing";
        maybe_decl->variants.push_back(std::move(nothing_variant));

        pending_generic_enums_["Maybe"] = maybe_decl.get();
        builtin_enum_decls_.push_back(std::move(maybe_decl));
    }

    {
        // Outcome[T, E] { Ok(T), Err(E) }
        auto outcome_decl = std::make_unique<parser::EnumDecl>();
        outcome_decl->name = "Outcome";
        outcome_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});
        outcome_decl->generics.push_back(parser::GenericParam{
            "E", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Ok(T) variant
        parser::EnumVariant ok_variant;
        ok_variant.name = "Ok";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> ok_fields;
        ok_fields.push_back(std::move(t_type));
        ok_variant.tuple_fields = std::move(ok_fields);
        outcome_decl->variants.push_back(std::move(ok_variant));

        // Err(E) variant
        parser::EnumVariant err_variant;
        err_variant.name = "Err";
        auto e_type = std::make_unique<parser::Type>();
        e_type->kind = parser::NamedType{parser::TypePath{{"E"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> err_fields;
        err_fields.push_back(std::move(e_type));
        err_variant.tuple_fields = std::move(err_fields);
        outcome_decl->variants.push_back(std::move(err_variant));

        pending_generic_enums_["Outcome"] = outcome_decl.get();
        builtin_enum_decls_.push_back(std::move(outcome_decl));
    }

    {
        // Poll[T] { Ready(T), Pending }
        auto poll_decl = std::make_unique<parser::EnumDecl>();
        poll_decl->name = "Poll";
        poll_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Ready(T) variant
        parser::EnumVariant ready_variant;
        ready_variant.name = "Ready";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> ready_fields;
        ready_fields.push_back(std::move(t_type));
        ready_variant.tuple_fields = std::move(ready_fields);
        poll_decl->variants.push_back(std::move(ready_variant));

        // Pending variant
        parser::EnumVariant pending_variant;
        pending_variant.name = "Pending";
        poll_decl->variants.push_back(std::move(pending_variant));

        pending_generic_enums_["Poll"] = poll_decl.get();
        builtin_enum_decls_.push_back(std::move(poll_decl));
    }

    emit_header();
    emit_debug_info_header(); // Initialize debug info metadata
    emit_runtime_decls();
    emit_module_lowlevel_decls();

    // Save headers before generating imported module code
    std::string headers = output_.str();
    cached_preamble_headers_ = headers; // Save for capture_library_state()
    output_.str("");

    std::string imported_func_code;
    std::string imported_type_defs;

    if (options_.cached_library_state && options_.cached_library_state->valid) {
        // FAST PATH: Restore pre-computed library state instead of regenerating.
        // This skips emit_module_pure_tml_functions() entirely (~9 seconds for zlib).
        const auto& state = *options_.cached_library_state;

        // Type definitions are the same regardless of library_decls_only
        imported_type_defs = state.imported_type_defs;

        // For function IR: if library_decls_only is true, use pre-computed declarations.
        // If false, use the full definitions.
        if (options_.library_decls_only) {
            // Use pre-computed declarations extracted from full library IR
            // (contains define→declare conversions for TML functions defined in the shared lib)
            std::ostringstream func_code;
            func_code << state.imported_func_decls;

            // imported_func_decls already includes both:
            // 1. define→declare conversions for TML library functions
            // 2. FFI declare lines (brotli_*, zlib_*, etc.) NOT in preamble
            imported_func_code = func_code.str();
        } else {
            // Use full definitions (for coverage mode or library_ir_only)
            imported_func_code = state.imported_func_code;

            // When force_internal_linkage is set (suite mode workers), convert
            // library function definitions to internal linkage. The cached library
            // state was generated without force_internal_linkage (needed for shared
            // .obj in non-coverage mode), but suite workers need internal linkage
            // to avoid duplicate symbol errors when multiple .obj files in the same
            // suite each contain the same library function definitions.
            if (options_.force_internal_linkage && !imported_func_code.empty()) {
                std::string result;
                result.reserve(imported_func_code.size() + 4096);
                std::istringstream stream(imported_func_code);
                std::string line;
                while (std::getline(stream, line)) {
                    // Convert "define <type>" to "define internal <type>" for @tml_ functions
                    // but skip lines already marked internal/linkonce_odr
                    if (line.find("define ") != std::string::npos &&
                        line.find("@tml_") != std::string::npos &&
                        line.find("define internal ") == std::string::npos &&
                        line.find("define linkonce_odr ") == std::string::npos) {
                        auto pos = line.find("define ");
                        if (pos != std::string::npos) {
                            // Check if it's "define dllexport"
                            auto dpos = line.find("define dllexport ");
                            if (dpos != std::string::npos) {
                                line.replace(dpos, 17, "define internal ");
                            } else {
                                line.replace(pos, 7, "define internal ");
                            }
                        }
                    }
                    result += line + "\n";
                }
                imported_func_code = std::move(result);
            }

            // Restore string literals referenced by function definitions
            for (const auto& sl : state.string_literals) {
                string_literals_.push_back(sl);
            }
        }

        // Restore internal registries
        for (const auto& [k, v] : state.struct_types) {
            if (struct_types_.find(k) == struct_types_.end()) {
                struct_types_[k] = v;
                // Restore nullable_maybe_types_ for nullable Maybe types from cache
                // These were optimized to "ptr" during gen_enum_instantiation
                if (v == "ptr" && k.starts_with("Maybe__")) {
                    nullable_maybe_types_.insert(k);
                }
            }
        }
        for (const auto& k : state.union_types) {
            union_types_.insert(k);
        }
        for (const auto& [k, v] : state.enum_variants) {
            if (enum_variants_.find(k) == enum_variants_.end()) {
                enum_variants_[k] = v;
            }
        }
        for (const auto& [k, v] : state.global_constants) {
            if (global_constants_.find(k) == global_constants_.end()) {
                global_constants_[k] = ConstInfo{v.first, v.second};
            }
        }
        for (const auto& [struct_name, fields] : state.struct_fields) {
            if (struct_fields_.find(struct_name) == struct_fields_.end()) {
                std::vector<FieldInfo> fi;
                fi.reserve(fields.size());
                for (const auto& f : fields) {
                    fi.push_back(FieldInfo{f.name, f.index, f.llvm_type, f.semantic_type});
                }
                struct_fields_[struct_name] = std::move(fi);
            }
        }
        for (const auto& [k, v] : state.functions) {
            if (functions_.find(k) == functions_.end()) {
                functions_[k] =
                    FuncInfo{v.llvm_name, v.llvm_func_type, v.ret_type, v.param_types, v.is_extern};
            }
        }
        for (const auto& [k, v] : state.func_return_types) {
            if (func_return_types_.find(k) == func_return_types_.end()) {
                func_return_types_[k] = v;
            }
        }
        for (const auto& name : state.generated_functions) {
            generated_functions_.insert(name);
        }
        // Restore declared externals to prevent duplicate declarations
        // when user code has @extern functions with the same symbol names
        for (const auto& name : state.declared_externals) {
            declared_externals_.insert(name);
        }

        // Restore class types (class_name -> LLVM type name)
        for (const auto& [k, v] : state.class_types) {
            if (class_types_.find(k) == class_types_.end()) {
                class_types_[k] = v;
            }
        }

        // Restore class field info
        for (const auto& [class_name, fields] : state.class_fields) {
            if (class_fields_.find(class_name) == class_fields_.end()) {
                std::vector<ClassFieldInfo> fi;
                fi.reserve(fields.size());
                for (const auto& f : fields) {
                    ClassFieldInfo cfi;
                    cfi.name = f.name;
                    cfi.index = f.index;
                    cfi.llvm_type = f.llvm_type;
                    cfi.vis = static_cast<parser::MemberVisibility>(f.vis);
                    cfi.is_inherited = f.is_inherited;
                    for (const auto& step : f.inheritance_path) {
                        cfi.inheritance_path.push_back({step.class_name, step.index});
                    }
                    fi.push_back(std::move(cfi));
                }
                class_fields_[class_name] = std::move(fi);
            }
        }

        // Restore value classes
        for (const auto& name : state.value_classes) {
            value_classes_.insert(name);
        }

        // Restore emitted dyn types (prevents duplicate %dyn.X type definitions)
        for (const auto& name : state.emitted_dyn_types) {
            emitted_dyn_types_.insert(name);
        }

        // Re-parse library module ASTs for pending generic registration.
        // We need the AST pointers to be valid for pending_generic_structs_ etc.
        // The GlobalASTCache already has these cached, so this is just pointer lookups.
        if (env_.module_registry()) {
            const auto& registry = env_.module_registry();
            const auto& all_modules = registry->get_all_modules();
            for (const auto& [mod_name, mod_info] : all_modules) {
                if (!mod_info.has_pure_tml_functions || mod_info.source_code.empty())
                    continue;
                if (!GlobalASTCache::should_cache(mod_name))
                    continue;
                const parser::Module* cached_ast = GlobalASTCache::instance().get(mod_name);
                if (!cached_ast)
                    continue;

                // Re-register generic structs/enums/funcs/impls from cached ASTs
                for (const auto& decl : cached_ast->decls) {
                    if (decl->is<parser::StructDecl>()) {
                        const auto& s = decl->as<parser::StructDecl>();
                        if (!s.generics.empty() && pending_generic_structs_.find(s.name) ==
                                                       pending_generic_structs_.end()) {
                            pending_generic_structs_[s.name] = &s;
                        }
                        if (struct_decls_.find(s.name) == struct_decls_.end()) {
                            struct_decls_[s.name] = &s;
                        }
                    } else if (decl->is<parser::EnumDecl>()) {
                        const auto& e = decl->as<parser::EnumDecl>();
                        if (!e.generics.empty() &&
                            pending_generic_enums_.find(e.name) == pending_generic_enums_.end()) {
                            pending_generic_enums_[e.name] = &e;
                        }
                    } else if (decl->is<parser::FuncDecl>()) {
                        const auto& func = decl->as<parser::FuncDecl>();
                        if (!func.generics.empty() && pending_generic_funcs_.find(func.name) ==
                                                          pending_generic_funcs_.end()) {
                            pending_generic_funcs_[func.name] = &func;
                        }
                    } else if (decl->is<parser::ImplDecl>()) {
                        const auto& impl = decl->as<parser::ImplDecl>();
                        if (!impl.generics.empty()) {
                            std::string type_name;
                            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                                type_name =
                                    impl.self_type->as<parser::NamedType>().path.segments.back();
                            }
                            if (!type_name.empty() && pending_generic_impls_.find(type_name) ==
                                                          pending_generic_impls_.end()) {
                                pending_generic_impls_[type_name] = &impl;
                            }
                        }
                        // Register for vtable generation
                        register_impl(&impl);
                    } else if (decl->is<parser::TraitDecl>()) {
                        const auto& trait = decl->as<parser::TraitDecl>();
                        if (trait_decls_.find(trait.name) == trait_decls_.end()) {
                            trait_decls_[trait.name] = &trait;
                        }
                    }
                }
            }
        }

        // Restore loop metadata from library functions (needed for !N references in cached IR)
        if (!state.loop_metadata.empty()) {
            loop_metadata_ = state.loop_metadata;
            loop_metadata_counter_ = state.loop_metadata_counter;
        }

        TML_DEBUG_LN("[CODEGEN] Restored library state: "
                     << state.struct_types.size() << " struct types, " << state.functions.size()
                     << " functions, " << state.enum_variants.size() << " enum variants");
    } else {
        // SLOW PATH: Generate library IR from scratch
        emit_module_pure_tml_functions();

        imported_func_code = output_.str();
        output_.str("");

        imported_type_defs = type_defs_buffer_.str();

        // Save for capture_library_state() (used when library_ir_only=true)
        cached_imported_func_code_ = imported_func_code;
        cached_imported_type_defs_ = imported_type_defs;
    }

    // Now reassemble with types before functions
    output_ << headers;

    if (!imported_type_defs.empty()) {
        emit_line("; Generic types from imported modules");
        output_ << imported_type_defs;
    }
    type_defs_buffer_.str(""); // Clear for main module processing

    // Emit imported module functions AFTER their type dependencies
    // Scan for runtime refs since this bypasses emit_line()
    scan_for_runtime_refs(imported_func_code);
    output_ << imported_func_code;

    // In library_ir_only mode, we only want the library IR (headers + types + library functions).
    // Skip all user code generation. This is used to produce a shared library object that
    // can be linked into multiple test files.
    if (options_.library_ir_only) {
        // Save the output position before generating instantiations.
        // We need to capture the instantiation code for cached_imported_func_code_
        // so that workers using library_decls_only=false get the complete library IR.
        std::string pre_instantiation_output = output_.str();

        // Flush ALL pending lazy library methods/functions so their `define` blocks
        // appear in the library IR. Without this, capture_library_state() cannot
        // extract `declare` stubs for worker threads (library_decls_only mode).
        // In library_ir_only mode there is no user code to scan for references,
        // so we emit everything unconditionally.
        if (options_.lazy_library_defs) {
            auto saved_module_prefix = current_module_prefix_;
            auto saved_submodule = current_submodule_name_;

            for (auto& [fn, info] : pending_library_methods_) {
                if (generated_functions_.count(fn))
                    continue;
                current_module_prefix_ = info.module_prefix;
                current_submodule_name_ = info.submodule_name;
                options_.lazy_library_defs = false;
                generated_functions_.erase(fn);
                gen_impl_method(info.type_name, *info.method);
                options_.lazy_library_defs = true;
            }

            for (auto& [fn, finfo] : pending_library_funcs_) {
                if (generated_functions_.count(fn))
                    continue;
                current_module_prefix_ = finfo.module_prefix;
                current_submodule_name_ = finfo.submodule_name;
                options_.lazy_library_defs = false;
                generated_functions_.erase(fn);
                gen_func_decl(*finfo.func);
                options_.lazy_library_defs = true;
            }

            current_module_prefix_ = saved_module_prefix;
            current_submodule_name_ = saved_submodule;
        }

        // Generate pending generic instantiations triggered by library functions.
        // Set in_library_body_ to disable Phase 4b Str temp tracking — library
        // generic instantiations (List[Str], HashMap[Str,X], etc.) manage their own
        // allocations and must not have temps auto-freed.
        {
            auto saved_lib = in_library_body_;
            in_library_body_ = true;
            generate_pending_instantiations();
            in_library_body_ = saved_lib;
        }

        // Update cached_imported_func_code_ to include instantiation-generated code.
        // Without this, workers using library_decls_only=false would miss instantiations
        // that were only generated by generate_pending_instantiations().
        {
            std::string post_output = output_.str();
            // The new code is everything after the pre-instantiation position
            if (post_output.size() > pre_instantiation_output.size()) {
                cached_imported_func_code_ += post_output.substr(pre_instantiation_output.size());
            }
            // Also update type defs (instantiations may generate new struct types)
            std::string new_type_defs = type_defs_buffer_.str();
            if (!new_type_defs.empty()) {
                cached_imported_type_defs_ += new_type_defs;
            }
        }

        // Emit string constants collected during library codegen
        emit_string_constants();

        // Emit attributes section (needed for function definitions)
        emit_line("");
        emit_line("attributes #0 = { nounwind }");

        // Emit loop metadata (generic instantiations may contain loops)
        emit_loop_metadata();

        // Emit module identification metadata
        {
            int ident_id = fresh_debug_id();
            emit_line("");
            emit_line("!llvm.ident = !{!" + std::to_string(ident_id) + "}");
            emit_line("!" + std::to_string(ident_id) + " = !{!\"tml version " +
                      std::string(tml::VERSION) + "\"}");
        }

        // Final sweep: scan the complete library IR for runtime function references
        scan_for_runtime_refs(output_.str());

        // Finalize runtime declarations and splice into output
        finalize_runtime_decls();
        std::string result = output_.str();
        {
            const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
            auto pos = result.find(placeholder);
            if (pos != std::string::npos) {
                result.replace(pos, placeholder.size(), deferred_runtime_decls_);
            }
        }

        // Update cached_preamble_headers_ with spliced declarations
        // so capture_library_state() gets the finalized preamble
        {
            const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
            auto pos = cached_preamble_headers_.find(placeholder);
            if (pos != std::string::npos) {
                cached_preamble_headers_.replace(pos, placeholder.size(), deferred_runtime_decls_);
            }
        }

        if (!errors_.empty()) {
            return errors_;
        }
        return result;
    }

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
            trait_decls_[trait_decl.name] = &trait_decl;
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

    // Buffer function code separately so we can emit type instantiations before functions
    std::stringstream func_output;
    std::stringstream saved_output;
    saved_output.str(output_.str()); // Save current output (headers, type defs, dyn types)
    output_.str("");                 // Clear for function code

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

    // Second pass: generate function declarations (into temp buffer)
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Generate impl methods as named functions inline
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) { // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                // Skip builtin types that have hard-coded implementations in method.cpp
                // File/Path now use normal dispatch via @extern FFI
                // Skip generic impl blocks - they will be instantiated when methods are called
                // (e.g., impl[T] Container[T] { ... } or impl Wrapper[T] { ... } is not generated
                // directly) Check both impl-level generics AND self_type generics
                bool has_impl_generics = !impl.generics.empty();
                bool has_type_generics = false;
                if (impl.self_type->kind.index() == 0) { // NamedType
                    const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                    if (named.generics.has_value() && !named.generics->args.empty()) {
                        has_type_generics = true;
                    }
                }
                // Also check if any methods have their own generic parameters
                bool has_method_generics = false;
                for (const auto& m : impl.methods) {
                    if (!m.generics.empty()) {
                        has_method_generics = true;
                        break;
                    }
                }
                if (has_impl_generics || has_type_generics) {
                    // Store the generic impl block for later instantiation
                    pending_generic_impls_[type_name] = &impl;
                    continue;
                }
                // For impls with generic methods, store for instantiation but continue
                // to generate non-generic methods
                if (has_method_generics) {
                    pending_generic_impls_[type_name] = &impl;
                }
                // Populate associated types from impl type_bindings
                current_associated_types_.clear();
                for (const auto& binding : impl.type_bindings) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*binding.type, {});
                    current_associated_types_[binding.name] = resolved;
                    // Also register in persistent per-type registry for cross-impl lookups
                    type_associated_types_[type_name + "::" + binding.name] = resolved;
                }
                // In suite mode, add prefix to avoid symbol collisions when linking multiple test
                // files Only for test-local types (not library types)
                std::string suite_prefix = "";
                if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
                    current_module_prefix_.empty()) {
                    suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
                }

                // Extract behavior type parameters for function name mangling
                // Only for PRIMITIVE types that have multiple TryFrom/From overloads
                // For impl TryFrom[I64] for I32, we extract "I64" to create I32_try_from_I64
                // Custom types like Celsius don't get the suffix
                auto is_primitive = [](const std::string& name) {
                    return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                           name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                           name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                           name == "Bool";
                };
                std::string behavior_type_suffix = "";
                if (is_primitive(type_name) && impl.trait_type &&
                    impl.trait_type->is<parser::NamedType>()) {
                    const auto& trait_named = impl.trait_type->as<parser::NamedType>();
                    if (trait_named.generics.has_value() && !trait_named.generics->args.empty()) {
                        for (const auto& arg : trait_named.generics->args) {
                            if (arg.is_type()) {
                                std::string arg_type_str = parser_type_to_string(*arg.as_type());
                                if (!arg_type_str.empty()) {
                                    behavior_type_suffix += "_" + arg_type_str;
                                }
                            }
                        }
                    }
                }

                for (const auto& method : impl.methods) {
                    // Skip methods with their own generic parameters
                    // These will be instantiated on-demand when called with concrete types
                    if (!method.generics.empty()) {
                        continue;
                    }

                    // Generate method with mangled name TypeName_MethodName_BehaviorTypeParams
                    // For impl TryFrom[I64] for I32, try_from becomes I32_try_from_I64
                    std::string method_name =
                        suite_prefix + type_name + "_" + method.name + behavior_type_suffix;
                    current_func_ = method_name;
                    current_impl_type_ = type_name; // Track impl self type for 'this' access
                    locals_.clear();
                    block_terminated_ = false;

                    // Determine return type
                    std::string ret_type = "void";
                    if (method.return_type.has_value()) {
                        ret_type = llvm_type_ptr(*method.return_type);
                    }
                    current_ret_type_ = ret_type;

                    // Build parameter list (including 'this')
                    std::string params;
                    std::string param_types;
                    std::vector<std::string> param_types_vec;

                    // Determine the LLVM type for 'this' based on the impl type
                    // For primitive types, pass by value; for structs/enums, pass by pointer
                    // For 'mut this' on primitives, pass by pointer so mutations propagate
                    std::string impl_llvm_type = llvm_type_name(type_name);
                    bool is_primitive_impl = (impl_llvm_type[0] != '%');

                    for (size_t i = 0; i < method.params.size(); ++i) {
                        if (i > 0) {
                            params += ", ";
                            param_types += ", ";
                        }
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        bool param_is_mut = false;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                            param_is_mut =
                                method.params[i].pattern->as<parser::IdentPattern>().is_mut;
                        } else {
                            param_name = "_anon";
                        }
                        // Handle 'this'/'self' parameter:
                        // - For 'mut this' on primitives: pass by pointer (ptr) so mutations
                        // propagate
                        // - For immutable 'this' on primitives: pass by value
                        // - For structs/enums: always pass by pointer
                        if ((param_name == "this" || param_name == "self") &&
                            param_type.find("This") != std::string::npos) {
                            if (is_primitive_impl && !param_is_mut) {
                                param_type = impl_llvm_type;
                            } else {
                                param_type = "ptr";
                            }
                        }
                        params += param_type + " %" + param_name;
                        param_types += param_type;
                        param_types_vec.push_back(param_type);
                    }

                    // Register function
                    std::string func_type = ret_type + " (" + param_types + ")";
                    functions_[method_name] =
                        FuncInfo{"@tml_" + method_name, func_type, ret_type, param_types_vec};

                    // Generate function
                    emit_line("");
                    emit_line("define internal " + ret_type + " @tml_" + method_name + "(" +
                              params + ") #0 {");
                    emit_line("entry:");

                    // Register params in locals
                    // Track whether this method has 'mut this' for body generation
                    bool method_has_mut_this = false;
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        bool param_is_mut = false;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                            param_is_mut =
                                method.params[i].pattern->as<parser::IdentPattern>().is_mut;
                        } else {
                            param_name = "_anon";
                        }
                        // Handle 'this'/'self' parameter:
                        // - For 'mut this' on primitives: ptr (so mutations propagate)
                        // - For immutable 'this' on primitives: pass by value
                        // - For structs/enums: always ptr
                        if ((param_name == "this" || param_name == "self") &&
                            param_type.find("This") != std::string::npos) {
                            if (is_primitive_impl && !param_is_mut) {
                                param_type = impl_llvm_type;
                            } else {
                                param_type = "ptr";
                            }
                            if (param_is_mut && is_primitive_impl) {
                                method_has_mut_this = true;
                            }
                        }

                        // 'this'/'self' is passed directly (by value for primitives, by pointer for
                        // structs) Don't create alloca for it
                        if (param_name == "this" || param_name == "self") {
                            // Create semantic type as the concrete impl type for field access
                            types::TypePtr semantic_type = std::make_shared<types::Type>();
                            semantic_type->kind = types::NamedType{type_name, "", {}};

                            if (method_has_mut_this) {
                                // For 'mut this' on primitives, the parameter is a ptr.
                                // Register with the inner primitive type and is_ptr_to_value=true
                                // so gen_ident will load the value from the pointer.
                                locals_["this"] = VarInfo{"%" + param_name, impl_llvm_type,
                                                          semantic_type, std::nullopt, true};
                                locals_["self"] = VarInfo{"%" + param_name, impl_llvm_type,
                                                          semantic_type, std::nullopt, true};
                            } else {
                                // Register the parameter under both 'this' and 'self' for
                                // flexibility
                                locals_["this"] = VarInfo{"%" + param_name, param_type,
                                                          semantic_type, std::nullopt};
                                locals_["self"] = VarInfo{"%" + param_name, param_type,
                                                          semantic_type, std::nullopt};
                            }
                        } else {
                            std::string alloca_reg = fresh_reg();
                            emit_line("  " + alloca_reg + " = alloca " + param_type);
                            emit_line("  store " + param_type + " %" + param_name + ", ptr " +
                                      alloca_reg);
                            locals_[param_name] =
                                VarInfo{alloca_reg, param_type, nullptr, std::nullopt};
                        }
                    }

                    // Generate body
                    if (method.body.has_value()) {
                        std::string result = gen_block(*method.body);
                        if (!block_terminated_) {
                            if (ret_type == "void") {
                                emit_line("  ret void");
                            } else if (ret_type == "{}") {
                                // Unit type always uses zeroinitializer
                                emit_line("  ret {} zeroinitializer");
                            } else if (ret_type == "ptr") {
                                // Use null only if result is "0" (placeholder)
                                emit_line("  ret ptr " + (result == "0" ? "null" : result));
                            } else if (result == "0" && ret_type.find("%struct.") == 0) {
                                // Struct type with "0" placeholder - use zeroinitializer
                                emit_line("  ret " + ret_type + " zeroinitializer");
                            } else {
                                // Use the actual result from gen_block
                                emit_line("  ret " + ret_type + " " + result);
                            }
                        }
                    } else {
                        if (ret_type == "void") {
                            emit_line("  ret void");
                        } else if (ret_type == "ptr") {
                            emit_line("  ret ptr null");
                        } else {
                            emit_line("  ret " + ret_type + " zeroinitializer");
                        }
                    }
                    emit_line("}");
                    current_impl_type_.clear(); // Clear impl type context
                }

                // Generate default implementations for missing methods
                std::string trait_name;
                if (impl.trait_type && impl.trait_type->is<parser::NamedType>()) {
                    const auto& named = impl.trait_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        trait_name = named.path.segments.back();
                    }
                }
                if (!trait_name.empty()) {
                    auto trait_it = trait_decls_.find(trait_name);
                    // If not found in trait_decls_, load the behavior's source
                    // file from disk and parse it to get the TraitDecl AST.
                    // This handles behaviors like Iterator that are defined
                    // in library modules not explicitly imported by user code.
                    if (trait_it == trait_decls_.end()) {
                        // Map behavior names to their module source paths
                        static const std::unordered_map<std::string, std::string> behavior_source =
                            {
                                {"Iterator", "core/src/iter/traits/iterator"},
                                {"IntoIterator", "core/src/iter/traits/into_iterator"},
                                {"FromIterator", "core/src/iter/traits/from_iterator"},
                                {"Display", "core/src/fmt/traits"},
                                {"Debug", "core/src/fmt/traits"},
                                {"Duplicate", "core/src/clone"},
                                {"Hash", "core/src/hash"},
                                {"Default", "core/src/default"},
                                {"Error", "core/src/error"},
                                {"From", "core/src/convert"},
                                {"Into", "core/src/convert"},
                                {"TryFrom", "core/src/convert"},
                                {"TryInto", "core/src/convert"},
                                {"PartialEq", "core/src/cmp"},
                                {"Eq", "core/src/cmp"},
                                {"PartialOrd", "core/src/cmp"},
                                {"Ord", "core/src/cmp"},
                                {"Add", "core/src/ops/arith"},
                                {"Sub", "core/src/ops/arith"},
                                {"Mul", "core/src/ops/arith"},
                                {"Div", "core/src/ops/arith"},
                                {"Rem", "core/src/ops/arith"},
                                {"Neg", "core/src/ops/arith"},
                            };
                        // Build module path key for GlobalASTCache
                        auto src_it = behavior_source.find(trait_name);
                        if (src_it != behavior_source.end()) {
                            std::string cache_key = src_it->second;
                            // Replace / with :: for cache key
                            std::string mod_key = cache_key;
                            for (auto& ch : mod_key) {
                                if (ch == '/')
                                    ch = ':';
                            }
                            // Remove "src:" prefix segments
                            // e.g. "core:src:iter:traits:iterator" ->
                            // "core::iter::traits::iterator"
                            std::string clean_key;
                            std::istringstream kss(mod_key);
                            std::string seg;
                            while (std::getline(kss, seg, ':')) {
                                if (seg.empty() || seg == "src")
                                    continue;
                                if (!clean_key.empty())
                                    clean_key += "::";
                                clean_key += seg;
                            }

                            // Check GlobalASTCache first
                            const parser::Module* mod_ast =
                                GlobalASTCache::instance().get(clean_key);
                            if (!mod_ast) {
                                // Find lib root and parse source file
                                namespace fs = std::filesystem;
                                auto cwd = fs::current_path();
                                std::vector<fs::path> candidates = {
                                    cwd / "lib",
                                    fs::path("lib"),
                                    fs::path("F:/Node/hivellm/tml/lib"),
                                    cwd.parent_path() / "lib",
                                    cwd.parent_path().parent_path() / "lib",
                                };
                                for (const auto& lib_root : candidates) {
                                    fs::path src_path = lib_root / (src_it->second + ".tml");
                                    if (fs::exists(src_path)) {
                                        auto source_result =
                                            lexer::Source::from_file(src_path.string());
                                        if (is_err(source_result))
                                            break;
                                        auto source =
                                            std::move(std::get<lexer::Source>(source_result));
                                        lexer::Lexer lex(source);
                                        auto tokens = lex.tokenize();
                                        if (lex.has_errors())
                                            break;
                                        auto stem = src_path.stem().string();
                                        parser::Parser p(std::move(tokens));
                                        auto result = p.parse_module(stem);
                                        if (std::holds_alternative<parser::Module>(result)) {
                                            GlobalASTCache::instance().put(
                                                clean_key,
                                                std::get<parser::Module>(std::move(result)));
                                            mod_ast = GlobalASTCache::instance().get(clean_key);
                                        }
                                        break;
                                    }
                                }
                            }
                            if (mod_ast) {
                                for (const auto& d : mod_ast->decls) {
                                    if (d->is<parser::TraitDecl>()) {
                                        const auto& t = d->as<parser::TraitDecl>();
                                        if (t.name == trait_name) {
                                            trait_decls_[t.name] = &t;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        trait_it = trait_decls_.find(trait_name);
                    }
                    if (trait_it != trait_decls_.end()) {
                        const auto* trait_decl = trait_it->second;

                        // Collect method names that impl provides
                        std::set<std::string> impl_method_names;
                        for (const auto& m : impl.methods) {
                            impl_method_names.insert(m.name);
                        }

                        // Generate default implementations for missing methods
                        for (const auto& trait_method : trait_decl->methods) {
                            // Skip if impl provides this method
                            if (impl_method_names.count(trait_method.name) > 0)
                                continue;
                            generate_default_method(type_name, trait_decl, trait_method, &impl);
                        }
                    }
                }
            }
        }
    }

    // Save function code (non-generic functions)
    func_output.str(output_.str());
    output_.str("");
    // Generate pending generic instantiations (types go to type_defs_buffer_, funcs go to output_)
    {
        auto saved_lib = in_library_body_;
        in_library_body_ = true;
        generate_pending_instantiations();
        in_library_body_ = saved_lib;
    }

    // Save generic function code
    std::stringstream generic_func_output;
    generic_func_output.str(output_.str());
    output_.str("");

    // Now reassemble in correct order: headers + types + generic funcs + non-generic funcs
    // 1. Headers
    output_ << saved_output.str();

    // 2. Type definitions (from type_defs_buffer_) - MUST come before functions
    std::string type_defs = type_defs_buffer_.str();
    if (!type_defs.empty()) {
        emit_line("; Generic type instantiations");
        output_ << type_defs;
    }
    emit_line("");

    // 3. Generic functions (instantiated class constructors/methods) - MUST come before
    //    non-generic functions that call them, to ensure correct forward reference handling
    output_ << generic_func_output.str();

    // 4. Non-generic functions (including test functions that call generic class methods)
    output_ << func_output.str();

    // Emit generated closure functions
    for (const auto& closure_func : module_functions_) {
        scan_for_runtime_refs(closure_func);
        emit(closure_func);
    }

    // Emit vtables for trait objects (dyn dispatch)
    // Note: generate_default_method() called during emit_vtables() may generate new
    // generic type instantiations (e.g. Outcome__Unit__I64). These go to type_defs_buffer_.
    // We need to capture and prepend any new type defs before the functions.
    type_defs_buffer_.str(""); // Clear before vtable generation
    emit_vtables();
    {
        std::string vtable_type_defs = type_defs_buffer_.str();
        if (!vtable_type_defs.empty()) {
            // Prepend type defs to the output - they must appear before functions
            std::string current_output = output_.str();
            output_.str("");
            // Find the position after the "; Generic type instantiations" header
            // by looking for the first "define" or "@vtable" line
            auto define_pos = current_output.find("\ndefine ");
            if (define_pos == std::string::npos)
                define_pos = current_output.find("\n@vtable.");
            if (define_pos != std::string::npos) {
                output_ << current_output.substr(0, define_pos + 1);
                output_ << "; Additional generic type instantiations (from vtable generation)\n";
                output_ << vtable_type_defs;
                output_ << current_output.substr(define_pos + 1);
            } else {
                output_ << vtable_type_defs;
                output_ << current_output;
            }
            type_defs_buffer_.str("");
        }
    }

    // Emit definitions for library functions that were actually referenced
    // by user code, generic instantiations, or other library functions.
    // This replaces the `declare` stubs emitted during module scanning.
    if (options_.lazy_library_defs && !options_.library_ir_only && !options_.library_decls_only) {
        emit_referenced_library_definitions();
    }

    // In library_decls_only + lazy mode: emit `declare` for referenced functions.
    // Without this, lazy mode stores functions as pending but never emits them.
    if (options_.lazy_library_defs && options_.library_decls_only) {
        emit_referenced_library_declarations();
    }

    // Collect test, benchmark, and fuzz functions BEFORE emitting string constants
    // so we can pre-register expected panic message strings
    struct TestInfo {
        std::string name;
        bool should_panic = false;
        std::string expected_panic_message;     // Empty means any panic is fine
        std::string expected_panic_message_str; // LLVM string constant reference
    };
    std::vector<TestInfo> test_functions;
    std::vector<std::string> fuzz_functions;
    struct BenchInfo {
        std::string name;
        int64_t iterations = 1000; // Default iterations
    };
    std::vector<BenchInfo> bench_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            bool is_test = false;
            bool should_panic = false;
            std::string expected_panic_message;

            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    is_test = true;
                } else if (decorator.name == "should_panic") {
                    should_panic = true;
                    // Check for expected message: @should_panic(expected = "message")
                    for (const auto& arg : decorator.args) {
                        if (arg->is<parser::BinaryExpr>()) {
                            // Handle named argument: expected = "message"
                            const auto& bin = arg->as<parser::BinaryExpr>();
                            if (bin.op == parser::BinaryOp::Assign &&
                                bin.left->is<parser::IdentExpr>() &&
                                bin.right->is<parser::LiteralExpr>()) {
                                const auto& ident = bin.left->as<parser::IdentExpr>();
                                const auto& lit = bin.right->as<parser::LiteralExpr>();
                                if (ident.name == "expected" &&
                                    lit.token.kind == lexer::TokenKind::StringLiteral) {
                                    expected_panic_message = lit.token.string_value().value;
                                }
                            }
                        } else if (arg->is<parser::LiteralExpr>()) {
                            // Also support @should_panic("message") without named argument
                            const auto& lit = arg->as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                                expected_panic_message = lit.token.string_value().value;
                            }
                        }
                    }
                } else if (decorator.name == "bench") {
                    BenchInfo info;
                    info.name = func.name;
                    // Check for iterations argument: @bench(1000) or @bench(iterations=1000)
                    if (!decorator.args.empty()) {
                        const auto& arg = *decorator.args[0];
                        if (arg.is<parser::LiteralExpr>()) {
                            const auto& lit = arg.as<parser::LiteralExpr>();
                            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                                info.iterations = static_cast<int64_t>(lit.token.int_value().value);
                            }
                        }
                    }
                    bench_functions.push_back(info);
                } else if (decorator.name == "fuzz") {
                    fuzz_functions.push_back(func.name);
                }
            }

            if (is_test) {
                TestInfo info;
                info.name = func.name;
                info.should_panic = should_panic;
                info.expected_panic_message = expected_panic_message;
                // Pre-register the expected message string BEFORE emit_string_constants
                if (!expected_panic_message.empty()) {
                    info.expected_panic_message_str = add_string_literal(expected_panic_message);
                }
                test_functions.push_back(info);
            }
        }
    }

    // Pre-register coverage output file string if needed (before emitting string constants)
    std::string coverage_output_str;
    if (options_.coverage_enabled && !options_.coverage_output_file.empty()) {
        coverage_output_str = add_string_literal(options_.coverage_output_file);
    }

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Generate main entry point
    bool has_user_main = false;
    bool main_returns_void = true;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            const auto& func = decl->as<parser::FuncDecl>();
            main_returns_void = !func.return_type.has_value();
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main with proper output
        // Note: time functions are always declared in preamble
        emit_line("; Auto-generated benchmark runner");
        emit_line("");

        // Add format strings for benchmark output
        // String lengths: \0A = 1 byte, \00 = 1 byte (null terminator)
        emit_line(
            "@.bench.header = private constant [23 x i8] c\"\\0A  Running benchmarks\\0A\\00\"");
        emit_line("@.bench.name = private constant [16 x i8] c\"  + bench %-20s\\00\"");
        emit_line("@.bench.time = private constant [19 x i8] c\" ... %lld ns/iter\\0A\\00\"");
        emit_line("@.bench.summary = private constant [30 x i8] c\"\\0A  %d benchmark(s) "
                  "completed\\0A\\00\"");

        // Add string constants for benchmark names
        int idx = 0;
        for (const auto& bench_info : bench_functions) {
            std::string name_const = "@.bench.fn." + std::to_string(idx);
            size_t name_len = bench_info.name.size() + 1;
            emit_line(name_const + " = private constant [" + std::to_string(name_len) +
                      " x i8] c\"" + bench_info.name + "\\00\"");
            idx++;
        }
        emit_line("");

        emit_line("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
        emit_line("entry:");

        // Print benchmark header
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.header)");
        emit_line("");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_info : bench_functions) {
            std::string bench_fn = "@tml_" + bench_info.name;
            std::string n = std::to_string(bench_num);
            std::string name_const = "@.bench.fn." + n;
            std::string iterations_str = std::to_string(bench_info.iterations);

            // Print benchmark name
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.name, ptr " + name_const + ")");

            // Warmup: Run 10 iterations to warm up caches
            std::string warmup_var = "%warmup_" + n;
            std::string warmup_header = "warmup_header_" + n;
            std::string warmup_body = "warmup_body_" + n;
            std::string warmup_end = "warmup_end_" + n;

            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_header + ":");
            emit_line("  " + warmup_var + " = phi i64 [ 0, %" + prev_block + " ], [ " + warmup_var +
                      "_next, %" + warmup_body + " ]");
            emit_line("  %warmup_cmp_" + n + " = icmp slt i64 " + warmup_var + ", 10");
            emit_line("  br i1 %warmup_cmp_" + n + ", label %" + warmup_body + ", label %" +
                      warmup_end);
            emit_line("");
            emit_line(warmup_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + warmup_var + "_next = add i64 " + warmup_var + ", 1");
            emit_line("  br label %" + warmup_header);
            emit_line("");
            emit_line(warmup_end + ":");

            // Get start time (nanoseconds for precision)
            std::string start_time = "%bench_start_" + n;
            emit_line("  " + start_time + " = call i64 @time_ns()");

            // Run benchmark with configured iterations (default 1000)
            std::string iter_var = "%bench_iter_" + n;
            std::string loop_header = "bench_loop_header_" + n;
            std::string loop_body = "bench_loop_body_" + n;
            std::string loop_end = "bench_loop_end_" + n;

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i64 [ 0, %" + warmup_end + " ], [ " + iter_var +
                      "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + n;
            emit_line("  " + cmp_var + " = icmp slt i64 " + iter_var + ", " + iterations_str);
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i64 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + n;
            std::string duration = "%bench_duration_" + n;
            emit_line("  " + end_time + " = call i64 @time_ns()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / iterations)
            std::string avg_time = "%bench_avg_" + n;
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", " + iterations_str);

            // Print benchmark time
            emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.time, i64 " + avg_time + ")");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        // Print summary
        emit_line("  call i32 (ptr, ...) @printf(ptr @.bench.summary, i32 " +
                  std::to_string(bench_num) + ")");
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (options_.generate_fuzz_entry && !fuzz_functions.empty()) {
        // Generate fuzz target entry point for fuzzing
        // The fuzz target receives (ptr data, i64 len) and calls @fuzz functions
        emit_line("; Auto-generated fuzz target entry point");
        emit_line("");

#ifdef _WIN32
        emit_line("define dllexport i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#else
        emit_line("define i32 @tml_fuzz_target(ptr %data, i64 %len) {");
#endif
        emit_line("entry:");

        // Call each @fuzz function with the input data
        // Fuzz functions should have signature: func fuzz_name(data: Ptr[U8], len: U64)
        for (const auto& fuzz_name : fuzz_functions) {
            std::string fuzz_fn = "@tml_" + fuzz_name;
            // Look up the function's return type from functions_ map
            auto it = functions_.find(fuzz_name);
            if (it != functions_.end()) {
                // Check if function takes (ptr, i64) parameters
                if (it->second.param_types.size() >= 2) {
                    emit_line("  call void " + fuzz_fn + "(ptr %data, i64 %len)");
                } else {
                    // Function doesn't take data parameters, just call it
                    emit_line("  call void " + fuzz_fn + "()");
                }
            } else {
                // Fallback - assume void function
                emit_line("  call void " + fuzz_fn + "()");
            }
        }

        // Return 0 for success (crash will never reach here)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main (or DLL entry point)
        // @test functions can return I32 (0 for success) or Unit
        // Assertions inside will call panic() on failure which doesn't return
        emit_line("; Auto-generated test runner");

        // Check if any tests need @should_panic support
        bool has_should_panic = false;
        for (const auto& test_info : test_functions) {
            if (test_info.should_panic) {
                has_should_panic = true;
                break;
            }
        }

        // Add error message strings for should_panic tests
        if (has_should_panic) {
            emit_line("");
            emit_line("; Error messages for @should_panic tests");
            // "test did not panic as expected\n\0" = 30 + 1 + 1 = 32 bytes
            emit_line("@.should_panic_no_panic = private constant [32 x i8] c\"test did not "
                      "panic as expected\\0A\\00\"");
            // "panic message did not contain expected string\n\0" = 45 + 1 + 1 = 47 bytes
            emit_line("@.should_panic_wrong_msg = private constant [47 x i8] c\"panic message "
                      "did not contain expected string\\0A\\00\"");
            emit_line("");
        }

        // String constant for coverage file environment variable name
        emit_line("; Environment variable name for coverage file (EXE mode)");
        emit_line("@.tml_cov_file_env = private constant [18 x i8] c\"TML_COVERAGE_FILE\\00\"");

        // For DLL entry, generate exported test entry function instead of main
        if (options_.generate_dll_entry) {
            // Determine entry function name (tml_test_entry or tml_test_N for suites)
            std::string entry_name = "tml_test_entry";
            if (options_.suite_test_index >= 0) {
                entry_name = "tml_test_" + std::to_string(options_.suite_test_index);
            }
#ifdef _WIN32
            emit_line("define dllexport i32 @" + entry_name + "() {");
#else
            emit_line("define i32 @" + entry_name + "() {");
#endif
        } else {
            emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        }
        emit_line("entry:");

        // In suite mode, test functions have a prefix to avoid collisions
        std::string test_suite_prefix = "";
        if (options_.suite_test_index >= 0 && options_.force_internal_linkage) {
            test_suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
        }

        int test_idx = 0;
        std::string prev_block = "entry";
        for (const auto& test_info : test_functions) {
            std::string test_fn = "@tml_" + test_suite_prefix + test_info.name;
            std::string idx_str = std::to_string(test_idx);

            if (test_info.should_panic) {
                // Generate panic-catching call for @should_panic tests
                // Uses callback approach: pass function pointer to tml_run_should_panic()
                // which keeps setjmp on the stack while the test runs

                // Call tml_run_should_panic with function pointer
                // Returns: 1 if panicked (success), 0 if didn't panic (failure)
                std::string result = "%panic_result_" + idx_str;
                emit_line("  " + result + " = call i32 @tml_run_should_panic(ptr " + test_fn + ")");

                // Check if test panicked
                std::string cmp = "%panic_cmp_" + idx_str;
                emit_line("  " + cmp + " = icmp eq i32 " + result + ", 0");

                std::string no_panic_label = "no_panic_" + idx_str;
                std::string panic_ok_label = "panic_ok_" + idx_str;
                std::string test_done_label = "test_done_" + idx_str;

                emit_line("  br i1 " + cmp + ", label %" + no_panic_label + ", label %" +
                          panic_ok_label);
                emit_line("");

                // Test didn't panic - that's an error for @should_panic
                emit_line(no_panic_label + ":");
                emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_no_panic)");
                emit_line("  call void @exit(i32 1)");
                emit_line("  unreachable");
                emit_line("");

                // Test panicked - check message if expected
                emit_line(panic_ok_label + ":");
                if (!test_info.expected_panic_message_str.empty()) {
                    // Check if panic message contains expected string
                    std::string msg_check = "%msg_check_" + idx_str;
                    emit_line("  " + msg_check + " = call i32 @tml_panic_message_contains(ptr " +
                              test_info.expected_panic_message_str + ")");

                    std::string msg_ok_label = "msg_ok_" + idx_str;
                    std::string msg_fail_label = "msg_fail_" + idx_str;
                    std::string msg_cmp = "%msg_cmp_" + idx_str;
                    emit_line("  " + msg_cmp + " = icmp ne i32 " + msg_check + ", 0");
                    emit_line("  br i1 " + msg_cmp + ", label %" + msg_ok_label + ", label %" +
                              msg_fail_label);
                    emit_line("");

                    // Message didn't match - fail
                    emit_line(msg_fail_label + ":");
                    emit_line("  call i32 (ptr, ...) @printf(ptr @.should_panic_wrong_msg)");
                    emit_line("  call void @exit(i32 1)");
                    emit_line("  unreachable");
                    emit_line("");

                    // Message matched - continue
                    emit_line(msg_ok_label + ":");
                    emit_line("  br label %" + test_done_label);
                } else {
                    // No expected message - any panic is fine
                    emit_line("  br label %" + test_done_label);
                }
                emit_line("");

                emit_line(test_done_label + ":");
                prev_block = test_done_label;
            } else {
                // Regular test - just call it
                auto it = functions_.find(test_info.name);
                if (it != functions_.end() && it->second.ret_type != "void") {
                    std::string tmp = "%test_result_" + idx_str;
                    emit_line("  " + tmp + " = call " + it->second.ret_type + " " + test_fn + "()");
                } else if (it != functions_.end()) {
                    emit_line("  call void " + test_fn + "()");
                } else {
                    // Test function not found in functions_ map - likely a name collision
                    // with an imported module function (e.g., test function "test_assert_str_empty"
                    // collides with module "test" function "assert_str_empty" -> both mangle to
                    // "tml_test_assert_str_empty"). Emit as i32 call (test convention) with a
                    // stderr warning.
                    emit_line("  ; WARNING: test function '" + test_info.name +
                              "' not found in functions_ map");
                    emit_line(
                        "  ; This may indicate a name collision with an imported module function.");
                    emit_line("  ; Consider renaming the test function to avoid the collision.");
                    std::string tmp = "%test_result_" + idx_str;
                    emit_line("  " + tmp + " = call i32 " + test_fn + "()");
                }
            }

            test_idx++;
        }

        // Print coverage report if enabled
        // In suite mode (coverage_quiet=true), the test runner handles printing
        // after all tests complete, so we don't print here
        emit_coverage_report_calls(coverage_output_str, true);

        // Write coverage data to file for EXE mode subprocess communication
        // When running under EXE mode, write covered functions to file specified by env var
        emit_line("  %cov_file_env = call ptr @getenv(ptr @.tml_cov_file_env)");
        emit_line("  %cov_file_not_null = icmp ne ptr %cov_file_env, null");
        emit_line("  br i1 %cov_file_not_null, label %write_cov_file, label %cov_file_done");
        emit_line("");
        emit_line("write_cov_file:");
        emit_line("  call void @tml_coverage_write_file(ptr %cov_file_env)");
        emit_line("  br label %cov_file_done");
        emit_line("");
        emit_line("cov_file_done:");

        // All tests passed (if we got here, no assertion failed)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");

        // In suite mode, tml_main has a prefix to avoid collisions
        std::string main_suite_prefix = "";
        if (options_.suite_test_index >= 0 && options_.force_internal_linkage) {
            main_suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
        }
        std::string tml_main_fn = "tml_" + main_suite_prefix + "main";

        // For DLL entry, generate exported test entry function instead of main
        if (options_.generate_dll_entry) {
            // Determine entry function name (tml_test_entry or tml_test_N for suites)
            std::string entry_name = "tml_test_entry";
            if (options_.suite_test_index >= 0) {
                entry_name = "tml_test_" + std::to_string(options_.suite_test_index);
            }
#ifdef _WIN32
            emit_line("define dllexport i32 @" + entry_name + "() {");
#else
            emit_line("define i32 @" + entry_name + "() {");
#endif
            emit_line("entry:");
            if (main_returns_void) {
                emit_line("  call void @" + tml_main_fn + "()");
            } else {
                emit_line("  %ret = call i32 @" + tml_main_fn + "()");
            }
            // Print coverage report if enabled
            // In suite mode (coverage_quiet=true), the test runner handles printing
            // after all tests complete, so we don't print here
            emit_coverage_report_calls(coverage_output_str, true);
            emit_line("  ret i32 " + std::string(main_returns_void ? "0" : "%ret"));
            emit_line("}");
        } else {
            emit_line("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
            emit_line("entry:");
            // Enable backtrace on panic if flag is set
            if (CompilerOptions::backtrace) {
                emit_line("  call void @tml_enable_backtrace_on_panic()");
            }
            if (main_returns_void) {
                emit_line("  call void @" + tml_main_fn + "()");
            } else {
                emit_line("  %ret = call i32 @" + tml_main_fn + "()");
            }
            // Print coverage report if enabled
            emit_coverage_report_calls(coverage_output_str, false);
            emit_line("  ret i32 " + std::string(main_returns_void ? "0" : "%ret"));
            emit_line("}");
        }
    }

    // Emit function attributes for optimization
    // When coverage is enabled, add noinline to prevent LLVM from inlining library functions
    emit_line("");
    emit_line("; Function attributes for optimization");
    if (options_.coverage_enabled) {
        emit_line("attributes #0 = { nounwind noinline }");
    } else {
        emit_line("attributes #0 = { nounwind }");
    }

    // Emit loop metadata at the end
    emit_loop_metadata();

    // Emit debug info metadata at the end
    emit_debug_info_footer();

    // Emit module identification metadata
    {
        int ident_id = fresh_debug_id();
        emit_line("");
        emit_line("!llvm.ident = !{!" + std::to_string(ident_id) + "}");
        emit_line("!" + std::to_string(ident_id) + " = !{!\"tml version " +
                  std::string(tml::VERSION) + "\"}");
    }

    // Final sweep: scan the complete IR output for any runtime function references
    // that were missed by emit_line() auto-detection. This catches references emitted
    // via emit() (which doesn't scan) — notably, void call instructions in call_user.cpp
    // use emit() for the function name part, bypassing emit_line()'s auto-detection.
    // Also catches references from generate_pending_instantiations() which generates
    // library method bodies (e.g., Text::print calling @print) outside the lazy path.
    scan_for_runtime_refs(output_.str());

    // Append any deferred enum drop functions generated during codegen
    if (!enum_drop_output_.str().empty()) {
        output_ << enum_drop_output_.str();
        // Scan enum drop functions for their own runtime references
        // (e.g., @tml_str_free called within drop functions)
        scan_for_runtime_refs(enum_drop_output_.str());
    }

    // Finalize runtime declarations and splice into output
    finalize_runtime_decls();
    std::string final_output = output_.str();
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = final_output.find(placeholder);
        if (pos != std::string::npos) {
            final_output.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    // Update cached_preamble_headers_ with spliced declarations
    // so capture_library_state() gets the finalized preamble
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = cached_preamble_headers_.find(placeholder);
        if (pos != std::string::npos) {
            cached_preamble_headers_.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    if (!errors_.empty()) {
        return errors_;
    }

    return final_output;
}

} // namespace tml::codegen
