//! # Signature Generation Implementation
//!
//! This file implements the generation of human-readable signatures
//! from AST nodes for documentation purposes.

#include "doc/signature.hpp"

#include <sstream>

namespace tml::doc {

// Forward declaration for mutual recursion
auto type_to_string(const parser::Type& type) -> std::string;

/// Converts a GenericArg to string representation.
auto generic_arg_to_string(const parser::GenericArg& arg) -> std::string {
    if (arg.is_type()) {
        return type_to_string(*arg.as_type());
    } else {
        // Const generic - for now just show placeholder
        return "...";
    }
}

auto type_to_string(const parser::Type& type) -> std::string {
    return std::visit(
        [](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                std::string result;
                for (size_t i = 0; i < t.path.segments.size(); ++i) {
                    if (i > 0) {
                        result += "::";
                    }
                    result += t.path.segments[i];
                }
                if (t.generics && !t.generics->args.empty()) {
                    result += "[";
                    for (size_t i = 0; i < t.generics->args.size(); ++i) {
                        if (i > 0) {
                            result += ", ";
                        }
                        result += generic_arg_to_string(t.generics->args[i]);
                    }
                    result += "]";
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                std::string result = t.is_mut ? "mut ref " : "ref ";
                result += type_to_string(*t.inner);
                return result;
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                std::string result = t.is_mut ? "mut ptr " : "ptr ";
                result += type_to_string(*t.inner);
                return result;
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                std::string result = "[";
                result += type_to_string(*t.element);
                result += "; ";
                // TODO: stringify the size expression
                result += "N";
                result += "]";
                return result;
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                std::string result = "[";
                result += type_to_string(*t.element);
                result += "]";
                return result;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::string result = "(";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0) {
                        result += ", ";
                    }
                    result += type_to_string(*t.elements[i]);
                }
                result += ")";
                return result;
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::string result = "func(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0) {
                        result += ", ";
                    }
                    result += type_to_string(*t.params[i]);
                }
                result += ")";
                if (t.return_type) {
                    result += " -> ";
                    result += type_to_string(*t.return_type);
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                return "_";
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                std::string result = t.is_mut ? "dyn mut " : "dyn ";
                for (size_t i = 0; i < t.behavior.segments.size(); ++i) {
                    if (i > 0) {
                        result += "::";
                    }
                    result += t.behavior.segments[i];
                }
                if (t.generics && !t.generics->args.empty()) {
                    result += "[";
                    for (size_t i = 0; i < t.generics->args.size(); ++i) {
                        if (i > 0) {
                            result += ", ";
                        }
                        result += generic_arg_to_string(t.generics->args[i]);
                    }
                    result += "]";
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::ImplBehaviorType>) {
                std::string result = "impl ";
                for (size_t i = 0; i < t.behavior.segments.size(); ++i) {
                    if (i > 0) {
                        result += "::";
                    }
                    result += t.behavior.segments[i];
                }
                if (t.generics && !t.generics->args.empty()) {
                    result += "[";
                    for (size_t i = 0; i < t.generics->args.size(); ++i) {
                        if (i > 0) {
                            result += ", ";
                        }
                        result += generic_arg_to_string(t.generics->args[i]);
                    }
                    result += "]";
                }
                return result;
            } else {
                return "?";
            }
        },
        type.kind);
}

auto generics_to_string(const std::vector<parser::GenericParam>& generics) -> std::string {
    if (generics.empty()) {
        return "";
    }

    std::string result = "[";
    for (size_t i = 0; i < generics.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        const auto& param = generics[i];

        if (param.is_const) {
            result += "const ";
        }
        result += param.name;

        if (!param.bounds.empty()) {
            result += ": ";
            for (size_t j = 0; j < param.bounds.size(); ++j) {
                if (j > 0) {
                    result += " + ";
                }
                result += type_to_string(*param.bounds[j]);
            }
        }

        if (param.default_type) {
            result += " = ";
            result += type_to_string(**param.default_type);
        }
    }
    result += "]";
    return result;
}

auto where_clause_to_string(const std::optional<parser::WhereClause>& where_clause) -> std::string {
    if (!where_clause || where_clause->constraints.empty()) {
        return "";
    }

    std::string result = " where ";
    bool first = true;
    for (const auto& [type, bounds] : where_clause->constraints) {
        if (!first) {
            result += ", ";
        }
        first = false;
        result += type_to_string(*type);
        result += ": ";
        for (size_t i = 0; i < bounds.size(); ++i) {
            if (i > 0) {
                result += " + ";
            }
            result += type_to_string(*bounds[i]);
        }
    }
    return result;
}

auto generate_func_signature(const parser::FuncDecl& func) -> std::string {
    std::string result;

    if (func.is_async) {
        result += "async ";
    }
    if (func.is_unsafe) {
        result += "lowlevel ";
    }

    result += "func ";
    result += func.name;
    result += generics_to_string(func.generics);
    result += "(";

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        const auto& param = func.params[i];

        // Handle pattern
        if (param.pattern->is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->as<parser::IdentPattern>();
            if (ident.is_mut) {
                result += "mut ";
            }
            result += ident.name;
        } else {
            // Other patterns - simplified
            result += "_";
        }

        // Check if this is 'this' parameter without explicit type
        bool is_this = param.pattern->is<parser::IdentPattern>() &&
                       param.pattern->as<parser::IdentPattern>().name == "this";

        if (!is_this || param.type->is<parser::RefType>()) {
            result += ": ";
            result += type_to_string(*param.type);
        }
    }

    result += ")";

    if (func.return_type) {
        result += " -> ";
        result += type_to_string(**func.return_type);
    }

    result += where_clause_to_string(func.where_clause);

    return result;
}

auto generate_struct_signature(const parser::StructDecl& struct_decl) -> std::string {
    std::string result = "type ";
    result += struct_decl.name;
    result += generics_to_string(struct_decl.generics);
    result += where_clause_to_string(struct_decl.where_clause);
    return result;
}

auto generate_enum_signature(const parser::EnumDecl& enum_decl) -> std::string {
    std::string result = "type ";
    result += enum_decl.name;
    result += generics_to_string(enum_decl.generics);
    result += where_clause_to_string(enum_decl.where_clause);
    return result;
}

auto generate_trait_signature(const parser::TraitDecl& trait) -> std::string {
    std::string result = "behavior ";
    result += trait.name;
    result += generics_to_string(trait.generics);

    if (!trait.super_traits.empty()) {
        result += ": ";
        for (size_t i = 0; i < trait.super_traits.size(); ++i) {
            if (i > 0) {
                result += " + ";
            }
            result += type_to_string(*trait.super_traits[i]);
        }
    }

    result += where_clause_to_string(trait.where_clause);
    return result;
}

auto generate_impl_signature(const parser::ImplDecl& impl) -> std::string {
    std::string result = "impl";
    result += generics_to_string(impl.generics);
    result += " ";

    if (impl.trait_type) {
        result += type_to_string(*impl.trait_type);
        result += " for ";
    }

    result += type_to_string(*impl.self_type);
    result += where_clause_to_string(impl.where_clause);
    return result;
}

auto generate_type_alias_signature(const parser::TypeAliasDecl& alias) -> std::string {
    std::string result = "type ";
    result += alias.name;
    result += generics_to_string(alias.generics);
    result += " = ";
    result += type_to_string(*alias.type);
    return result;
}

auto generate_const_signature(const parser::ConstDecl& const_decl) -> std::string {
    std::string result = "const ";
    result += const_decl.name;
    result += ": ";
    result += type_to_string(*const_decl.type);
    return result;
}

auto generate_field_signature(const parser::StructField& field) -> std::string {
    std::string result = field.name;
    result += ": ";
    result += type_to_string(*field.type);
    return result;
}

auto generate_variant_signature(const parser::EnumVariant& variant) -> std::string {
    std::string result = variant.name;

    if (variant.tuple_fields) {
        result += "(";
        for (size_t i = 0; i < variant.tuple_fields->size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += type_to_string(*(*variant.tuple_fields)[i]);
        }
        result += ")";
    } else if (variant.struct_fields) {
        result += " { ";
        for (size_t i = 0; i < variant.struct_fields->size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += generate_field_signature((*variant.struct_fields)[i]);
        }
        result += " }";
    }

    return result;
}

} // namespace tml::doc
