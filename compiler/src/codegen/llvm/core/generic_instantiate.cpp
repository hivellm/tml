TML_MODULE("codegen_x86")

//! # LLVM IR Generator - generate_pending_instantiations
//!
//! This file implements `LLVMIRGen::generate_pending_instantiations`, the main
//! monomorphization loop that drives all pending struct/enum/class/function
//! and impl-method instantiations to completion.
//!
//! ## Static Helpers (used only by generate_pending_instantiations)
//!
//! | Helper                              | Purpose                                      |
//! |-------------------------------------|----------------------------------------------|
//! | `extract_type_params_from_parser`   | Pattern-match parser type → type param subs  |
//! | `tokenize_mangled`                  | Split mangled name on "__" into token list   |
//! | `parse_tokens_with_pattern`         | Consume tokens guided by parser type arity   |
//! | `parse_mangled_type_string`         | Reconstruct semantic type from mangled string|
//! | `match_pattern_type`                | Recursively match pattern → concrete type    |
//! | `resolve_where_clause_type_equalities` | Derive extra type subs from where clauses |
//!
//! ## Instantiation Pipeline (inside generate_pending_instantiations)
//!
//! | Phase | What Happens                                         |
//! |-------|------------------------------------------------------|
//! | 1     | Generate all pending class type definitions          |
//! | 2     | Generate pending function instantiations             |
//! | 3     | Generate any new class instantiations from phase 2  |
//! | 4     | Generate pending impl method instantiations          |
//! | 5     | Loop until no new instantiations (handles recursion) |
//!
//! Split from generic.cpp to keep that file under 900 lines.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"

#include <fstream>
#include <functional>

namespace tml::codegen {

// Helper: Extract type parameter substitutions by pattern-matching a parser::Type pattern
// (from an impl's self_type generics) against a concrete semantic type (from the mangled name).
// For example: pattern=NamedType("Outcome", [T, E]), concrete=NamedType("Outcome", [I32, Str])
// gives T=I32, E=Str.
static void extract_type_params_from_parser(const parser::Type& pattern,
                                            const types::TypePtr& concrete,
                                            const std::vector<parser::GenericParam>& impl_generics,
                                            std::unordered_map<std::string, types::TypePtr>& subs) {
    if (!concrete)
        return;

    // Check if pattern is a NamedType (includes bare type params)
    if (pattern.is<parser::NamedType>()) {
        const auto& named = pattern.as<parser::NamedType>();
        std::string name = named.path.segments.empty() ? "" : named.path.segments.back();

        // Check if this is a bare type parameter name
        bool is_type_param = false;
        if (!named.generics.has_value() || named.generics->args.empty()) {
            for (const auto& gp : impl_generics) {
                if (gp.name == name) {
                    is_type_param = true;
                    break;
                }
            }
        }

        if (is_type_param) {
            subs[name] = concrete;
            return;
        }

        // If both are NamedType with same base name, recurse into type args
        if (concrete->is<types::NamedType>()) {
            const auto& concrete_named = concrete->as<types::NamedType>();
            if (concrete_named.name == name && named.generics.has_value()) {
                const auto& args = named.generics->args;
                if (args.size() == concrete_named.type_args.size()) {
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (args[i].is_type()) {
                            extract_type_params_from_parser(*args[i].as_type(),
                                                            concrete_named.type_args[i],
                                                            impl_generics, subs);
                        }
                    }
                }
            }
        }
        return;
    }

    // RefType pattern
    if (pattern.is<parser::RefType>()) {
        const auto& ref = pattern.as<parser::RefType>();
        if (concrete->is<types::RefType>()) {
            extract_type_params_from_parser(*ref.inner, concrete->as<types::RefType>().inner,
                                            impl_generics, subs);
        }
        return;
    }
}

// Helper: Tokenize a mangled name by splitting on "__"
// e.g., "Outcome__Outcome__I32__Str__Str" -> ["Outcome", "Outcome", "I32", "Str", "Str"]
static std::vector<std::string> tokenize_mangled(const std::string& s) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    while (pos < s.size()) {
        auto next = s.find("__", pos);
        if (next == std::string::npos) {
            if (pos < s.size())
                tokens.push_back(s.substr(pos));
            break;
        }
        tokens.push_back(s.substr(pos, next - pos));
        pos = next + 2;
    }
    return tokens;
}

// Forward declaration for mutual recursion
static types::TypePtr parse_mangled_type_string(const std::string& s);

// Helper: Parse a type from a flat token list guided by a parser::Type pattern.
// Uses the pattern's arity to correctly group tokens into nested types.
// For bare type params (e.g., T, E): reads exactly one token.
// For generic types (e.g., Outcome[T, E]): reads the base token then recurses for each arg.
// Extracts type param bindings into subs.
static types::TypePtr
parse_tokens_with_pattern(const parser::Type& pattern, const std::vector<std::string>& tokens,
                          size_t& pos, const std::vector<parser::GenericParam>& impl_generics,
                          std::unordered_map<std::string, types::TypePtr>& subs,
                          size_t remaining_siblings = 0) {

    if (pos >= tokens.size())
        return nullptr;

    if (!pattern.is<parser::NamedType>())
        return nullptr;

    const auto& named = pattern.as<parser::NamedType>();
    std::string name = named.path.segments.empty() ? "" : named.path.segments.back();

    // Check if this pattern node is a bare type parameter
    bool is_type_param = false;
    if (!named.generics.has_value() || named.generics->args.empty()) {
        for (const auto& gp : impl_generics) {
            if (gp.name == name) {
                is_type_param = true;
                break;
            }
        }
    }

    if (is_type_param) {
        // Consume tokens for this type parameter.
        // A bare type param can map to a generic type like SliceIter[I32] which is
        // encoded as multiple tokens ["SliceIter", "I32"] in the mangled name.
        // We need to figure out how many tokens to consume.
        //
        // When remaining_siblings > 0 (more type params after this one), consume
        // exactly 1 token to avoid stealing tokens from subsequent params.
        // Example: Map[I, F] with tokens ["Counter", "Fn"] → I=Counter, F=Fn.
        //
        // When remaining_siblings == 0 (last type param), consume ALL remaining
        // tokens to handle generic types like SliceIter[I32] encoded as
        // ["SliceIter", "I32"]. Example: Cloned[I] with tokens ["SliceIter", "I32"]
        // → I=SliceIter__I32 = SliceIter[I32].
        std::string base_token = tokens[pos++];

        if (remaining_siblings == 0 && pos < tokens.size()) {
            // Last (or only) type param: try consuming all remaining tokens
            std::string full_mangled = base_token;
            size_t saved_pos = pos;
            while (pos < tokens.size()) {
                full_mangled += "__" + tokens[pos++];
            }
            auto t = parse_mangled_type_string(full_mangled);
            if (t && subs.find(name) == subs.end()) {
                subs[name] = t;
            }
            if (t)
                return t;
            // Multi-token parse failed, fall back to base token
            pos = saved_pos;
        }

        auto t = parse_mangled_type_string(base_token);
        if (t && subs.find(name) == subs.end()) {
            subs[name] = t;
        }
        return t;
    }

    // Non-param: verify the current token matches the expected type name
    if (tokens[pos] != name)
        return nullptr;
    pos++;

    // Recursively parse type args based on pattern arity
    std::vector<types::TypePtr> type_args;
    if (named.generics.has_value()) {
        // Count remaining type args to pass as remaining_siblings so each
        // bare type param knows how many tokens to leave for subsequent args.
        size_t total_type_args = 0;
        for (const auto& arg : named.generics->args) {
            if (arg.is_type())
                total_type_args++;
        }
        size_t type_arg_index = 0;
        for (const auto& arg : named.generics->args) {
            if (arg.is_type()) {
                size_t siblings_after = total_type_args - type_arg_index - 1;
                auto arg_type = parse_tokens_with_pattern(*arg.as_type(), tokens, pos,
                                                          impl_generics, subs, siblings_after);
                if (arg_type)
                    type_args.push_back(arg_type);
                type_arg_index++;
            }
        }
    }

    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{name, "", std::move(type_args)};
    return t;
}

// Helper: Parse a mangled type string back into a semantic type
// e.g., "ptr_ChannelNode__I32" -> PtrType{inner=NamedType{name="ChannelNode", type_args=[I32]}}
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Handle primitive types
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "I128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "U128")
        return types::make_primitive(types::PrimitiveKind::U128);
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s == "Unit")
        return types::make_unit();
    if (s == "Usize")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "Isize")
        return types::make_primitive(types::PrimitiveKind::I64);

    // Function types are mangled as "Fn" (see llvm_types.cpp:1063).
    // Return a NamedType("Fn") which llvm_type_from_semantic maps to "{ ptr, ptr }"
    // (fat pointer). We can't recover the actual FuncType signature from just "Fn",
    // so we use NamedType as a marker. When used as a closure call's semantic_type,
    // the call codegen falls back to "i32" return type (default).
    if (s == "Fn") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{"Fn", "", {}};
        return t;
    }

    // Check for const generic integer value (e.g., "3", "10", "-1")
    if (!s.empty() && (std::isdigit(s[0]) || (s[0] == '-' && s.size() > 1 && std::isdigit(s[1])))) {
        try {
            int64_t val = std::stoll(s);
            auto t = std::make_shared<types::Type>();
            t->kind = types::ConstGenericType{s, types::make_i64(), val};
            return t;
        } catch (...) {}
    }

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable pointer prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for ref prefix
    if (s.size() > 4 && s.substr(0, 4) == "ref_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable ref prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutref_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32, ChannelNode__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);

        // Parse all type arguments (separated by __)
        std::vector<types::TypePtr> type_args;
        size_t pos = 0;
        while (pos < arg_str.size()) {
            // Find next __ delimiter
            auto next_delim = arg_str.find("__", pos);
            std::string arg_part;
            if (next_delim == std::string::npos) {
                arg_part = arg_str.substr(pos);
                pos = arg_str.size();
            } else {
                arg_part = arg_str.substr(pos, next_delim - pos);
                pos = next_delim + 2;
            }

            auto arg_type = parse_mangled_type_string(arg_part);
            if (arg_type) {
                type_args.push_back(arg_type);
            } else {
                // Fallback: create NamedType
                auto t = std::make_shared<types::Type>();
                t->kind = types::NamedType{arg_part, "", {}};
                type_args.push_back(t);
            }
        }

        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{base, "", std::move(type_args)};
        return t;
    }

    // Simple struct type (no generics, no prefix)
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

// Helper: Recursively match a parser type pattern against a concrete semantic type
// to extract type parameter bindings. For example:
//   pattern = Maybe[T], concrete = Maybe[I32] -> extracts T = I32
//   pattern = T, concrete = I32 -> extracts T = I32
//   pattern = ref T, concrete = ref I32 -> extracts T = I32
static void match_pattern_type(const parser::Type& pattern, const types::TypePtr& concrete,
                               std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete)
        return;

    // Handle RefType pattern: `ref T` matches against `RefType{inner=I32}` -> T = I32
    if (pattern.is<parser::RefType>()) {
        const auto& ref_pattern = pattern.as<parser::RefType>();
        if (ref_pattern.inner && concrete->is<types::RefType>()) {
            const auto& concrete_ref = concrete->as<types::RefType>();
            if (concrete_ref.inner) {
                match_pattern_type(*ref_pattern.inner, concrete_ref.inner, type_subs);
            }
        }
        return;
    }

    if (pattern.is<parser::NamedType>()) {
        const auto& named = pattern.as<parser::NamedType>();
        if (named.path.segments.empty())
            return;
        const std::string& name = named.path.segments.back();

        bool has_type_args = named.generics.has_value() && !named.generics->args.empty();

        if (!has_type_args) {
            // Simple name like "T" — check if it's a type parameter (not already resolved)
            auto existing = type_subs.find(name);
            bool is_placeholder = false;
            if (existing != type_subs.end() && existing->second &&
                existing->second->is<types::NamedType>()) {
                // Check if the existing mapping is a self-referential placeholder
                // (e.g., T -> NamedType{"T"}) which means it hasn't been resolved yet
                const auto& existing_named = existing->second->as<types::NamedType>();
                if (existing_named.name == name && existing_named.type_args.empty()) {
                    is_placeholder = true;
                }
            }
            if (existing == type_subs.end() || is_placeholder) {
                type_subs[name] = concrete;
                TML_DEBUG_LN("[WHERE_EQ] Resolved " << name << " via pattern matching");
            }
        } else {
            // Generic type like Maybe[T] — recurse into type args
            if (concrete->is<types::NamedType>()) {
                const auto& concrete_named = concrete->as<types::NamedType>();
                if (concrete_named.name == name) {
                    const auto& pattern_args = named.generics->args;
                    size_t min_args =
                        std::min(pattern_args.size(), concrete_named.type_args.size());
                    for (size_t i = 0; i < min_args; ++i) {
                        if (pattern_args[i].is_type() && concrete_named.type_args[i]) {
                            const auto& pattern_type = pattern_args[i].as_type();
                            if (pattern_type) {
                                match_pattern_type(*pattern_type, concrete_named.type_args[i],
                                                   type_subs);
                            }
                        }
                    }
                }
            }
        }
    }
}

// Helper: Extract additional type substitutions from where clause type equalities.
// For example, given `where F = func() -> Maybe[T]` and `F` mapped to `func() -> Maybe[I32]`,
// this will extract `T -> I32` by recursively matching the pattern against the concrete type.
//
// Also handles associated type equalities like `where I::Item = ref T`:
// Given I -> SliceIter[I32] in type_subs, resolves SliceIter[I32]::Item -> ref I32,
// then matches pattern `ref T` against `ref I32` to derive T -> I32.
//
// The assoc_type_lookup callback resolves associated types for concrete types.
// It takes (concrete_type, assoc_name) and returns the fully substituted type, or nullptr.
// The concrete_type is the full NamedType (with type_args) so the callback can
// build the right substitution map for the inner type's generic params.
using AssocTypeLookupFn = std::function<types::TypePtr(const types::TypePtr&, const std::string&)>;

static void
resolve_where_clause_type_equalities(const std::optional<parser::WhereClause>& where_clause,
                                     std::unordered_map<std::string, types::TypePtr>& type_subs,
                                     AssocTypeLookupFn assoc_type_lookup = nullptr) {
    if (!where_clause || where_clause->type_equalities.empty())
        return;

    for (const auto& [lhs, rhs] : where_clause->type_equalities) {
        if (!lhs || !rhs)
            continue;

        if (!lhs->is<parser::NamedType>())
            continue;
        const auto& lhs_named = lhs->as<parser::NamedType>();
        if (lhs_named.path.segments.empty())
            continue;

        // Case 1: Two-segment path like I::Item (associated type equality)
        // LHS = I::Item, RHS = ref T (pattern)
        // Resolve: look up I in type_subs to get the concrete type (e.g., SliceIter[I32]),
        // then find what "Item" resolves to for that concrete type (e.g., ref I32),
        // then match RHS pattern (ref T) against that to derive T = I32.
        if (lhs_named.path.segments.size() >= 2) {
            const std::string& param_name = lhs_named.path.segments[0];
            const std::string& assoc_name = lhs_named.path.segments.back();

            // Find the concrete type that param maps to
            auto param_it = type_subs.find(param_name);
            if (param_it == type_subs.end() || !param_it->second)
                continue;

            // Resolve the associated type using the callback, which handles
            // both lookup and substitution of the inner type's generic params.
            types::TypePtr concrete_assoc;
            if (assoc_type_lookup) {
                concrete_assoc = assoc_type_lookup(param_it->second, assoc_name);
            }

            if (concrete_assoc) {
                TML_DEBUG_LN("[WHERE_EQ] Resolved "
                             << param_name << "::" << assoc_name
                             << " to concrete type, matching against RHS pattern");
                match_pattern_type(*rhs, concrete_assoc, type_subs);
            }
            continue;
        }

        // Case 2: Simple name like "F" that's already in type_subs
        const std::string& param_name = lhs_named.path.segments.back();

        auto it = type_subs.find(param_name);
        if (it == type_subs.end() || !it->second)
            continue;

        // RHS is the pattern type (e.g., func() -> T or func(A) -> Maybe[B])
        // The concrete type is it->second
        const auto& concrete = it->second;

        // Match the RHS pattern against the concrete type to extract type params
        match_pattern_type(*rhs, concrete, type_subs);

        // Also handle func/closure types specially for parameter/return matching.
        // ClosureType arises when callers pass `do() -> T { ... }` lambdas to a
        // function with `where F = func() -> T` clauses.
        if (rhs->is<parser::FuncType>() &&
            (concrete->is<types::FuncType>() || concrete->is<types::ClosureType>())) {
            const auto& pattern_func = rhs->as<parser::FuncType>();
            types::TypePtr concrete_ret;
            const std::vector<types::TypePtr>* concrete_params = nullptr;
            if (concrete->is<types::FuncType>()) {
                const auto& concrete_func = concrete->as<types::FuncType>();
                concrete_ret = concrete_func.return_type;
                concrete_params = &concrete_func.params;
            } else {
                const auto& concrete_clos = concrete->as<types::ClosureType>();
                concrete_ret = concrete_clos.return_type;
                concrete_params = &concrete_clos.params;
            }

            // Match return type pattern against concrete return type
            if (pattern_func.return_type && concrete_ret) {
                match_pattern_type(*pattern_func.return_type, concrete_ret, type_subs);
            }

            // Match parameter type patterns
            size_t min_params = std::min(pattern_func.params.size(), concrete_params->size());
            for (size_t i = 0; i < min_params; ++i) {
                if (pattern_func.params[i] && (*concrete_params)[i]) {
                    match_pattern_type(*pattern_func.params[i], (*concrete_params)[i], type_subs);
                }
            }
        }
    }
}

// ============ Generate Pending Generic Instantiations ============
// Iteratively generate all pending struct/enum/func instantiations
// Loops until no new instantiations are added (handles recursive types)

void LLVMIRGen::generate_pending_instantiations() {
    const int MAX_ITERATIONS = 100; // Prevent infinite loops
    int iterations = 0;

    // First pass: generate ALL type definitions (classes only — structs and enums
    // are generated immediately in require_*_instantiation, so no scanning needed)
    while (!pending_class_keys_.empty() && iterations < MAX_ITERATIONS) {
        ++iterations;

        auto keys = std::move(pending_class_keys_);
        pending_class_keys_.clear();

        for (const auto& key : keys) {
            auto inst_it = class_instantiations_.find(key);
            if (inst_it == class_instantiations_.end() || inst_it->second.generated)
                continue;
            inst_it->second.generated = true;

            auto it = pending_generic_classes_.find(inst_it->second.base_name);
            if (it != pending_generic_classes_.end()) {
                gen_class_instantiation(*it->second, inst_it->second.type_args);
            }
        }
    }

    // Second pass: generate functions (may discover new types/classes, so we loop)
    iterations = 0;
    bool changed = true;
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        ++iterations;

        // Generate pending function instantiations from queue
        if (!pending_func_keys_.empty()) {
            auto keys = std::move(pending_func_keys_);
            pending_func_keys_.clear();

            for (const auto& key : keys) {
                auto inst_it = func_instantiations_.find(key);
                if (inst_it == func_instantiations_.end() || inst_it->second.generated)
                    continue;
                inst_it->second.generated = true;

                auto it = pending_generic_funcs_.find(inst_it->second.base_name);
                if (it != pending_generic_funcs_.end()) {
                    // If the base_name is an arity-disambiguated key (contains "___a"),
                    // temporarily set generic_func_modules_ for the real function name
                    // so that gen_func_instantiation produces the correct mangled symbol.
                    const std::string& base = inst_it->second.base_name;
                    std::string saved_module;
                    std::string real_func_name = it->second->name;
                    bool needs_module_fix = false;
                    auto arity_pos = base.find("___a");
                    if (arity_pos != std::string::npos) {
                        auto mod_it2 = generic_func_modules_.find(base);
                        if (mod_it2 != generic_func_modules_.end()) {
                            auto existing = generic_func_modules_.find(real_func_name);
                            if (existing != generic_func_modules_.end()) {
                                saved_module = existing->second;
                            }
                            generic_func_modules_[real_func_name] = mod_it2->second;
                            needs_module_fix = true;
                        }
                    }
                    gen_func_instantiation(*it->second, inst_it->second.type_args);
                    // Restore the original module mapping
                    if (needs_module_fix) {
                        if (saved_module.empty()) {
                            generic_func_modules_.erase(real_func_name);
                        } else {
                            generic_func_modules_[real_func_name] = saved_module;
                        }
                    }
                    changed = true;
                }
            }
        }

        // Generate any new class instantiations discovered during function generation
        if (!pending_class_keys_.empty()) {
            auto keys = std::move(pending_class_keys_);
            pending_class_keys_.clear();

            for (const auto& key : keys) {
                auto inst_it = class_instantiations_.find(key);
                if (inst_it == class_instantiations_.end() || inst_it->second.generated)
                    continue;
                inst_it->second.generated = true;

                auto it = pending_generic_classes_.find(inst_it->second.base_name);
                if (it != pending_generic_classes_.end()) {
                    gen_class_instantiation(*it->second, inst_it->second.type_args);
                    changed = true;
                }
            }
        }

        // Generate pending impl method instantiations (extracted to generic_instantiate_impl.cpp)
        if (generate_pending_impl_method_instantiations()) {
            changed = true;
        }

        // Generate pending generic class method instantiations
        while (!pending_generic_class_method_insts_.empty()) {
            auto pending = std::move(pending_generic_class_method_insts_);
            pending_generic_class_method_insts_.clear();

            for (const auto& p : pending) {
                gen_generic_class_static_method(*p.class_decl, *p.method, p.method_suffix,
                                                p.type_subs);
            }
            changed = true;
        }
    }
}

} // namespace tml::codegen
