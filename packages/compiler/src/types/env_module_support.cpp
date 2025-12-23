#include "tml/types/env.hpp"
#include "tml/types/module.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>

namespace tml::types {

void TypeEnv::set_module_registry(std::shared_ptr<ModuleRegistry> registry) {
    module_registry_ = std::move(registry);
    // Modules will be loaded lazily when imported via 'use'
    // No hardcoded initialization here
}

void TypeEnv::set_current_module(const std::string& module_path) {
    current_module_path_ = module_path;
}

auto TypeEnv::module_registry() const -> std::shared_ptr<ModuleRegistry> {
    return module_registry_;
}

auto TypeEnv::current_module() const -> const std::string& {
    return current_module_path_;
}

void TypeEnv::import_symbol(const std::string& module_path, const std::string& symbol_name,
                           std::optional<std::string> alias) {
    // Determine the local name (use alias if provided, otherwise original name)
    std::string local_name = alias.value_or(symbol_name);

    // Create the imported symbol entry
    ImportedSymbol import{
        .original_name = symbol_name,
        .local_name = local_name,
        .module_path = module_path,
        .visibility = parser::Visibility::Public  // Imported symbols are accessible
    };

    // Store the import
    imported_symbols_[local_name] = import;
}

void TypeEnv::import_all_from(const std::string& module_path) {
    if (!module_registry_) {
        return;  // No module registry available
    }

    auto module = module_registry_->get_module(module_path);
    if (!module) {
        return;  // Module not found
    }

    // Import all functions
    for (const auto& [name, func_sig] : module->functions) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all structs
    for (const auto& [name, struct_def] : module->structs) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all enums
    for (const auto& [name, enum_def] : module->enums) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all behaviors
    for (const auto& [name, behavior_def] : module->behaviors) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all type aliases
    for (const auto& [name, type_ptr] : module->type_aliases) {
        import_symbol(module_path, name, std::nullopt);
    }
}

auto TypeEnv::resolve_imported_symbol(const std::string& name) const
    -> std::optional<std::string> {
    auto it = imported_symbols_.find(name);
    if (it != imported_symbols_.end()) {
        // Return the full qualified name: module_path::original_name
        return it->second.module_path + "::" + it->second.original_name;
    }
    return std::nullopt;
}

bool TypeEnv::load_module_from_file(const std::string& module_path, const std::string& file_path) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true;  // Already loaded
    }

    // Read the TML file
    std::ifstream file(file_path);
    if (!file) {
        TML_DEBUG_LN("[MODULE] Failed to open: " << file_path);
        return false;
    }

    std::string source_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Lex and parse the module
    auto source = lexer::Source::from_string(source_code, file_path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        TML_DEBUG_LN("[MODULE] Lexer errors in " << file_path << ":");
        for (const auto& error : lex.errors()) {
            TML_DEBUG_LN("  " << error.message);
        }
        return false;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = std::filesystem::path(file_path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        TML_DEBUG_LN("[MODULE] Parse errors in " << file_path << ":");
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            TML_DEBUG_LN("  " << error.message);
        }
        return false;
    }

    const auto& parsed_module = std::get<parser::Module>(parse_result);

    // Create module structure and extract declarations
    Module mod;
    mod.name = module_path;

    SourceSpan builtin_span{};

    // Helper function to resolve simple types (primitive types for now)
    std::function<types::TypePtr(const parser::Type&)> resolve_simple_type;
    resolve_simple_type = [this, &resolve_simple_type](const parser::Type& type) -> types::TypePtr {
        if (type.is<parser::NamedType>()) {
            const auto& named = type.as<parser::NamedType>();
            const std::string& name = named.path.segments.empty() ? "" : named.path.segments[0];

            // Primitive types
            if (name == "I8") return make_primitive(PrimitiveKind::I8);
            if (name == "I16") return make_primitive(PrimitiveKind::I16);
            if (name == "I32") return make_primitive(PrimitiveKind::I32);
            if (name == "I64") return make_primitive(PrimitiveKind::I64);
            if (name == "I128") return make_primitive(PrimitiveKind::I128);
            if (name == "U8") return make_primitive(PrimitiveKind::U8);
            if (name == "U16") return make_primitive(PrimitiveKind::U16);
            if (name == "U32") return make_primitive(PrimitiveKind::U32);
            if (name == "U64") return make_primitive(PrimitiveKind::U64);
            if (name == "U128") return make_primitive(PrimitiveKind::U128);
            if (name == "F32") return make_primitive(PrimitiveKind::F32);
            if (name == "F64") return make_primitive(PrimitiveKind::F64);
            if (name == "Bool") return make_primitive(PrimitiveKind::Bool);
            if (name == "Char") return make_primitive(PrimitiveKind::Char);
            if (name == "Str") return make_primitive(PrimitiveKind::Str);
            if (name == "Unit") return make_unit();

            // Collection types
            if (name == "List") return std::make_shared<Type>(Type{NamedType{"List", "", {}}});
            if (name == "HashMap") return std::make_shared<Type>(Type{NamedType{"HashMap", "", {}}});
            if (name == "Buffer") return std::make_shared<Type>(Type{NamedType{"Buffer", "", {}}});
        }
        else if (type.is<parser::RefType>()) {
            const auto& ref = type.as<parser::RefType>();
            auto inner = resolve_simple_type(*ref.inner);
            return std::make_shared<Type>(Type{RefType{ref.is_mut, inner}});
        }

        // Fallback: return I32 for unknown types
        TML_DEBUG_LN("[MODULE] Warning: Could not resolve type, using I32 as fallback");
        return make_primitive(PrimitiveKind::I32);
    };

    // Extract public function declarations (especially lowlevel functions)
    for (const auto& decl : parsed_module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();

            // Only include public functions
            if (func.vis != parser::Visibility::Public) {
                continue;
            }

            // Convert parameter types
            std::vector<types::TypePtr> param_types;
            for (const auto& param : func.params) {
                // param.type is parser::TypePtr (std::unique_ptr<parser::Type>)
                if (param.type) {
                    param_types.push_back(resolve_simple_type(*param.type));
                }
            }

            // Convert return type
            types::TypePtr return_type;
            if (func.return_type.has_value()) {
                // func.return_type is std::optional<parser::TypePtr>
                const auto& type_ptr = func.return_type.value();
                return_type = resolve_simple_type(*type_ptr);
            } else {
                return_type = make_unit();
            }

            // Create function signature
            FuncSig sig{
                func.name,
                param_types,
                return_type,
                {},     // type_params
                false,  // is_async
                builtin_span,
                StabilityLevel::Stable,
                "",     // deprecated_message
                "1.0",  // since_version
                {},     // where_constraints
                func.is_unsafe  // is_lowlevel (lowlevel keyword maps to is_unsafe in parser)
            };

            mod.functions[func.name] = sig;
        }
        // TODO: Extract structs, enums, type aliases, etc.
    }

    // Check if module has any pure TML functions (non-lowlevel)
    for (const auto& decl : parsed_module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            if (func.vis == parser::Visibility::Public && !func.is_unsafe && func.body.has_value()) {
                mod.has_pure_tml_functions = true;
                break;
            }
        }
    }

    // Store source code if module has pure TML functions (for re-parsing in codegen)
    if (mod.has_pure_tml_functions) {
        mod.source_code = source_code;
        mod.file_path = file_path;
    }

    // Register the module
    TML_DEBUG_LN("[MODULE] Loaded " << module_path << " from " << file_path
              << " (" << mod.functions.size() << " functions)");
    module_registry_->register_module(module_path, std::move(mod));
    return true;
}

bool TypeEnv::load_native_module(const std::string& module_path) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true;  // Already loaded
    }

    // Special case: test module (keeps hardcoded init for now)
    if (module_path == "test") {
        init_test_module();
        return true;
    }

    // Core library modules - load from filesystem
    if (module_path.substr(0, 6) == "core::") {
        // Extract module name: "core::mem" -> "mem"
        std::string module_name = module_path.substr(6);

        // Try multiple possible paths for the module file
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("packages") / "core" / "src" / (module_name + ".tml"),  // From project root
            std::filesystem::path("..") / ".." / "core" / "src" / (module_name + ".tml"),  // From build/
            std::filesystem::path("..") / "packages" / "core" / "src" / (module_name + ".tml"),  // From tests/
            std::filesystem::path("core") / "src" / (module_name + ".tml")  // From packages/
        };

        for (const auto& module_file : search_paths) {
            if (std::filesystem::exists(module_file)) {
                TML_DEBUG_LN("[MODULE] Found core module at: " << module_file);
                return load_module_from_file(module_path, module_file.string());
            }
        }

        // Module file not found - this is a real error, not debug
        std::cerr << "error: Core module file not found: " << module_path << "\n";
        return false;
    }

    // Standard library modules - load from filesystem
    if (module_path.substr(0, 5) == "std::") {
        // Extract module name: "std::math" -> "math"
        std::string module_name = module_path.substr(5);

        // Try multiple possible paths for the module file
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("packages") / "std" / "src" / (module_name + ".tml"),  // From project root
            std::filesystem::path("..") / ".." / "std" / "src" / (module_name + ".tml"),  // From build/
            std::filesystem::path("..") / "packages" / "std" / "src" / (module_name + ".tml"),  // From tests/
            std::filesystem::path("std") / "src" / (module_name + ".tml")  // From packages/
        };

        for (const auto& module_file : search_paths) {
            if (std::filesystem::exists(module_file)) {
                TML_DEBUG_LN("[MODULE] Found module at: " << module_file);
                return load_module_from_file(module_path, module_file.string());
            }
        }

        // Module file not found - this is a real error, not debug
        std::cerr << "error: Module file not found: " << module_path << "\n";
        return false;
    }

    // Not a std:: module, will be loaded from file system later
    return false;
}

} // namespace tml::types
