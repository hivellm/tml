TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Generic Instantiation
//!
//! This file implements monomorphization of generic types and functions.
//!
//! ## Monomorphization Strategy
//!
//! TML uses monomorphization like Rust: each use of a generic with concrete
//! types generates a specialized version of the code.
//!
//! ## Instantiation Pipeline
//!
//! | Phase | What Happens                                    |
//! |-------|------------------------------------------------|
//! | 1     | Collect pending struct instantiations          |
//! | 2     | Collect pending enum instantiations            |
//! | 3     | Collect pending function instantiations        |
//! | 4     | Loop until no new instantiations (handles recursion) |
//!
//! ## Key Methods
//!
//! | Method                        | Purpose                          |
//! |-------------------------------|----------------------------------|
//! | `generate_pending_instantiations` | Main instantiation loop      |
//! | `require_struct_instantiation`| Queue struct for instantiation   |
//! | `require_enum_instantiation`  | Queue enum for instantiation     |
//! | `require_func_instantiation`  | Queue function for instantiation |
//!
//! ## Naming Convention
//!
//! Instantiated names include type arguments: `List_I32`, `HashMap_Str_I32`

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
                          std::unordered_map<std::string, types::TypePtr>& subs) {

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
        // Strategy: look up the base type name in the module registry to find its
        // arity (number of generic params). Consume 1 + arity tokens.
        std::string base_token = tokens[pos++];

        // A bare type param can map to a generic type like SliceIter[I32] which is
        // encoded as multiple tokens ["SliceIter", "I32"] in the mangled name.
        // Strategy: join all remaining tokens with "__" and use parse_mangled_type_string
        // which handles Name__Arg1__Arg2 patterns correctly.
        if (pos < tokens.size()) {
            std::string full_mangled = base_token;
            size_t saved_pos = pos;
            // Consume remaining tokens that belong to this type parameter.
            // Since we don't know the arity, tentatively consume all remaining tokens
            // and let parse_mangled_type_string handle nested types.
            // The caller will check if pos is valid after return.
            while (pos < tokens.size()) {
                full_mangled += "__" + tokens[pos++];
            }
            auto t = parse_mangled_type_string(full_mangled);
            if (t && subs.find(name) == subs.end()) {
                subs[name] = t;
            }
            if (t)
                return t;
            // If parsing the full string failed, fall back to just the base token
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
        for (const auto& arg : named.generics->args) {
            if (arg.is_type()) {
                auto arg_type =
                    parse_tokens_with_pattern(*arg.as_type(), tokens, pos, impl_generics, subs);
                if (arg_type)
                    type_args.push_back(arg_type);
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

        // Also handle func types specially for parameter/return matching
        if (rhs->is<parser::FuncType>() && concrete->is<types::FuncType>()) {
            const auto& pattern_func = rhs->as<parser::FuncType>();
            const auto& concrete_func = concrete->as<types::FuncType>();

            // Match return type pattern against concrete return type
            if (pattern_func.return_type && concrete_func.return_type) {
                match_pattern_type(*pattern_func.return_type, concrete_func.return_type, type_subs);
            }

            // Match parameter type patterns
            size_t min_params = std::min(pattern_func.params.size(), concrete_func.params.size());
            for (size_t i = 0; i < min_params; ++i) {
                if (pattern_func.params[i] && concrete_func.params[i]) {
                    match_pattern_type(*pattern_func.params[i], concrete_func.params[i], type_subs);
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
                    gen_func_instantiation(*it->second, inst_it->second.type_args);
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

        // Generate pending impl method instantiations
        // Track processed methods to avoid duplicate lookups (expensive module searches)
        std::unordered_set<std::string> processed_impl_methods;

        // Save module context — impl method instantiation may need to set module context
        // for intra-module call resolution (e.g., Arena::alloc_raw calling align_up needs
        // current_module_name_="core::arena" so the qualified lookup finds the mangled name).
        auto saved_module_prefix = current_module_prefix_;
        auto saved_module_name = current_module_name_;
        auto saved_submodule = current_submodule_name_;

        int instantiation_round = 0;
        while (!pending_impl_method_instantiations_.empty()) {
            auto pending = std::move(pending_impl_method_instantiations_);
            pending_impl_method_instantiations_.clear();
            ++instantiation_round;
            if (instantiation_round > 100) {
                TML_LOG_WARN("codegen", "Infinite instantiation loop detected after 100 rounds");
                break;
            }

            for (const auto& pim : pending) {
                TML_LOG_TRACE("codegen", "[GENERIC_DBG] R" << instantiation_round << " "
                                                           << pim.mangled_type_name
                                                           << "::" << pim.method_name);
                // Build deduplication key
                std::string method_key = pim.mangled_type_name + "::" + pim.method_name;
                if (!pim.method_type_suffix.empty()) {
                    method_key += "__" + pim.method_type_suffix;
                }

                // Skip if already processed or already generated
                if (processed_impl_methods.count(method_key) > 0) {
                    continue;
                }

                // Also check if already generated (from previous compilation phases)
                // Only check generated_impl_methods_output_ which tracks ACTUALLY generated
                // methods. Don't check generated_impl_methods_ here - that's for queue
                // deduplication only.
                std::string method_name_full = pim.method_name;
                if (!pim.method_type_suffix.empty()) {
                    method_name_full += "__" + pim.method_type_suffix;
                }
                std::string generated_key =
                    mangle_impl_method(pim.mangled_type_name, method_name_full);
                if (generated_impl_methods_output_.count(generated_key) > 0) {
                    processed_impl_methods.insert(method_key);
                    continue;
                }

                // NOTE: GlobalLibraryIRCache is DISABLED for now.
                // The cache was causing issues where only declarations were emitted
                // but implementations were missing when compiling multiple test suites.
                // Each suite needs its own complete implementation of library methods.
                // TODO: Revisit caching strategy - perhaps cache at the object file level
                // instead of the IR level, or clear cache between suites.

                processed_impl_methods.insert(method_key);

                TML_DEBUG_LN("[IMPL_INST] Looking for "
                             << pim.base_type_name << "::" << pim.method_name
                             << " (mangled: " << pim.mangled_type_name << ")"
                             << " is_library_type=" << (pim.is_library_type ? "true" : "false")
                             << " method_type_suffix=" << pim.method_type_suffix);
                bool method_generated = false;

                // First check locally defined impls
                auto impl_it = pending_generic_impls_.find(pim.base_type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    const parser::ImplDecl* impl_ptr = impl_it->second;

                    // Check if this impl has the method we're looking for BEFORE
                    // doing any processing. This handles the case where multiple
                    // modules define the same type (e.g., core::range::Range vs
                    // core::ops::range::Range) but with different methods.
                    bool has_method = false;
                    for (const auto& m : impl_ptr->methods) {
                        if (m.name == pim.method_name) {
                            has_method = true;
                            break;
                        }
                    }

                    // If primary impl doesn't have the method, check all
                    // registered impls for this type. Types like tuples have
                    // separate impl blocks per trait (Default, Clone, PartialEq,
                    // etc.) but pending_generic_impls_ stores only the first one.
                    if (!has_method) {
                        auto all_it = pending_generic_impls_all_.find(pim.base_type_name);
                        if (all_it != pending_generic_impls_all_.end()) {
                            for (const auto* alt : all_it->second) {
                                if (alt == impl_ptr)
                                    continue;
                                for (const auto& m : alt->methods) {
                                    if (m.name == pim.method_name) {
                                        impl_ptr = alt;
                                        has_method = true;
                                        break;
                                    }
                                }
                                if (has_method)
                                    break;
                            }
                        }
                    }

                    const auto& impl = *impl_ptr;

                    // DEBUG: Log method lookup result for Range types
                    if (pim.base_type_name == "RangeInclusive" || pim.base_type_name == "Range") {
                        std::ostringstream dbg;
                        dbg << "[DEBUG GENERIC] " << pim.base_type_name << "::" << pim.method_name
                            << " - local impl has " << impl.methods.size()
                            << " methods, has_method=" << (has_method ? "yes" : "no")
                            << ", generics=";
                        for (const auto& g : impl.generics) {
                            dbg << g.name << " ";
                        }
                        TML_LOG_TRACE("codegen", dbg.str());
                    }

                    if (has_method) {
                        // Process associated type bindings from the impl block
                        // e.g., `type Item = I::Item` becomes `Item -> I64` when I -> RangeIterI64
                        auto saved_associated_types = current_associated_types_;
                        current_associated_types_.clear();

                        // First, we need to find the associated types from the concrete types
                        // that the generic params were substituted to
                        // For example: if I -> RangeIterI64, look up RangeIterI64's Item type
                        for (const auto& [param_name, concrete_type] : pim.type_subs) {
                            if (concrete_type && concrete_type->is<types::NamedType>()) {
                                const auto& concrete_named = concrete_type->as<types::NamedType>();
                                // Find the impl block for this concrete type to get its associated
                                // types
                                auto concrete_impl_it =
                                    pending_generic_impls_.find(concrete_named.name);
                                if (concrete_impl_it != pending_generic_impls_.end()) {
                                    const auto& concrete_impl = *concrete_impl_it->second;
                                    for (const auto& concrete_binding :
                                         concrete_impl.type_bindings) {
                                        auto concrete_resolved = resolve_parser_type_with_subs(
                                            *concrete_binding.type, {});
                                        current_associated_types_[concrete_binding.name] =
                                            concrete_resolved;
                                    }
                                }
                            }
                        }

                        // Recover type_subs from mangled_type_name.
                        // For example: mangled_type_name="Range__I64", base_type_name="Range"
                        // Extract "I64" and map to impl generics (e.g., T -> I64)
                        auto effective_type_subs = pim.type_subs;

                        // Detect specialized impls that need token-based parsing:
                        // 1. Self-type args contain nested types: impl[T,E] Outcome[Outcome[T,E],
                        // E]
                        // 2. Self-type args contain RefType: impl[T] Pin[ref T]
                        // 3. Self-type has fewer generic args than impl generics:
                        //    impl[I, T] Iterator for Cloned[I] where I::Item = ref T
                        //    The mangled name only encodes I (not T), so naive "__" splitting
                        //    would misparse "SliceIter__I32" as two separate params instead of
                        //    one nested generic type.
                        bool is_specialized_impl = false;
                        if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                            const auto& self_named = impl.self_type->as<parser::NamedType>();
                            if (self_named.generics.has_value()) {
                                // Count how many impl generics appear in the self_type's args
                                size_t self_type_arity = self_named.generics->args.size();
                                if (self_type_arity < impl.generics.size()) {
                                    // More impl generics than self_type args — extra params
                                    // are derived from where clauses, not the mangled name.
                                    // Use token-based parser so it consumes all tokens for the
                                    // self_type args rather than splitting naively.
                                    is_specialized_impl = true;
                                }
                                for (const auto& arg : self_named.generics->args) {
                                    if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                                        const auto& arg_named =
                                            arg.as_type()->as<parser::NamedType>();
                                        if (arg_named.generics.has_value() &&
                                            !arg_named.generics->args.empty()) {
                                            is_specialized_impl = true;
                                            break;
                                        }
                                    }
                                    if (arg.is_type() && arg.as_type()->is<parser::RefType>()) {
                                        is_specialized_impl = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!impl.generics.empty() &&
                            pim.mangled_type_name.length() > pim.base_type_name.length() + 2) {

                            if (is_specialized_impl) {
                                // Use token-based pattern-guided extraction to correctly handle
                                // nested generic types (e.g., Outcome[Outcome[T,E], E]).
                                // The token list is built by splitting on "__", then the pattern
                                // provides the arity to correctly group tokens into nested types.
                                // This correctly derives T=I32, E=Str from
                                // "Outcome__Outcome__I32__Str__Str" with pattern
                                // Outcome[Outcome[T,E], E].
                                auto tokens = tokenize_mangled(pim.mangled_type_name);
                                size_t tok_pos = 0;
                                std::unordered_map<std::string, types::TypePtr> new_subs;
                                if (impl.self_type) {
                                    parse_tokens_with_pattern(*impl.self_type, tokens, tok_pos,
                                                              impl.generics, new_subs);
                                }
                                if (!new_subs.empty()) {
                                    // Merge new_subs into effective_type_subs, overriding
                                    // existing entries. This preserves entries from pim.type_subs
                                    // (like Self, This, associated types) that aren't derived
                                    // from the mangled name.
                                    for (const auto& [k, v] : new_subs) {
                                        effective_type_subs[k] = v;
                                    }
                                }
                            } else if (effective_type_subs.empty()) {
                                std::string suffix =
                                    pim.mangled_type_name.substr(pim.base_type_name.length());
                                if (suffix.starts_with("__")) {
                                    suffix = suffix.substr(2);
                                    // For single type param, use entire suffix
                                    if (impl.generics.size() == 1) {
                                        auto type_arg = parse_mangled_type_string(suffix);
                                        if (type_arg) {
                                            effective_type_subs[impl.generics[0].name] = type_arg;
                                            TML_DEBUG_LN("[IMPL_INST] Recovered type_subs from "
                                                         "mangled name: "
                                                         << impl.generics[0].name << " -> "
                                                         << suffix);
                                        }
                                    } else {
                                        // Multiple type params - split on "__"
                                        std::vector<std::string> parts;
                                        size_t pos = 0;
                                        while (pos < suffix.size()) {
                                            size_t next = suffix.find("__", pos);
                                            if (next == std::string::npos) {
                                                parts.push_back(suffix.substr(pos));
                                                break;
                                            }
                                            parts.push_back(suffix.substr(pos, next - pos));
                                            pos = next + 2;
                                        }
                                        for (size_t i = 0;
                                             i < impl.generics.size() && i < parts.size(); ++i) {
                                            auto type_arg = parse_mangled_type_string(parts[i]);
                                            if (type_arg) {
                                                effective_type_subs[impl.generics[i].name] =
                                                    type_arg;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Resolve where clause type equalities to derive additional
                        // type substitutions (e.g., `where F = func() -> T` with F=func()->I32
                        // derives T=I32; `where I::Item = ref T` derives T from I's Item)
                        resolve_where_clause_type_equalities(
                            impl.where_clause, effective_type_subs,
                            [this](const types::TypePtr& concrete_type,
                                   const std::string& assoc_name) -> types::TypePtr {
                                return resolve_assoc_type_for_concrete(concrete_type, assoc_name);
                            });

                        // Now resolve the impl's own type bindings with the substitutions
                        for (const auto& binding : impl.type_bindings) {
                            // Resolve the binding type with the current type substitutions
                            auto resolved =
                                resolve_parser_type_with_subs(*binding.type, effective_type_subs);
                            current_associated_types_[binding.name] = resolved;
                        }

                        // Set module context for intra-module call resolution.
                        // Look up which module defines this type so that calls within
                        // the method body (e.g., Arena::alloc_raw calling align_up) can
                        // resolve the qualified name in the functions_ map.
                        if (pim.is_library_type && env_.module_registry()) {
                            const auto& all_modules = env_.module_registry()->get_all_modules();
                            for (const auto& [mn, mod] : all_modules) {
                                if (mod.structs.count(pim.base_type_name) > 0 ||
                                    mod.enums.count(pim.base_type_name) > 0 ||
                                    mod.internal_structs.count(pim.base_type_name) > 0) {
                                    current_module_name_ = mn;
                                    std::string prefix = mn;
                                    size_t pos = 0;
                                    while ((pos = prefix.find("::", pos)) != std::string::npos) {
                                        prefix.replace(pos, 2, "_");
                                        pos += 1;
                                    }
                                    current_module_prefix_ = prefix;
                                    break;
                                }
                            }
                        }

                        // Find the method in the impl block and generate it
                        for (const auto& m : impl.methods) {
                            if (m.name == pim.method_name) {
                                gen_impl_method_instantiation(
                                    pim.mangled_type_name, m, effective_type_subs, impl.generics,
                                    pim.method_type_suffix, pim.is_library_type, pim.base_type_name,
                                    impl.self_type.get());
                                method_generated = true;
                                break;
                            }
                        }

                        // Restore module context and associated types
                        current_module_name_ = saved_module_name;
                        current_module_prefix_ = saved_module_prefix;
                        current_associated_types_ = saved_associated_types;
                    }
                }

                // If not found in local impls, check imported modules
                if (!method_generated && env_.module_registry()) {
                    // Check imported modules - need to re-parse to get impl AST
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    TML_DEBUG_LN("[IMPL_INST]   Not in local impls, searching "
                                 << all_modules.size() << " modules for " << pim.base_type_name
                                 << "::" << pim.method_name);
                    bool found = false;
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (found)
                            break;

                        // Check if this module has the struct (public or internal)
                        // For library-internal types (pim.is_library_type), skip this check
                        // and search the source code directly
                        auto struct_it = mod.structs.find(pim.base_type_name);
                        auto internal_struct_it = mod.internal_structs.find(pim.base_type_name);
                        auto enum_it = mod.enums.find(pim.base_type_name);
                        bool has_type = struct_it != mod.structs.end() ||
                                        internal_struct_it != mod.internal_structs.end() ||
                                        enum_it != mod.enums.end();
                        if (!has_type && !pim.is_library_type)
                            continue;

                        TML_DEBUG_LN("[IMPL_INST]   Checking module: "
                                     << mod_name
                                     << " has_source=" << (!mod.source_code.empty() ? "yes" : "no")
                                     << " has_type=" << (has_type ? "yes" : "no"));
                        // Get parsed AST from global cache or parse if not cached

                        // Try to resolve source code: prefer stored source_code, but
                        // fall back to reading from file_path if the module binary is old
                        // (compiled before source code storage was added to the binary format).
                        std::string effective_source = mod.source_code;
                        if (effective_source.empty() && !mod.file_path.empty()) {
                            std::ifstream fallback_file(mod.file_path);
                            if (fallback_file) {
                                effective_source =
                                    std::string(std::istreambuf_iterator<char>(fallback_file),
                                                std::istreambuf_iterator<char>());
                                TML_DEBUG_LN(
                                    "[IMPL_INST]   Loaded source from disk: " << mod.file_path);
                            }
                        }

                        if (effective_source.empty()) {
                            TML_DEBUG_LN("[IMPL_INST]   Module has no source_code, skipping");
                            continue;
                        }

                        const parser::Module* parsed_mod_ptr = nullptr;

                        // Check global AST cache first
                        if (GlobalASTCache::should_cache(mod_name)) {
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                            if (parsed_mod_ptr) {
                                TML_DEBUG_LN("[IMPL_INST]   AST cache hit for: " << mod_name);
                            }
                        }

                        // If not in cache, parse the module
                        parser::Module local_parsed_mod;
                        if (!parsed_mod_ptr) {
                            auto source =
                                lexer::Source::from_string(effective_source, mod.file_path);
                            lexer::Lexer lex(source);
                            auto tokens = lex.tokenize();
                            if (lex.has_errors())
                                continue;

                            parser::Parser mod_parser(std::move(tokens));
                            auto module_name_stem = mod.name;
                            if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                                module_name_stem = module_name_stem.substr(pos + 2);
                            }
                            auto parse_result = mod_parser.parse_module(module_name_stem);
                            if (!std::holds_alternative<parser::Module>(parse_result))
                                continue;

                            local_parsed_mod = std::get<parser::Module>(std::move(parse_result));

                            // Store in global cache for library modules
                            if (GlobalASTCache::should_cache(mod_name)) {
                                GlobalASTCache::instance().put(mod_name,
                                                               std::move(local_parsed_mod));
                                parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                                TML_DEBUG_LN("[IMPL_INST]   AST cached: " << mod_name);
                            } else {
                                parsed_mod_ptr = &local_parsed_mod;
                            }
                        }

                        if (!parsed_mod_ptr)
                            continue;

                        const auto& parsed_mod = *parsed_mod_ptr;

                        // Find the impl block for this type
                        for (const auto& decl : parsed_mod.decls) {
                            if (!decl->is<parser::ImplDecl>())
                                continue;
                            const auto& impl_decl = decl->as<parser::ImplDecl>();

                            // Check if this impl is for our type
                            if (!impl_decl.self_type)
                                continue;
                            std::string impl_type_name;
                            if (impl_decl.self_type->is<parser::NamedType>()) {
                                const auto& target = impl_decl.self_type->as<parser::NamedType>();
                                if (!target.path.segments.empty())
                                    impl_type_name = target.path.segments.back();
                            } else if (impl_decl.self_type->is<parser::TupleType>()) {
                                const auto& tuple = impl_decl.self_type->as<parser::TupleType>();
                                impl_type_name = "Tuple" + std::to_string(tuple.elements.size());
                            }
                            if (impl_type_name.empty() || impl_type_name != pim.base_type_name)
                                continue;

                            // For TryFrom/From on primitive types, match the behavior type
                            // parameter e.g., for I32::try_from(I64), find impl TryFrom[I64] for
                            // I32
                            if (!pim.method_type_suffix.empty() && impl_decl.trait_type) {
                                if (impl_decl.trait_type->is<parser::NamedType>()) {
                                    const auto& trait =
                                        impl_decl.trait_type->as<parser::NamedType>();
                                    std::string trait_name = trait.path.segments.empty()
                                                                 ? ""
                                                                 : trait.path.segments.back();
                                    // Only check for TryFrom/From behaviors
                                    if ((trait_name == "TryFrom" || trait_name == "From") &&
                                        trait.generics.has_value() &&
                                        !trait.generics->args.empty()) {
                                        // Extract the behavior type parameter
                                        bool matches = false;
                                        for (const auto& arg : trait.generics->args) {
                                            if (arg.is_type() &&
                                                arg.as_type()->is<parser::NamedType>()) {
                                                const auto& arg_named =
                                                    arg.as_type()->as<parser::NamedType>();
                                                std::string arg_type_name =
                                                    arg_named.path.segments.empty()
                                                        ? ""
                                                        : arg_named.path.segments.back();
                                                if (arg_type_name == pim.method_type_suffix) {
                                                    matches = true;
                                                    break;
                                                }
                                            }
                                        }
                                        if (!matches) {
                                            continue; // Wrong behavior type parameter, skip this
                                                      // impl
                                        }
                                    }
                                }
                            }

                            TML_DEBUG_LN("[IMPL_INST]   Found impl for "
                                         << pim.base_type_name
                                         << ", methods: " << impl_decl.methods.size());
                            // Process associated type bindings from the imported impl
                            auto saved_associated_types = current_associated_types_;
                            current_associated_types_.clear();

                            // First, find associated types from concrete types in substitutions
                            for (const auto& [param_name, concrete_type] : pim.type_subs) {
                                if (concrete_type && concrete_type->is<types::NamedType>()) {
                                    const auto& concrete_named =
                                        concrete_type->as<types::NamedType>();
                                    // Check pending_generic_impls_ for local impls
                                    auto concrete_impl_it =
                                        pending_generic_impls_.find(concrete_named.name);
                                    if (concrete_impl_it != pending_generic_impls_.end()) {
                                        const auto& concrete_impl = *concrete_impl_it->second;
                                        for (const auto& concrete_binding :
                                             concrete_impl.type_bindings) {
                                            auto concrete_resolved = resolve_parser_type_with_subs(
                                                *concrete_binding.type, {});
                                            current_associated_types_[concrete_binding.name] =
                                                concrete_resolved;
                                        }
                                    }
                                }
                            }

                            // Recover type_subs from mangled_type_name.
                            // For specialized impls (e.g., impl[T,E] Outcome[Outcome[T,E], E]),
                            // the passed type_subs may use the outer type's generic params (wrong);
                            // re-derive using the impl self_type pattern.
                            auto effective_type_subs = pim.type_subs;

                            bool is_specialized_impl_imp = false;
                            if (impl_decl.self_type &&
                                impl_decl.self_type->is<parser::NamedType>()) {
                                const auto& sn = impl_decl.self_type->as<parser::NamedType>();
                                if (sn.generics.has_value()) {
                                    // If self_type has fewer generic args than impl generics,
                                    // the extra params come from where clauses, not the mangled
                                    // name.
                                    if (sn.generics->args.size() < impl_decl.generics.size()) {
                                        is_specialized_impl_imp = true;
                                    }
                                    for (const auto& arg : sn.generics->args) {
                                        if (arg.is_type() &&
                                            arg.as_type()->is<parser::NamedType>()) {
                                            const auto& an = arg.as_type()->as<parser::NamedType>();
                                            if (an.generics.has_value() &&
                                                !an.generics->args.empty()) {
                                                is_specialized_impl_imp = true;
                                                break;
                                            }
                                        }
                                        if (arg.is_type() && arg.as_type()->is<parser::RefType>()) {
                                            is_specialized_impl_imp = true;
                                            break;
                                        }
                                    }
                                }
                            }

                            if (!impl_decl.generics.empty() &&
                                pim.mangled_type_name.length() > pim.base_type_name.length() + 2) {
                                if (is_specialized_impl_imp) {
                                    // Use token-based pattern-guided extraction to correctly
                                    // handle nested generic types in specialized impls.
                                    auto tokens = tokenize_mangled(pim.mangled_type_name);
                                    size_t tok_pos = 0;
                                    std::unordered_map<std::string, types::TypePtr> new_subs;
                                    if (impl_decl.self_type) {
                                        parse_tokens_with_pattern(*impl_decl.self_type, tokens,
                                                                  tok_pos, impl_decl.generics,
                                                                  new_subs);
                                    }
                                    if (!new_subs.empty()) {
                                        effective_type_subs = new_subs;
                                    }
                                } else if (effective_type_subs.empty()) {
                                    std::string suffix =
                                        pim.mangled_type_name.substr(pim.base_type_name.length());
                                    if (suffix.starts_with("__")) {
                                        suffix = suffix.substr(2);
                                        if (impl_decl.generics.size() == 1) {
                                            auto type_arg = parse_mangled_type_string(suffix);
                                            if (type_arg) {
                                                effective_type_subs[impl_decl.generics[0].name] =
                                                    type_arg;
                                                TML_DEBUG_LN("[IMPL_INST] Recovered type_subs "
                                                             "(imported): "
                                                             << impl_decl.generics[0].name << " -> "
                                                             << suffix);
                                            }
                                        } else {
                                            std::vector<std::string> parts;
                                            size_t pos = 0;
                                            while (pos < suffix.size()) {
                                                size_t next = suffix.find("__", pos);
                                                if (next == std::string::npos) {
                                                    parts.push_back(suffix.substr(pos));
                                                    break;
                                                }
                                                parts.push_back(suffix.substr(pos, next - pos));
                                                pos = next + 2;
                                            }
                                            for (size_t i = 0;
                                                 i < impl_decl.generics.size() && i < parts.size();
                                                 ++i) {
                                                auto type_arg = parse_mangled_type_string(parts[i]);
                                                if (type_arg) {
                                                    effective_type_subs[impl_decl.generics[i]
                                                                            .name] = type_arg;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Resolve where clause type equalities for imported impls
                            resolve_where_clause_type_equalities(
                                impl_decl.where_clause, effective_type_subs,
                                [this](const types::TypePtr& concrete_type,
                                       const std::string& assoc_name) -> types::TypePtr {
                                    return resolve_assoc_type_for_concrete(concrete_type,
                                                                           assoc_name);
                                });

                            // Then resolve the impl's own type bindings
                            for (const auto& binding : impl_decl.type_bindings) {
                                auto resolved = resolve_parser_type_with_subs(*binding.type,
                                                                              effective_type_subs);
                                current_associated_types_[binding.name] = resolved;
                            }

                            // Set module context for intra-module call resolution.
                            // When a library method (e.g., Arena::alloc_raw) calls another
                            // function in the same module (e.g., align_up), the call-site
                            // lookup uses current_module_name_ to build the qualified name
                            // (e.g., "core::arena::align_up") for functions_ map lookup.
                            {
                                std::string prefix = mod_name;
                                size_t pos = 0;
                                while ((pos = prefix.find("::", pos)) != std::string::npos) {
                                    prefix.replace(pos, 2, "_");
                                    pos += 1;
                                }
                                current_module_name_ = mod_name;
                                current_module_prefix_ = prefix;
                            }

                            // Find the method
                            for (size_t mi = 0; mi < impl_decl.methods.size(); ++mi) {
                                const auto& method_decl = impl_decl.methods[mi];
                                if (method_decl.name == pim.method_name) {
                                    gen_impl_method_instantiation(
                                        pim.mangled_type_name, method_decl, effective_type_subs,
                                        impl_decl.generics, pim.method_type_suffix,
                                        pim.is_library_type, pim.base_type_name,
                                        impl_decl.self_type.get());
                                    found = true;
                                    break;
                                }
                            }

                            // If method not in impl, check if the impl's trait has a
                            // default implementation (e.g., ne/lt/le/gt/ge from
                            // PartialEq/PartialOrd)
                            if (!found && impl_decl.trait_type &&
                                impl_decl.trait_type->is<parser::NamedType>()) {
                                const auto& trait_nt =
                                    impl_decl.trait_type->as<parser::NamedType>();
                                std::string dflt_trait_name = trait_nt.path.segments.empty()
                                                                  ? ""
                                                                  : trait_nt.path.segments.back();
                                if (!dflt_trait_name.empty()) {
                                    // Find trait declaration
                                    const parser::TraitDecl* dflt_trait = nullptr;
                                    auto dti = trait_decls_.find(dflt_trait_name);
                                    if (dti != trait_decls_.end()) {
                                        dflt_trait = dti->second;
                                    }
                                    // Search in same parsed module
                                    if (!dflt_trait) {
                                        for (const auto& d : parsed_mod.decls) {
                                            if (d->is<parser::TraitDecl>() &&
                                                d->as<parser::TraitDecl>().name ==
                                                    dflt_trait_name) {
                                                dflt_trait = &d->as<parser::TraitDecl>();
                                                trait_decls_[dflt_trait_name] = dflt_trait;
                                                break;
                                            }
                                        }
                                    }
                                    // Load from behavior source files if needed
                                    if (!dflt_trait) {
                                        static const std::unordered_map<std::string, std::string>
                                            behavior_src = {
                                                {"PartialEq", "core/src/cmp"},
                                                {"Eq", "core/src/cmp"},
                                                {"PartialOrd", "core/src/cmp"},
                                                {"Ord", "core/src/cmp"},
                                                {"Iterator", "core/src/iter/traits/iterator"},
                                                {"DoubleEndedIterator",
                                                 "core/src/iter/traits/double_ended"},
                                                {"Display", "core/src/fmt/traits"},
                                                {"Debug", "core/src/fmt/traits"},
                                                {"Duplicate", "core/src/clone"},
                                                {"Clone", "core/src/clone"},
                                                {"Hash", "core/src/hash"},
                                                {"Default", "core/src/default"},
                                                {"Error", "core/src/error"},
                                                {"From", "core/src/convert"},
                                                {"Into", "core/src/convert"},
                                                {"TryFrom", "core/src/convert"},
                                                {"TryInto", "core/src/convert"},
                                            };
                                        auto bs_it = behavior_src.find(dflt_trait_name);
                                        if (bs_it != behavior_src.end()) {
                                            std::string bskey = bs_it->second;
                                            for (auto& ch : bskey)
                                                if (ch == '/')
                                                    ch = ':';
                                            std::string clean_key;
                                            std::istringstream kss(bskey);
                                            std::string seg;
                                            while (std::getline(kss, seg, ':')) {
                                                if (seg.empty() || seg == "src")
                                                    continue;
                                                if (!clean_key.empty())
                                                    clean_key += "::";
                                                clean_key += seg;
                                            }
                                            const parser::Module* tmod =
                                                GlobalASTCache::instance().get(clean_key);
                                            if (!tmod) {
                                                namespace fs = std::filesystem;
                                                std::vector<fs::path> roots = {
                                                    fs::current_path() / "lib",
                                                    fs::path("lib"),
                                                    fs::path("F:/Node/hivellm/tml/lib"),
                                                };
                                                for (const auto& lr : roots) {
                                                    fs::path sp = lr / (bs_it->second + ".tml");
                                                    if (fs::exists(sp)) {
                                                        auto sr =
                                                            lexer::Source::from_file(sp.string());
                                                        if (is_err(sr))
                                                            break;
                                                        auto src =
                                                            std::move(std::get<lexer::Source>(sr));
                                                        lexer::Lexer lx(src);
                                                        auto toks = lx.tokenize();
                                                        if (lx.has_errors())
                                                            break;
                                                        parser::Parser pp(std::move(toks));
                                                        auto res =
                                                            pp.parse_module(sp.stem().string());
                                                        if (std::holds_alternative<parser::Module>(
                                                                res)) {
                                                            GlobalASTCache::instance().put(
                                                                clean_key, std::get<parser::Module>(
                                                                               std::move(res)));
                                                            tmod = GlobalASTCache::instance().get(
                                                                clean_key);
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                            if (tmod) {
                                                for (const auto& d : tmod->decls) {
                                                    if (d->is<parser::TraitDecl>() &&
                                                        d->as<parser::TraitDecl>().name ==
                                                            dflt_trait_name) {
                                                        dflt_trait = &d->as<parser::TraitDecl>();
                                                        trait_decls_[dflt_trait_name] = dflt_trait;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    // Generate the default method from the trait
                                    if (dflt_trait) {
                                        for (const auto& tm : dflt_trait->methods) {
                                            if (tm.name == pim.method_name && tm.body.has_value()) {
                                                // Default methods may call other methods from
                                                // the same trait (e.g., ne calls eq, lt calls
                                                // partial_cmp). Ensure dependencies are
                                                // generated first.
                                                static const std::unordered_map<std::string,
                                                                                std::string>
                                                    method_deps = {
                                                        {"ne", "eq"},
                                                        {"lt", "partial_cmp"},
                                                        {"le", "partial_cmp"},
                                                        {"gt", "partial_cmp"},
                                                        {"ge", "partial_cmp"},
                                                        {"max", "cmp"},
                                                        {"min", "cmp"},
                                                        {"clamp", "cmp"},
                                                    };
                                                auto dep_it = method_deps.find(pim.method_name);
                                                if (dep_it != method_deps.end()) {
                                                    std::string dep_key = pim.mangled_type_name +
                                                                          "_" + dep_it->second;
                                                    if (functions_.find(dep_key) ==
                                                        functions_.end()) {
                                                        // Generate the dependency from impl
                                                        for (const auto& dm : impl_decl.methods) {
                                                            if (dm.name == dep_it->second) {
                                                                gen_impl_method_instantiation(
                                                                    pim.mangled_type_name, dm,
                                                                    effective_type_subs,
                                                                    impl_decl.generics,
                                                                    pim.method_type_suffix,
                                                                    pim.is_library_type,
                                                                    pim.base_type_name,
                                                                    impl_decl.self_type.get());
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }

                                                // Set up generic type subs for resolution
                                                auto saved_ts = current_type_subs_;
                                                for (const auto& [k, v] : effective_type_subs) {
                                                    current_type_subs_[k] = v;
                                                }
                                                found = generate_default_method(
                                                    pim.mangled_type_name, dflt_trait, tm,
                                                    &impl_decl);
                                                current_type_subs_ = saved_ts;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }

                            // Restore module context and associated types
                            current_module_name_ = saved_module_name;
                            current_module_prefix_ = saved_module_prefix;
                            current_associated_types_ = saved_associated_types;

                            if (found)
                                break;
                        }
                    }

                    // Fallback: search GlobalModuleCache for library modules that aren't
                    // in the local module registry (e.g., behavior impl modules loaded
                    // transitively from parent module binaries but not explicitly registered).
                    if (!found) {
                        auto cached_modules = types::GlobalModuleCache::instance().get_all();
                        for (const auto& [cached_name, cached_mod] : cached_modules) {
                            if (found)
                                break;
                            // Skip modules already in local registry (already searched above)
                            if (env_.module_registry() &&
                                env_.module_registry()->get_module(cached_name)) {
                                continue;
                            }
                            if (cached_mod.source_code.empty())
                                continue;

                            // Parse this module
                            parser::Module local_cached_parsed;
                            const parser::Module* cached_parsed_ptr = nullptr;
                            if (GlobalASTCache::should_cache(cached_name)) {
                                cached_parsed_ptr = GlobalASTCache::instance().get(cached_name);
                            }
                            if (!cached_parsed_ptr) {
                                auto src = lexer::Source::from_string(cached_mod.source_code,
                                                                      cached_mod.file_path);
                                lexer::Lexer lex(src);
                                auto tokens = lex.tokenize();
                                if (lex.has_errors())
                                    continue;
                                auto cached_name_stem = cached_name;
                                if (auto p = cached_name_stem.rfind("::"); p != std::string::npos)
                                    cached_name_stem = cached_name_stem.substr(p + 2);
                                parser::Parser mp(std::move(tokens));
                                auto res = mp.parse_module(cached_name_stem);
                                if (!std::holds_alternative<parser::Module>(res))
                                    continue;
                                local_cached_parsed = std::get<parser::Module>(std::move(res));
                                if (GlobalASTCache::should_cache(cached_name)) {
                                    GlobalASTCache::instance().put(cached_name,
                                                                   std::move(local_cached_parsed));
                                    cached_parsed_ptr = GlobalASTCache::instance().get(cached_name);
                                } else {
                                    cached_parsed_ptr = &local_cached_parsed;
                                }
                            }
                            if (!cached_parsed_ptr)
                                continue;

                            for (const auto& decl : cached_parsed_ptr->decls) {
                                if (found)
                                    break;
                                if (!decl->is<parser::ImplDecl>())
                                    continue;
                                const auto& impl_decl = decl->as<parser::ImplDecl>();
                                if (!impl_decl.self_type)
                                    continue;
                                std::string cached_impl_type_name;
                                if (impl_decl.self_type->is<parser::NamedType>()) {
                                    const auto& target =
                                        impl_decl.self_type->as<parser::NamedType>();
                                    if (!target.path.segments.empty())
                                        cached_impl_type_name = target.path.segments.back();
                                } else if (impl_decl.self_type->is<parser::TupleType>()) {
                                    const auto& tuple =
                                        impl_decl.self_type->as<parser::TupleType>();
                                    cached_impl_type_name =
                                        "Tuple" + std::to_string(tuple.elements.size());
                                }
                                if (cached_impl_type_name.empty() ||
                                    cached_impl_type_name != pim.base_type_name)
                                    continue;

                                // Resolve associated types
                                auto saved_associated_types = current_associated_types_;
                                current_associated_types_.clear();
                                for (const auto& [pname, ctype] : pim.type_subs) {
                                    if (ctype && ctype->is<types::NamedType>()) {
                                        auto ci = pending_generic_impls_.find(
                                            ctype->as<types::NamedType>().name);
                                        if (ci != pending_generic_impls_.end()) {
                                            for (const auto& b : ci->second->type_bindings) {
                                                current_associated_types_[b.name] =
                                                    resolve_parser_type_with_subs(*b.type, {});
                                            }
                                        }
                                    }
                                }
                                // For specialized impls (e.g., impl[T,E] Outcome[Outcome[T,E],E]),
                                // re-derive type subs from the mangled name and impl pattern.
                                bool is_spec_gc = false;
                                if (impl_decl.self_type &&
                                    impl_decl.self_type->is<parser::NamedType>()) {
                                    const auto& sn = impl_decl.self_type->as<parser::NamedType>();
                                    if (sn.generics.has_value()) {
                                        if (sn.generics->args.size() < impl_decl.generics.size()) {
                                            is_spec_gc = true;
                                        }
                                        for (const auto& arg : sn.generics->args) {
                                            if (arg.is_type() &&
                                                arg.as_type()->is<parser::NamedType>()) {
                                                const auto& an =
                                                    arg.as_type()->as<parser::NamedType>();
                                                if (an.generics.has_value() &&
                                                    !an.generics->args.empty()) {
                                                    is_spec_gc = true;
                                                    break;
                                                }
                                            }
                                            if (arg.is_type() &&
                                                arg.as_type()->is<parser::RefType>()) {
                                                is_spec_gc = true;
                                                break;
                                            }
                                        }
                                    }
                                }

                                auto effective_type_subs_gc = pim.type_subs;
                                if (is_spec_gc && !impl_decl.generics.empty() &&
                                    pim.mangled_type_name.length() >
                                        pim.base_type_name.length() + 2) {
                                    auto tokens = tokenize_mangled(pim.mangled_type_name);
                                    size_t tok_pos = 0;
                                    std::unordered_map<std::string, types::TypePtr> new_subs;
                                    if (impl_decl.self_type) {
                                        parse_tokens_with_pattern(*impl_decl.self_type, tokens,
                                                                  tok_pos, impl_decl.generics,
                                                                  new_subs);
                                    }
                                    if (!new_subs.empty()) {
                                        effective_type_subs_gc = new_subs;
                                    }
                                }
                                resolve_where_clause_type_equalities(
                                    impl_decl.where_clause, effective_type_subs_gc,
                                    [this](const types::TypePtr& concrete_type,
                                           const std::string& assoc_name) -> types::TypePtr {
                                        return resolve_assoc_type_for_concrete(concrete_type,
                                                                               assoc_name);
                                    });
                                for (const auto& binding : impl_decl.type_bindings) {
                                    current_associated_types_[binding.name] =
                                        resolve_parser_type_with_subs(*binding.type,
                                                                      effective_type_subs_gc);
                                }

                                // Set module context
                                std::string gc_prefix = cached_name;
                                size_t gc_pos = 0;
                                while ((gc_pos = gc_prefix.find("::", gc_pos)) !=
                                       std::string::npos) {
                                    gc_prefix.replace(gc_pos, 2, "_");
                                    gc_pos += 1;
                                }
                                current_module_name_ = cached_name;
                                current_module_prefix_ = gc_prefix;

                                for (const auto& meth : impl_decl.methods) {
                                    if (meth.name == pim.method_name) {
                                        gen_impl_method_instantiation(
                                            pim.mangled_type_name, meth, effective_type_subs_gc,
                                            impl_decl.generics, pim.method_type_suffix,
                                            pim.is_library_type, pim.base_type_name,
                                            impl_decl.self_type.get());
                                        found = true;
                                        break;
                                    }
                                }

                                // Check trait default if method not in impl
                                if (!found && impl_decl.trait_type &&
                                    impl_decl.trait_type->is<parser::NamedType>()) {
                                    const auto& gc_tn =
                                        impl_decl.trait_type->as<parser::NamedType>();
                                    std::string gc_trait = gc_tn.path.segments.empty()
                                                               ? ""
                                                               : gc_tn.path.segments.back();
                                    if (!gc_trait.empty()) {
                                        const parser::TraitDecl* gc_td = nullptr;
                                        auto gc_dti = trait_decls_.find(gc_trait);
                                        if (gc_dti != trait_decls_.end())
                                            gc_td = gc_dti->second;
                                        // Search in same cached module
                                        if (!gc_td && cached_parsed_ptr) {
                                            for (const auto& d : cached_parsed_ptr->decls) {
                                                if (d->is<parser::TraitDecl>() &&
                                                    d->as<parser::TraitDecl>().name == gc_trait) {
                                                    gc_td = &d->as<parser::TraitDecl>();
                                                    trait_decls_[gc_trait] = gc_td;
                                                    break;
                                                }
                                            }
                                        }
                                        if (gc_td) {
                                            for (const auto& tm : gc_td->methods) {
                                                if (tm.name == pim.method_name &&
                                                    tm.body.has_value()) {
                                                    auto saved_ts2 = current_type_subs_;
                                                    for (const auto& [k, v] :
                                                         effective_type_subs_gc) {
                                                        current_type_subs_[k] = v;
                                                    }
                                                    found = generate_default_method(
                                                        pim.mangled_type_name, gc_td, tm,
                                                        &impl_decl);
                                                    current_type_subs_ = saved_ts2;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }

                                current_module_name_ = saved_module_name;
                                current_module_prefix_ = saved_module_prefix;
                                current_associated_types_ = saved_associated_types;
                            }
                        }
                    }
                }
            }
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

// Recursively walk a semantic type tree and ensure all generic enums/structs are instantiated.
// This is needed for default behavior method generation where return types like
// Outcome[Unit, I64] or (I64, Maybe[I64]) may not have been instantiated yet.
void LLVMIRGen::ensure_generic_types_instantiated(const types::TypePtr& type) {
    if (!type)
        return;

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            // Try as enum first, then as struct
            if (env_.all_enums().find(named.name) != env_.all_enums().end()) {
                require_enum_instantiation(named.name, named.type_args);
            } else if (env_.lookup_struct(named.name)) {
                require_struct_instantiation(named.name, named.type_args);
            }
            // Recursively check type args
            for (const auto& arg : named.type_args) {
                ensure_generic_types_instantiated(arg);
            }
        }
    } else if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        for (const auto& elem : tuple.elements) {
            ensure_generic_types_instantiated(elem);
        }
    }
}

// Request enum instantiation - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_enum_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = enum_instantiations_.find(mangled);
    if (it != enum_instantiations_.end()) {
        return mangled;
    }

    enum_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register enum variants and generate type definition immediately
    auto decl_it = pending_generic_enums_.find(base_name);
    if (decl_it != pending_generic_enums_.end()) {
        const parser::EnumDecl* decl = decl_it->second;

        // Register variant tags with mangled enum name
        int tag = 0;
        for (const auto& variant : decl->variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }

        // Generate type definition immediately to type_defs_buffer_
        gen_enum_instantiation(*decl, type_args);
    }

    return mangled;
}

// Placeholder for function instantiation (will implement when adding generic functions)
auto LLVMIRGen::require_func_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_func_name(base_name, type_args);

    // Register the instantiation if not already registered
    if (func_instantiations_.find(mangled) == func_instantiations_.end()) {
        func_instantiations_[mangled] = GenericInstantiation{
            base_name, type_args, mangled,
            false // not generated yet
        };
        pending_func_keys_.push_back(mangled);
    }

    return mangled;
}

// Request class instantiation - returns mangled name
// Records the instantiation request; actual generation is deferred to
// generate_pending_instantiations
auto LLVMIRGen::require_class_instantiation(const std::string& base_name,
                                            const std::vector<types::TypePtr>& type_args)
    -> std::string {
    // Use the same mangling as structs for consistency
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = class_instantiations_.find(mangled);
    if (it != class_instantiations_.end()) {
        return mangled;
    }

    // Record instantiation request - generation is deferred to generate_pending_instantiations
    class_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        false // Mark as NOT generated - will be generated in generate_pending_instantiations
    };
    pending_class_keys_.push_back(mangled);

    return mangled;
}

// Generate a monomorphized class instance from a generic class declaration
void LLVMIRGen::gen_class_instantiation(const parser::ClassDecl& c,
                                        const std::vector<types::TypePtr>& type_args) {
    // Build mangled name
    std::string mangled = mangle_struct_name(c.name, type_args);

    // Skip if already generated
    if (class_types_.find(mangled) != class_types_.end()) {
        return;
    }

    // Create type substitution map
    std::unordered_map<std::string, types::TypePtr> type_subs;
    for (size_t i = 0; i < c.generics.size() && i < type_args.size(); ++i) {
        type_subs[c.generics[i].name] = type_args[i];
    }

    // Save and set current type substitutions for field type resolution
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Generate LLVM type name
    std::string type_name = "%class." + mangled;

    // Collect field types with substituted generic parameters
    std::vector<std::string> field_types;
    field_types.push_back("ptr"); // Vtable pointer is always first

    // If class extends another, include base class as embedded struct
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        field_types.push_back("%class." + base_name);
    }

    // Add own instance fields with generic substitution
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size();

    for (const auto& field : c.fields) {
        if (field.is_static)
            continue;

        // Resolve field type with generic substitution - always use substitution
        // to handle generic type parameters like T -> I32
        auto resolved = resolve_parser_type_with_subs(*field.type, type_subs);
        std::string ft = llvm_type_from_semantic(resolved);
        if (ft == "void")
            ft = "{}";
        field_types.push_back(ft);

        field_info.push_back(
            {field.name, static_cast<int>(field_offset++), ft, field.vis, false, {}});
    }

    // Emit class type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";

    // Register class type and fields
    class_types_[mangled] = type_name;
    class_fields_[mangled] = field_info;

    // Generate vtable for this instantiation
    std::string vtable_type_name = "%vtable." + mangled;
    std::string vtable_name = "@vtable." + mangled;

    // Collect virtual methods and their function names
    std::vector<std::string> vtable_func_names;
    std::vector<VirtualMethodInfo> vtable_methods;
    size_t vtable_idx = 0;
    for (const auto& method : c.methods) {
        if (method.is_virtual || method.is_abstract) {
            // Use mangled name for generic class method instantiations
            std::string method_func_name = "@" + mangle_impl_method(mangled, method.name);
            vtable_func_names.push_back(method_func_name);
            vtable_methods.push_back({method.name, mangled, mangled, vtable_idx++});
        }
    }

    if (!vtable_func_names.empty()) {
        // Generate vtable type
        std::string vtable_type = vtable_type_name + " = type { ";
        for (size_t i = 0; i < vtable_func_names.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";
        type_defs_buffer_ << vtable_type << "\n";

        // Generate vtable global
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < vtable_func_names.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            vtable_value += "ptr " + vtable_func_names[i];
        }
        vtable_value += " }";
        type_defs_buffer_ << vtable_name << " = internal constant " << vtable_type_name << " "
                          << vtable_value << "\n";
    } else {
        // Empty vtable - just emit a pointer placeholder
        type_defs_buffer_ << vtable_type_name << " = type { ptr }\n";
        type_defs_buffer_ << vtable_name << " = internal constant " << vtable_type_name
                          << " { ptr null }\n";
    }

    // Store vtable layout
    class_vtable_layout_[mangled] = vtable_methods;

    // Generate constructors with mangled name
    for (const auto& ctor : c.constructors) {
        gen_class_constructor_instantiation(c, ctor, mangled, type_subs);
    }

    // Generate methods with mangled name
    for (const auto& method : c.methods) {
        if (!method.is_abstract) {
            gen_class_method_instantiation(c, method, mangled, type_subs);
        }
    }

    // Restore type substitutions
    current_type_subs_ = saved_subs;
}

} // namespace tml::codegen
