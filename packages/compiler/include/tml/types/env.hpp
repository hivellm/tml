#ifndef TML_TYPES_ENV_HPP
#define TML_TYPES_ENV_HPP

#include "tml/types/env_stability.hpp"
#include "tml/types/type.hpp"
#include "tml/types/module.hpp"
#include <unordered_map>
#include <string>
#include <optional>
#include <vector>
#include <memory>

namespace tml::types
{

    // Symbol information
    struct Symbol
    {
        std::string name;
        TypePtr type;
        bool is_mutable;
        SourceSpan span;
    };

    // Where clause constraint: type parameter -> required behaviors
    struct WhereConstraint
    {
        std::string type_param;
        std::vector<std::string> required_behaviors;
    };

    // Function signature with stability tracking
    struct FuncSig
    {
        std::string name;
        std::vector<TypePtr> params;
        TypePtr return_type;
        std::vector<std::string> type_params;
        bool is_async;
        SourceSpan span;
        StabilityLevel stability = StabilityLevel::Unstable;
        std::string deprecated_message = {};                 // Migration guide for deprecated functions
        std::string since_version = {};                      // Version when this status was assigned
        std::vector<WhereConstraint> where_constraints = {}; // At end to not break existing code
        bool is_lowlevel = false;                            // For C runtime functions

        // Helper methods
        [[nodiscard]] bool is_stable() const { return stability == StabilityLevel::Stable; }
        [[nodiscard]] bool is_deprecated() const { return stability == StabilityLevel::Deprecated; }
        [[nodiscard]] bool is_unstable() const { return stability == StabilityLevel::Unstable; }
    };

    // Struct definition
    struct StructDef
    {
        std::string name;
        std::vector<std::string> type_params;
        std::vector<std::pair<std::string, TypePtr>> fields;
        SourceSpan span;
    };

    // Enum definition
    struct EnumDef
    {
        std::string name;
        std::vector<std::string> type_params;
        std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
        SourceSpan span;
    };

    // Behavior (trait) definition
    struct BehaviorDef
    {
        std::string name;
        std::vector<std::string> type_params;
        std::vector<FuncSig> methods;
        std::vector<std::string> super_behaviors;
        SourceSpan span;
    };

    // Scope for local variables
    class Scope
    {
    public:
        Scope() = default;
        explicit Scope(std::shared_ptr<Scope> parent);

        void define(const std::string &name, TypePtr type, bool is_mutable, SourceSpan span);
        [[nodiscard]] auto lookup(const std::string &name) const -> std::optional<Symbol>;
        [[nodiscard]] auto lookup_local(const std::string &name) const -> std::optional<Symbol>;
        [[nodiscard]] auto parent() const -> std::shared_ptr<Scope>;

    private:
        std::unordered_map<std::string, Symbol> symbols_;
        std::shared_ptr<Scope> parent_;
    };

    // Type environment for the entire module
    class TypeEnv
    {
    public:
        TypeEnv();

        // Type definitions
        void define_struct(StructDef def);
        void define_enum(EnumDef def);
        void define_behavior(BehaviorDef def);
        void define_func(FuncSig sig);
        void define_type_alias(const std::string &name, TypePtr type);

        [[nodiscard]] auto lookup_struct(const std::string &name) const -> std::optional<StructDef>;
        [[nodiscard]] auto lookup_enum(const std::string &name) const -> std::optional<EnumDef>;
        [[nodiscard]] auto lookup_behavior(const std::string &name) const -> std::optional<BehaviorDef>;
        [[nodiscard]] auto lookup_func(const std::string &name) const -> std::optional<FuncSig>;
        [[nodiscard]] auto lookup_type_alias(const std::string &name) const -> std::optional<TypePtr>;

        // Get all enums (for enum constructor lookup)
        [[nodiscard]] auto all_enums() const -> const std::unordered_map<std::string, EnumDef> &;

        // Scopes
        void push_scope();
        void pop_scope();
        [[nodiscard]] auto current_scope() -> std::shared_ptr<Scope>;

        // Type variable management
        [[nodiscard]] auto fresh_type_var() -> TypePtr;
        void unify(TypePtr a, TypePtr b);
        [[nodiscard]] auto resolve(TypePtr type) -> TypePtr;

        // Builtin types
        [[nodiscard]] auto builtin_types() const -> const std::unordered_map<std::string, TypePtr> &;
        // Module support
        void set_module_registry(std::shared_ptr<ModuleRegistry> registry);
        void set_current_module(const std::string &module_path);
        [[nodiscard]] auto module_registry() const -> std::shared_ptr<ModuleRegistry>;
        [[nodiscard]] auto current_module() const -> const std::string &;

        // Import management
        void import_symbol(const std::string &module_path, const std::string &symbol_name,
                           std::optional<std::string> alias = std::nullopt);
        void import_all_from(const std::string &module_path);

        // Symbol resolution with imports
        [[nodiscard]] auto resolve_imported_symbol(const std::string &name) const
            -> std::optional<std::string /* full path */>;

        // Module lookup
        [[nodiscard]] auto get_module(const std::string &module_path) const -> std::optional<Module>;

        // Load native module on demand
        bool load_native_module(const std::string& module_path);

        // Load module from TML file
        bool load_module_from_file(const std::string& module_path, const std::string& file_path);

    private:
        std::unordered_map<std::string, StructDef> structs_;
        std::unordered_map<std::string, EnumDef> enums_;
        std::unordered_map<std::string, BehaviorDef> behaviors_;
        std::unordered_map<std::string, FuncSig> functions_;
        std::unordered_map<std::string, TypePtr> type_aliases_;
        std::unordered_map<std::string, TypePtr> builtins_;

        std::shared_ptr<Scope> current_scope_;
        uint32_t type_var_counter_ = 0;
        std::unordered_map<uint32_t, TypePtr> substitutions_;
        // Module system
        std::shared_ptr<ModuleRegistry> module_registry_;
        std::string current_module_path_;
        std::unordered_map<std::string, ImportedSymbol> imported_symbols_; // local_name -> import info

        void init_builtins();
        void init_test_module();
        // NOTE: init_std_*_module() functions removed - modules now load from .tml files
    };

} // namespace tml::types

#endif // TML_TYPES_ENV_HPP
