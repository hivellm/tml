//! # LLVM IR Generator - Types
//!
//! This file implements type conversion and name mangling.
//!
//! ## Type Conversion
//!
//! | TML Type   | LLVM Type     |
//! |------------|---------------|
//! | I8, U8     | i8            |
//! | I16, U16   | i16           |
//! | I32, U32   | i32           |
//! | I64, U64   | i64           |
//! | I128, U128 | i128          |
//! | F32        | float         |
//! | F64        | double        |
//! | Bool       | i1            |
//! | Char       | i32           |
//! | Str        | ptr           |
//! | Unit       | void          |
//! | *T         | ptr           |
//! | ref T      | ptr           |
//! | Struct     | %struct.Name  |
//!
//! ## Name Mangling
//!
//! | Method              | Purpose                        |
//! |---------------------|--------------------------------|
//! | `mangle_type`       | Type name for generics         |
//! | `mangle_struct_name`| Generic struct instantiation   |
//! | `mangle_func_name`  | Generic function instantiation |
//!
//! Example: `List[I32]` becomes `List_I32`

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace tml::codegen {

auto LLVMIRGen::llvm_type_name(const std::string& name) -> std::string {
    // Primitive types
    if (name == "I8")
        return "i8";
    if (name == "I16")
        return "i16";
    if (name == "I32")
        return "i32";
    if (name == "I64")
        return "i64";
    if (name == "I128")
        return "i128";
    if (name == "U8")
        return "i8";
    if (name == "U16")
        return "i16";
    if (name == "U32")
        return "i32";
    if (name == "U64")
        return "i64";
    if (name == "U128")
        return "i128";
    if (name == "F32")
        return "float";
    if (name == "F64")
        return "double";
    if (name == "Bool")
        return "i1";
    if (name == "Char")
        return "i32";
    if (name == "Str" || name == "String")
        return "ptr"; // String is a pointer to struct
    if (name == "Unit")
        return "void";
    if (name == "Never")
        return "void"; // Never type (bottom type) - represents no value
    // Platform-sized types (64-bit on 64-bit platforms)
    if (name == "Usize")
        return "i64";
    if (name == "Isize")
        return "i64";

    // Tuple types: "(U8, U8, U8)" -> "{ i8, i8, i8 }"
    if (name.size() >= 2 && name.front() == '(' && name.back() == ')') {
        std::string inner = name.substr(1, name.size() - 2);
        if (inner.empty())
            return "{}";
        // Parse comma-separated type names
        std::string result = "{ ";
        size_t start = 0;
        bool first = true;
        while (start < inner.size()) {
            // Skip whitespace
            while (start < inner.size() && inner[start] == ' ')
                start++;
            // Find next comma or end
            size_t end = inner.find(',', start);
            if (end == std::string::npos)
                end = inner.size();
            // Extract and trim type name
            std::string elem = inner.substr(start, end - start);
            // Trim trailing whitespace
            while (!elem.empty() && elem.back() == ' ')
                elem.pop_back();
            if (!elem.empty()) {
                if (!first)
                    result += ", ";
                result += llvm_type_name(elem);
                first = false;
            }
            start = end + 1;
        }
        result += " }";
        return result;
    }

    // Ptr[T] syntax in TML uses NamedType "Ptr" - it should be a pointer type
    if (name == "Ptr")
        return "ptr";

    // Collection types - wrapper structs containing handles to runtime data
    // These are struct { ptr } where the ptr is the runtime handle
    if (name == "List" || name == "Vec" || name == "Array")
        return "%struct.List";
    if (name == "HashMap" || name == "Map" || name == "Dict")
        return "%struct.HashMap";
    if (name == "Buffer")
        return "%struct.Buffer";
    if (name == "Text")
        return "%struct.Text";
    if (name == "Channel")
        return "ptr";
    if (name == "WaitGroup")
        return "ptr";
    // Note: Mutex[T] is a generic struct now, not a runtime handle.
    // It will be handled via generic struct instantiation.

    // Check if this is a class type (via type environment or codegen registry)
    auto class_def = env_.lookup_class(name);
    if (class_def.has_value()) {
        // Value class candidates (sealed, no virtual methods) use struct type
        // for stack allocation and value semantics
        if (env_.is_value_class_candidate(name)) {
            return "%class." + name;
        }
        // Regular classes are reference types (heap allocated)
        return "ptr";
    }
    // Also check codegen's own class_types_ registry — classes from imported
    // modules that aren't directly imported by the user (e.g. exception subclasses
    // in the same module file) are registered here during Phase 1 of
    // emit_module_pure_tml_functions but aren't in env_.lookup_class().
    if (class_types_.find(name) != class_types_.end()) {
        if (value_classes_.find(name) != value_classes_.end()) {
            return "%class." + name;
        }
        return "ptr";
    }

    // Check if this is a union type
    if (union_types_.find(name) != union_types_.end()) {
        return "%union." + name;
    }

    // User-defined type - return struct type
    return "%struct." + name;
}

auto LLVMIRGen::llvm_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            std::string base_name = named.path.segments.back();
            // Handle associated types like This::Item or Self::Item
            if (named.path.segments.size() == 2) {
                const std::string& first = named.path.segments[0];
                const std::string& second = named.path.segments[1];
                if (first == "This" || first == "Self") {
                    auto assoc_it = current_associated_types_.find(second);
                    if (assoc_it != current_associated_types_.end()) {
                        return llvm_type_from_semantic(assoc_it->second);
                    }
                }
            }

            // Check if this is a generic type with type arguments
            if (named.generics.has_value() && !named.generics->args.empty()) {
                // Convert parser type args to semantic types first
                std::vector<types::TypePtr> type_args;
                for (const auto& arg : named.generics->args) {
                    if (arg.is_type()) {
                        types::TypePtr semantic_type =
                            resolve_parser_type_with_subs(*arg.as_type(), {});
                        type_args.push_back(semantic_type);
                    }
                }

                // Check if this is a known generic struct/enum - locally defined
                auto it = pending_generic_structs_.find(base_name);
                if (it != pending_generic_structs_.end()) {
                    // Get mangled name and ensure instantiation
                    std::string mangled = require_struct_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }

                // Check enum locally defined
                auto enum_it = pending_generic_enums_.find(base_name);
                if (enum_it != pending_generic_enums_.end()) {
                    std::string mangled = require_enum_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }

                // Check imported generic structs from module registry
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it = mod.structs.find(base_name);
                        if (struct_it != mod.structs.end() &&
                            !struct_it->second.type_params.empty()) {
                            // Found imported generic struct
                            std::string mangled =
                                require_struct_instantiation(base_name, type_args);
                            return "%struct." + mangled;
                        }
                        // Also check enums
                        auto import_enum_it = mod.enums.find(base_name);
                        if (import_enum_it != mod.enums.end() &&
                            !import_enum_it->second.type_params.empty()) {
                            std::string mangled = require_enum_instantiation(base_name, type_args);
                            return "%struct." + mangled;
                        }
                    }
                }

                // Check if this is a generic type alias (e.g., CryptoResult[X509Certificate])
                // Delegate to llvm_type_from_semantic which has full alias resolution
                auto sem_type =
                    std::make_shared<types::Type>(types::NamedType{base_name, "", type_args});
                return llvm_type_from_semantic(sem_type, true);
            }

            // For non-generic named types, ensure the type is defined
            // This handles structs imported from other modules
            auto sem_type = std::make_shared<types::Type>(types::NamedType{base_name, "", {}});
            return llvm_type_from_semantic(sem_type, true);
        }
    } else if (type.is<parser::RefType>()) {
        return "ptr";
    } else if (type.is<parser::PtrType>()) {
        return "ptr";
    } else if (type.is<parser::ArrayType>()) {
        // Fixed-size array: [T; N] -> [N x llvm_type(T)]
        const auto& arr = type.as<parser::ArrayType>();
        std::string elem_type = llvm_type_ptr(arr.element);

        // Get the size from the expression
        size_t arr_size = 0;
        if (arr.size && arr.size->is<parser::LiteralExpr>()) {
            const auto& lit = arr.size->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                const auto& val = lit.token.int_value();
                arr_size = static_cast<size_t>(val.value);
            }
        }

        return "[" + std::to_string(arr_size) + " x " + elem_type + "]";
    } else if (type.is<parser::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    } else if (type.is<parser::DynType>()) {
        // Dyn types are fat pointers: { data_ptr, vtable_ptr }
        const auto& dyn = type.as<parser::DynType>();
        std::string behavior_name;
        if (!dyn.behavior.segments.empty()) {
            behavior_name = dyn.behavior.segments.back();
        }
        // Ensure the dyn type is defined before use
        emit_dyn_type(behavior_name);
        return "%dyn." + behavior_name;
    } else if (type.is<parser::TupleType>()) {
        // Tuple types are anonymous structs: { type1, type2, ... }
        const auto& tuple = type.as<parser::TupleType>();
        if (tuple.elements.empty()) {
            return "{}";
        }
        std::string result = "{ ";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += llvm_type_ptr(tuple.elements[i]);
        }
        result += " }";
        return result;
    } else if (type.is<parser::ImplBehaviorType>()) {
        // impl Behavior return types - look up the concrete type from function analysis
        if (!current_func_.empty()) {
            auto it = impl_behavior_concrete_types_.find(current_func_);
            if (it != impl_behavior_concrete_types_.end()) {
                return it->second;
            }
        }
        // Fallback to pointer type
        return "ptr";
    }
    return "i32"; // Default
}

auto LLVMIRGen::llvm_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type)
        return "void";
    return llvm_type(*type);
}

auto LLVMIRGen::llvm_type_from_semantic(const types::TypePtr& type, bool for_data) -> std::string {
    if (!type)
        return for_data ? "{}" : "void";

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
        case types::PrimitiveKind::I8:
            return "i8";
        case types::PrimitiveKind::I16:
            return "i16";
        case types::PrimitiveKind::I32:
            return "i32";
        case types::PrimitiveKind::I64:
            return "i64";
        case types::PrimitiveKind::I128:
            return "i128";
        case types::PrimitiveKind::U8:
            return "i8";
        case types::PrimitiveKind::U16:
            return "i16";
        case types::PrimitiveKind::U32:
            return "i32";
        case types::PrimitiveKind::U64:
            return "i64";
        case types::PrimitiveKind::U128:
            return "i128";
        case types::PrimitiveKind::F32:
            return "float";
        case types::PrimitiveKind::F64:
            return "double";
        case types::PrimitiveKind::Bool:
            return "i1";
        case types::PrimitiveKind::Char:
            return "i32";
        case types::PrimitiveKind::Str:
            return "ptr";
        // Unit: use "{}" (empty struct) when used as data, "void" for return types
        case types::PrimitiveKind::Unit:
            return for_data ? "{}" : "void";
        // Never type (bottom type) - use void as it represents no value
        case types::PrimitiveKind::Never:
            return "void";
        }
    } else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Handle primitive type names that may appear as NamedType after generic substitution
        if (named.name == "I8")
            return "i8";
        if (named.name == "I16")
            return "i16";
        if (named.name == "I32")
            return "i32";
        if (named.name == "I64")
            return "i64";
        if (named.name == "I128")
            return "i128";
        if (named.name == "U8")
            return "i8";
        if (named.name == "U16")
            return "i16";
        if (named.name == "U32")
            return "i32";
        if (named.name == "U64")
            return "i64";
        if (named.name == "U128")
            return "i128";
        if (named.name == "F32")
            return "float";
        if (named.name == "F64")
            return "double";
        if (named.name == "Bool")
            return "i1";
        if (named.name == "Char")
            return "i32";
        if (named.name == "Str")
            return "ptr";
        if (named.name == "Unit")
            return for_data ? "{}" : "void";
        // Platform-sized types (64-bit on 64-bit platforms)
        if (named.name == "Usize")
            return "i64";
        if (named.name == "Isize")
            return "i64";

        // Never type (bottom type) - use void as it represents no value
        // Sometimes Never appears as NamedType instead of PrimitiveType
        if (named.name == "Never") {
            return "void";
        }

        // Handle unresolved associated types like "T::Owned" that were deferred from type checking
        // These need to be resolved using current type substitutions
        auto colon_pos = named.name.find("::");
        if (colon_pos != std::string::npos && !current_type_subs_.empty()) {
            std::string first_part = named.name.substr(0, colon_pos);
            std::string second_part = named.name.substr(colon_pos + 2);

            // Try to resolve the first part (e.g., "T" -> I32)
            auto it = current_type_subs_.find(first_part);
            if (it != current_type_subs_.end() && it->second) {
                const auto& concrete_type = it->second;
                // For primitives with "Owned" associated type, return the primitive LLVM type
                if (second_part == "Owned" && concrete_type->is<types::PrimitiveType>()) {
                    return llvm_type_from_semantic(concrete_type, for_data);
                }
                // For named types, look up the associated type
                if (concrete_type->is<types::NamedType>()) {
                    const auto& concrete_named = concrete_type->as<types::NamedType>();
                    auto assoc_type = lookup_associated_type(concrete_named.name, second_part);
                    if (assoc_type) {
                        return llvm_type_from_semantic(assoc_type, for_data);
                    }
                }
            }
        }

        // Ptr[T] in TML syntax is represented as NamedType with name "Ptr"
        // It should be lowered to "ptr" in LLVM IR
        if (named.name == "Ptr") {
            return "ptr";
        }

        // Check if this NamedType is actually a class type (can happen when
        // method return types are resolved before the class is fully registered)
        auto class_def = env_.lookup_class(named.name);
        if (class_def.has_value()) {
            // Value class candidates (sealed, no virtual methods) use struct type
            // for stack allocation and value semantics
            if (env_.is_value_class_candidate(named.name)) {
                return "%class." + named.name;
            }
            // Regular classes are reference types - variables store pointers
            return "ptr";
        }
        // Also check codegen's class_types_ registry for non-imported classes
        if (class_types_.find(named.name) != class_types_.end()) {
            if (value_classes_.find(named.name) != value_classes_.end()) {
                return "%class." + named.name;
            }
            return "ptr";
        }

        // Runtime-managed wrapper types - these are small structs containing handles
        // to runtime-allocated data. Return their struct type to match function definitions.
        // This ensures consistency between function definitions (using llvm_type) and
        // call instructions (using llvm_type_from_semantic).
        if (named.name == "Text") {
            return "%struct.Text";
        }
        if (named.name == "Buffer") {
            return "%struct.Buffer";
        }
        // List/HashMap are generic types that get instantiated - skip here
        // Channel/WaitGroup are pure runtime handles (opaque pointers)
        // Note: Mutex[T] is now a generic struct, handled via require_struct_instantiation
        if (named.name == "Channel" || named.name == "WaitGroup") {
            return "ptr";
        }

        // If it has type arguments, need to use mangled name and ensure instantiation
        if (!named.type_args.empty()) {
            // Check if any type argument contains unresolved generic parameters
            // If so, try to apply current type substitutions first
            std::vector<types::TypePtr> resolved_type_args = named.type_args;
            bool has_unresolved = false;
            for (const auto& arg : named.type_args) {
                if (contains_unresolved_generic(arg)) {
                    has_unresolved = true;
                    break;
                }
            }

            // If there are unresolved generics, try to apply current_type_subs_
            if (has_unresolved && !current_type_subs_.empty()) {
                resolved_type_args.clear();
                for (const auto& arg : named.type_args) {
                    resolved_type_args.push_back(apply_type_substitutions(arg, current_type_subs_));
                }
                // Re-check if still has unresolved generics
                has_unresolved = false;
                for (const auto& arg : resolved_type_args) {
                    if (contains_unresolved_generic(arg)) {
                        has_unresolved = true;
                        break;
                    }
                }
            }

            // If still has unresolved generics after substitution, skip instantiation
            // This prevents creating invalid struct types with incomplete type arguments
            if (has_unresolved) {
                // Return opaque pointer type - the struct will be instantiated later
                // when called with concrete types
                return "ptr";
            }

            // Check if this is a generic type alias (e.g., CryptoResult[SecretKey])
            // If so, resolve the alias body with substituted type args
            auto alias_type = env_.lookup_type_alias(named.name);
            auto alias_generics = env_.lookup_type_alias_generics(named.name);
            // If not found in local env, search all modules in registry
            if ((!alias_type || !alias_generics) && env_.module_registry()) {
                const auto& all_mods = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_mods) {
                    auto ta_it = mod.type_aliases.find(named.name);
                    auto tg_it = mod.type_alias_generics.find(named.name);
                    if (ta_it != mod.type_aliases.end() && tg_it != mod.type_alias_generics.end()) {
                        alias_type = ta_it->second;
                        alias_generics = tg_it->second;
                        break;
                    }
                }
            }
            if (alias_type && alias_generics && !alias_generics->empty()) {
                // Build substitution map
                std::unordered_map<std::string, types::TypePtr> subs;
                for (size_t i = 0; i < alias_generics->size() && i < resolved_type_args.size();
                     ++i) {
                    subs[(*alias_generics)[i]] = resolved_type_args[i];
                }
                auto resolved = types::substitute_type(*alias_type, subs);
                return llvm_type_from_semantic(resolved, for_data);
            }

            // Check if it's a generic enum (like Maybe, Outcome)
            auto enum_it = pending_generic_enums_.find(named.name);
            if (enum_it != pending_generic_enums_.end()) {
                std::string mangled = require_enum_instantiation(named.name, resolved_type_args);
                return "%struct." + mangled;
            }
            // Otherwise try as struct
            std::string mangled = require_struct_instantiation(named.name, resolved_type_args);
            return "%struct." + mangled;
        }

        // For non-generic structs, ensure the type is defined before use
        // This handles structs from imported modules
        if (struct_types_.find(named.name) == struct_types_.end()) {
            bool found = false;
            // Try to find struct definition from module registry
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto struct_it = mod.structs.find(named.name);
                    if (struct_it != mod.structs.end()) {
                        const auto& struct_def = struct_it->second;
                        // Emit the struct type definition
                        std::string type_name = "%struct." + named.name;
                        std::string def = type_name + " = type { ";
                        bool first = true;
                        for (const auto& field : struct_def.fields) {
                            if (!first)
                                def += ", ";
                            first = false;
                            std::string ft = llvm_type_from_semantic(field.type, true);
                            // Function pointer fields use fat pointer to support closures
                            if (field.type && field.type->is<types::FuncType>()) {
                                ft = "{ ptr, ptr }";
                            }
                            def += ft;
                        }
                        def += " }";
                        type_defs_buffer_ << def << "\n";
                        struct_types_[named.name] = type_name;

                        // Also register fields
                        std::vector<FieldInfo> fields;
                        for (size_t i = 0; i < struct_def.fields.size(); ++i) {
                            std::string ft =
                                llvm_type_from_semantic(struct_def.fields[i].type, true);
                            // Function pointer fields use fat pointer to support closures
                            if (struct_def.fields[i].type &&
                                struct_def.fields[i].type->is<types::FuncType>()) {
                                ft = "{ ptr, ptr }";
                            }
                            fields.push_back({struct_def.fields[i].name, static_cast<int>(i), ft,
                                              struct_def.fields[i].type});
                        }
                        struct_fields_[named.name] = fields;
                        found = true;
                        break;
                    }
                }

                // If not found in public structs, try internal_structs
                if (!found) {
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto internal_it = mod.internal_structs.find(named.name);
                        if (internal_it != mod.internal_structs.end()) {
                            const auto& struct_def = internal_it->second;
                            // Emit the struct type definition
                            std::string type_name = "%struct." + named.name;
                            std::string def = type_name + " = type { ";
                            bool first = true;
                            for (const auto& field : struct_def.fields) {
                                if (!first)
                                    def += ", ";
                                first = false;
                                std::string ft = llvm_type_from_semantic(field.type, true);
                                // Function pointer fields use fat pointer to support closures
                                if (field.type && field.type->is<types::FuncType>()) {
                                    ft = "{ ptr, ptr }";
                                }
                                def += ft;
                            }
                            def += " }";
                            type_defs_buffer_ << def << "\n";
                            struct_types_[named.name] = type_name;

                            // Also register fields
                            std::vector<FieldInfo> fields;
                            for (size_t i = 0; i < struct_def.fields.size(); ++i) {
                                std::string ft =
                                    llvm_type_from_semantic(struct_def.fields[i].type, true);
                                // Function pointer fields use fat pointer to support closures
                                if (struct_def.fields[i].type &&
                                    struct_def.fields[i].type->is<types::FuncType>()) {
                                    ft = "{ ptr, ptr }";
                                }
                                fields.push_back({struct_def.fields[i].name, static_cast<int>(i),
                                                  ft, struct_def.fields[i].type});
                            }
                            struct_fields_[named.name] = fields;
                            found = true;
                            break;
                        }
                    }
                }

                // If still not found, try re-parsing module source to find
                // private structs (needed for types like RawRwLock used as field types)
                // Skip if we already know this type can't be found (negative cache)
                if (!found && not_found_struct_types_.count(named.name) == 0) {
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod.source_code.empty())
                            continue;

                        // Re-parse the module to find the struct
                        auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        if (lex.has_errors())
                            continue;

                        parser::Parser mod_parser(std::move(tokens));
                        std::string module_name_stem = mod.name;
                        if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                            module_name_stem = module_name_stem.substr(pos + 2);
                        }
                        auto parse_result = mod_parser.parse_module(module_name_stem);
                        if (!std::holds_alternative<parser::Module>(parse_result))
                            continue;

                        const auto& parsed_mod = std::get<parser::Module>(parse_result);

                        // Find the struct (including private ones)
                        for (const auto& decl : parsed_mod.decls) {
                            if (!decl->is<parser::StructDecl>())
                                continue;
                            const auto& struct_decl = decl->as<parser::StructDecl>();
                            if (struct_decl.name != named.name)
                                continue;

                            // Found the struct - emit its type definition
                            std::string type_name = "%struct." + named.name;
                            std::string def = type_name + " = type { ";
                            bool first = true;
                            std::vector<FieldInfo> fields;
                            int field_idx = 0;
                            for (const auto& field : struct_decl.fields) {
                                if (!first)
                                    def += ", ";
                                first = false;
                                // Resolve field type
                                types::TypePtr field_type =
                                    resolve_parser_type_with_subs(*field.type, {});
                                std::string ft = llvm_type_from_semantic(field_type, true);
                                // Function pointer fields use fat pointer to support closures
                                if (field_type && field_type->is<types::FuncType>()) {
                                    ft = "{ ptr, ptr }";
                                }
                                def += ft;
                                fields.push_back({field.name, field_idx++, ft, field_type});
                            }
                            def += " }";
                            type_defs_buffer_ << def << "\n";
                            struct_types_[named.name] = type_name;
                            struct_fields_[named.name] = fields;
                            found = true;
                            break;
                        }
                        if (found)
                            break;
                    }
                    // Cache negative result to avoid re-parsing for this type again
                    // (e.g., enum types like "Ordering" will never be found as structs)
                    if (!found) {
                        not_found_struct_types_.insert(named.name);
                    }
                }
            }
        }

        // Check if this is a union type
        if (union_types_.find(named.name) != union_types_.end()) {
            return "%union." + named.name;
        }

        // Check if this NamedType is actually a generic type parameter that should be
        // substituted. TypeEnv sometimes stores generic params as NamedType("T") instead
        // of GenericType("T"). Without this check, unsubstituted params produce
        // "%struct.T" which is invalid in monomorphized code.
        if (!current_type_subs_.empty()) {
            auto sub_it = current_type_subs_.find(named.name);
            if (sub_it != current_type_subs_.end() && sub_it->second) {
                return llvm_type_from_semantic(sub_it->second, for_data);
            }
        }

        return "%struct." + named.name;
    } else if (type->is<types::GenericType>()) {
        // Check if there's a substitution available for this generic type parameter
        const auto& generic = type->as<types::GenericType>();
        auto it = current_type_subs_.find(generic.name);
        if (it != current_type_subs_.end() && it->second) {
            // Substitute the generic with its concrete type
            return llvm_type_from_semantic(it->second, for_data);
        }
        // Fallback: uninstantiated generic type - shouldn't happen in codegen normally
        // Return a placeholder (will cause error if actually used)
        return "i32";
    } else if (type->is<types::RefType>() || type->is<types::PtrType>()) {
        return "ptr";
    } else if (type->is<types::TupleType>()) {
        // Tuple types are anonymous structs in LLVM: { element1, element2, ... }
        const auto& tuple = type->as<types::TupleType>();
        if (tuple.elements.empty()) {
            return "{}";
        }
        std::string result = "{ ";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += llvm_type_from_semantic(tuple.elements[i], true);
        }
        result += " }";
        return result;
    } else if (type->is<types::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    } else if (type->is<types::DynBehaviorType>()) {
        // Trait objects are fat pointers: { data_ptr, vtable_ptr }
        // We use a struct type: %dyn.BehaviorName
        const auto& dyn = type->as<types::DynBehaviorType>();
        // Ensure the dyn type is defined before use
        emit_dyn_type(dyn.behavior_name);
        return "%dyn." + dyn.behavior_name;
    } else if (type->is<types::ImplBehaviorType>()) {
        // impl Behavior return types - the concrete type should be determined from
        // analyzing the function body. Check if we have a stored concrete type.
        // For now, if we're in a function context, check impl_behavior_concrete_types_.
        if (!current_func_.empty()) {
            auto it = impl_behavior_concrete_types_.find(current_func_);
            if (it != impl_behavior_concrete_types_.end()) {
                return it->second;
            }
        }
        // Fallback: use pointer type (caller should handle casting)
        // This is a placeholder - proper impl Behavior handling requires analyzing
        // the function body to determine the concrete return type.
        return "ptr";
    } else if (type->is<types::ArrayType>()) {
        // Fixed-size arrays: [T; N] -> [N x llvm_type(T)]
        const auto& arr = type->as<types::ArrayType>();
        std::string elem_type = llvm_type_from_semantic(arr.element, true);
        return "[" + std::to_string(arr.size) + " x " + elem_type + "]";
    } else if (type->is<types::SliceType>()) {
        // Slices are fat pointers: { ptr, i64 } - data pointer and length
        return "{ ptr, i64 }";
    } else if (type->is<types::ClassType>()) {
        // Class types are reference types - variables store pointers to heap-allocated instances
        // The class struct is %class.ClassName, but variables are pointers (ptr)
        return "ptr";
    }

    return "i32"; // Default
}

// ============ Type Definition Ensuring ============
// Ensures that a type is defined in the LLVM IR output before it's used

void LLVMIRGen::ensure_type_defined(const parser::TypePtr& type) {
    if (!type)
        return;

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        std::string base_name;
        if (!named.path.segments.empty()) {
            base_name = named.path.segments.back();
        } else {
            return;
        }

        // Skip primitive types
        if (base_name == "I8" || base_name == "I16" || base_name == "I32" || base_name == "I64" ||
            base_name == "I128" || base_name == "U8" || base_name == "U16" || base_name == "U32" ||
            base_name == "U64" || base_name == "U128" || base_name == "F32" || base_name == "F64" ||
            base_name == "Bool" || base_name == "Char" || base_name == "Str" ||
            base_name == "Unit" || base_name == "Never" || base_name == "Ptr" ||
            base_name == "Usize" || base_name == "Isize") {
            return;
        }

        // Skip if already defined
        if (struct_types_.find(base_name) != struct_types_.end()) {
            return;
        }

        // Try to find and emit the type from the module registry
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                // Check for struct
                auto struct_it = mod.structs.find(base_name);
                if (struct_it != mod.structs.end()) {
                    // For structs with type params (generic), skip - will be instantiated later
                    if (!struct_it->second.type_params.empty()) {
                        return;
                    }
                    // Emit non-generic struct type
                    std::string type_name = "%struct." + base_name;
                    std::string def = type_name + " = type { ";
                    bool first = true;
                    for (const auto& fld : struct_it->second.fields) {
                        if (!first)
                            def += ", ";
                        first = false;
                        std::string ft = llvm_type_from_semantic(fld.type, true);
                        // Function pointer fields use fat pointer to support closures
                        if (fld.type && fld.type->is<types::FuncType>()) {
                            ft = "{ ptr, ptr }";
                        }
                        def += ft;
                    }
                    def += " }";
                    type_defs_buffer_ << def << "\n";
                    struct_types_[base_name] = type_name;

                    // Register fields
                    std::vector<FieldInfo> fields;
                    for (size_t i = 0; i < struct_it->second.fields.size(); ++i) {
                        std::string ft =
                            llvm_type_from_semantic(struct_it->second.fields[i].type, true);
                        // Function pointer fields use fat pointer to support closures
                        if (struct_it->second.fields[i].type &&
                            struct_it->second.fields[i].type->is<types::FuncType>()) {
                            ft = "{ ptr, ptr }";
                        }
                        fields.push_back({struct_it->second.fields[i].name, static_cast<int>(i), ft,
                                          struct_it->second.fields[i].type});
                    }
                    struct_fields_[base_name] = fields;
                    return;
                }

                // Check for enum
                auto enum_it = mod.enums.find(base_name);
                if (enum_it != mod.enums.end()) {
                    // For enums with type params (generic), skip - will be instantiated later
                    if (!enum_it->second.type_params.empty()) {
                        return;
                    }
                    // Emit non-generic enum type (simple enum = struct { i32 })
                    std::string type_name = "%struct." + base_name;
                    type_defs_buffer_ << type_name << " = type { i32 }\n";
                    struct_types_[base_name] = type_name;

                    // Register variant values
                    int tag = 0;
                    for (const auto& [variant_name, _payload] : enum_it->second.variants) {
                        std::string key = base_name + "::" + variant_name;
                        enum_variants_[key] = tag++;
                    }
                    return;
                }
            }
        }
    }
}

// ============ Generic Type Mangling ============
// Converts type to mangled string for LLVM IR names
// e.g., I32 -> "I32", List[I32] -> "List__I32", HashMap[Str, Bool] -> "HashMap__Str__Bool"

auto LLVMIRGen::mangle_type(const types::TypePtr& type) -> std::string {
    if (!type)
        return "void";

    if (type->is<types::PrimitiveType>()) {
        auto kind = type->as<types::PrimitiveType>().kind;
        // Special handling for Unit and Never - symbols invalid in LLVM identifiers
        if (kind == types::PrimitiveKind::Unit)
            return "Unit";
        if (kind == types::PrimitiveKind::Never)
            return "Never";
        return types::primitive_kind_to_string(kind);
    } else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Handle unresolved associated types like "T::Owned" that were deferred from type checking
        // These need to be resolved using current type substitutions
        auto colon_pos = named.name.find("::");
        if (colon_pos != std::string::npos && !current_type_subs_.empty()) {
            std::string first_part = named.name.substr(0, colon_pos);
            std::string second_part = named.name.substr(colon_pos + 2);

            // Try to resolve the first part (e.g., "T" -> I32)
            auto it = current_type_subs_.find(first_part);
            if (it != current_type_subs_.end() && it->second) {
                const auto& concrete_type = it->second;
                // For primitives with "Owned" associated type, return the primitive itself
                if (second_part == "Owned" && concrete_type->is<types::PrimitiveType>()) {
                    return mangle_type(concrete_type);
                }
                // For named types, look up the associated type
                if (concrete_type->is<types::NamedType>()) {
                    const auto& concrete_named = concrete_type->as<types::NamedType>();
                    auto assoc_type = lookup_associated_type(concrete_named.name, second_part);
                    if (assoc_type) {
                        return mangle_type(assoc_type);
                    }
                }
            }
        }

        // Handle Ptr[T] stored as NamedType - convert to ptr_ prefix for consistency
        // This ensures consistent mangling whether Ptr comes as NamedType or PtrType
        if (named.name == "Ptr" && !named.type_args.empty()) {
            return "ptr_" + mangle_type_args(named.type_args);
        }

        if (named.type_args.empty()) {
            return named.name;
        }
        // Mangle with type arguments: List[I32] -> List__I32
        return named.name + "__" + mangle_type_args(named.type_args);
    } else if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        return (ref.is_mut ? "mutref_" : "ref_") + mangle_type(ref.inner);
    } else if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        return (ptr.is_mut ? "mutptr_" : "ptr_") + mangle_type(ptr.inner);
    } else if (type->is<types::DynBehaviorType>()) {
        const auto& dyn = type->as<types::DynBehaviorType>();
        if (dyn.type_args.empty()) {
            return "dyn_" + dyn.behavior_name;
        }
        return "dyn_" + dyn.behavior_name + "__" + mangle_type_args(dyn.type_args);
    } else if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        return "arr_" + mangle_type(arr.element) + "_" + std::to_string(arr.size);
    } else if (type->is<types::TupleType>()) {
        // Tuple type: (A, B, C) -> "tuple_A_B_C"
        const auto& tuple = type->as<types::TupleType>();
        if (tuple.elements.empty()) {
            return "Unit"; // () is semantically identical to Unit
        }
        std::string result = "tuple";
        for (const auto& elem : tuple.elements) {
            result += "_" + mangle_type(elem);
        }
        return result;
    } else if (type->is<types::GenericType>()) {
        // Check if there's a substitution available for this generic type parameter
        const auto& generic = type->as<types::GenericType>();
        auto it = current_type_subs_.find(generic.name);
        if (it != current_type_subs_.end() && it->second) {
            // Substitute the generic with its concrete type and mangle that
            return mangle_type(it->second);
        }
        // Fallback: uninstantiated generic - shouldn't reach codegen normally
        return generic.name;
    } else if (type->is<types::FuncType>()) {
        // Function types are opaque pointers in LLVM, mangle as "Fn"
        return "Fn";
    }

    return "unknown";
}

auto LLVMIRGen::mangle_type_args(const std::vector<types::TypePtr>& args) -> std::string {
    std::string result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            result += "__";
        result += mangle_type(args[i]);
    }
    return result;
}

auto LLVMIRGen::mangle_struct_name(const std::string& base_name,
                                   const std::vector<types::TypePtr>& type_args) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

auto LLVMIRGen::mangle_func_name(const std::string& base_name,
                                 const std::vector<types::TypePtr>& type_args) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

} // namespace tml::codegen
