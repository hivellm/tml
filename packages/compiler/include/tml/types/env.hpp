#ifndef TML_TYPES_ENV_HPP
#define TML_TYPES_ENV_HPP

#include "tml/types/type.hpp"
#include <unordered_map>
#include <string>
#include <optional>
#include <vector>

namespace tml::types {

// Symbol information
struct Symbol {
    std::string name;
    TypePtr type;
    bool is_mutable;
    SourceSpan span;
};

// Function signature
struct FuncSig {
    std::string name;
    std::vector<TypePtr> params;
    TypePtr return_type;
    std::vector<std::string> type_params;
    bool is_async;
    SourceSpan span;
};

// Struct definition
struct StructDef {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<std::pair<std::string, TypePtr>> fields;
    SourceSpan span;
};

// Enum definition
struct EnumDef {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
    SourceSpan span;
};

// Behavior (trait) definition
struct BehaviorDef {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<FuncSig> methods;
    std::vector<std::string> super_behaviors;
    SourceSpan span;
};

// Scope for local variables
class Scope {
public:
    Scope() = default;
    explicit Scope(std::shared_ptr<Scope> parent);

    void define(const std::string& name, TypePtr type, bool is_mutable, SourceSpan span);
    [[nodiscard]] auto lookup(const std::string& name) const -> std::optional<Symbol>;
    [[nodiscard]] auto parent() const -> std::shared_ptr<Scope>;

private:
    std::unordered_map<std::string, Symbol> symbols_;
    std::shared_ptr<Scope> parent_;
};

// Type environment for the entire module
class TypeEnv {
public:
    TypeEnv();

    // Type definitions
    void define_struct(StructDef def);
    void define_enum(EnumDef def);
    void define_behavior(BehaviorDef def);
    void define_func(FuncSig sig);
    void define_type_alias(const std::string& name, TypePtr type);

    [[nodiscard]] auto lookup_struct(const std::string& name) const -> std::optional<StructDef>;
    [[nodiscard]] auto lookup_enum(const std::string& name) const -> std::optional<EnumDef>;
    [[nodiscard]] auto lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef>;
    [[nodiscard]] auto lookup_func(const std::string& name) const -> std::optional<FuncSig>;
    [[nodiscard]] auto lookup_type_alias(const std::string& name) const -> std::optional<TypePtr>;

    // Scopes
    void push_scope();
    void pop_scope();
    [[nodiscard]] auto current_scope() -> std::shared_ptr<Scope>;

    // Type variable management
    [[nodiscard]] auto fresh_type_var() -> TypePtr;
    void unify(TypePtr a, TypePtr b);
    [[nodiscard]] auto resolve(TypePtr type) -> TypePtr;

    // Builtin types
    [[nodiscard]] auto builtin_types() const -> const std::unordered_map<std::string, TypePtr>&;

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

    void init_builtins();
};

} // namespace tml::types

#endif // TML_TYPES_ENV_HPP
