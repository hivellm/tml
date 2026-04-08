TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Pure TML Module Function Codegen
//!
//! This file emits LLVM IR for pure TML functions from imported modules.
//! Split from runtime_modules.cpp to keep files under 1500 lines.
//!
//! Contains `emit_module_pure_tml_functions` and its exclusive static helpers.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"

#include <filesystem>
#include <unordered_set>

namespace tml::codegen {

// Helper: Get the LLVM type string for a constant's declared type
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
    return "i64"; // Default for unknown types
}

/// Try to extract a compile-time constant scalar value from an expression.
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
                if (lit.token.kind == lexer::TokenKind::IntLiteral)
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
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
            if (lit.token.kind == lexer::TokenKind::IntLiteral)
                return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
        }
        return "";
    }
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral)
            return std::to_string(lit.token.int_value().value);
        if (lit.token.kind == lexer::TokenKind::BoolLiteral)
            return lit.token.bool_value() ? "1" : "0";
        if (lit.token.kind == lexer::TokenKind::NullLiteral)
            return "null";
        if (lit.token.kind == lexer::TokenKind::StringLiteral)
            return "STR:" + std::string(lit.token.string_value().value);
    }
    return "";
}

/// Try to extract a compile-time constant value (scalar or tuple) from an expression.
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
            for (const auto& et : type->as<parser::TupleType>().elements)
                elem_types.push_back(get_const_llvm_type(et));
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
            for (size_t i = 0; i < elem_values.size(); ++i)
                elem_types.push_back("i64");
        }
        std::string llvm_type = "{ ";
        std::string llvm_value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0) {
                llvm_type += ", ";
                llvm_value += ", ";
            }
            llvm_type += elem_types[i];
            llvm_value += elem_types[i] + " " + elem_values[i];
        }
        llvm_type += " }";
        llvm_value += " }";
        out_llvm_type = llvm_type;
        return llvm_value;
    }
    std::string scalar = try_extract_scalar_const(expr);
    if (!scalar.empty()) {
        out_llvm_type = get_const_llvm_type(type);
        return scalar;
    }
    return "";
}

void LLVMIRGen::emit_module_pure_tml_functions(
    const std::unordered_set<std::string>& skip_modules) {
    // Emit LLVM IR for pure TML functions from imported modules
    if (!env_.module_registry()) {
        return;
    }

    auto registry = env_.module_registry();

    // Ensure essential library modules are in the ModuleRegistry even if not
    // explicitly imported.  The type checker handles List[T] as a builtin type,
    // so modules like core::str return List[Str] without importing
    // std::collections::List.  When List was C-backed this was fine (runtime
    // provided @list_len etc.), but now that List is pure TML the codegen needs
    // the module's source code and function signatures.
    // Similarly, core::str and core::char contain `impl Str` and `impl Char`
    // blocks whose methods are called via method dispatch on primitive types
    // (e.g., "hello".contains("h")).  These must always be available even when
    // the user code doesn't explicitly import them.
    {
        static const std::vector<std::string> essential_library_modules = {
            "std::collections::list",
            "std::collections::buffer",
            // Behavior impls for collection types (PartialEq, Duplicate, etc.)
            // These are in a separate module from the type definitions.
            "std::collections::behaviors",
            "core::str",
            "core::hash",
            "std::text",
            "core::fmt::impls",
            "core::fmt::helpers",
            // Builtin enum impl sources — Maybe[T], Outcome[T,E], etc. are
            // always available as types, but their method bodies live in these
            // modules.  Without them the generic instantiation search at
            // generic.cpp:554 cannot find the source to emit method bodies.
            "core::option",
            "core::result",
            // Default trait impls for built-in types (Maybe::default, Outcome::default, etc.)
            // core::default is not transitively imported by most user code but its
            // impl[T] Default for Maybe[T] is needed when Maybe::default() is called.
            "core::default",
            // PartialEq/Eq impls for Maybe/Outcome (needed for == operator on these types)
            "core::cmp",
            // Clone/Duplicate impls for Maybe/Outcome
            "core::clone",
        };
        for (const auto& mod_path : essential_library_modules) {
            if (registry->has_module(mod_path))
                continue;
            // Try GlobalModuleCache first (populated by type checker during this run)
            auto cached = types::GlobalModuleCache::instance().get(mod_path);
            if (cached) {
                registry->register_module(mod_path, std::move(*cached));
                TML_DEBUG_LN("[MODULE] Auto-registered essential module from GlobalModuleCache: "
                             << mod_path);
            } else {
                // Fall back to binary meta cache (.tml.meta files on disk).
                // This handles trait-impl modules (core::default, core::cmp, etc.)
                // that may not be loaded into GlobalModuleCache during this run
                // because user code doesn't explicitly import them.
                auto from_disk = types::load_module_from_cache(mod_path);
                if (from_disk && !from_disk->source_code.empty()) {
                    registry->register_module(mod_path, std::move(*from_disk));
                    TML_DEBUG_LN(
                        "[MODULE] Auto-registered essential module from disk cache: " << mod_path);
                }
            }
        }
    }

    const auto& all_modules = registry->get_all_modules();

    // Collect imported module paths AND type names for filtering
    // This dramatically reduces codegen time by skipping unneeded modules
    std::unordered_set<std::string> imported_types;
    std::unordered_set<std::string> imported_module_paths;
    const auto& all_imports = env_.all_imports();
    for (const auto& [name, sym] : all_imports) {
        imported_types.insert(sym.original_name);
        imported_types.insert(name); // Also add local alias
        // Track the module path this symbol was imported from
        if (!sym.module_path.empty()) {
            imported_module_paths.insert(sym.module_path);
        }
    }

    // --- Transitive import expansion ---
    // When module A imports module B, and B imports module C, we must also
    // process C. Without this, cross-library imports (e.g., std::uuid importing
    // core::encoding::hex) would leave C's functions unresolved at link time.
    // This also follows `pub use` re-exports: when module A imports from B which
    // re-exports symbols from C, C must be in the imported set for its function
    // bodies and their dependencies to be emitted.
    {
        std::vector<std::string> worklist(imported_module_paths.begin(),
                                          imported_module_paths.end());
        while (!worklist.empty()) {
            std::string path = std::move(worklist.back());
            worklist.pop_back();
            auto it = all_modules.find(path);
            if (it == all_modules.end())
                continue;

            // Follow private_imports (transitive dependencies)
            for (const auto& dep : it->second.private_imports) {
                // private_imports may contain symbol-level paths like
                // "std::net::sys::RawSocket" where the actual module is
                // "std::net::sys". Try exact match first, then strip last
                // segment(s) to find the real module.
                std::string resolved = dep;
                if (all_modules.find(resolved) == all_modules.end()) {
                    // Strip last segment (likely a symbol name, not a module)
                    auto sep = resolved.rfind("::");
                    if (sep != std::string::npos) {
                        resolved = resolved.substr(0, sep);
                    }
                    // If still not found, try one more level
                    if (all_modules.find(resolved) == all_modules.end()) {
                        sep = resolved.rfind("::");
                        if (sep != std::string::npos) {
                            resolved = resolved.substr(0, sep);
                        }
                    }
                }
                if (imported_module_paths.insert(resolved).second) {
                    worklist.push_back(resolved);
                }
                // Also insert the original dep path (it may match an exact module)
                if (resolved != dep) {
                    imported_module_paths.insert(dep);
                }
            }

            // Follow pub use re-exports — their source modules contain the actual
            // function definitions and may have their own transitive dependencies.
            for (const auto& re_export : it->second.re_exports) {
                std::string source = re_export.source_path;
                // Re-export source_path may be a symbol-level path (e.g.,
                // "std::http::middleware::cors") — resolve to actual module.
                if (all_modules.find(source) == all_modules.end()) {
                    auto sep = source.rfind("::");
                    if (sep != std::string::npos) {
                        std::string parent = source.substr(0, sep);
                        if (all_modules.find(parent) != all_modules.end()) {
                            source = parent;
                        }
                    }
                }
                if (imported_module_paths.insert(source).second) {
                    worklist.push_back(source);
                }
            }
        }
    }

    // --- Compute conditional sync module requirements ---
    // Only include sync essential modules when sync/thread is actually imported.
    // This avoids processing ~1400 lines of atomic.tml + mutex/condvar for non-sync tests.
    bool needs_sync_atomic_essential = false;
    bool needs_sync_mutex_essential = false;
    bool needs_sync_condvar_essential = false;

    for (const auto& imported_path : imported_module_paths) {
        // Any sync/thread import needs atomic (for Ordering used by all sync types)
        if (imported_path.find("std::sync") == 0 || imported_path.find("std::thread") == 0) {
            needs_sync_atomic_essential = true;
        }
        // Mutex module needed when mutex/mpsc/barrier/once/rwlock is used
        if (imported_path.find("std::sync::mutex") == 0 ||
            imported_path.find("std::sync::mpsc") == 0 ||
            imported_path.find("std::sync::barrier") == 0 ||
            imported_path.find("std::sync::once") == 0 ||
            imported_path.find("std::sync::rwlock") == 0 || imported_path == "std::sync") {
            needs_sync_mutex_essential = true;
        }
        // Condvar module needed when condvar/mpsc/barrier is used
        if (imported_path.find("std::sync::condvar") == 0 ||
            imported_path.find("std::sync::mpsc") == 0 ||
            imported_path.find("std::sync::barrier") == 0 || imported_path == "std::sync") {
            needs_sync_condvar_essential = true;
        }
    }
    // Also check direct type imports
    if (imported_types.count("Mutex") || imported_types.count("MutexGuard")) {
        needs_sync_mutex_essential = true;
        needs_sync_atomic_essential = true;
    }
    if (imported_types.count("Condvar")) {
        needs_sync_condvar_essential = true;
        needs_sync_atomic_essential = true;
    }
    if (imported_types.count("Arc") || imported_types.count("Weak")) {
        needs_sync_atomic_essential = true;
    }

    // Build dynamic always_generate set based on actual import needs
    // This replaces the static set that generated ALL sync types unconditionally
    std::unordered_set<std::string> dynamic_always_generate = {
        "Ordering",
        "Layout",
        "LayoutError",
    };
    // Arc dependencies
    if (imported_types.count("Arc") || imported_types.count("Weak") ||
        imported_types.count("ArcInner")) {
        dynamic_always_generate.insert("AtomicUsize");
        dynamic_always_generate.insert("Weak");
        dynamic_always_generate.insert("ArcInner");
    }
    // Mutex dependencies
    if (needs_sync_mutex_essential) {
        dynamic_always_generate.insert("Mutex");
        dynamic_always_generate.insert("RawMutex");
        dynamic_always_generate.insert("MutexGuard");
    }
    // Condvar dependencies
    if (needs_sync_condvar_essential) {
        dynamic_always_generate.insert("Condvar");
        dynamic_always_generate.insert("RawCondvar");
    }
    // Thread module dependencies
    bool has_thread_import = false;
    for (const auto& p : imported_module_paths) {
        if (p.find("std::thread") == 0) {
            has_thread_import = true;
            break;
        }
    }
    if (has_thread_import) {
        dynamic_always_generate.insert("AtomicBool");
        dynamic_always_generate.insert("AtomicPtr");
        dynamic_always_generate.insert("AtomicUsize");
    }
    // RwLock dependencies
    if (imported_types.count("RwLock") || imported_types.count("RwLockReadGuard") ||
        imported_types.count("RwLockWriteGuard")) {
        dynamic_always_generate.insert("RwLockReadGuard");
        dynamic_always_generate.insert("RwLockWriteGuard");
    }

    // Pre-scan: collect types imported by modules that will be processed.
    // This enriches imported_types with transitive dependencies, allowing
    // the impl block filter to skip unused types more precisely.
    for (const auto& [module_name, module] : all_modules) {
        if (!module.has_pure_tml_functions || module.source_code.empty()) {
            continue;
        }
        // Check if this module will be processed (same logic as main loop)
        bool will_process = false;
        for (const auto& imported_path : imported_module_paths) {
            if (module_name == imported_path || imported_path.find(module_name + "::") == 0 ||
                module_name.find(imported_path + "::") == 0) {
                will_process = true;
                break;
            }
        }
        if (!will_process) {
            static const std::unordered_set<std::string> core_essential = {
                "core::ordering",         "core::alloc",      "core::option",       "core::types",
                "std::collections::list", "core::fmt::impls", "core::fmt::helpers",
            };
            will_process = core_essential.count(module_name) > 0;
            // Conditionally add sync essential modules
            if (!will_process) {
                if (module_name == "std::sync::atomic" && needs_sync_atomic_essential)
                    will_process = true;
                else if (module_name == "std::sync::mutex" && needs_sync_mutex_essential)
                    will_process = true;
                else if (module_name == "std::sync::condvar" && needs_sync_condvar_essential)
                    will_process = true;
            }
            if (!will_process) {
                std::string last_seg = module_name;
                auto sep = module_name.rfind("::");
                if (sep != std::string::npos)
                    last_seg = module_name.substr(sep + 2);
                static const std::unordered_set<std::string> essential_segs = {
                    "ordering",
                    "alloc",
                    "option",
                };
                will_process = essential_segs.count(last_seg) > 0;
            }
        }
        if (!will_process)
            continue;

        // Quick scan: extract type names from use declarations in module source
        // Look for patterns like: use std::sync::atomic::{AtomicBool, AtomicUsize}
        // and: use std::sync::arc::Arc
        const auto& src = module.source_code;
        size_t pos = 0;
        while ((pos = src.find("use ", pos)) != std::string::npos) {
            // Make sure this is at line start (preceded by newline or start of file)
            if (pos > 0 && src[pos - 1] != '\n' && src[pos - 1] != '\r') {
                pos += 4;
                continue;
            }
            size_t line_end = src.find('\n', pos);
            if (line_end == std::string::npos)
                line_end = src.size();
            std::string line = src.substr(pos, line_end - pos);

            // Handle grouped imports: use foo::{Bar, Baz}
            auto brace_start = line.find('{');
            if (brace_start != std::string::npos) {
                auto brace_end = line.find('}', brace_start);
                if (brace_end != std::string::npos) {
                    std::string symbols = line.substr(brace_start + 1, brace_end - brace_start - 1);
                    // Split by comma
                    size_t s = 0;
                    while (s < symbols.size()) {
                        while (s < symbols.size() && (symbols[s] == ' ' || symbols[s] == ','))
                            ++s;
                        size_t e = s;
                        while (e < symbols.size() && symbols[e] != ',' && symbols[e] != ' ')
                            ++e;
                        if (e > s) {
                            std::string sym = symbols.substr(s, e - s);
                            // Only add type-like names (start with uppercase)
                            if (!sym.empty() && std::isupper(sym[0])) {
                                imported_types.insert(sym);
                            }
                        }
                        s = e;
                    }
                }
            } else {
                // Handle simple imports: use foo::bar::Baz
                auto last_sep = line.rfind("::");
                if (last_sep != std::string::npos && last_sep + 2 < line.size()) {
                    std::string sym = line.substr(last_sep + 2);
                    // Trim trailing whitespace
                    while (!sym.empty() &&
                           (sym.back() == '\r' || sym.back() == '\n' || sym.back() == ' '))
                        sym.pop_back();
                    if (!sym.empty() && std::isupper(sym[0])) {
                        imported_types.insert(sym);
                    }
                }
            }
            pos = line_end;
        }
    }

    emit_line("; Pure TML functions from imported modules");

    // Collect eligible modules: filter, parse ASTs, store per-module info
    struct ModuleInfo {
        std::string module_name;
        std::string mod_name; // stem of file path
        std::string sanitized_prefix;
        const types::Module* module_ptr;
        const parser::Module* parsed_module_ptr;
    };
    std::vector<ModuleInfo> eligible_modules;

    for (const auto& [module_name, module] : all_modules) {
        // Check if module has pure TML functions
        if (!module.has_pure_tml_functions || module.source_code.empty()) {
            continue;
        }

        // Skip modules already processed by the cached library state.
        // This is used in supplemental mode: only process modules that were
        // NOT covered by the bootstrap's cached state.
        if (!skip_modules.empty() && skip_modules.count(module_name) > 0) {
            continue;
        }

        // Early skip: Only process modules that were actually imported from
        // This avoids expensive re-parsing of modules we don't need
        if (!imported_module_paths.empty()) {
            bool should_process = false;

            // Check if this module path matches, is a parent/child, or sibling of an imported
            // module
            for (const auto& imported_path : imported_module_paths) {
                if (module_name == imported_path) {
                    should_process = true;
                    break;
                }
                // Module is parent of imported (e.g., core::unicode for core::unicode::char)
                if (imported_path.find(module_name + "::") == 0) {
                    should_process = true;
                    break;
                }
                // Module is child of imported (e.g., core::unicode::char for core::unicode)
                if (module_name.find(imported_path + "::") == 0) {
                    should_process = true;
                    break;
                }
                // Sibling module: same parent prefix (e.g., core::unicode::unicode_data
                // is sibling of core::unicode::char -- both under core::unicode)
                auto mod_sep = module_name.rfind("::");
                auto imp_sep = imported_path.rfind("::");
                if (mod_sep != std::string::npos && imp_sep != std::string::npos) {
                    std::string mod_parent = module_name.substr(0, mod_sep);
                    std::string imp_parent = imported_path.substr(0, imp_sep);
                    if (mod_parent == imp_parent) {
                        should_process = true;
                        break;
                    }
                }
            }

            // Essential modules: core always needed, sync conditionally
            if (!should_process) {
                static const std::unordered_set<std::string> core_essential_modules = {
                    "core::ordering",
                    "core::alloc",
                    "core::option",
                    "core::types",
                    "core::ops",
                    "core::ops::arith",
                    "core::str",
                    "std::collections::list",
                    "std::collections::List",
                    "std::collections::buffer",
                    "std::text",
                    "core::fmt::impls",
                    "core::fmt::helpers",
                };
                should_process = core_essential_modules.count(module_name) > 0;
                if (!should_process) {
                    if (module_name == "std::sync::atomic" && needs_sync_atomic_essential)
                        should_process = true;
                    else if (module_name == "std::sync::mutex" && needs_sync_mutex_essential)
                        should_process = true;
                    else if (module_name == "std::sync::condvar" && needs_sync_condvar_essential)
                        should_process = true;
                }
                if (!should_process) {
                    std::string last_segment = module_name;
                    auto last_sep = module_name.rfind("::");
                    if (last_sep != std::string::npos) {
                        last_segment = module_name.substr(last_sep + 2);
                    }
                    static const std::unordered_set<std::string> essential_last_segments = {
                        "ordering",
                        "alloc",
                        "option",
                    };
                    should_process = essential_last_segments.count(last_segment) > 0;
                }
            }

            if (!should_process) {
                TML_DEBUG_LN("[MODULE] Early skip module: " << module_name);
                continue;
            }
        }

        // Parse or retrieve cached AST
        const parser::Module* cached_ast = nullptr;
        if (GlobalASTCache::should_cache(module_name)) {
            cached_ast = GlobalASTCache::instance().get(module_name);
        }

        const parser::Module* parsed_module_ptr = nullptr;
        auto mod_name = std::filesystem::path(module.file_path).stem().string();

        if (cached_ast) {
            TML_DEBUG_LN("[CODEGEN] AST cache hit for: " << module_name);
            parsed_module_ptr = cached_ast;
        } else {
            auto source = lexer::Source::from_string(module.source_code, module.file_path);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();

            if (lex.has_errors()) {
                TML_DEBUG_LN("[MODULE] Lex errors for: " << module_name);
                continue;
            }

            parser::Parser parser(std::move(tokens));
            auto parse_result = parser.parse_module(mod_name);

            if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
                const auto& errs = std::get<std::vector<parser::ParseError>>(parse_result);
                TML_DEBUG_LN("[MODULE] Parse errors for: " << module_name << " (" << errs.size()
                                                           << " errors)");
                for (const auto& e : errs) {
                    TML_DEBUG_LN("[MODULE]   " << e.span.start.line << ":" << e.span.start.column
                                               << " " << e.message);
                }
                continue;
            }

            auto parsed_mod = std::get<parser::Module>(std::move(parse_result));

            if (GlobalASTCache::should_cache(module_name)) {
                GlobalASTCache::instance().put(module_name, std::move(parsed_mod));
                parsed_module_ptr = GlobalASTCache::instance().get(module_name);
                TML_DEBUG_LN("[CODEGEN] AST cached: " << module_name);
            } else {
                imported_module_asts_.push_back(std::move(parsed_mod));
                parsed_module_ptr = &imported_module_asts_.back();
            }
        }

        if (!parsed_module_ptr) {
            continue;
        }

        // Compute sanitized module prefix
        std::string sanitized_prefix = module_name;
        size_t pos = 0;
        while ((pos = sanitized_prefix.find("::", pos)) != std::string::npos) {
            sanitized_prefix.replace(pos, 2, "_");
            pos += 1;
        }

        eligible_modules.push_back(
            {module_name, mod_name, sanitized_prefix, &module, parsed_module_ptr});
    }

    // Record which modules are being processed (for capture_library_state())
    for (const auto& info : eligible_modules) {
        processed_module_paths_.insert(info.module_name);
    }

    // ========================================================================
    // PHASE 0: Register struct/enum types from ALL imported modules, including
    // type-only modules (has_pure_tml_functions=false, source_code empty).
    // These modules are excluded from eligible_modules but their types are
    // needed by modules that ARE eligible (e.g., ir_diff::types defines
    // IrModule used by ir_diff::differ). Without this, struct field access
    // fails during Phase 2 codegen because struct_fields_ doesn't have the
    // type's field layout.
    //
    // This uses the module registry's StructDef/EnumDef directly (no re-parsing).
    // ========================================================================
    TML_LOG_INFO("codegen", "Phase 0: all_modules.size=" << all_modules.size()
                                                         << " imported_module_paths.size="
                                                         << imported_module_paths.size());
    for (const auto& ip : imported_module_paths) {
        TML_LOG_INFO("codegen", "Phase 0: imported_path=" << ip);
    }
    for (const auto& [module_name, module] : all_modules) {
        // Only process if this module is actually imported
        bool is_needed = false;
        for (const auto& imported_path : imported_module_paths) {
            if (module_name == imported_path || imported_path.find(module_name + "::") == 0 ||
                module_name.find(imported_path + "::") == 0) {
                is_needed = true;
                break;
            }
        }
        if (!is_needed)
            continue;

        // Register structs from the module registry's StructDef
        TML_LOG_INFO("codegen", "Phase 0: Processing module "
                                    << module_name << " structs=" << module.structs.size()
                                    << " has_pure_tml=" << module.has_pure_tml_functions
                                    << " source_empty=" << module.source_code.empty());
        for (const auto& [struct_name, struct_def] : module.structs) {
            // Skip if already registered
            if (struct_fields_.find(struct_name) != struct_fields_.end())
                continue;
            // Skip generic structs (will be instantiated on demand)
            if (!struct_def.type_params.empty())
                continue;

            std::string type_name = "%struct." + struct_name;
            if (struct_types_.find(struct_name) == struct_types_.end()) {
                // Collect field types and field info
                std::vector<FieldInfo> fields;
                std::vector<std::string> field_types;
                for (size_t i = 0; i < struct_def.fields.size(); ++i) {
                    const auto& fld = struct_def.fields[i];
                    std::string ft = llvm_type_from_semantic(fld.type, true);
                    // Function pointer fields use fat pointer to support closures
                    if (fld.type && fld.type->is<types::FuncType>()) {
                        ft = "{ ptr, ptr }";
                    }
                    field_types.push_back(ft);
                    fields.push_back({fld.name, static_cast<int>(i), ft, fld.type});
                }

                // @simd structs emit LLVM vector types instead of regular structs
                if (struct_def.is_simd && !field_types.empty()) {
                    std::string elem_type = field_types[0];
                    int lane_count = static_cast<int>(field_types.size());
                    std::string def = type_name + " = type <" + std::to_string(lane_count) + " x " +
                                      elem_type + ">";
                    type_defs_buffer_ << def << "\n";
                    simd_types_[struct_name] = {elem_type, lane_count};
                } else {
                    std::string def = type_name + " = type { ";
                    bool first = true;
                    for (const auto& ft : field_types) {
                        if (!first)
                            def += ", ";
                        first = false;
                        def += ft;
                    }
                    def += " }";
                    type_defs_buffer_ << def << "\n";
                }
                struct_types_[struct_name] = type_name;
                struct_fields_[struct_name] = std::move(fields);
            }
        }

        // Register enums similarly
        for (const auto& [enum_name, enum_def] : module.enums) {
            if (enum_variants_.find(enum_name) != enum_variants_.end())
                continue;
            if (!enum_def.type_params.empty())
                continue;
            // Register enum variant info for pattern matching/construction
            for (const auto& variant : enum_def.variants) {
                std::string key = enum_name + "::" + variant.name;
                if (enum_variants_.find(key) == enum_variants_.end()) {
                    enum_variants_[key] = EnumVariantInfo{
                        variant.name, static_cast<int>(&variant - &enum_def.variants[0])};
                }
            }
        }
    }

    // ========================================================================
    // PHASE 1: Register ALL types (structs, enums, constants, function signatures)
    // from ALL modules BEFORE generating any code. This ensures types like
    // "Ordering" are registered before any impl method tries to use them,
    // regardless of unordered_map iteration order.
    // ========================================================================
    for (const auto& info : eligible_modules) {
        const auto& parsed_module = *info.parsed_module_ptr;
        current_module_prefix_ = info.sanitized_prefix;
        current_module_name_ = info.module_name;
        current_submodule_name_ = info.mod_name;

        // First pass: register struct/enum declarations (including generic ones)
        // IMPORTANT: Register ALL structs/enums, not just public ones!
        // Private types like StackNode[T] are still used internally and need to be
        // instantiated when size_of[T]() or similar intrinsics are called.
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                // Register all structs (public AND private) for generic instantiation
                // Private types are needed for internal use (e.g., size_of[StackNode[T]])
                gen_struct_decl(s);
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                // Register all enums (public AND private) for generic instantiation
                gen_enum_decl(e);
            } else if (decl->is<parser::ClassDecl>()) {
                const auto& c = decl->as<parser::ClassDecl>();
                // Register class types so %class.ClassName is properly sized.
                // Use emit_external_class_type for type + field registration,
                // then gen_class_vtable for vtable type + global.
                // First try the env's lookup (works for imported classes), then
                // fall back to direct module registry lookup (for non-imported
                // classes in the same module file, e.g. exception subclasses).
                auto class_def = env_.lookup_class(c.name);
                if (!class_def.has_value() && env_.module_registry()) {
                    class_def = env_.module_registry()->lookup_class(info.module_name, c.name);
                }
                if (class_def.has_value()) {
                    emit_external_class_type(c.name, *class_def);
                    // Generate vtable type and global constant.
                    // gen_class_vtable emits the vtable type and global with
                    // method pointers. Methods themselves are generated in PHASE 2.
                    gen_class_vtable(c);
                }
            } else if (decl->is<parser::ConstDecl>()) {
                // Register module-level constants for use in functions
                const auto& const_decl = decl->as<parser::ConstDecl>();
                std::string llvm_type;
                std::string value =
                    try_extract_const_value(const_decl.value.get(), const_decl.type, llvm_type);

                if (!value.empty()) {
                    // Register both with and without module prefix
                    global_constants_[const_decl.name] = {value, llvm_type};
                    // Also register with qualified name for explicit lookups
                    std::string qualified_name = info.module_name + "::" + const_decl.name;
                    global_constants_[qualified_name] = {value, llvm_type};
                }
            } else if (decl->is<parser::UseDecl>()) {
                // Handle wildcard imports: "use module::*" brings all constants into scope
                const auto& use_decl = decl->as<parser::UseDecl>();
                if (use_decl.is_glob && !use_decl.path.segments.empty()) {
                    // Build the imported module path
                    std::string import_path;
                    for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                        if (i > 0)
                            import_path += "::";
                        import_path += use_decl.path.segments[i];
                    }

                    // Look up the imported module and register its constants
                    auto imported_mod = registry->get_module(import_path);
                    if (imported_mod) {
                        // Import constants directly from the module
                        for (const auto& [const_name, const_info] : imported_mod->constants) {
                            // Only import non-qualified constants (not Type::CONST)
                            if (const_name.find("::") == std::string::npos) {
                                // Convert TML type to LLVM type
                                std::string llvm_type = llvm_type_name(const_info.tml_type);
                                global_constants_[const_name] = {const_info.value, llvm_type};
                                TML_DEBUG_LN("[MODULE] Imported constant via wildcard: "
                                             << const_name << " = " << const_info.value << " from "
                                             << import_path);
                            }
                        }

                        // Also follow re-exports (pub use) to import constants from
                        // re-exported modules. This handles chains like:
                        // std::zlib -> std::zlib::constants (via pub use zlib::constants::*)
                        for (const auto& re_export : imported_mod->re_exports) {
                            if (re_export.is_glob) {
                                auto re_exported_mod = registry->get_module(re_export.source_path);
                                if (re_exported_mod) {
                                    for (const auto& [const_name, const_info] :
                                         re_exported_mod->constants) {
                                        if (const_name.find("::") == std::string::npos) {
                                            std::string llvm_type =
                                                llvm_type_name(const_info.tml_type);
                                            global_constants_[const_name] = {const_info.value,
                                                                             llvm_type};
                                            TML_DEBUG_LN(
                                                "[MODULE] Imported constant via re-export: "
                                                << const_name << " = " << const_info.value
                                                << " from " << re_export.source_path);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (decl->is<parser::TraitDecl>()) {
                // Register behavior/trait declarations so that default method
                // bodies can be generated for impl blocks in Phase 2.
                // Register by FQN + first-write-wins short name to avoid collisions
                // between same-named behaviors in different modules.
                const auto& trait = decl->as<parser::TraitDecl>();
                std::string fqn = info.module_name + "::" + trait.name;
                trait_decls_[fqn] = &trait;
                trait_decls_.emplace(trait.name, &trait);
            }
        }

        // Pre-register ALL function signatures before generating any code
        // This ensures intra-module calls (like mod.tml calling unicode_data::func) resolve
        // correctly. Includes PRIVATE functions to support same-module calls.
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                if (!func.is_unsafe && func.body.has_value()) {
                    pre_register_func(func);
                }
            }
        }

        // Pre-register generic impl blocks into pending_generic_impls_ so that
        // any module processed in Phase 2 can instantiate generic methods
        // (e.g., core::str calling List[Str].len() needs impl[T] List[T] registered
        // before core::str's function bodies are generated, regardless of iteration order).
        for (const auto& decl : parsed_module.decls) {
            if (!decl->is<parser::ImplDecl>())
                continue;
            const auto& impl = decl->as<parser::ImplDecl>();

            // Get the type name
            std::string type_name;
            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                const auto& named = impl.self_type->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            } else if (impl.self_type && impl.self_type->is<parser::TupleType>()) {
                const auto& tuple = impl.self_type->as<parser::TupleType>();
                type_name = "Tuple" + std::to_string(tuple.elements.size());
            }
            if (type_name.empty())
                continue;

            bool has_impl_generics = !impl.generics.empty();
            bool has_type_generics = false;
            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                const auto& named = impl.self_type->as<parser::NamedType>();
                if (named.generics.has_value() && !named.generics->args.empty()) {
                    has_type_generics = true;
                }
            }
            if (has_impl_generics || has_type_generics) {
                pending_generic_impls_[type_name] = &impl;
                pending_generic_impls_all_[type_name].push_back(&impl);
                TML_DEBUG_LN("[MODULE] Phase 1: Pre-registered generic impl for: "
                             << type_name << " (generics=" << impl.generics.size() << ")");
            }

            // Also register the impl block for vtable generation (dyn dispatch)
            register_impl(&impl);
        }
    }
    current_module_prefix_.clear();
    current_module_name_.clear();

    // ========================================================================
    // PHASE 2: Generate code for functions and impl methods.
    // All types are now registered from Phase 1, so type lookups
    // (like "Ordering") will always find their definitions.
    //
    // In library_decls_only mode, gen_func_decl and gen_impl_method emit
    // `declare` statements instead of full function definitions. The
    // implementations come from a shared library object compiled once per suite.
    // ========================================================================
    for (const auto& info : eligible_modules) {
        const auto& parsed_module = *info.parsed_module_ptr;
        const auto& module_name = info.module_name;
        current_module_prefix_ = info.sanitized_prefix;
        current_module_name_ = info.module_name;
        current_submodule_name_ = info.mod_name;

        emit_line("; Module: " + module_name);

        // Collect types defined in THIS module (struct/enum/class declarations)
        // so their impl blocks are not skipped by the imported_types filter
        std::unordered_set<std::string> module_defined_types;
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                if (!s.name.empty())
                    module_defined_types.insert(s.name);
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                if (!e.name.empty())
                    module_defined_types.insert(e.name);
            } else if (decl->is<parser::ClassDecl>()) {
                const auto& c = decl->as<parser::ClassDecl>();
                if (!c.name.empty())
                    module_defined_types.insert(c.name);
            }
        }

        // Generate code for each function (both public AND private)
        TML_DEBUG_LN("[MODULE] Processing " << parsed_module.decls.size() << " decls for "
                                            << module_name);
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();

                // Process extern functions - emit declarations
                if (func.extern_abi.has_value()) {
                    TML_DEBUG_LN("[MODULE] Found @extern func: " << func.name
                                                                 << " abi=" << *func.extern_abi);
                    gen_func_decl(func);
                    continue;
                }

                // Generate code for both public AND private functions with bodies
                // Private functions are needed for intra-module helper functions
                if (!func.is_unsafe && func.body.has_value()) {
                    gen_func_decl(func);
                }
            }
            // Also handle impl blocks - generate methods for imported types
            else if (decl->is<parser::ImplDecl>()) {
                const auto& impl = decl->as<parser::ImplDecl>();

                // Register impl block for vtable generation (enables dyn dispatch)
                register_impl(&impl);

                // Get the type name for the impl (needed before generic check)
                std::string type_name;
                if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                } else if (impl.self_type && impl.self_type->is<parser::TupleType>()) {
                    const auto& tuple = impl.self_type->as<parser::TupleType>();
                    type_name = "Tuple" + std::to_string(tuple.elements.size());
                }

                // Skip generic impls - they need to be instantiated on demand
                // Generic impls have type parameters like impl[T] or impl[I: Iterator]
                // But store them in pending_generic_impls_ so they can be instantiated later
                bool has_impl_generics = !impl.generics.empty();
                bool has_type_generics = false;
                if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl.self_type->as<parser::NamedType>();
                    if (named.generics.has_value() && !named.generics->args.empty()) {
                        has_type_generics = true;
                    }
                }
                if (has_impl_generics || has_type_generics) {
                    // Generic impls are already registered in Phase 1 (pre-registration).
                    // Skip to avoid double-adding to pending_generic_impls_all_.
                    TML_DEBUG_LN(
                        "[MODULE] Phase 2: Skipping generic impl (pre-registered): " << type_name);
                    continue;
                }

                // Skip impl blocks for types that aren't actually imported
                // This dramatically reduces codegen time (e.g., don't generate AtomicBool when only
                // Arc is used). Uses dynamic_always_generate computed based on actual imports.
                // IMPORTANT: Never skip impl blocks for primitive types (I32, U8, etc.)
                // because behavior impls like `impl PartialEq for I32` must always be
                // generated -- primitives are builtin types that are never "imported".
                auto is_primitive_type = [](const std::string& name) {
                    return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                           name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                           name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                           name == "Bool" || name == "Char" || name == "Str";
                };
                if (!type_name.empty() && !imported_types.empty() &&
                    imported_types.find(type_name) == imported_types.end() &&
                    !is_primitive_type(type_name)) {
                    if (dynamic_always_generate.find(type_name) == dynamic_always_generate.end() &&
                        module_defined_types.find(type_name) == module_defined_types.end()) {
                        TML_DEBUG_LN("[MODULE] Skipping impl for non-imported type: " << type_name);
                        continue;
                    }
                }

                if (!type_name.empty()) {
                    // Extract constants from impl block (e.g., I32::MIN, I32::MAX)
                    for (const auto& const_decl : impl.constants) {
                        std::string qualified_name = type_name + "::" + const_decl.name;
                        std::string llvm_type;
                        std::string value = try_extract_const_value(const_decl.value.get(),
                                                                    const_decl.type, llvm_type);
                        if (!value.empty()) {
                            global_constants_[qualified_name] = {value, llvm_type};
                        }
                    }

                    // For generic types (like Maybe[T]), ensure the generic struct type exists
                    // This is needed because impl methods use the base type name
                    auto enum_it = pending_generic_enums_.find(type_name);
                    if (enum_it != pending_generic_enums_.end()) {
                        // Check if generic struct type already declared
                        if (struct_types_.find(type_name) == struct_types_.end()) {
                            // Emit generic type definition with i64 payload (fits all
                            // instantiations)
                            type_defs_buffer_ << "%struct." << type_name
                                              << " = type { i32, i64 }\n";
                            struct_types_[type_name] = "%struct." + type_name;
                        }
                    }

                    // First pass: pre-instantiate generic types used in method signatures
                    for (const auto& method : impl.methods) {
                        if (method.return_type.has_value()) {
                            const auto& ret_type = *method.return_type;
                            if (ret_type->is<parser::NamedType>()) {
                                const auto& named = ret_type->as<parser::NamedType>();
                                if (named.generics.has_value() && !named.generics->args.empty()) {
                                    // Check if this is a pending generic enum
                                    std::string base_name;
                                    if (!named.path.segments.empty()) {
                                        base_name = named.path.segments.back();
                                    }
                                    auto it = pending_generic_enums_.find(base_name);
                                    if (it != pending_generic_enums_.end()) {
                                        // Convert parser type args to semantic types
                                        std::vector<types::TypePtr> type_args;
                                        for (const auto& arg : named.generics->args) {
                                            if (arg.is_type()) {
                                                type_args.push_back(resolve_parser_type_with_subs(
                                                    *arg.as_type(), {}));
                                            }
                                        }
                                        // Check if already instantiated
                                        std::string mangled =
                                            mangle_struct_name(base_name, type_args);
                                        if (struct_types_.find(mangled) == struct_types_.end()) {
                                            gen_enum_instantiation(*it->second, type_args);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Second pass: generate the methods
                    for (const auto& method : impl.methods) {
                        // Skip generic methods - they are instantiated on demand when called
                        // with concrete types. Emitting the uninstantiated template would use
                        // %struct.T (unsized opaque type) which LLVM rejects.
                        if (!method.generics.empty()) {
                            continue;
                        }
                        // In lazy mode, also process private methods so they can be
                        // generated on demand when public methods reference them.
                        // Without lazy mode, only public methods are needed (private
                        // methods are generated inline within their callers).
                        bool should_generate = method.vis == parser::Visibility::Public &&
                                               !method.is_unsafe && method.body.has_value();
                        if (!should_generate && options_.lazy_library_defs &&
                            !options_.library_ir_only && method.body.has_value() &&
                            !method.is_unsafe) {
                            should_generate = true;
                        }
                        if (should_generate) {
                            gen_impl_method(type_name, method);
                        }
                    }
                }
            }
            // Handle class declarations - generate methods for imported classes
            // This ensures class methods like Object::reference_equals get declare statements
            else if (decl->is<parser::ClassDecl>()) {
                const auto& cls = decl->as<parser::ClassDecl>();
                std::string class_name = cls.name;

                // Apply same filter as impl blocks
                if (!class_name.empty() && !imported_types.empty() &&
                    imported_types.find(class_name) == imported_types.end()) {
                    if (dynamic_always_generate.find(class_name) == dynamic_always_generate.end() &&
                        module_defined_types.find(class_name) == module_defined_types.end()) {
                        TML_DEBUG_LN(
                            "[MODULE] Skipping class for non-imported type: " << class_name);
                        continue;
                    }
                }

                // Generate methods for this class
                // gen_class_method handles library_decls_only mode
                for (const auto& method : cls.methods) {
                    if (!method.generics.empty()) {
                        continue; // Skip generic methods
                    }
                    gen_class_method(cls, method);
                }
            }
        }

        // Clear module prefix and name after processing this module
        current_module_prefix_.clear();
        current_module_name_.clear();
    }

    emit_line("");
}

} // namespace tml::codegen
