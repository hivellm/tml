TML_MODULE("compiler")

//! # Type Checker - Struct and Enum Registration
//!
//! This file implements struct and enum type registration with @derive support.
//!
//! ## @derive Support
//!
//! | Derive Macro   | Method Generated                           |
//! |----------------|---------------------------------------------|
//! | Reflect        | type_info(), runtime_type_info()            |
//! | PartialEq      | eq()                                        |
//! | Duplicate      | duplicate()                                 |
//! | Hash           | hash()                                      |
//! | Default        | default()                                   |
//! | PartialOrd     | partial_cmp()                               |
//! | Ord            | cmp()                                       |
//! | Debug          | debug_string()                              |
//! | Display        | to_string()                                 |
//! | Serialize      | to_json()                                   |
//! | Deserialize    | from_json()                                 |
//! | FromStr        | from_str()                                  |

#include "types/checker.hpp"

#include <set>

namespace tml::types {

// Reserved type names - primitive types that cannot be redefined by user code
// Only language primitives are reserved - library types like Maybe, List can be shadowed
static const std::set<std::string> RESERVED_TYPE_NAMES = {
    // Primitive types
    "I8",
    "I16",
    "I32",
    "I64",
    "I128",
    "U8",
    "U16",
    "U32",
    "U64",
    "U128",
    "F32",
    "F64",
    "Bool",
    "Char",
    "Str",
    "Unit",
    "Never",
    // String builder
    "StringBuilder",
    // Async types
    "Future",
    "Context",
    "Waker",
};

void TypeChecker::register_struct_decl(const parser::StructDecl& decl) {
    // Check if the type name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<StructFieldDef> fields;
    for (const auto& field : decl.fields) {
        StructFieldDef field_def;
        field_def.name = field.name;
        field_def.type = resolve_type(*field.type);
        field_def.has_default = field.default_value.has_value();

        // Validate default value type if present - check_expr reports type mismatch errors
        if (field.default_value.has_value()) {
            check_expr(**field.default_value, field_def.type);
        }

        fields.push_back(std::move(field_def));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            type_params.push_back(param.name);
        }
    }

    // Extract const generic parameters
    std::vector<ConstGenericParam> const_params = extract_const_params(decl.generics);

    // Use qualified name for namespaced types
    std::string full_name = qualified_name(decl.name);

    // Check for decorators
    bool is_interior_mutable = false;
    bool has_derive_reflect = false;
    bool has_derive_partial_eq = false;
    bool has_derive_duplicate = false;
    bool has_derive_hash = false;
    bool has_derive_default = false;
    bool has_derive_partial_ord = false;
    bool has_derive_ord = false;
    bool has_derive_debug = false;
    bool has_derive_display = false;
    bool has_derive_serialize = false;
    bool has_derive_deserialize = false;
    bool has_derive_fromstr = false;
    for (const auto& decorator : decl.decorators) {
        if (decorator.name == "interior_mutable") {
            is_interior_mutable = true;
        } else if (decorator.name == "derive") {
            // Check for @derive arguments
            for (const auto& arg : decorator.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Reflect") {
                        has_derive_reflect = true;
                    } else if (name == "PartialEq" || name == "Eq") {
                        has_derive_partial_eq = true;
                    } else if (name == "Duplicate" || name == "Copy") {
                        has_derive_duplicate = true;
                    } else if (name == "Hash") {
                        has_derive_hash = true;
                    } else if (name == "Default") {
                        has_derive_default = true;
                    } else if (name == "PartialOrd") {
                        has_derive_partial_ord = true;
                    } else if (name == "Ord") {
                        has_derive_ord = true;
                    } else if (name == "Debug") {
                        has_derive_debug = true;
                    } else if (name == "Display") {
                        has_derive_display = true;
                    } else if (name == "Serialize") {
                        has_derive_serialize = true;
                    } else if (name == "Deserialize") {
                        has_derive_deserialize = true;
                    } else if (name == "FromStr") {
                        has_derive_fromstr = true;
                    }
                }
            }
        }
    }

    env_.define_struct(StructDef{.name = full_name,
                                 .type_params = std::move(type_params),
                                 .const_params = std::move(const_params),
                                 .fields = std::move(fields),
                                 .span = decl.span,
                                 .is_interior_mutable = is_interior_mutable});

    // Handle @derive(Reflect) - register impl and type_info method
    // Skip generic types - they need instantiation first
    if (has_derive_reflect && decl.generics.empty()) {
        // Register that this type implements Reflect behavior
        env_.register_impl(full_name, "Reflect");

        // Create return type: ref TypeInfo
        auto type_info_type = std::make_shared<Type>();
        type_info_type->kind = NamedType{"TypeInfo", "", {}};
        auto ref_type_info = std::make_shared<Type>();
        ref_type_info->kind = RefType{false, type_info_type, std::nullopt};

        // Create self type: ref TypeName
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register TypeName::type_info() -> ref TypeInfo (static method)
        std::string static_method = full_name + "::type_info";
        env_.define_func(FuncSig{.name = static_method,
                                 .params = {},
                                 .return_type = ref_type_info,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Register TypeName::runtime_type_info(ref this) -> ref TypeInfo (instance method)
        std::string instance_method = full_name + "::runtime_type_info";
        env_.define_func(FuncSig{.name = instance_method,
                                 .params = {ref_self},
                                 .return_type = ref_type_info,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(PartialEq) - register impl and eq method
    // Skip generic types - they need instantiation first
    if (has_derive_partial_eq && decl.generics.empty()) {
        // Register that this type implements PartialEq behavior
        env_.register_impl(full_name, "PartialEq");

        // Create return type: Bool
        auto bool_type = make_bool();

        // Create self type: ref TypeName
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register TypeName::eq(ref this, other: ref Self) -> Bool (instance method)
        std::string eq_method = full_name + "::eq";
        env_.define_func(FuncSig{.name = eq_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = bool_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Duplicate) - register impl and duplicate method
    // Skip generic types - they need instantiation first
    if (has_derive_duplicate && decl.generics.empty()) {
        // Register that this type implements Duplicate behavior
        env_.register_impl(full_name, "Duplicate");

        // Create self type for return
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};

        // Create ref self type for parameter
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register TypeName::duplicate(ref this) -> Self (instance method)
        std::string dup_method = full_name + "::duplicate";
        env_.define_func(FuncSig{.name = dup_method,
                                 .params = {ref_self},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Hash) - register impl and hash method
    // Skip generic types - they need instantiation first
    if (has_derive_hash && decl.generics.empty()) {
        // Register that this type implements Hash behavior
        env_.register_impl(full_name, "Hash");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create I64 return type
        auto i64_type = types::make_i64();

        // Register TypeName::hash(ref this) -> I64 (instance method)
        std::string hash_method = full_name + "::hash";
        env_.define_func(FuncSig{.name = hash_method,
                                 .params = {ref_self},
                                 .return_type = i64_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Default) - register impl and default static method
    // Skip generic types - they need instantiation first
    if (has_derive_default && decl.generics.empty()) {
        // Register that this type implements Default behavior
        env_.register_impl(full_name, "Default");

        // Create self type for return
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};

        // Register TypeName::default() -> Self (static method, no params)
        std::string default_method = full_name + "::default";
        env_.define_func(FuncSig{.name = default_method,
                                 .params = {},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(PartialOrd) - register impl and partial_cmp method
    // Skip generic types - they need instantiation first
    if (has_derive_partial_ord && decl.generics.empty()) {
        // Register that this type implements PartialOrd behavior
        env_.register_impl(full_name, "PartialOrd");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Ordering type
        auto ordering_type = std::make_shared<Type>();
        ordering_type->kind = NamedType{"Ordering", "", {}};

        // Create Maybe[Ordering] return type
        auto maybe_ordering_type = std::make_shared<Type>();
        maybe_ordering_type->kind = NamedType{"Maybe", "", {ordering_type}};

        // Register TypeName::partial_cmp(ref this, other: ref Self) -> Maybe[Ordering]
        std::string partial_cmp_method = full_name + "::partial_cmp";
        env_.define_func(FuncSig{.name = partial_cmp_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = maybe_ordering_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Ord) - register impl and cmp method
    // Skip generic types - they need instantiation first
    if (has_derive_ord && decl.generics.empty()) {
        // Register that this type implements Ord behavior
        env_.register_impl(full_name, "Ord");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Ordering return type
        auto ordering_type = std::make_shared<Type>();
        ordering_type->kind = NamedType{"Ordering", "", {}};

        // Register TypeName::cmp(ref this, other: ref Self) -> Ordering
        std::string cmp_method = full_name + "::cmp";
        env_.define_func(FuncSig{.name = cmp_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = ordering_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Debug) - register impl and debug_string method
    // Skip generic types - they need instantiation first
    if (has_derive_debug && decl.generics.empty()) {
        // Register that this type implements Debug behavior
        env_.register_impl(full_name, "Debug");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register TypeName::debug_string(ref this) -> Str
        std::string debug_method = full_name + "::debug_string";
        env_.define_func(FuncSig{.name = debug_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Display) - register impl and to_string method
    // Skip generic types - they need instantiation first
    if (has_derive_display && decl.generics.empty()) {
        // Register that this type implements Display behavior
        env_.register_impl(full_name, "Display");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register TypeName::to_string(ref this) -> Str
        std::string to_string_method = full_name + "::to_string";
        env_.define_func(FuncSig{.name = to_string_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Serialize) - register impl and to_json method
    // Skip generic types - they need instantiation first
    if (has_derive_serialize && decl.generics.empty()) {
        // Register that this type implements Serialize behavior
        env_.register_impl(full_name, "Serialize");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register TypeName::to_json(ref this) -> Str
        std::string to_json_method = full_name + "::to_json";
        env_.define_func(FuncSig{.name = to_json_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Deserialize) - register impl and from_json static method
    // Skip generic types - they need instantiation first
    if (has_derive_deserialize && decl.generics.empty()) {
        // Register that this type implements Deserialize behavior
        env_.register_impl(full_name, "Deserialize");

        // Create self type for Outcome payload
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};

        // Create Str type for error and parameter
        auto str_type = types::make_str();

        // Create Outcome[Self, Str] return type
        auto outcome_type = std::make_shared<Type>();
        outcome_type->kind = NamedType{"Outcome", "", {self_type, str_type}};

        // Register TypeName::from_json(s: Str) -> Outcome[Self, Str] (static method)
        std::string from_json_method = full_name + "::from_json";
        env_.define_func(FuncSig{.name = from_json_method,
                                 .params = {str_type},
                                 .return_type = outcome_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(FromStr) - register impl and from_str static method
    // Skip generic types - they need instantiation first
    if (has_derive_fromstr && decl.generics.empty()) {
        // Register that this type implements FromStr behavior
        env_.register_impl(full_name, "FromStr");

        // Create self type for Outcome payload
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{full_name, "", {}};

        // Create Str type for error and parameter
        auto str_type = types::make_str();

        // Create Outcome[Self, Str] return type
        auto outcome_type = std::make_shared<Type>();
        outcome_type->kind = NamedType{"Outcome", "", {self_type, str_type}};

        // Register TypeName::from_str(s: Str) -> Outcome[Self, Str] (static method)
        std::string from_str_method = full_name + "::from_str";
        env_.define_func(FuncSig{.name = from_str_method,
                                 .params = {str_type},
                                 .return_type = outcome_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }
}

void TypeChecker::register_union_decl(const parser::UnionDecl& decl) {
    // Check if the type name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<StructFieldDef> fields;
    for (const auto& field : decl.fields) {
        StructFieldDef field_def;
        field_def.name = field.name;
        field_def.type = resolve_type(*field.type);
        field_def.has_default = false; // Unions don't support default values
        fields.push_back(std::move(field_def));
    }

    // Use qualified name for namespaced types
    std::string full_name = qualified_name(decl.name);

    // Create StructDef with is_union = true
    StructDef struct_def{
        .name = full_name,
        .type_params = {},  // Unions don't support generics
        .const_params = {}, // Unions don't support const generics
        .fields = std::move(fields),
        .span = decl.span,
        .is_interior_mutable = false,
        .is_union = true,
    };

    env_.define_struct(std::move(struct_def));
}

void TypeChecker::register_enum_decl(const parser::EnumDecl& decl) {
    // Check if the type name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
    for (const auto& variant : decl.variants) {
        std::vector<TypePtr> types;
        if (variant.tuple_fields) {
            for (const auto& type : *variant.tuple_fields) {
                types.push_back(resolve_type(*type));
            }
        }
        variants.emplace_back(variant.name, std::move(types));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            type_params.push_back(param.name);
        }
    }

    // Extract const generic parameters
    std::vector<ConstGenericParam> const_params = extract_const_params(decl.generics);

    // Check for @derive decorators
    bool has_derive_reflect = false;
    bool has_derive_partial_eq = false;
    bool has_derive_duplicate = false;
    bool has_derive_hash = false;
    bool has_derive_default = false;
    bool has_derive_partial_ord = false;
    bool has_derive_ord = false;
    bool has_derive_debug = false;
    bool has_derive_display = false;
    bool has_derive_serialize = false;
    bool has_derive_deserialize = false;
    bool has_derive_fromstr = false;
    for (const auto& decorator : decl.decorators) {
        if (decorator.name == "derive") {
            for (const auto& arg : decorator.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Reflect") {
                        has_derive_reflect = true;
                    } else if (name == "PartialEq" || name == "Eq") {
                        has_derive_partial_eq = true;
                    } else if (name == "Duplicate" || name == "Copy") {
                        has_derive_duplicate = true;
                    } else if (name == "Hash") {
                        has_derive_hash = true;
                    } else if (name == "Default") {
                        has_derive_default = true;
                    } else if (name == "PartialOrd") {
                        has_derive_partial_ord = true;
                    } else if (name == "Ord") {
                        has_derive_ord = true;
                    } else if (name == "Debug") {
                        has_derive_debug = true;
                    } else if (name == "Display") {
                        has_derive_display = true;
                    } else if (name == "Serialize") {
                        has_derive_serialize = true;
                    } else if (name == "Deserialize") {
                        has_derive_deserialize = true;
                    } else if (name == "FromStr") {
                        has_derive_fromstr = true;
                    }
                }
            }
        }
    }

    // Check for @flags decorator
    bool is_flags = false;
    std::string flags_underlying = "U32";
    for (const auto& decorator : decl.decorators) {
        if (decorator.name == "flags") {
            is_flags = true;
            if (!decorator.args.empty() && decorator.args[0]->is<parser::IdentExpr>()) {
                const auto& type_arg = decorator.args[0]->as<parser::IdentExpr>().name;
                if (type_arg == "U8" || type_arg == "U16" || type_arg == "U32" ||
                    type_arg == "U64") {
                    flags_underlying = type_arg;
                } else {
                    error("@flags underlying type must be U8, U16, U32, or U64, got '" + type_arg +
                              "'",
                          decorator.span, "T080");
                    return;
                }
            }
            break;
        }
    }

    // Validate @flags constraints
    std::vector<uint64_t> discriminant_values;
    if (is_flags) {
        if (!decl.generics.empty()) {
            error("@flags enum cannot have generic parameters", decl.span, "T081");
            return;
        }
        for (const auto& variant : decl.variants) {
            if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
                error("@flags enum variant '" + variant.name +
                          "' cannot have data fields. "
                          "Bitflag enums must use unit variants only.",
                      variant.span, "T082");
                return;
            }
        }
        size_t max_bits = 32;
        if (flags_underlying == "U8")
            max_bits = 8;
        else if (flags_underlying == "U16")
            max_bits = 16;
        else if (flags_underlying == "U64")
            max_bits = 64;
        if (decl.variants.size() > max_bits) {
            error("@flags(" + flags_underlying + ") enum has " +
                      std::to_string(decl.variants.size()) +
                      " variants but underlying type only supports " + std::to_string(max_bits) +
                      " bits",
                  decl.span, "T083");
            return;
        }

        // Compute discriminant values (power-of-2 auto-assignment)
        uint64_t next_power = 1;
        for (const auto& variant : decl.variants) {
            if (variant.discriminant) {
                // Evaluate integer literal discriminant
                if (variant.discriminant->get()->is<parser::LiteralExpr>()) {
                    const auto& lit = variant.discriminant->get()->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        uint64_t val = std::stoull(std::string(lit.token.lexeme));
                        discriminant_values.push_back(val);
                        // Don't advance next_power for explicit values
                        continue;
                    }
                }
                error("@flags discriminant must be an integer literal", variant.span, "T084");
                return;
            } else {
                discriminant_values.push_back(next_power);
                next_power <<= 1;
            }
        }
    }

    env_.define_enum(EnumDef{.name = decl.name,
                             .type_params = std::move(type_params),
                             .const_params = std::move(const_params),
                             .variants = std::move(variants),
                             .span = decl.span,
                             .is_flags = is_flags,
                             .flags_underlying_type = flags_underlying,
                             .discriminant_values = std::move(discriminant_values)});

    // Handle @flags — register built-in method signatures
    if (is_flags) {
        auto underlying_type = std::make_shared<Type>();
        if (flags_underlying == "U8")
            underlying_type->kind = PrimitiveType{PrimitiveKind::U8};
        else if (flags_underlying == "U16")
            underlying_type->kind = PrimitiveType{PrimitiveKind::U16};
        else if (flags_underlying == "U64")
            underlying_type->kind = PrimitiveType{PrimitiveKind::U64};
        else
            underlying_type->kind = PrimitiveType{PrimitiveKind::U32};

        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};
        auto bool_type = make_bool();

        // .has(flag) -> Bool
        env_.define_func(FuncSig{.name = decl.name + "::has",
                                 .params = {ref_self, ref_self},
                                 .return_type = bool_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // .is_empty() -> Bool
        env_.define_func(FuncSig{.name = decl.name + "::is_empty",
                                 .params = {ref_self},
                                 .return_type = bool_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // .bits() -> UnderlyingType
        env_.define_func(FuncSig{.name = decl.name + "::bits",
                                 .params = {ref_self},
                                 .return_type = underlying_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // .add(flag) -> Self
        env_.define_func(FuncSig{.name = decl.name + "::add",
                                 .params = {ref_self, ref_self},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // .remove(flag) -> Self
        env_.define_func(FuncSig{.name = decl.name + "::remove",
                                 .params = {ref_self, ref_self},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // .toggle(flag) -> Self
        env_.define_func(FuncSig{.name = decl.name + "::toggle",
                                 .params = {ref_self, ref_self},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // ::none() -> Self
        env_.define_func(FuncSig{.name = decl.name + "::none",
                                 .params = {},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // ::all() -> Self
        env_.define_func(FuncSig{.name = decl.name + "::all",
                                 .params = {},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        // ::from_bits(val) -> Self
        env_.define_func(FuncSig{.name = decl.name + "::from_bits",
                                 .params = {underlying_type},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Auto-register PartialEq and Flags
        env_.register_impl(decl.name, "PartialEq");
        env_.register_impl(decl.name, "Flags");
        env_.define_func(FuncSig{.name = decl.name + "::eq",
                                 .params = {ref_self, ref_self},
                                 .return_type = bool_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Auto-register Display and Debug for @flags enums
        auto str_type = types::make_str();
        env_.register_impl(decl.name, "Display");
        env_.define_func(FuncSig{.name = decl.name + "::to_string",
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
        env_.register_impl(decl.name, "Debug");
        env_.define_func(FuncSig{.name = decl.name + "::debug_string",
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Reflect) - register impl and type_info method
    // Skip generic enums - they need instantiation first
    if (has_derive_reflect && decl.generics.empty()) {
        // Register that this type implements Reflect behavior
        env_.register_impl(decl.name, "Reflect");

        // Create return type: ref TypeInfo
        auto type_info_type = std::make_shared<Type>();
        type_info_type->kind = NamedType{"TypeInfo", "", {}};
        auto ref_type_info = std::make_shared<Type>();
        ref_type_info->kind = RefType{false, type_info_type, std::nullopt};

        // Create self type: ref EnumName
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register EnumName::type_info() -> ref TypeInfo (static method)
        std::string static_method = decl.name + "::type_info";
        env_.define_func(FuncSig{.name = static_method,
                                 .params = {},
                                 .return_type = ref_type_info,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Register EnumName::runtime_type_info(ref this) -> ref TypeInfo (instance method)
        std::string instance_method = decl.name + "::runtime_type_info";
        env_.define_func(FuncSig{.name = instance_method,
                                 .params = {ref_self},
                                 .return_type = ref_type_info,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Create Str return type for variant_name
        auto str_type = std::make_shared<Type>();
        str_type->kind = PrimitiveType{PrimitiveKind::Str};

        // Create I64 return type for variant_tag
        auto i64_type = std::make_shared<Type>();
        i64_type->kind = PrimitiveType{PrimitiveKind::I64};

        // Register EnumName::variant_name(this) -> Str
        std::string variant_name_method = decl.name + "::variant_name";
        env_.define_func(FuncSig{.name = variant_name_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});

        // Register EnumName::variant_tag(this) -> I64
        std::string variant_tag_method = decl.name + "::variant_tag";
        env_.define_func(FuncSig{.name = variant_tag_method,
                                 .params = {ref_self},
                                 .return_type = i64_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(PartialEq) - register impl and eq method
    // Skip generic enums - they need instantiation first
    if (has_derive_partial_eq && decl.generics.empty()) {
        // Register that this type implements PartialEq behavior
        env_.register_impl(decl.name, "PartialEq");

        // Create return type: Bool
        auto bool_type = make_bool();

        // Create self type: ref EnumName
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register EnumName::eq(ref this, other: ref Self) -> Bool (instance method)
        std::string eq_method = decl.name + "::eq";
        env_.define_func(FuncSig{.name = eq_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = bool_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Duplicate) - register impl and duplicate method
    // Skip generic enums - they need instantiation first
    if (has_derive_duplicate && decl.generics.empty()) {
        // Register that this type implements Duplicate behavior
        env_.register_impl(decl.name, "Duplicate");

        // Create self type for return
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};

        // Create ref self type for parameter
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Register EnumName::duplicate(ref this) -> Self (instance method)
        std::string dup_method = decl.name + "::duplicate";
        env_.define_func(FuncSig{.name = dup_method,
                                 .params = {ref_self},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Hash) - register impl and hash method
    // Skip generic enums - they need instantiation first
    if (has_derive_hash && decl.generics.empty()) {
        // Register that this type implements Hash behavior
        env_.register_impl(decl.name, "Hash");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create I64 return type
        auto i64_type = types::make_i64();

        // Register EnumName::hash(ref this) -> I64 (instance method)
        std::string hash_method = decl.name + "::hash";
        env_.define_func(FuncSig{.name = hash_method,
                                 .params = {ref_self},
                                 .return_type = i64_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Default) - register impl and default static method
    // Skip generic enums - they need instantiation first
    if (has_derive_default && decl.generics.empty()) {
        // Register that this type implements Default behavior
        env_.register_impl(decl.name, "Default");

        // Create self type for return
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};

        // Register EnumName::default() -> Self (static method, no params)
        std::string default_method = decl.name + "::default";
        env_.define_func(FuncSig{.name = default_method,
                                 .params = {},
                                 .return_type = self_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(PartialOrd) - register impl and partial_cmp method
    // Skip generic enums - they need instantiation first
    if (has_derive_partial_ord && decl.generics.empty()) {
        // Register that this type implements PartialOrd behavior
        env_.register_impl(decl.name, "PartialOrd");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Ordering type
        auto ordering_type = std::make_shared<Type>();
        ordering_type->kind = NamedType{"Ordering", "", {}};

        // Create Maybe[Ordering] return type
        auto maybe_ordering_type = std::make_shared<Type>();
        maybe_ordering_type->kind = NamedType{"Maybe", "", {ordering_type}};

        // Register EnumName::partial_cmp(ref this, other: ref Self) -> Maybe[Ordering]
        std::string partial_cmp_method = decl.name + "::partial_cmp";
        env_.define_func(FuncSig{.name = partial_cmp_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = maybe_ordering_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Ord) - register impl and cmp method
    // Skip generic enums - they need instantiation first
    if (has_derive_ord && decl.generics.empty()) {
        // Register that this type implements Ord behavior
        env_.register_impl(decl.name, "Ord");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Ordering return type
        auto ordering_type = std::make_shared<Type>();
        ordering_type->kind = NamedType{"Ordering", "", {}};

        // Register EnumName::cmp(ref this, other: ref Self) -> Ordering
        std::string cmp_method = decl.name + "::cmp";
        env_.define_func(FuncSig{.name = cmp_method,
                                 .params = {ref_self, ref_self},
                                 .return_type = ordering_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Debug) - register impl and debug_string method
    // Skip generic enums - they need instantiation first
    if (has_derive_debug && decl.generics.empty()) {
        // Register that this type implements Debug behavior
        env_.register_impl(decl.name, "Debug");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register EnumName::debug_string(ref this) -> Str
        std::string debug_method = decl.name + "::debug_string";
        env_.define_func(FuncSig{.name = debug_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Display) - register impl and to_string method
    // Skip generic enums - they need instantiation first
    if (has_derive_display && decl.generics.empty()) {
        // Register that this type implements Display behavior
        env_.register_impl(decl.name, "Display");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register EnumName::to_string(ref this) -> Str
        std::string to_string_method = decl.name + "::to_string";
        env_.define_func(FuncSig{.name = to_string_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Serialize) - register impl and to_json method
    // Skip generic enums - they need instantiation first
    if (has_derive_serialize && decl.generics.empty()) {
        // Register that this type implements Serialize behavior
        env_.register_impl(decl.name, "Serialize");

        // Create ref self type for parameter
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};
        auto ref_self = std::make_shared<Type>();
        ref_self->kind = RefType{false, self_type, std::nullopt};

        // Create Str return type
        auto str_type = types::make_str();

        // Register EnumName::to_json(ref this) -> Str
        std::string to_json_method = decl.name + "::to_json";
        env_.define_func(FuncSig{.name = to_json_method,
                                 .params = {ref_self},
                                 .return_type = str_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(Deserialize) - register impl and from_json static method
    // Skip generic enums - they need instantiation first
    if (has_derive_deserialize && decl.generics.empty()) {
        // Register that this type implements Deserialize behavior
        env_.register_impl(decl.name, "Deserialize");

        // Create self type for Outcome payload
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};

        // Create Str type for error and parameter
        auto str_type = types::make_str();

        // Create Outcome[Self, Str] return type
        auto outcome_type = std::make_shared<Type>();
        outcome_type->kind = NamedType{"Outcome", "", {self_type, str_type}};

        // Register EnumName::from_json(s: Str) -> Outcome[Self, Str] (static method)
        std::string from_json_method = decl.name + "::from_json";
        env_.define_func(FuncSig{.name = from_json_method,
                                 .params = {str_type},
                                 .return_type = outcome_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }

    // Handle @derive(FromStr) - register impl and from_str static method
    // Skip generic enums - they need instantiation first
    if (has_derive_fromstr && decl.generics.empty()) {
        // Register that this type implements FromStr behavior
        env_.register_impl(decl.name, "FromStr");

        // Create self type for Outcome payload
        auto self_type = std::make_shared<Type>();
        self_type->kind = NamedType{decl.name, "", {}};

        // Create Str type for error and parameter
        auto str_type = types::make_str();

        // Create Outcome[Self, Str] return type
        auto outcome_type = std::make_shared<Type>();
        outcome_type->kind = NamedType{"Outcome", "", {self_type, str_type}};

        // Register EnumName::from_str(s: Str) -> Outcome[Self, Str] (static method)
        std::string from_str_method = decl.name + "::from_str";
        env_.define_func(FuncSig{.name = from_str_method,
                                 .params = {str_type},
                                 .return_type = outcome_type,
                                 .type_params = {},
                                 .is_async = false,
                                 .span = decl.span});
    }
}

} // namespace tml::types
