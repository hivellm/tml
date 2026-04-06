TML_MODULE("codegen_x86")

//! # LLVM IR Generator - generate_pending_impl_method_instantiations
//!
//! This file implements `LLVMIRGen::generate_pending_impl_method_instantiations`,
//! the inner loop from `generate_pending_instantiations` that handles all pending
//! impl method instantiations.
//!
//! Extracted from generic_instantiate.cpp to keep that file under ~900 lines.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"

#include <fstream>
#include <functional>

namespace tml::codegen {

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
        // Closures arise when callers pass `do() -> T { ... }` lambdas to a function
        // with a `where F = func() -> T` clause — F is bound to ClosureType, not FuncType.
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

// ============ Generate Pending Impl Method Instantiations ============
// Iteratively process all pending impl method instantiations until the queue is empty.
// Returns true if any methods were generated (so the caller can set changed=true).

bool LLVMIRGen::generate_pending_impl_method_instantiations() {
    // Track processed methods to avoid duplicate lookups (expensive module searches)
    std::unordered_set<std::string> processed_impl_methods;

    // Save module context — impl method instantiation may need to set module context
    // for intra-module call resolution (e.g., Arena::alloc_raw calling align_up needs
    // current_module_name_="core::arena" so the qualified lookup finds the mangled name).
    auto saved_module_prefix = current_module_prefix_;
    auto saved_module_name = current_module_name_;
    auto saved_submodule = current_submodule_name_;

    bool any_generated = false;
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
            std::string generated_key = mangle_impl_method(pim.mangled_type_name, method_name_full);
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
                        << " methods, has_method=" << (has_method ? "yes" : "no") << ", generics=";
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
                                for (const auto& concrete_binding : concrete_impl.type_bindings) {
                                    auto concrete_resolved =
                                        resolve_parser_type_with_subs(*concrete_binding.type, {});
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
                                    const auto& arg_named = arg.as_type()->as<parser::NamedType>();
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
                                // Exception: don't override a rich FuncType OR ClosureType
                                // (with actual params/return) with a degenerate
                                // NamedType("Fn") recovered from mangling, which loses all
                                // signature information. Closures arise when callers pass
                                // `do() -> T { ... }` lambdas to functions with `where F =
                                // func() -> T` clauses (e.g., OnceWith, RepeatWith). Without
                                // this guard, the where-clause T binding is lost and the
                                // method body emits Maybe[T] instead of Maybe[I32].
                                for (const auto& [k, v] : new_subs) {
                                    auto existing = effective_type_subs.find(k);
                                    if (existing != effective_type_subs.end() && existing->second &&
                                        v &&
                                        (existing->second->is<types::FuncType>() ||
                                         existing->second->is<types::ClosureType>()) &&
                                        v->is<types::NamedType>() &&
                                        v->as<types::NamedType>().name == "Fn") {
                                        continue; // Keep the richer FuncType/ClosureType entry
                                    }
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
                                                     << impl.generics[0].name << " -> " << suffix);
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
                                    for (size_t i = 0; i < impl.generics.size() && i < parts.size();
                                         ++i) {
                                        auto type_arg = parse_mangled_type_string(parts[i]);
                                        if (type_arg) {
                                            effective_type_subs[impl.generics[i].name] = type_arg;
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
                        auto source = lexer::Source::from_string(effective_source, mod.file_path);
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
                            GlobalASTCache::instance().put(mod_name, std::move(local_parsed_mod));
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                            TML_DEBUG_LN("[IMPL_INST]   AST cached: " << mod_name);
                        } else {
                            parsed_mod_ptr = &local_parsed_mod;
                        }
                    }

                    if (!parsed_mod_ptr)
                        continue;

                    const auto& parsed_mod = *parsed_mod_ptr;

                    // Register any generic structs/enums from this module into
                    // pending_generic_structs_ so that require_struct_instantiation
                    // can find them. This is needed when the method being instantiated
                    // references a generic struct from its own module (e.g., MaybeIter[T]
                    // returned by Maybe[T]::iter()).
                    for (const auto& d : parsed_mod.decls) {
                        if (d->is<parser::StructDecl>()) {
                            const auto& s = d->as<parser::StructDecl>();
                            if (!s.generics.empty() &&
                                pending_generic_structs_.find(s.name) ==
                                    pending_generic_structs_.end() &&
                                local_generic_struct_names_.find(s.name) ==
                                    local_generic_struct_names_.end()) {
                                pending_generic_structs_[s.name] = &s;
                                if (struct_decls_.find(s.name) == struct_decls_.end()) {
                                    struct_decls_[s.name] = &s;
                                }
                            }
                        } else if (d->is<parser::EnumDecl>()) {
                            const auto& e = d->as<parser::EnumDecl>();
                            if (!e.generics.empty() && pending_generic_enums_.find(e.name) ==
                                                           pending_generic_enums_.end()) {
                                pending_generic_enums_[e.name] = &e;
                            }
                        }
                    }

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
                                const auto& trait = impl_decl.trait_type->as<parser::NamedType>();
                                std::string trait_name =
                                    trait.path.segments.empty() ? "" : trait.path.segments.back();
                                // Only check for TryFrom/From behaviors
                                if ((trait_name == "TryFrom" || trait_name == "From") &&
                                    trait.generics.has_value() && !trait.generics->args.empty()) {
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
                                const auto& concrete_named = concrete_type->as<types::NamedType>();
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
                        if (impl_decl.self_type && impl_decl.self_type->is<parser::NamedType>()) {
                            const auto& sn = impl_decl.self_type->as<parser::NamedType>();
                            if (sn.generics.has_value()) {
                                // If self_type has fewer generic args than impl generics,
                                // the extra params come from where clauses, not the mangled
                                // name.
                                if (sn.generics->args.size() < impl_decl.generics.size()) {
                                    is_specialized_impl_imp = true;
                                }
                                for (const auto& arg : sn.generics->args) {
                                    if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                                        const auto& an = arg.as_type()->as<parser::NamedType>();
                                        if (an.generics.has_value() && !an.generics->args.empty()) {
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
                                    parse_tokens_with_pattern(*impl_decl.self_type, tokens, tok_pos,
                                                              impl_decl.generics, new_subs);
                                }
                                if (!new_subs.empty()) {
                                    // Merge new_subs into effective_type_subs, preserving
                                    // rich FuncType/ClosureType entries over degenerate
                                    // NamedType("Fn"). Closures arise from `do() -> T { }`
                                    // lambdas passed to where-F=func() generic constructors.
                                    for (const auto& [k, v] : new_subs) {
                                        auto existing = effective_type_subs.find(k);
                                        if (existing != effective_type_subs.end() &&
                                            existing->second && v &&
                                            (existing->second->is<types::FuncType>() ||
                                             existing->second->is<types::ClosureType>()) &&
                                            v->is<types::NamedType>() &&
                                            v->as<types::NamedType>().name == "Fn") {
                                            continue; // Keep the richer FuncType/ClosureType entry
                                        }
                                        effective_type_subs[k] = v;
                                    }
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
                                                effective_type_subs[impl_decl.generics[i].name] =
                                                    type_arg;
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
                                return resolve_assoc_type_for_concrete(concrete_type, assoc_name);
                            });

                        // Then resolve the impl's own type bindings
                        for (const auto& binding : impl_decl.type_bindings) {
                            auto resolved =
                                resolve_parser_type_with_subs(*binding.type, effective_type_subs);
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
                                    impl_decl.generics, pim.method_type_suffix, pim.is_library_type,
                                    pim.base_type_name, impl_decl.self_type.get());
                                found = true;
                                break;
                            }
                        }

                        // If method not in impl, check if the impl's trait has a
                        // default implementation (e.g., ne/lt/le/gt/ge from
                        // PartialEq/PartialOrd)
                        if (!found && impl_decl.trait_type &&
                            impl_decl.trait_type->is<parser::NamedType>()) {
                            const auto& trait_nt = impl_decl.trait_type->as<parser::NamedType>();
                            std::string dflt_trait_name =
                                trait_nt.path.segments.empty() ? "" : trait_nt.path.segments.back();
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
                                            d->as<parser::TraitDecl>().name == dflt_trait_name) {
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
                                                    auto sr = lexer::Source::from_file(sp.string());
                                                    if (is_err(sr))
                                                        break;
                                                    auto src =
                                                        std::move(std::get<lexer::Source>(sr));
                                                    lexer::Lexer lx(src);
                                                    auto toks = lx.tokenize();
                                                    if (lx.has_errors())
                                                        break;
                                                    parser::Parser pp(std::move(toks));
                                                    auto res = pp.parse_module(sp.stem().string());
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
                                                    {"ne", "eq"},          {"lt", "partial_cmp"},
                                                    {"le", "partial_cmp"}, {"gt", "partial_cmp"},
                                                    {"ge", "partial_cmp"}, {"max", "cmp"},
                                                    {"min", "cmp"},        {"clamp", "cmp"},
                                                };
                                            auto dep_it = method_deps.find(pim.method_name);
                                            if (dep_it != method_deps.end()) {
                                                std::string dep_key =
                                                    pim.mangled_type_name + "_" + dep_it->second;
                                                if (functions_.find(dep_key) == functions_.end()) {
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
                                                pim.mangled_type_name, dflt_trait, tm, &impl_decl,
                                                pim.method_type_suffix);
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
                                const auto& target = impl_decl.self_type->as<parser::NamedType>();
                                if (!target.path.segments.empty())
                                    cached_impl_type_name = target.path.segments.back();
                            } else if (impl_decl.self_type->is<parser::TupleType>()) {
                                const auto& tuple = impl_decl.self_type->as<parser::TupleType>();
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
                                            const auto& an = arg.as_type()->as<parser::NamedType>();
                                            if (an.generics.has_value() &&
                                                !an.generics->args.empty()) {
                                                is_spec_gc = true;
                                                break;
                                            }
                                        }
                                        if (arg.is_type() && arg.as_type()->is<parser::RefType>()) {
                                            is_spec_gc = true;
                                            break;
                                        }
                                    }
                                }
                            }

                            auto effective_type_subs_gc = pim.type_subs;
                            if (is_spec_gc && !impl_decl.generics.empty() &&
                                pim.mangled_type_name.length() > pim.base_type_name.length() + 2) {
                                auto tokens = tokenize_mangled(pim.mangled_type_name);
                                size_t tok_pos = 0;
                                std::unordered_map<std::string, types::TypePtr> new_subs;
                                if (impl_decl.self_type) {
                                    parse_tokens_with_pattern(*impl_decl.self_type, tokens, tok_pos,
                                                              impl_decl.generics, new_subs);
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
                            while ((gc_pos = gc_prefix.find("::", gc_pos)) != std::string::npos) {
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
                                const auto& gc_tn = impl_decl.trait_type->as<parser::NamedType>();
                                std::string gc_trait =
                                    gc_tn.path.segments.empty() ? "" : gc_tn.path.segments.back();
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
                                            if (tm.name == pim.method_name && tm.body.has_value()) {
                                                auto saved_ts2 = current_type_subs_;
                                                for (const auto& [k, v] : effective_type_subs_gc) {
                                                    current_type_subs_[k] = v;
                                                }
                                                found = generate_default_method(
                                                    pim.mangled_type_name, gc_td, tm, &impl_decl,
                                                    pim.method_type_suffix);
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
        any_generated = true;
    }

    current_module_prefix_ = saved_module_prefix;
    current_module_name_ = saved_module_name;
    current_submodule_name_ = saved_submodule;

    return any_generated;
}

} // namespace tml::codegen
