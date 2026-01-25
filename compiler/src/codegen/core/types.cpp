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

#include "codegen/llvm_ir_gen.hpp"
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

    // Check if this is a class type
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
            }

            return llvm_type_name(base_name);
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
            // Check if it's a generic enum (like Maybe, Outcome)
            auto enum_it = pending_generic_enums_.find(named.name);
            if (enum_it != pending_generic_enums_.end()) {
                std::string mangled = require_enum_instantiation(named.name, named.type_args);
                return "%struct." + mangled;
            }
            // Otherwise try as struct
            std::string mangled = require_struct_instantiation(named.name, named.type_args);
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
                        for (const auto& [field_name, field_type] : struct_def.fields) {
                            if (!first)
                                def += ", ";
                            first = false;
                            def += llvm_type_from_semantic(field_type, true);
                        }
                        def += " }";
                        type_defs_buffer_ << def << "\n";
                        struct_types_[named.name] = type_name;

                        // Also register fields
                        std::vector<FieldInfo> fields;
                        for (size_t i = 0; i < struct_def.fields.size(); ++i) {
                            std::string ft =
                                llvm_type_from_semantic(struct_def.fields[i].second, true);
                            fields.push_back({struct_def.fields[i].first, static_cast<int>(i), ft,
                                              struct_def.fields[i].second});
                        }
                        struct_fields_[named.name] = fields;
                        found = true;
                        break;
                    }
                }

                // If not found in public structs, try re-parsing module source to find
                // private structs (needed for types like RawRwLock used as field types)
                if (!found) {
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
                }
            }
        }
        return "%struct." + named.name;
    } else if (type->is<types::GenericType>()) {
        // Uninstantiated generic type - this shouldn't happen in codegen normally
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
                    for (const auto& [field_name, field_type] : struct_it->second.fields) {
                        if (!first)
                            def += ", ";
                        first = false;
                        def += llvm_type_from_semantic(field_type, true);
                    }
                    def += " }";
                    type_defs_buffer_ << def << "\n";
                    struct_types_[base_name] = type_name;

                    // Register fields
                    std::vector<FieldInfo> fields;
                    for (size_t i = 0; i < struct_it->second.fields.size(); ++i) {
                        std::string ft =
                            llvm_type_from_semantic(struct_it->second.fields[i].second, true);
                        fields.push_back({struct_it->second.fields[i].first, static_cast<int>(i),
                                          ft, struct_it->second.fields[i].second});
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
            return "tuple_empty";
        }
        std::string result = "tuple";
        for (const auto& elem : tuple.elements) {
            result += "_" + mangle_type(elem);
        }
        return result;
    } else if (type->is<types::GenericType>()) {
        // Uninstantiated generic - shouldn't reach codegen normally
        return type->as<types::GenericType>().name;
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

// ============ Parser Type to Semantic Type with Substitution ============
// Converts parser::Type to types::TypePtr, applying generic substitutions

auto LLVMIRGen::resolve_parser_type_with_subs(
    const parser::Type& type, const std::unordered_map<std::string, types::TypePtr>& subs)
    -> types::TypePtr {
    return std::visit(
        [this, &subs](const auto& t) -> types::TypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                // Handle associated types like This::Item or Self::Item
                // Path will have segments ["This", "Item"] or ["Self", "Item"]
                if (t.path.segments.size() == 2) {
                    const std::string& first = t.path.segments[0];
                    const std::string& second = t.path.segments[1];
                    if (first == "This" || first == "Self") {
                        // Look up in current associated types
                        auto assoc_it = current_associated_types_.find(second);
                        if (assoc_it != current_associated_types_.end()) {
                            return assoc_it->second;
                        }
                    }
                    // Handle T::AssociatedType where T is a generic param
                    // Example: I::Item where I -> RangeIterI64 should resolve to I64
                    auto param_it = subs.find(first);
                    if (param_it != subs.end() && param_it->second) {
                        // Get the concrete type that T was substituted to
                        const auto& concrete_type = param_it->second;
                        if (concrete_type->is<types::NamedType>()) {
                            const auto& named = concrete_type->as<types::NamedType>();
                            // Look up the associated type of this concrete type
                            auto assoc_type = lookup_associated_type(named.name, second);
                            if (assoc_type) {
                                return assoc_type;
                            }
                        }
                        // Fallback: check current_associated_types_
                        auto assoc_it = current_associated_types_.find(second);
                        if (assoc_it != current_associated_types_.end()) {
                            return assoc_it->second;
                        }
                    }
                }

                // Get the type name
                std::string name;
                if (!t.path.segments.empty()) {
                    name = t.path.segments.back();
                }

                // Check if it's a generic parameter that needs substitution
                auto it = subs.find(name);
                if (it != subs.end()) {
                    return it->second; // Return substituted type
                }

                // Check for primitive types
                static const std::unordered_map<std::string, types::PrimitiveKind> primitives = {
                    {"I8", types::PrimitiveKind::I8},     {"I16", types::PrimitiveKind::I16},
                    {"I32", types::PrimitiveKind::I32},   {"I64", types::PrimitiveKind::I64},
                    {"I128", types::PrimitiveKind::I128}, {"U8", types::PrimitiveKind::U8},
                    {"U16", types::PrimitiveKind::U16},   {"U32", types::PrimitiveKind::U32},
                    {"U64", types::PrimitiveKind::U64},   {"U128", types::PrimitiveKind::U128},
                    {"F32", types::PrimitiveKind::F32},   {"F64", types::PrimitiveKind::F64},
                    {"Bool", types::PrimitiveKind::Bool}, {"Char", types::PrimitiveKind::Char},
                    {"Str", types::PrimitiveKind::Str},   {"String", types::PrimitiveKind::Str},
                    {"Unit", types::PrimitiveKind::Unit}, {"Usize", types::PrimitiveKind::U64},
                    {"Isize", types::PrimitiveKind::I64},
                };

                auto prim_it = primitives.find(name);
                if (prim_it != primitives.end()) {
                    return types::make_primitive(prim_it->second);
                }

                // Handle Ptr[T] - convert to PtrType for proper dereference handling
                if (name == "Ptr" && t.generics.has_value() && !t.generics->args.empty()) {
                    auto inner =
                        resolve_parser_type_with_subs(*t.generics->args[0].as_type(), subs);
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::PtrType{false, inner};
                    return result;
                }

                // Check if it's a class type (non-generic)
                auto class_def = env_.lookup_class(name);
                if (class_def.has_value()) {
                    auto result = std::make_shared<types::Type>();
                    // Process generic arguments for generic classes
                    std::vector<types::TypePtr> class_type_args;
                    if (t.generics.has_value()) {
                        for (const auto& arg : t.generics->args) {
                            if (arg.is_type()) {
                                class_type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), subs));
                            }
                        }
                    }
                    result->kind = types::ClassType{name, "", std::move(class_type_args)};
                    return result;
                }

                // Check if it's a pending generic class (e.g., Box[T] before instantiation)
                auto pending_it = pending_generic_classes_.find(name);
                if (pending_it != pending_generic_classes_.end()) {
                    auto result = std::make_shared<types::Type>();
                    // Process generic arguments
                    std::vector<types::TypePtr> class_type_args;
                    if (t.generics.has_value()) {
                        for (const auto& arg : t.generics->args) {
                            if (arg.is_type()) {
                                class_type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), subs));
                            }
                        }
                    }
                    result->kind = types::ClassType{name, "", std::move(class_type_args)};
                    return result;
                }

                // Named type - process generic arguments if present
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        if (arg.is_type()) {
                            type_args.push_back(
                                resolve_parser_type_with_subs(*arg.as_type(), subs));
                        }
                    }
                }

                // Look up module_path from registry - necessary for method resolution
                // when library code is re-parsed during generic instantiation
                std::string module_path = "";
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod.structs.find(name) != mod.structs.end() ||
                            mod.enums.find(name) != mod.enums.end()) {
                            module_path = mod_name;
                            break;
                        }
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{name, module_path, std::move(type_args)};
                return result;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                auto inner = resolve_parser_type_with_subs(*t.inner, subs);
                auto result = std::make_shared<types::Type>();
                result->kind =
                    types::RefType{.is_mut = t.is_mut, .inner = inner, .lifetime = t.lifetime};
                return result;
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                auto inner = resolve_parser_type_with_subs(*t.inner, subs);
                auto result = std::make_shared<types::Type>();
                result->kind = types::PtrType{t.is_mut, inner};
                return result;
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                auto element = resolve_parser_type_with_subs(*t.element, subs);
                // parser::ArrayType::size is an ExprPtr, need to evaluate it
                // For now, use a default size of 0 (will be computed elsewhere if needed)
                size_t arr_size = 0;
                if (t.size && t.size->template is<parser::LiteralExpr>()) {
                    const auto& lit = t.size->template as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        arr_size = static_cast<size_t>(lit.token.int_value().value);
                    }
                }
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{element, arr_size};
                return result;
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                auto element = resolve_parser_type_with_subs(*t.element, subs);
                auto result = std::make_shared<types::Type>();
                result->kind = types::SliceType{element};
                return result;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::vector<types::TypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(resolve_parser_type_with_subs(*elem, subs));
                }
                return types::make_tuple(std::move(elements));
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::vector<types::TypePtr> params;
                for (const auto& param : t.params) {
                    params.push_back(resolve_parser_type_with_subs(*param, subs));
                }
                types::TypePtr ret = types::make_unit();
                if (t.return_type) {
                    ret = resolve_parser_type_with_subs(*t.return_type, subs);
                }
                return types::make_func(std::move(params), ret);
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                // dyn Behavior[T] - convert to DynBehaviorType
                std::string behavior_name;
                if (!t.behavior.segments.empty()) {
                    behavior_name = t.behavior.segments.back();
                }

                // Process type arguments if present (e.g., dyn Processor[I32])
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        if (arg.is_type()) {
                            type_args.push_back(
                                resolve_parser_type_with_subs(*arg.as_type(), subs));
                        }
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind =
                    types::DynBehaviorType{behavior_name, std::move(type_args), t.is_mut};
                return result;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                // Infer type - return a type variable or Unit as placeholder
                return types::make_unit();
            } else {
                // Default: return Unit
                return types::make_unit();
            }
        },
        type.kind);
}

// ============ Semantic Type Substitution ============
// Apply type substitutions to a semantic type

auto LLVMIRGen::apply_type_substitutions(
    const types::TypePtr& type, const std::unordered_map<std::string, types::TypePtr>& subs)
    -> types::TypePtr {
    if (!type)
        return type;

    // Check if it's a named type that might need substitution
    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Check if the name itself is a substitution target (e.g., T -> I64)
        auto it = subs.find(named.name);
        if (it != subs.end() && it->second) {
            return it->second;
        }

        // If it has type args, recursively apply substitutions to them
        if (!named.type_args.empty()) {
            std::vector<types::TypePtr> new_args;
            bool changed = false;
            for (const auto& arg : named.type_args) {
                auto new_arg = apply_type_substitutions(arg, subs);
                if (new_arg != arg)
                    changed = true;
                new_args.push_back(new_arg);
            }
            if (changed) {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{named.name, named.module_path, std::move(new_args)};
                return result;
            }
        }
    } else if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        auto new_inner = apply_type_substitutions(ref.inner, subs);
        if (new_inner != ref.inner) {
            return types::make_ref(new_inner, ref.is_mut);
        }
    } else if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        auto new_inner = apply_type_substitutions(ptr.inner, subs);
        if (new_inner != ptr.inner) {
            return types::make_ptr(new_inner, ptr.is_mut);
        }
    } else if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        auto new_elem = apply_type_substitutions(arr.element, subs);
        if (new_elem != arr.element) {
            return types::make_array(new_elem, arr.size);
        }
    } else if (type->is<types::SliceType>()) {
        const auto& slice = type->as<types::SliceType>();
        auto new_elem = apply_type_substitutions(slice.element, subs);
        if (new_elem != slice.element) {
            return types::make_slice(new_elem);
        }
    } else if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        std::vector<types::TypePtr> new_elems;
        bool changed = false;
        for (const auto& elem : tuple.elements) {
            auto new_elem = apply_type_substitutions(elem, subs);
            if (new_elem != elem)
                changed = true;
            new_elems.push_back(new_elem);
        }
        if (changed) {
            return types::make_tuple(std::move(new_elems));
        }
    } else if (type->is<types::GenericType>()) {
        // Handle uninstantiated generic type parameters (e.g., T in Mutex[T])
        // Look up the substitution for this generic type parameter
        const auto& generic = type->as<types::GenericType>();
        auto it = subs.find(generic.name);
        if (it != subs.end() && it->second) {
            return it->second;
        }
    }

    return type;
}

// ============ Type Unification ============
// Unify a parser type pattern with a semantic type to extract type bindings.
// For example: unify(Maybe[T], Maybe[I32], {T}) -> {T: I32}

void LLVMIRGen::unify_types(const parser::Type& pattern, const types::TypePtr& concrete,
                            const std::unordered_set<std::string>& generics,
                            std::unordered_map<std::string, types::TypePtr>& bindings) {
    if (!concrete)
        return;

    std::visit(
        [this, &concrete, &generics, &bindings](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                // Get the pattern's name
                std::string pattern_name;
                if (!p.path.segments.empty()) {
                    pattern_name = p.path.segments.back();
                }

                // Check if this is a generic parameter we're looking for
                if (generics.count(pattern_name) > 0) {
                    // Found a binding: T = concrete
                    // Check if we already have a binding
                    auto existing = bindings.find(pattern_name);
                    if (existing != bindings.end()) {
                        // Prefer existing non-Unit binding over Unit
                        bool existing_is_unit = existing->second->is<types::PrimitiveType>() &&
                                                existing->second->as<types::PrimitiveType>().kind ==
                                                    types::PrimitiveKind::Unit;
                        bool new_is_unit =
                            concrete->is<types::PrimitiveType>() &&
                            concrete->as<types::PrimitiveType>().kind == types::PrimitiveKind::Unit;
                        if (!existing_is_unit && new_is_unit) {
                            // Keep existing non-Unit binding
                            return;
                        }
                    }
                    bindings[pattern_name] = concrete;
                    return;
                }

                // Not a generic param - try to match structurally
                if (auto* named = std::get_if<types::NamedType>(&concrete->kind)) {
                    // If both are the same named type (e.g., Maybe), match type args
                    if (named->name == pattern_name && p.generics.has_value()) {
                        const auto& pattern_args = p.generics->args;
                        const auto& concrete_args = named->type_args;

                        size_t concrete_idx = 0;
                        for (size_t i = 0;
                             i < pattern_args.size() && concrete_idx < concrete_args.size(); ++i) {
                            // Only process type arguments (skip const generics for now)
                            if (pattern_args[i].is_type()) {
                                unify_types(*pattern_args[i].as_type(), concrete_args[concrete_idx],
                                            generics, bindings);
                                ++concrete_idx;
                            }
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                if (auto* ref = std::get_if<types::RefType>(&concrete->kind)) {
                    unify_types(*p.inner, ref->inner, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                if (auto* ptr = std::get_if<types::PtrType>(&concrete->kind)) {
                    unify_types(*p.inner, ptr->inner, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                if (auto* arr = std::get_if<types::ArrayType>(&concrete->kind)) {
                    unify_types(*p.element, arr->element, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                if (auto* slice = std::get_if<types::SliceType>(&concrete->kind)) {
                    unify_types(*p.element, slice->element, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                if (auto* tup = std::get_if<types::TupleType>(&concrete->kind)) {
                    for (size_t i = 0; i < p.elements.size() && i < tup->elements.size(); ++i) {
                        unify_types(*p.elements[i], tup->elements[i], generics, bindings);
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                if (auto* func = std::get_if<types::FuncType>(&concrete->kind)) {
                    for (size_t i = 0; i < p.params.size() && i < func->params.size(); ++i) {
                        unify_types(*p.params[i], func->params[i], generics, bindings);
                    }
                    if (p.return_type && func->return_type) {
                        unify_types(*p.return_type, func->return_type, generics, bindings);
                    }
                }
            }
        },
        pattern.kind);
}

// ============ LLVM Type to Semantic Type ============
// Converts common LLVM type strings back to semantic types

auto LLVMIRGen::semantic_type_from_llvm(const std::string& llvm_type) -> types::TypePtr {
    // Primitive types
    if (llvm_type == "i8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (llvm_type == "i16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (llvm_type == "i32")
        return types::make_primitive(types::PrimitiveKind::I32);
    if (llvm_type == "i64")
        return types::make_primitive(types::PrimitiveKind::I64);
    if (llvm_type == "i128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (llvm_type == "float")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (llvm_type == "double")
        return types::make_primitive(types::PrimitiveKind::F64);
    if (llvm_type == "i1")
        return types::make_primitive(types::PrimitiveKind::Bool);
    if (llvm_type == "ptr")
        return types::make_primitive(types::PrimitiveKind::Str);
    if (llvm_type == "void" || llvm_type == "{}")
        return types::make_unit();

    // For struct types like %struct.TypeName, extract the type name
    if (llvm_type.substr(0, 8) == "%struct.") {
        std::string type_name = llvm_type.substr(8);
        // Check if it's a mangled generic type (contains "__")
        // For now, just create a simple NamedType
        auto result = std::make_shared<types::Type>();
        result->kind = types::NamedType{type_name, "", {}};
        return result;
    }

    // Default: return I32
    return types::make_primitive(types::PrimitiveKind::I32);
}

// ============ Associated Type Lookup ============
// Finds an associated type for a concrete type by searching impl blocks.
// For example: lookup_associated_type("RangeIterI64", "Item") -> I64
//
// This searches:
// 1. Local pending_generic_impls_ (for impls in current module)
// 2. Imported modules via module_registry

auto LLVMIRGen::lookup_associated_type(const std::string& type_name, const std::string& assoc_name)
    -> types::TypePtr {
    // First check current_associated_types_ (might already be resolved)
    auto current_it = current_associated_types_.find(assoc_name);
    if (current_it != current_associated_types_.end()) {
        return current_it->second;
    }

    // Check local impl blocks
    auto impl_it = pending_generic_impls_.find(type_name);
    if (impl_it != pending_generic_impls_.end()) {
        const auto& impl = *impl_it->second;
        for (const auto& binding : impl.type_bindings) {
            if (binding.name == assoc_name && binding.type) {
                return resolve_parser_type_with_subs(*binding.type, {});
            }
        }
    }

    // Check imported modules
    if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            // Check if this module has the struct
            auto struct_it = mod.structs.find(type_name);
            if (struct_it == mod.structs.end())
                continue;

            // Re-parse the module source to get impl AST
            if (mod.source_code.empty())
                continue;

            auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
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

            const auto& parsed_mod = std::get<parser::Module>(parse_result);

            // Find the impl block for this type that has the associated type
            for (const auto& decl : parsed_mod.decls) {
                if (!decl->is<parser::ImplDecl>())
                    continue;
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Check if this impl is for our type
                if (!impl_decl.self_type)
                    continue;
                if (!impl_decl.self_type->is<parser::NamedType>())
                    continue;
                const auto& target = impl_decl.self_type->as<parser::NamedType>();
                if (target.path.segments.empty())
                    continue;
                if (target.path.segments.back() != type_name)
                    continue;

                // Look for the associated type binding
                for (const auto& binding : impl_decl.type_bindings) {
                    if (binding.name == assoc_name && binding.type) {
                        return resolve_parser_type_with_subs(*binding.type, {});
                    }
                }
            }
        }
    }

    // Not found
    return nullptr;
}

} // namespace tml::codegen
