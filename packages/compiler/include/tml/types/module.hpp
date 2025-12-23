#ifndef TML_TYPES_MODULE_HPP
#define TML_TYPES_MODULE_HPP

#include "tml/types/type.hpp"
#include "tml/types/env_stability.hpp"
#include "tml/parser/ast.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>
#include <variant>
#include <memory>

namespace tml::types {

// Forward declarations
struct FuncSig;
struct StructDef;
struct EnumDef;
struct BehaviorDef;

// Represents a single module with its symbols
struct Module {
    std::string name;
    std::string file_path;  // Source file location

    // Symbol tables for this module
    std::unordered_map<std::string, FuncSig> functions;
    std::unordered_map<std::string, StructDef> structs;
    std::unordered_map<std::string, EnumDef> enums;
    std::unordered_map<std::string, BehaviorDef> behaviors;
    std::unordered_map<std::string, TypePtr> type_aliases;
    std::unordered_map<std::string, std::string> submodules;  // name -> path

    // Source code for pure TML modules (non-lowlevel)
    // Stored so codegen can re-parse and generate LLVM IR
    std::string source_code;
    bool has_pure_tml_functions = false;

    parser::Visibility default_visibility = parser::Visibility::Private;
};

// Tracks what symbols are imported into current scope
struct ImportedSymbol {
    std::string original_name;    // Name in source module
    std::string local_name;       // Name in current scope (after 'as')
    std::string module_path;      // Full module path (e.g., "std::io")
    parser::Visibility visibility;
};

// Union type for any symbol that can be imported
using ModuleSymbol = std::variant<FuncSig, StructDef, EnumDef, BehaviorDef, TypePtr>;

// Central registry for all modules in the program
class ModuleRegistry {
public:
    ModuleRegistry() = default;

    // Module registration
    void register_module(const std::string& path, Module module);
    auto get_module(const std::string& path) const -> std::optional<Module>;
    auto get_module_mut(const std::string& path) -> Module*;
    bool has_module(const std::string& path) const;

    // List all registered modules
    auto list_modules() const -> std::vector<std::string>;

    // Get all modules
    auto get_all_modules() const -> const std::unordered_map<std::string, Module>& {
        return modules_;
    }

    // File-based module resolution
    auto resolve_file_module(const std::string& path) const -> std::optional<std::string>;
    void register_file_mapping(const std::string& file_path, const std::string& module_path);

    // Symbol lookup across modules
    auto lookup_function(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<FuncSig>;
    auto lookup_struct(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<StructDef>;
    auto lookup_enum(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<EnumDef>;
    auto lookup_behavior(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<BehaviorDef>;
    auto lookup_type_alias(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<TypePtr>;

    // Generic symbol lookup
    auto lookup_symbol(const std::string& module_path, const std::string& symbol_name) const
        -> std::optional<ModuleSymbol>;

private:
    std::unordered_map<std::string, Module> modules_;
    std::unordered_map<std::string, std::string> file_to_module_;  // foo.tml -> foo
};

} // namespace tml::types

#endif // TML_TYPES_MODULE_HPP
