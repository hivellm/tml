TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Function Bodies (Second Pass)
//!
//! Implements `generate_function_bodies()`, the main codegen loop that iterates
//! all module declarations and generates LLVM IR for function and impl-method bodies.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"

#include <filesystem>
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

void LLVMIRGen::generate_function_bodies(const parser::Module& module) {
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
            } else if (impl.self_type->is<parser::TupleType>()) {
                // Tuple impls: use arity-based name like "Tuple2", "Tuple3", etc.
                const auto& tuple = impl.self_type->as<parser::TupleType>();
                type_name = "Tuple" + std::to_string(tuple.elements.size());
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
                    pending_generic_impls_all_[type_name].push_back(&impl);
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
                                    behavior_type_suffix += "__" + arg_type_str;
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

                    // Generate method with mangled name using mangle_impl_method
                    // For behavior type suffixes (e.g., TryFrom[I64] for I32),
                    // the suffix is appended to the method name
                    std::string full_method_name = method.name + behavior_type_suffix;
                    std::string func_llvm_name = mangle_impl_method(type_name, full_method_name);

                    // For functions_ lookup key, use the flat name pattern
                    std::string method_key = suite_prefix + type_name + "_" + full_method_name;
                    current_func_ = method_key;
                    current_impl_type_ = type_name; // Track impl self type for 'this' access
                    locals_.clear();
                    block_terminated_ = false;
                    last_semantic_type_ = nullptr;

                    // Determine return type
                    std::string ret_type = "void";
                    if (method.return_type.has_value()) {
                        ret_type = llvm_type_ptr(*method.return_type);
                    }
                    current_ret_type_ = ret_type;
                    func_ret_type_ = ret_type;

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
                        if (!params.empty()) {
                            params += ", ";
                            param_types += ", ";
                        }
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        // Normalize void -> {} for Unit params (void invalid in LLVM data contexts)
                        if (param_type == "void")
                            param_type = "{}";
                        std::string param_name;
                        bool param_is_mut = false;
                        bool param_is_ref_sig = false;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                            param_is_mut =
                                method.params[i].pattern->as<parser::IdentPattern>().is_mut;
                        } else {
                            param_name = "_anon";
                        }
                        // Check if parameter has ref type (e.g. this: ref This)
                        // Only for non-mut — 'mut this' already uses ptr with is_ptr_to_value
                        if (!param_is_mut && method.params[i].type &&
                            method.params[i].type->is<parser::RefType>()) {
                            param_is_ref_sig = true;
                        }
                        // Handle 'this'/'self' parameter:
                        // - For 'mut this' on primitives: pass by pointer (ptr) so mutations
                        // propagate
                        // - For 'this: ref This' on primitives: pass by pointer (reference)
                        // - For immutable 'this' on primitives: pass by value
                        // - For structs/enums: always pass by pointer
                        // NOTE: Don't check param_type for "This" string — llvm_type_ptr
                        // resolves This to the concrete type (e.g. %struct.Counter), so the
                        // "This" literal is already gone by this point.
                        if (param_name == "this" || param_name == "self") {
                            if (impl_llvm_type == "void" || impl_llvm_type == "{}") {
                                // Unit type: skip this param entirely
                                continue;
                            } else if (is_primitive_impl && !param_is_mut && !param_is_ref_sig) {
                                param_type = impl_llvm_type;
                            } else {
                                param_type = "ptr";
                            }
                        } else if (i == 0 && (param_type.find("%struct.") == 0 ||
                                              param_type.find("%enum.") == 0)) {
                            // First non-this/self struct/enum param is passed as ptr
                            // (matches calling convention from method-syntax calls)
                            param_type = "ptr";
                        }
                        params += param_type + " %" + param_name;
                        param_types += param_type;
                        param_types_vec.push_back(param_type);
                    }

                    // Register function under multiple keys for lookup
                    std::string func_type = ret_type + " (" + param_types + ")";
                    FuncInfo finfo{"@" + func_llvm_name, func_type, ret_type, param_types_vec};
                    functions_[method_key] = finfo;

                    // Also register under the flat key without suite prefix
                    // This is needed because call sites may look up the method by
                    // its unprefixed flat name from library context
                    std::string unprefixed_key = type_name + "_" + full_method_name;
                    if (functions_.find(unprefixed_key) == functions_.end()) {
                        functions_[unprefixed_key] = finfo;
                    }

                    // Generate function
                    emit_line("");
                    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params +
                              ") #0 {");
                    TML_LOG_TRACE("codegen", "[INLINE_CODEGEN] " << func_llvm_name
                                                                 << " type_name=" << type_name);
                    emit_line("entry:");

                    // Register params in locals
                    // Track whether this method has 'mut this' or 'this: ref This' for body
                    // generation
                    bool method_has_mut_this = false;
                    bool method_has_ref_this = false;
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        // Normalize void -> {} for Unit params (void invalid in LLVM data contexts)
                        if (param_type == "void")
                            param_type = "{}";
                        std::string param_name;
                        bool param_is_mut = false;
                        bool param_is_ref = false;
                        if (method.params[i].pattern &&
                            method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                            param_is_mut =
                                method.params[i].pattern->as<parser::IdentPattern>().is_mut;
                        } else {
                            param_name = "_anon";
                        }
                        // Check if parameter has ref type (e.g. this: ref This)
                        // Only for non-mut — 'mut this' already uses ptr with is_ptr_to_value
                        if (!param_is_mut && method.params[i].type &&
                            method.params[i].type->is<parser::RefType>()) {
                            param_is_ref = true;
                        }
                        // Handle 'this'/'self' parameter:
                        // - For 'mut this' on primitives: ptr (so mutations propagate)
                        // - For 'this: ref This' on primitives: ptr (reference semantics)
                        // - For immutable 'this' on primitives: pass by value
                        // - For structs/enums: always ptr
                        // NOTE: Don't check param_type for "This" string — llvm_type_ptr
                        // resolves This to the concrete type before we get here.
                        if (param_name == "this" || param_name == "self") {
                            if (is_primitive_impl && !param_is_mut && !param_is_ref) {
                                param_type = impl_llvm_type;
                            } else {
                                param_type = "ptr";
                            }
                            if (param_is_mut && is_primitive_impl) {
                                method_has_mut_this = true;
                            }
                            if (param_is_ref && is_primitive_impl) {
                                method_has_ref_this = true;
                            }
                        }

                        // 'this'/'self' is passed directly (by value for primitives, by pointer for
                        // structs) Don't create alloca for it
                        if (param_name == "this" || param_name == "self") {
                            // Create semantic type as the concrete impl type for field access
                            types::TypePtr semantic_type = std::make_shared<types::Type>();
                            semantic_type->kind = types::NamedType{type_name, "", {}};

                            if (method_has_ref_this) {
                                // For 'this: ref This' on primitives, the parameter is a ptr
                                // (reference). Register as ptr type WITHOUT is_ptr_to_value,
                                // so gen_ident returns the pointer as-is. The deref operator
                                // (*this) will do the actual load.
                                locals_["this"] = VarInfo{"%" + param_name, "ptr", semantic_type,
                                                          std::nullopt, false};
                                locals_["self"] = VarInfo{"%" + param_name, "ptr", semantic_type,
                                                          std::nullopt, false};
                            } else if (method_has_mut_this) {
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
                        } else if (i == 0 && (param_name != "this" && param_name != "self") &&
                                   (param_type.find("%struct.") == 0 ||
                                    param_type.find("%enum.") == 0)) {
                            // First non-this/self struct/enum param is passed as ptr.
                            // Copy from ptr into local alloca so field access (GEP) works.
                            types::TypePtr semantic_type = std::make_shared<types::Type>();
                            semantic_type->kind = types::NamedType{type_name, "", {}};
                            std::string alloca_reg = fresh_reg();
                            std::string loaded_reg = fresh_reg();
                            emit_line("  " + alloca_reg + " = alloca " + param_type);
                            emit_line("  " + loaded_reg + " = load " + param_type + ", ptr %" +
                                      param_name);
                            emit_line("  store " + param_type + " " + loaded_reg + ", ptr " +
                                      alloca_reg);
                            locals_[param_name] =
                                VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
                        } else {
                            // Resolve semantic type from annotation so field access (GEP) and
                            // method dispatch work for ref/ptr parameters (e.g. other: ref TypeId)
                            types::TypePtr param_sem_type = nullptr;
                            if (method.params[i].type) {
                                param_sem_type = resolve_parser_type_with_subs(
                                    *method.params[i].type, current_type_subs_);
                            }
                            std::string alloca_reg = fresh_reg();
                            emit_line("  " + alloca_reg + " = alloca " + param_type);
                            emit_line("  store " + param_type + " %" + param_name + ", ptr " +
                                      alloca_reg);
                            locals_[param_name] =
                                VarInfo{alloca_reg, param_type, param_sem_type, std::nullopt};
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

                    // Mark as generated to prevent duplicate emission by impl.cpp
                    generated_impl_methods_output_.insert(func_llvm_name);
                    generated_functions_.insert("@" + func_llvm_name);
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
                                {"DoubleEndedIterator", "core/src/iter/traits/double_ended"},
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
}

} // namespace tml::codegen
