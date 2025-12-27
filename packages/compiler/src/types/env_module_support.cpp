#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/env.hpp"
#include "types/module.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
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
        .visibility = parser::Visibility::Public // Imported symbols are accessible
    };

    // Store the import
    imported_symbols_[local_name] = import;
}

void TypeEnv::import_all_from(const std::string& module_path) {
    if (!module_registry_) {
        return; // No module registry available
    }

    auto module = module_registry_->get_module(module_path);
    if (!module) {
        return; // Module not found
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

auto TypeEnv::resolve_imported_symbol(const std::string& name) const -> std::optional<std::string> {
    auto it = imported_symbols_.find(name);
    if (it != imported_symbols_.end()) {
        // Return the full qualified name: module_path::original_name
        return it->second.module_path + "::" + it->second.original_name;
    }
    return std::nullopt;
}

auto TypeEnv::all_imports() const -> const std::unordered_map<std::string, ImportedSymbol>& {
    return imported_symbols_;
}

// Helper to parse a single TML file and extract public functions
static std::optional<std::pair<std::vector<parser::DeclPtr>, std::string>>
parse_tml_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return std::nullopt;
    }

    std::string source_code((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    auto source = lexer::Source::from_string(source_code, file_path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        return std::nullopt;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = std::filesystem::path(file_path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        return std::nullopt;
    }

    auto parsed_module = std::get<parser::Module>(std::move(parse_result));
    return std::make_pair(std::move(parsed_module.decls), source_code);
}

bool TypeEnv::load_module_from_file(const std::string& module_path, const std::string& file_path) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true; // Already loaded
    }

    // Collect all declarations to process
    std::vector<std::pair<std::vector<parser::DeclPtr>, std::string>> all_parsed;

    // Check if this is a mod.tml file - if so, load all sibling .tml files
    auto fs_path = std::filesystem::path(file_path);
    TML_DEBUG_LN("[MODULE] load_module_from_file: " << file_path << " (stem: " << fs_path.stem()
                                                    << ")");
    if (fs_path.stem() == "mod") {
        auto dir = fs_path.parent_path();
        TML_DEBUG_LN("[MODULE] Loading directory module from: " << dir);

        // Load all .tml files in the directory (except mod.tml itself)
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                auto entry_path = entry.path().string();
                TML_DEBUG_LN("[MODULE]   Parsing: " << entry.path().filename());
                auto parsed = parse_tml_file(entry_path);
                if (parsed) {
                    TML_DEBUG_LN("[MODULE]   OK: " << entry.path().filename());
                    all_parsed.push_back(std::move(*parsed));
                } else {
                    TML_DEBUG_LN("[MODULE]   FAILED: " << entry.path().filename());
                }
            }
        }
    } else {
        // Single file module
        auto parsed = parse_tml_file(file_path);
        if (!parsed) {
            TML_DEBUG_LN("[MODULE] Failed to parse: " << file_path);
            return false;
        }
        all_parsed.push_back(std::move(*parsed));
    }

    if (all_parsed.empty()) {
        TML_DEBUG_LN("[MODULE] No valid files found for: " << module_path);
        return false;
    }
    TML_DEBUG_LN("[MODULE] Parsed " << all_parsed.size() << " files for module: " << module_path);

    // Create module structure and extract declarations
    Module mod;
    mod.name = module_path;

    SourceSpan builtin_span{};

    // Helper function to resolve simple types (primitive types for now)
    std::function<types::TypePtr(const parser::Type&)> resolve_simple_type;
    resolve_simple_type = [&resolve_simple_type](const parser::Type& type) -> types::TypePtr {
        if (type.is<parser::NamedType>()) {
            const auto& named = type.as<parser::NamedType>();
            const std::string& name = named.path.segments.empty() ? "" : named.path.segments[0];

            // Primitive types
            if (name == "I8")
                return make_primitive(PrimitiveKind::I8);
            if (name == "I16")
                return make_primitive(PrimitiveKind::I16);
            if (name == "I32")
                return make_primitive(PrimitiveKind::I32);
            if (name == "I64")
                return make_primitive(PrimitiveKind::I64);
            if (name == "I128")
                return make_primitive(PrimitiveKind::I128);
            if (name == "U8")
                return make_primitive(PrimitiveKind::U8);
            if (name == "U16")
                return make_primitive(PrimitiveKind::U16);
            if (name == "U32")
                return make_primitive(PrimitiveKind::U32);
            if (name == "U64")
                return make_primitive(PrimitiveKind::U64);
            if (name == "U128")
                return make_primitive(PrimitiveKind::U128);
            if (name == "F32")
                return make_primitive(PrimitiveKind::F32);
            if (name == "F64")
                return make_primitive(PrimitiveKind::F64);
            if (name == "Bool")
                return make_primitive(PrimitiveKind::Bool);
            if (name == "Char")
                return make_primitive(PrimitiveKind::Char);
            if (name == "Str")
                return make_primitive(PrimitiveKind::Str);
            if (name == "Unit")
                return make_unit();

            // Collection types
            if (name == "List")
                return std::make_shared<Type>(Type{NamedType{"List", "", {}}});
            if (name == "HashMap")
                return std::make_shared<Type>(Type{NamedType{"HashMap", "", {}}});
            if (name == "Buffer")
                return std::make_shared<Type>(Type{NamedType{"Buffer", "", {}}});

            // std::file types
            if (name == "File")
                return std::make_shared<Type>(Type{NamedType{"File", "std::file", {}}});
            if (name == "Path")
                return std::make_shared<Type>(Type{NamedType{"Path", "std::file", {}}});

            // Other non-primitive types - resolve any generic type arguments
            std::vector<TypePtr> type_args;
            if (named.generics.has_value()) {
                for (const auto& arg : named.generics->args) {
                    type_args.push_back(resolve_simple_type(*arg));
                }
            }
            return std::make_shared<Type>(Type{NamedType{name, "", std::move(type_args)}});
        } else if (type.is<parser::RefType>()) {
            const auto& ref = type.as<parser::RefType>();
            auto inner = resolve_simple_type(*ref.inner);
            return std::make_shared<Type>(Type{RefType{ref.is_mut, inner}});
        } else if (type.is<parser::FuncType>()) {
            const auto& func_type = type.as<parser::FuncType>();
            std::vector<TypePtr> param_types;
            for (const auto& param : func_type.params) {
                param_types.push_back(resolve_simple_type(*param));
            }
            TypePtr return_type = make_unit();
            if (func_type.return_type) {
                return_type = resolve_simple_type(*func_type.return_type);
            }
            return std::make_shared<Type>(
                Type{FuncType{std::move(param_types), return_type, false}});
        }

        // Fallback: return I32 for unknown types
        TML_DEBUG_LN("[MODULE] Warning: Could not resolve type, using I32 as fallback");
        return make_primitive(PrimitiveKind::I32);
    };

    // Extract public declarations from all parsed files
    for (const auto& [decls, source_code] : all_parsed) {
        for (const auto& decl : decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();

                // Only include public functions
                if (func.vis != parser::Visibility::Public) {
                    continue;
                }

                // Convert parameter types
                std::vector<types::TypePtr> param_types;
                for (const auto& param : func.params) {
                    if (param.type) {
                        param_types.push_back(resolve_simple_type(*param.type));
                    }
                }

                // Convert return type
                types::TypePtr return_type;
                if (func.return_type.has_value()) {
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
                    {},    // type_params
                    false, // is_async
                    builtin_span,
                    StabilityLevel::Stable,
                    "",            // deprecated_message
                    "1.0",         // since_version
                    {},            // where_constraints
                    func.is_unsafe // is_lowlevel
                };

                mod.functions[func.name] = sig;
            } else if (decl->is<parser::StructDecl>()) {
                const auto& struct_decl = decl->as<parser::StructDecl>();

                // Only include public structs
                if (struct_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Convert fields
                std::vector<std::pair<std::string, TypePtr>> fields;
                for (const auto& field : struct_decl.fields) {
                    if (field.type) {
                        fields.emplace_back(field.name, resolve_simple_type(*field.type));
                    }
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : struct_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Create struct definition
                StructDef struct_def{struct_decl.name, std::move(type_params), std::move(fields),
                                     struct_decl.span};

                mod.structs[struct_decl.name] = std::move(struct_def);
                TML_DEBUG_LN("[MODULE] Registered struct: " << struct_decl.name << " in module "
                                                            << module_path);
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& enum_decl = decl->as<parser::EnumDecl>();

                // Only include public enums
                if (enum_decl.vis != parser::Visibility::Public) {
                    continue;
                }

                // Convert variants
                std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
                for (const auto& variant : enum_decl.variants) {
                    std::vector<TypePtr> payload_types;
                    // Handle tuple fields (e.g., Some(T))
                    if (variant.tuple_fields.has_value()) {
                        for (const auto& type_ptr : variant.tuple_fields.value()) {
                            payload_types.push_back(resolve_simple_type(*type_ptr));
                        }
                    }
                    // Handle struct fields (e.g., Point { x: I32, y: I32 })
                    if (variant.struct_fields.has_value()) {
                        for (const auto& field : variant.struct_fields.value()) {
                            if (field.type) {
                                payload_types.push_back(resolve_simple_type(*field.type));
                            }
                        }
                    }
                    variants.emplace_back(variant.name, std::move(payload_types));
                }

                // Extract type params
                std::vector<std::string> type_params;
                for (const auto& param : enum_decl.generics) {
                    type_params.push_back(param.name);
                }

                // Create enum definition
                EnumDef enum_def{enum_decl.name, std::move(type_params), std::move(variants),
                                 enum_decl.span};

                mod.enums[enum_decl.name] = std::move(enum_def);
                TML_DEBUG_LN("[MODULE] Registered enum: " << enum_decl.name << " in module "
                                                          << module_path);
            } else if (decl->is<parser::ImplDecl>()) {
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Get the type name being implemented from self_type
                std::string type_name;
                if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                    const auto& named = impl_decl.self_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }

                if (type_name.empty()) {
                    continue; // Skip if we couldn't determine the type name
                }

                // Extract methods from impl block (methods is std::vector<FuncDecl>)
                for (const auto& func : impl_decl.methods) {
                    // Only include public methods
                    if (func.vis != parser::Visibility::Public) {
                        continue;
                    }

                    // Convert parameter types
                    std::vector<types::TypePtr> param_types;
                    for (const auto& param : func.params) {
                        if (param.type) {
                            param_types.push_back(resolve_simple_type(*param.type));
                        }
                    }

                    // Convert return type
                    types::TypePtr return_type;
                    if (func.return_type.has_value()) {
                        return_type = resolve_simple_type(*func.return_type.value());
                    } else {
                        return_type = make_unit();
                    }

                    // Create qualified function name: Type::method
                    std::string qualified_name = type_name + "::" + func.name;

                    FuncSig sig{
                        qualified_name,
                        param_types,
                        return_type,
                        {},    // type_params
                        false, // is_async
                        builtin_span,
                        StabilityLevel::Stable,
                        "",            // deprecated_message
                        "1.0",         // since_version
                        {},            // where_constraints
                        func.is_unsafe // is_lowlevel
                    };

                    mod.functions[qualified_name] = sig;
                    TML_DEBUG_LN("[MODULE] Registered impl method: "
                                 << qualified_name << " in module " << module_path);
                }
            }
        }
    }

    // Check if module has any pure TML functions and collect source code
    std::string combined_source;
    for (const auto& [decls, src] : all_parsed) {
        for (const auto& decl : decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                if (func.vis == parser::Visibility::Public && !func.is_unsafe &&
                    func.body.has_value()) {
                    mod.has_pure_tml_functions = true;
                }
            }
        }
        if (!src.empty()) {
            combined_source += src + "\n";
        }
    }

    // Store source code if module has pure TML functions (for re-parsing in codegen)
    if (mod.has_pure_tml_functions) {
        mod.source_code = combined_source;
        mod.file_path = file_path;
    }

    // Register the module
    TML_DEBUG_LN("[MODULE] Loaded " << module_path << " from " << file_path << " ("
                                    << mod.functions.size() << " functions)");
    module_registry_->register_module(module_path, std::move(mod));
    return true;
}

bool TypeEnv::load_native_module(const std::string& module_path) {
    if (!module_registry_) {
        return false;
    }

    // Check if module is already registered
    if (module_registry_->has_module(module_path)) {
        return true; // Already loaded
    }

    // Test module - load from packages/test/
    // Note: We prioritize assertions/mod.tml since mod.tml uses `pub use` re-exports
    // which aren't fully supported yet
    if (module_path == "test") {
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("packages") / "test" / "src" / "assertions" / "mod.tml",
            std::filesystem::path("packages") / "test" / "src" / "mod.tml",
            std::filesystem::path("..") / ".." / "test" / "src" / "assertions" /
                "mod.tml", // From build/
            std::filesystem::path("..") / "packages" / "test" / "src" / "assertions" /
                "mod.tml", // From tests/
        };

        for (const auto& module_file : search_paths) {
            if (std::filesystem::exists(module_file)) {
                TML_DEBUG_LN("[MODULE] Found test module at: " << module_file);
                return load_module_from_file(module_path, module_file.string());
            }
        }

        std::cerr << "error: Test module file not found\n";
        return false;
    }

    // Core library modules - load from filesystem
    if (module_path.substr(0, 6) == "core::") {
        // Extract module name: "core::mem" -> "mem"
        std::string module_name = module_path.substr(6);

        // Try multiple possible paths for the module file
        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::path("packages") / "core" / "src" / (module_name + ".tml"),
            std::filesystem::path("packages") / "core" / "src" / module_name / "mod.tml",
            std::filesystem::path("..") / ".." / "core" / "src" /
                (module_name + ".tml"), // From build/
            std::filesystem::path("..") / "packages" / "core" / "src" /
                (module_name + ".tml"),                                    // From tests/
            std::filesystem::path("core") / "src" / (module_name + ".tml") // From packages/
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
            std::filesystem::path("packages") / "std" / "src" / (module_name + ".tml"),
            std::filesystem::path("packages") / "std" / "src" / module_name / "mod.tml",
            std::filesystem::path("..") / ".." / "std" / "src" /
                (module_name + ".tml"), // From build/
            std::filesystem::path("..") / ".." / "std" / "src" / module_name /
                "mod.tml", // From build/
            std::filesystem::path("..") / "packages" / "std" / "src" /
                (module_name + ".tml"), // From tests/
            std::filesystem::path("..") / "packages" / "std" / "src" / module_name /
                "mod.tml",                                                  // From tests/
            std::filesystem::path("std") / "src" / (module_name + ".tml"),  // From packages/
            std::filesystem::path("std") / "src" / module_name / "mod.tml", // From packages/
            // Absolute fallback
            std::filesystem::path("F:/Node/hivellm/tml/packages/std/src") / (module_name + ".tml"),
            std::filesystem::path("F:/Node/hivellm/tml/packages/std/src") / module_name / "mod.tml",
        };

        TML_DEBUG_LN("[MODULE] Looking for std module: " << module_path << " (name: " << module_name
                                                         << ")");
        for (const auto& module_file : search_paths) {
            TML_DEBUG_LN("[MODULE]   Checking: " << module_file);
            if (std::filesystem::exists(module_file)) {
                TML_DEBUG_LN("[MODULE]   FOUND!");
                return load_module_from_file(module_path, module_file.string());
            }
        }

        // Module file not found
        std::cerr << "error: std module file not found: " << module_path << "\n";
        return false;
    }

    // Not a std:: module, will be loaded from file system later
    return false;
}

} // namespace tml::types
