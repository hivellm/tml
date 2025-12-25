// LLVM IR text generator for TML - Core utilities
// Generates LLVM IR as text (.ll format) for compilation with clang
//
// This file contains:
// - Constructor and core utilities
// - Type translation helpers
// - Module structure generation (header, runtime decls, string constants)
// - Main generate() function
// - infer_print_type helper
//
// Split files:
// - llvm_ir_gen_decl.cpp: struct, enum, function declarations
// - llvm_ir_gen_stmt.cpp: let statements, expression statements
// - llvm_ir_gen_expr.cpp: literals, identifiers, binary/unary ops
// - llvm_ir_gen_builtins.cpp: builtin function calls (print, memory, atomics, threading)
// - llvm_ir_gen_control.cpp: if, block, loop, while, for, return
// - llvm_ir_gen_types.cpp: struct expressions, fields, arrays, indexing, method calls

#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace tml::codegen {

LLVMIRGen::LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options)
    : env_(env), options_(std::move(options)) {}

auto LLVMIRGen::fresh_reg() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto LLVMIRGen::fresh_label(const std::string& prefix) -> std::string {
    return prefix + std::to_string(label_counter_++);
}

void LLVMIRGen::emit(const std::string& code) {
    output_ << code;
}

void LLVMIRGen::emit_line(const std::string& code) {
    output_ << code << "\n";
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span) {
    errors_.push_back(LLVMGenError{msg, span, {}});
}

auto LLVMIRGen::llvm_type_name(const std::string& name) -> std::string {
    // Primitive types
    if (name == "I8") return "i8";
    if (name == "I16") return "i16";
    if (name == "I32") return "i32";
    if (name == "I64") return "i64";
    if (name == "I128") return "i128";
    if (name == "U8") return "i8";
    if (name == "U16") return "i16";
    if (name == "U32") return "i32";
    if (name == "U64") return "i64";
    if (name == "U128") return "i128";
    if (name == "F32") return "float";
    if (name == "F64") return "double";
    if (name == "Bool") return "i1";
    if (name == "Char") return "i32";
    if (name == "Str" || name == "String") return "ptr";  // String is a pointer to struct
    if (name == "Unit") return "void";

    // Collection types - all are pointers to runtime structs
    if (name == "List" || name == "Vec" || name == "Array") return "ptr";
    if (name == "HashMap" || name == "Map" || name == "Dict") return "ptr";
    if (name == "Buffer") return "ptr";
    if (name == "Channel") return "ptr";
    if (name == "Mutex") return "ptr";
    if (name == "WaitGroup") return "ptr";

    // User-defined type - return struct type
    return "%struct." + name;
}

auto LLVMIRGen::llvm_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            std::string base_name = named.path.segments.back();

            // Check if this is a generic type with type arguments
            if (named.generics.has_value() && !named.generics->args.empty()) {
                // Check if this is a known generic struct/enum
                auto it = pending_generic_structs_.find(base_name);
                if (it != pending_generic_structs_.end()) {
                    // Convert parser type args to semantic types
                    std::vector<types::TypePtr> type_args;
                    for (const auto& arg : named.generics->args) {
                        types::TypePtr semantic_type = resolve_parser_type_with_subs(*arg, {});
                        type_args.push_back(semantic_type);
                    }
                    // Get mangled name and ensure instantiation
                    std::string mangled = require_struct_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }
                // Check enum
                auto enum_it = pending_generic_enums_.find(base_name);
                if (enum_it != pending_generic_enums_.end()) {
                    std::vector<types::TypePtr> type_args;
                    for (const auto& arg : named.generics->args) {
                        types::TypePtr semantic_type = resolve_parser_type_with_subs(*arg, {});
                        type_args.push_back(semantic_type);
                    }
                    std::string mangled = require_enum_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }
            }

            return llvm_type_name(base_name);
        }
    } else if (type.is<parser::RefType>()) {
        return "ptr";
    } else if (type.is<parser::PtrType>()) {
        return "ptr";
    } else if (type.is<parser::ArrayType>()) {
        return "ptr";
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
        return "%dyn." + behavior_name;
    }
    return "i32";  // Default
}

auto LLVMIRGen::llvm_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type) return "void";
    return llvm_type(*type);
}

auto LLVMIRGen::llvm_type_from_semantic(const types::TypePtr& type) -> std::string {
    if (!type) return "void";

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
            case types::PrimitiveKind::I8: return "i8";
            case types::PrimitiveKind::I16: return "i16";
            case types::PrimitiveKind::I32: return "i32";
            case types::PrimitiveKind::I64: return "i64";
            case types::PrimitiveKind::I128: return "i128";
            case types::PrimitiveKind::U8: return "i8";
            case types::PrimitiveKind::U16: return "i16";
            case types::PrimitiveKind::U32: return "i32";
            case types::PrimitiveKind::U64: return "i64";
            case types::PrimitiveKind::U128: return "i128";
            case types::PrimitiveKind::F32: return "float";
            case types::PrimitiveKind::F64: return "double";
            case types::PrimitiveKind::Bool: return "i1";
            case types::PrimitiveKind::Char: return "i32";
            case types::PrimitiveKind::Str: return "ptr";
            case types::PrimitiveKind::Unit: return "void";
        }
    } else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // If it has type arguments, need to use mangled name and ensure instantiation
        if (!named.type_args.empty()) {
            // Request instantiation (will be generated later if needed)
            std::string mangled = require_struct_instantiation(named.name, named.type_args);
            return "%struct." + mangled;
        }

        return "%struct." + named.name;
    } else if (type->is<types::GenericType>()) {
        // Uninstantiated generic type - this shouldn't happen in codegen normally
        // Return a placeholder (will cause error if actually used)
        return "i32";
    } else if (type->is<types::RefType>() || type->is<types::PtrType>()) {
        return "ptr";
    } else if (type->is<types::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    } else if (type->is<types::DynBehaviorType>()) {
        // Trait objects are fat pointers: { data_ptr, vtable_ptr }
        // We use a struct type: %dyn.BehaviorName
        const auto& dyn = type->as<types::DynBehaviorType>();
        return "%dyn." + dyn.behavior_name;
    }

    return "i32";  // Default
}

// ============ Generic Type Mangling ============
// Converts type to mangled string for LLVM IR names
// e.g., I32 -> "I32", List[I32] -> "List__I32", HashMap[Str, Bool] -> "HashMap__Str__Bool"

auto LLVMIRGen::mangle_type(const types::TypePtr& type) -> std::string {
    if (!type) return "void";

    if (type->is<types::PrimitiveType>()) {
        auto kind = type->as<types::PrimitiveType>().kind;
        // Special handling for Unit and Never - symbols invalid in LLVM identifiers
        if (kind == types::PrimitiveKind::Unit) return "Unit";
        if (kind == types::PrimitiveKind::Never) return "Never";
        return types::primitive_kind_to_string(kind);
    }
    else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        if (named.type_args.empty()) {
            return named.name;
        }
        // Mangle with type arguments: List[I32] -> List__I32
        return named.name + "__" + mangle_type_args(named.type_args);
    }
    else if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        return (ref.is_mut ? "mutref_" : "ref_") + mangle_type(ref.inner);
    }
    else if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        return (ptr.is_mut ? "mutptr_" : "ptr_") + mangle_type(ptr.inner);
    }
    else if (type->is<types::DynBehaviorType>()) {
        const auto& dyn = type->as<types::DynBehaviorType>();
        if (dyn.type_args.empty()) {
            return "dyn_" + dyn.behavior_name;
        }
        return "dyn_" + dyn.behavior_name + "__" + mangle_type_args(dyn.type_args);
    }
    else if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        return "arr_" + mangle_type(arr.element) + "_" + std::to_string(arr.size);
    }
    else if (type->is<types::GenericType>()) {
        // Uninstantiated generic - shouldn't reach codegen normally
        return type->as<types::GenericType>().name;
    }

    return "unknown";
}

auto LLVMIRGen::mangle_type_args(const std::vector<types::TypePtr>& args) -> std::string {
    std::string result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) result += "__";
        result += mangle_type(args[i]);
    }
    return result;
}

auto LLVMIRGen::mangle_struct_name(
    const std::string& base_name,
    const std::vector<types::TypePtr>& type_args
) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

auto LLVMIRGen::mangle_func_name(
    const std::string& base_name,
    const std::vector<types::TypePtr>& type_args
) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

// ============ Parser Type to Semantic Type with Substitution ============
// Converts parser::Type to types::TypePtr, applying generic substitutions

auto LLVMIRGen::resolve_parser_type_with_subs(
    const parser::Type& type,
    const std::unordered_map<std::string, types::TypePtr>& subs
) -> types::TypePtr {
    return std::visit([this, &subs](const auto& t) -> types::TypePtr {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, parser::NamedType>) {
            // Get the type name
            std::string name;
            if (!t.path.segments.empty()) {
                name = t.path.segments.back();
            }

            // Check if it's a generic parameter that needs substitution
            auto it = subs.find(name);
            if (it != subs.end()) {
                return it->second;  // Return substituted type
            }

            // Check for primitive types
            static const std::unordered_map<std::string, types::PrimitiveKind> primitives = {
                {"I8", types::PrimitiveKind::I8},
                {"I16", types::PrimitiveKind::I16},
                {"I32", types::PrimitiveKind::I32},
                {"I64", types::PrimitiveKind::I64},
                {"I128", types::PrimitiveKind::I128},
                {"U8", types::PrimitiveKind::U8},
                {"U16", types::PrimitiveKind::U16},
                {"U32", types::PrimitiveKind::U32},
                {"U64", types::PrimitiveKind::U64},
                {"U128", types::PrimitiveKind::U128},
                {"F32", types::PrimitiveKind::F32},
                {"F64", types::PrimitiveKind::F64},
                {"Bool", types::PrimitiveKind::Bool},
                {"Char", types::PrimitiveKind::Char},
                {"Str", types::PrimitiveKind::Str},
                {"String", types::PrimitiveKind::Str},
                {"Unit", types::PrimitiveKind::Unit},
            };

            auto prim_it = primitives.find(name);
            if (prim_it != primitives.end()) {
                return types::make_primitive(prim_it->second);
            }

            // Named type - process generic arguments if present
            std::vector<types::TypePtr> type_args;
            if (t.generics.has_value()) {
                for (const auto& arg : t.generics->args) {
                    type_args.push_back(resolve_parser_type_with_subs(*arg, subs));
                }
            }

            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{name, "", std::move(type_args)};
            return result;
        }
        else if constexpr (std::is_same_v<T, parser::RefType>) {
            auto inner = resolve_parser_type_with_subs(*t.inner, subs);
            auto result = std::make_shared<types::Type>();
            result->kind = types::RefType{t.is_mut, inner};
            return result;
        }
        else if constexpr (std::is_same_v<T, parser::PtrType>) {
            auto inner = resolve_parser_type_with_subs(*t.inner, subs);
            auto result = std::make_shared<types::Type>();
            result->kind = types::PtrType{t.is_mut, inner};
            return result;
        }
        else if constexpr (std::is_same_v<T, parser::ArrayType>) {
            auto element = resolve_parser_type_with_subs(*t.element, subs);
            // parser::ArrayType::size is an ExprPtr, need to evaluate it
            // For now, use a default size of 0 (will be computed elsewhere if needed)
            size_t arr_size = 0;
            if (t.size && t.size->is<parser::LiteralExpr>()) {
                const auto& lit = t.size->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    arr_size = static_cast<size_t>(lit.token.int_value().value);
                }
            }
            auto result = std::make_shared<types::Type>();
            result->kind = types::ArrayType{element, arr_size};
            return result;
        }
        else if constexpr (std::is_same_v<T, parser::SliceType>) {
            auto element = resolve_parser_type_with_subs(*t.element, subs);
            auto result = std::make_shared<types::Type>();
            result->kind = types::SliceType{element};
            return result;
        }
        else if constexpr (std::is_same_v<T, parser::TupleType>) {
            std::vector<types::TypePtr> elements;
            for (const auto& elem : t.elements) {
                elements.push_back(resolve_parser_type_with_subs(*elem, subs));
            }
            return types::make_tuple(std::move(elements));
        }
        else if constexpr (std::is_same_v<T, parser::FuncType>) {
            std::vector<types::TypePtr> params;
            for (const auto& param : t.params) {
                params.push_back(resolve_parser_type_with_subs(*param, subs));
            }
            types::TypePtr ret = types::make_unit();
            if (t.return_type) {
                ret = resolve_parser_type_with_subs(*t.return_type, subs);
            }
            return types::make_func(std::move(params), ret);
        }
        else if constexpr (std::is_same_v<T, parser::InferType>) {
            // Infer type - return a type variable or Unit as placeholder
            return types::make_unit();
        }
        else {
            // Default: return Unit
            return types::make_unit();
        }
    }, type.kind);
}


// ============ Type Unification ============
// Unify a parser type pattern with a semantic type to extract type bindings.
// For example: unify(Maybe[T], Maybe[I32], {T}) -> {T: I32}

void LLVMIRGen::unify_types(
    const parser::Type& pattern,
    const types::TypePtr& concrete,
    const std::unordered_set<std::string>& generics,
    std::unordered_map<std::string, types::TypePtr>& bindings
) {
    if (!concrete) return;

    std::visit([this, &concrete, &generics, &bindings](const auto& p) {
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
                bindings[pattern_name] = concrete;
                return;
            }

            // Not a generic param - try to match structurally
            if (auto* named = std::get_if<types::NamedType>(&concrete->kind)) {
                // If both are the same named type (e.g., Maybe), match type args
                if (named->name == pattern_name && p.generics.has_value()) {
                    const auto& pattern_args = p.generics->args;
                    const auto& concrete_args = named->type_args;

                    for (size_t i = 0; i < pattern_args.size() && i < concrete_args.size(); ++i) {
                        unify_types(*pattern_args[i], concrete_args[i], generics, bindings);
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<T, parser::RefType>) {
            if (auto* ref = std::get_if<types::RefType>(&concrete->kind)) {
                unify_types(*p.inner, ref->inner, generics, bindings);
            }
        }
        else if constexpr (std::is_same_v<T, parser::PtrType>) {
            if (auto* ptr = std::get_if<types::PtrType>(&concrete->kind)) {
                unify_types(*p.inner, ptr->inner, generics, bindings);
            }
        }
        else if constexpr (std::is_same_v<T, parser::ArrayType>) {
            if (auto* arr = std::get_if<types::ArrayType>(&concrete->kind)) {
                unify_types(*p.element, arr->element, generics, bindings);
            }
        }
        else if constexpr (std::is_same_v<T, parser::SliceType>) {
            if (auto* slice = std::get_if<types::SliceType>(&concrete->kind)) {
                unify_types(*p.element, slice->element, generics, bindings);
            }
        }
        else if constexpr (std::is_same_v<T, parser::TupleType>) {
            if (auto* tup = std::get_if<types::TupleType>(&concrete->kind)) {
                for (size_t i = 0; i < p.elements.size() && i < tup->elements.size(); ++i) {
                    unify_types(*p.elements[i], tup->elements[i], generics, bindings);
                }
            }
        }
        else if constexpr (std::is_same_v<T, parser::FuncType>) {
            if (auto* func = std::get_if<types::FuncType>(&concrete->kind)) {
                for (size_t i = 0; i < p.params.size() && i < func->params.size(); ++i) {
                    unify_types(*p.params[i], func->params[i], generics, bindings);
                }
                if (p.return_type && func->return_type) {
                    unify_types(*p.return_type, func->return_type, generics, bindings);
                }
            }
        }
    }, pattern.kind);
}
auto LLVMIRGen::add_string_literal(const std::string& value) -> std::string {
    std::string name = "@.str." + std::to_string(string_literals_.size());
    string_literals_.emplace_back(name, value);
    return name;
}

// ============ Generate Pending Generic Instantiations ============
// Iteratively generate all pending struct/enum/func instantiations
// Loops until no new instantiations are added (handles recursive types)

void LLVMIRGen::generate_pending_instantiations() {
    const int MAX_ITERATIONS = 100;  // Prevent infinite loops
    int iterations = 0;

    bool changed = true;
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        ++iterations;

        // Generate pending struct instantiations
        for (auto& [key, inst] : struct_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                // Find the generic struct declaration
                auto it = pending_generic_structs_.find(inst.base_name);
                if (it != pending_generic_structs_.end()) {
                    gen_struct_instantiation(*it->second, inst.type_args);
                    changed = true;
                }
            }
        }

        // Generate pending enum instantiations
        for (auto& [key, inst] : enum_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_enums_.find(inst.base_name);
                if (it != pending_generic_enums_.end()) {
                    gen_enum_instantiation(*it->second, inst.type_args);
                    changed = true;
                }
            }
        }

        // Generate pending function instantiations
        for (auto& [key, inst] : func_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_funcs_.find(inst.base_name);
                if (it != pending_generic_funcs_.end()) {
                    gen_func_instantiation(*it->second, inst.type_args);
                    changed = true;
                }
            }
        }
    }
}

// Request enum instantiation - returns mangled name
auto LLVMIRGen::require_enum_instantiation(
    const std::string& base_name,
    const std::vector<types::TypePtr>& type_args
) -> std::string {
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = enum_instantiations_.find(mangled);
    if (it != enum_instantiations_.end()) {
        return mangled;
    }

    enum_instantiations_[mangled] = GenericInstantiation{
        base_name,
        type_args,
        mangled,
        false
    };

    // Also register enum variants immediately so they're available during code generation
    // (enum_variants_ is used before generate_pending_instantiations runs)
    auto decl_it = pending_generic_enums_.find(base_name);
    if (decl_it != pending_generic_enums_.end()) {
        const parser::EnumDecl* decl = decl_it->second;

        // Register variant tags with mangled enum name
        int tag = 0;
        for (const auto& variant : decl->variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }

    return mangled;
}

// Placeholder for function instantiation (will implement when adding generic functions)
auto LLVMIRGen::require_func_instantiation(
    const std::string& base_name,
    const std::vector<types::TypePtr>& type_args
) -> std::string {
    std::string mangled = mangle_func_name(base_name, type_args);

    // Register the instantiation if not already registered
    if (func_instantiations_.find(mangled) == func_instantiations_.end()) {
        func_instantiations_[mangled] = GenericInstantiation{
            base_name,
            type_args,
            mangled,
            false  // not generated yet
        };
    }

    return mangled;
}

void LLVMIRGen::emit_header() {
    emit_line("; Generated by TML Compiler");
    emit_line("target triple = \"" + options_.target_triple + "\"");
    emit_line("");
}

void LLVMIRGen::emit_runtime_decls() {
    // String type: { ptr, i64 } (pointer to data, length)
    emit_line("; Runtime type declarations");
    emit_line("%struct.tml_str = type { ptr, i64 }");

    // File I/O types (from std::file)
    emit_line("%struct.File = type { ptr }");  // handle field
    emit_line("%struct.Path = type { ptr }");  // path string field
    emit_line("");

    // External C functions
    emit_line("; External function declarations");
    emit_line("declare i32 @printf(ptr, ...)");
    emit_line("declare i32 @puts(ptr)");
    emit_line("declare i32 @putchar(i32)");
    emit_line("declare ptr @malloc(i64)");
    emit_line("declare void @free(ptr)");
    emit_line("declare void @exit(i32) noreturn");
    emit_line("");

    // TML runtime functions
    emit_line("; TML runtime functions");
    emit_line("declare void @panic(ptr) noreturn");
    emit_line("");

    // Note: TML test assertions are now provided by the test module's TML code
    // They call panic() internally and don't need external declarations
    emit_line("");

    // TML code coverage functions
    emit_line("; TML code coverage");
    emit_line("declare void @cover_func(ptr)");
    emit_line("declare void @cover_line(ptr, i32)");
    emit_line("declare void @cover_branch(ptr, i32, i32)");
    emit_line("declare void @print_coverage_report()");
    emit_line("declare i32 @get_covered_func_count()");
    emit_line("declare i32 @get_covered_line_count()");
    emit_line("declare i32 @get_covered_branch_count()");
    emit_line("declare void @reset_coverage()");
    emit_line("declare i32 @is_func_covered(ptr)");
    emit_line("declare i32 @get_coverage_percent()");
    emit_line("");

    // Threading runtime declarations
    emit_line("; Threading runtime (tml_runtime.c)");
    emit_line("declare ptr @thread_spawn(ptr, ptr)");
    emit_line("declare void @thread_join(ptr)");
    emit_line("declare void @thread_yield()");
    emit_line("declare void @thread_sleep(i32)");
    emit_line("declare i32 @thread_id()");
    emit_line("");

    // I/O functions (print, println) - polymorphic, accept any type
    emit_line("; I/O functions");
    emit_line("declare void @print(ptr)");
    emit_line("declare void @println(ptr)");
    emit_line("");

    // NOTE: Math functions moved to core::math module
    // Import with: use core::math

    // NOTE: Assertion functions moved to test module
    // Import with: use test
    emit_line("; Black box (prevent optimization)");
    emit_line("declare i32 @black_box_i32(i32)");
    emit_line("declare i64 @black_box_i64(i64)");
    emit_line("; SIMD operations (auto-vectorized)");
    emit_line("declare i64 @simd_sum_i32(ptr, i64)");
    emit_line("declare i64 @simd_sum_i64(ptr, i64)");
    emit_line("declare double @simd_sum_f64(ptr, i64)");
    emit_line("declare double @simd_dot_f64(ptr, ptr, i64)");
    emit_line("declare void @simd_fill_i32(ptr, i32, i64)");
    emit_line("declare void @simd_add_i32(ptr, ptr, ptr, i64)");
    emit_line("declare void @simd_mul_i32(ptr, ptr, ptr, i64)");
    emit_line("");

    // Float functions
    emit_line("; Float functions");
    emit_line("declare ptr @float_to_fixed(double, i32)");
    emit_line("declare ptr @float_to_precision(double, i32)");
    emit_line("declare ptr @float_to_string(double)");
    emit_line("declare double @int_to_float(i32)");
    emit_line("declare double @i64_to_float(i64)");
    emit_line("declare i32 @float_to_int(double)");
    emit_line("declare i64 @float_to_i64(double)");
    emit_line("declare i32 @float_round(double)");
    emit_line("declare i32 @float_floor(double)");
    emit_line("declare i32 @float_ceil(double)");
    emit_line("declare double @float_abs(double)");
    emit_line("declare double @float_sqrt(double)");
    emit_line("declare double @float_pow(double, i32)");
    emit_line("");

    // Overloaded abs functions
    emit_line("; Overloaded abs");
    emit_line("declare i32 @abs_i32(i32)");
    emit_line("declare double @abs_f64(double)");
    emit_line("");

    // Bit manipulation runtime declarations
    emit_line("; Bit manipulation runtime");
    emit_line("declare i32 @float32_bits(float)");
    emit_line("declare float @float32_from_bits(i32)");
    emit_line("declare i64 @float64_bits(double)");
    emit_line("declare double @float64_from_bits(i64)");
    emit_line("");

    // Special float value runtime declarations
    emit_line("; Special float values runtime");
    emit_line("declare double @infinity(i32)");
    emit_line("declare double @nan()");
    emit_line("declare i32 @is_inf(double, i32)");
    emit_line("declare i32 @is_nan(double)");
    emit_line("");

    // Nextafter runtime declarations
    emit_line("; Nextafter runtime");
    emit_line("declare double @nextafter(double, double)");
    emit_line("declare float @nextafter32(float, float)");
    emit_line("");

    // Channel runtime declarations
    emit_line("; Channel runtime (Go-style)");
    emit_line("declare ptr @channel_create()");
    emit_line("declare i32 @channel_send(ptr, i32)");
    emit_line("declare i32 @channel_recv(ptr, ptr)");
    emit_line("declare i32 @channel_try_send(ptr, i32)");
    emit_line("declare i32 @channel_try_recv(ptr, ptr)");
    emit_line("declare void @channel_close(ptr)");
    emit_line("declare void @channel_destroy(ptr)");
    emit_line("declare i32 @channel_len(ptr)");
    emit_line("");

    // Mutex runtime declarations
    emit_line("; Mutex runtime");
    emit_line("declare ptr @mutex_create()");
    emit_line("declare void @mutex_lock(ptr)");
    emit_line("declare void @mutex_unlock(ptr)");
    emit_line("declare i32 @mutex_try_lock(ptr)");
    emit_line("declare void @mutex_destroy(ptr)");
    emit_line("");

    // WaitGroup runtime declarations
    emit_line("; WaitGroup runtime (Go-style)");
    emit_line("declare ptr @waitgroup_create()");
    emit_line("declare void @waitgroup_add(ptr, i32)");
    emit_line("declare void @waitgroup_done(ptr)");
    emit_line("declare void @waitgroup_wait(ptr)");
    emit_line("declare void @waitgroup_destroy(ptr)");
    emit_line("");

    // Atomic counter runtime declarations
    emit_line("; Atomic counter runtime");
    emit_line("declare ptr @atomic_counter_create(i32)");
    emit_line("declare i32 @atomic_counter_inc(ptr)");
    emit_line("declare i32 @atomic_counter_dec(ptr)");
    emit_line("declare i32 @atomic_counter_get(ptr)");
    emit_line("declare void @atomic_counter_set(ptr, i32)");
    emit_line("declare void @atomic_counter_destroy(ptr)");
    emit_line("");

    // List runtime declarations
    emit_line("; List (dynamic array) runtime");
    emit_line("declare ptr @list_create(i64)");
    emit_line("declare void @list_destroy(ptr)");
    emit_line("declare void @list_push(ptr, i64)");
    emit_line("declare i64 @list_pop(ptr)");
    emit_line("declare i64 @list_get(ptr, i64)");
    emit_line("declare void @list_set(ptr, i64, i64)");
    emit_line("declare i64 @list_len(ptr)");
    emit_line("declare i64 @list_capacity(ptr)");
    emit_line("declare void @list_clear(ptr)");
    emit_line("declare i32 @list_is_empty(ptr)");
    emit_line("");

    // HashMap runtime declarations
    emit_line("; HashMap runtime");
    emit_line("declare ptr @hashmap_create(i64)");
    emit_line("declare void @hashmap_destroy(ptr)");
    emit_line("declare void @hashmap_set(ptr, i64, i64)");
    emit_line("declare i64 @hashmap_get(ptr, i64)");
    emit_line("declare i1 @hashmap_has(ptr, i64)");
    emit_line("declare i1 @hashmap_remove(ptr, i64)");
    emit_line("declare i64 @hashmap_len(ptr)");
    emit_line("declare void @hashmap_clear(ptr)");
    emit_line("");

    // Buffer runtime declarations
    emit_line("; Buffer runtime");
    emit_line("declare ptr @buffer_create(i64)");
    emit_line("declare void @buffer_destroy(ptr)");
    emit_line("declare void @buffer_write_byte(ptr, i32)");
    emit_line("declare void @buffer_write_i32(ptr, i32)");
    emit_line("declare void @buffer_write_i64(ptr, i64)");
    emit_line("declare i32 @buffer_read_byte(ptr)");
    emit_line("declare i32 @buffer_read_i32(ptr)");
    emit_line("declare i64 @buffer_read_i64(ptr)");
    emit_line("declare i64 @buffer_len(ptr)");
    emit_line("declare i64 @buffer_capacity(ptr)");
    emit_line("declare i64 @buffer_remaining(ptr)");
    emit_line("declare void @buffer_clear(ptr)");
    emit_line("declare void @buffer_reset_read(ptr)");
    emit_line("");

    // File I/O runtime declarations
    emit_line("; File I/O runtime");
    emit_line("declare ptr @file_open_read(ptr)");
    emit_line("declare ptr @file_open_write(ptr)");
    emit_line("declare ptr @file_open_append(ptr)");
    emit_line("declare void @file_close(ptr)");
    emit_line("declare i1 @file_is_open(ptr)");
    emit_line("declare ptr @file_read_line(ptr)");
    emit_line("declare i1 @file_write_str(ptr, ptr)");
    emit_line("declare i64 @file_size(ptr)");
    emit_line("declare ptr @file_read_all(ptr)");
    emit_line("declare i1 @file_write_all(ptr, ptr)");
    emit_line("declare i1 @file_append_all(ptr, ptr)");
    emit_line("");

    // Path utilities runtime declarations
    emit_line("; Path utilities runtime");
    emit_line("declare i1 @path_exists(ptr)");
    emit_line("declare i1 @path_is_file(ptr)");
    emit_line("declare i1 @path_is_dir(ptr)");
    emit_line("declare i1 @path_create_dir(ptr)");
    emit_line("declare i1 @path_create_dir_all(ptr)");
    emit_line("declare i1 @path_remove(ptr)");
    emit_line("declare i1 @path_remove_dir(ptr)");
    emit_line("declare i1 @path_rename(ptr, ptr)");
    emit_line("declare i1 @path_copy(ptr, ptr)");
    emit_line("declare ptr @path_join(ptr, ptr)");
    emit_line("declare ptr @path_parent(ptr)");
    emit_line("declare ptr @path_filename(ptr)");
    emit_line("declare ptr @path_extension(ptr)");
    emit_line("declare ptr @path_absolute(ptr)");
    emit_line("");

    // String utilities
    emit_line("; String utilities");
    emit_line("declare i32 @str_len(ptr)");
    emit_line("declare i32 @str_hash(ptr)");
    emit_line("declare i32 @str_eq(ptr, ptr)");
    emit_line("");

    // Time functions - only declare if not imported from core::time module
    // (core::time module declares its own lowlevel functions)
    bool has_time_module = env_.module_registry() && env_.module_registry()->has_module("core::time");
    if (!has_time_module) {
        emit_line("; Time functions");
        emit_line("declare i32 @time_ms()");
        emit_line("declare i64 @time_us()");
        emit_line("declare i64 @time_ns()");
        emit_line("");
    }

    // Format strings for print/println
    // Size calculation: count actual bytes (each escape like \0A = 1 byte, not 3)
    emit_line("; Format strings");
    emit_line("@.fmt.int = private constant [4 x i8] c\"%d\\0A\\00\"");           // %d\n\0 = 4 bytes
    emit_line("@.fmt.int.no_nl = private constant [3 x i8] c\"%d\\00\"");         // %d\0 = 3 bytes
    emit_line("@.fmt.i64 = private constant [5 x i8] c\"%ld\\0A\\00\"");          // %ld\n\0 = 5 bytes
    emit_line("@.fmt.i64.no_nl = private constant [4 x i8] c\"%ld\\00\"");        // %ld\0 = 4 bytes
    emit_line("@.fmt.float = private constant [4 x i8] c\"%f\\0A\\00\"");         // %f\n\0 = 4 bytes
    emit_line("@.fmt.float.no_nl = private constant [3 x i8] c\"%f\\00\"");       // %f\0 = 3 bytes
    emit_line("@.fmt.float3 = private constant [6 x i8] c\"%.3f\\0A\\00\"");      // %.3f\n\0 = 6 bytes
    emit_line("@.fmt.float3.no_nl = private constant [5 x i8] c\"%.3f\\00\"");    // %.3f\0 = 5 bytes
    emit_line("@.fmt.str.no_nl = private constant [3 x i8] c\"%s\\00\"");         // %s\0 = 3 bytes
    emit_line("@.str.true = private constant [5 x i8] c\"true\\00\"");            // true\0 = 5 bytes
    emit_line("@.str.false = private constant [6 x i8] c\"false\\00\"");          // false\0 = 6 bytes
    emit_line("@.str.space = private constant [2 x i8] c\" \\00\"");              // " "\0 = 2 bytes
    emit_line("@.str.newline = private constant [2 x i8] c\"\\0A\\00\"");         // \n\0 = 2 bytes
    emit_line("");
}

void LLVMIRGen::emit_module_lowlevel_decls() {
    // Emit declarations for lowlevel functions from imported modules
    if (!env_.module_registry()) {
        return;
    }

    emit_line("; Lowlevel functions from imported modules");

    // Get all modules from registry
    const auto& registry = env_.module_registry();
    const auto& all_modules = registry->get_all_modules();

    for (const auto& [module_name, module] : all_modules) {
        for (const auto& [func_name, func_sig] : module.functions) {
            if (func_sig.is_lowlevel) {
                // Generate LLVM declaration using semantic types
                std::string llvm_ret_type = llvm_type_from_semantic(func_sig.return_type);

                std::string params_str;
                for (size_t i = 0; i < func_sig.params.size(); ++i) {
                    if (i > 0) params_str += ", ";
                    params_str += llvm_type_from_semantic(func_sig.params[i]);
                }

                // Emit declaration with tml_ prefix
                emit_line("declare " + llvm_ret_type + " @tml_" + func_name + "(" + params_str + ")");
            }
        }
    }

    emit_line("");
}

void LLVMIRGen::emit_module_pure_tml_functions() {
    // Emit LLVM IR for pure TML functions from imported modules
    if (!env_.module_registry()) {
        return;
    }

    const auto& registry = env_.module_registry();
    const auto& all_modules = registry->get_all_modules();

    emit_line("; Pure TML functions from imported modules");

    for (const auto& [module_name, module] : all_modules) {
        // Check if module has pure TML functions
        if (!module.has_pure_tml_functions || module.source_code.empty()) {
            continue;
        }

        // Re-parse the module source code
        auto source = lexer::Source::from_string(module.source_code, module.file_path);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();

        if (lex.has_errors()) {
            continue;  // Skip modules with lexer errors
        }

        parser::Parser parser(std::move(tokens));
        auto mod_name = std::filesystem::path(module.file_path).stem().string();
        auto parse_result = parser.parse_module(mod_name);

        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            continue;  // Skip modules with parse errors
        }

        // Store the AST persistently so that pending_generic_funcs_ pointers remain valid
        imported_module_asts_.push_back(std::get<parser::Module>(std::move(parse_result)));
        const auto& parsed_module = imported_module_asts_.back();

        emit_line("; Module: " + module_name);

        // First pass: register struct/enum declarations (including generic ones)
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                if (s.vis == parser::Visibility::Public) {
                    gen_struct_decl(s);  // This registers generic structs in pending_generic_structs_
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                if (e.vis == parser::Visibility::Public) {
                    gen_enum_decl(e);  // This registers generic enums in pending_generic_enums_
                }
            }
        }

        // Second pass: generate code for each public function
        for (const auto& decl : parsed_module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();

                // Only generate code for public, non-lowlevel functions with bodies
                if (func.vis == parser::Visibility::Public &&
                    !func.is_unsafe &&
                    func.body.has_value()) {
                    gen_func_decl(func);
                }
            }
        }
    }

    emit_line("");
}

void LLVMIRGen::emit_string_constants() {
    if (string_literals_.empty()) return;

    emit_line("; String constants");
    for (const auto& [name, value] : string_literals_) {
        // Escape the string and add null terminator
        std::string escaped;
        for (char c : value) {
            if (c == '\n') escaped += "\\0A";
            else if (c == '\t') escaped += "\\09";
            else if (c == '\\') escaped += "\\5C";
            else if (c == '"') escaped += "\\22";
            else escaped += c;
        }
        escaped += "\\00";

        emit_line(name + " = private constant [" +
                  std::to_string(value.size() + 1) + " x i8] c\"" + escaped + "\"");
    }
    emit_line("");
}

auto LLVMIRGen::generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    string_literals_.clear();
    temp_counter_ = 0;
    label_counter_ = 0;

    emit_header();
    emit_runtime_decls();
    emit_module_lowlevel_decls();
    emit_module_pure_tml_functions();  // Generate code for pure TML imported functions (like std::math)

    // First pass: collect const declarations and struct/enum declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            // For now, only support literal constants
            if (const_decl.value->is<parser::LiteralExpr>()) {
                const auto& lit = const_decl.value->as<parser::LiteralExpr>();
                std::string value;
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    value = std::to_string(lit.token.int_value().value);
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    value = (lit.token.lexeme == "true") ? "1" : "0";
                }
                global_constants_[const_decl.name] = value;
            }
        } else if (decl->is<parser::StructDecl>()) {
            gen_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            gen_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Register impl block for vtable generation
            register_impl(&decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::TraitDecl>()) {
            // Register trait/behavior declaration for default implementations
            const auto& trait_decl = decl->as<parser::TraitDecl>();
            trait_decls_[trait_decl.name] = &trait_decl;
        }
    }

    // Generate any pending generic instantiations collected during first pass
    // This happens after structs/enums are registered but before function codegen
    generate_pending_instantiations();

    // Emit dyn types for all registered behaviors before function generation
    for (const auto& [key, vtable_name] : vtables_) {
        // key is "TypeName::BehaviorName", extract behavior name
        size_t pos = key.find("::");
        if (pos != std::string::npos) {
            std::string behavior_name = key.substr(pos + 2);
            emit_dyn_type(behavior_name);
        }
    }

    // Buffer function code separately so we can emit type instantiations before functions
    std::stringstream func_output;
    std::stringstream saved_output;
    saved_output.str(output_.str());  // Save current output (headers, type defs)
    output_.str("");  // Clear for function code

    // Second pass: generate function declarations (into temp buffer)
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            // Generate impl methods as named functions inline
            const auto& impl = decl->as<parser::ImplDecl>();
            std::string type_name;
            if (impl.self_type->kind.index() == 0) {  // NamedType
                const auto& named = std::get<parser::NamedType>(impl.self_type->kind);
                if (!named.path.segments.empty()) {
                    type_name = named.path.segments.back();
                }
            }
            if (!type_name.empty()) {
                for (const auto& method : impl.methods) {
                    // Generate method with mangled name TypeName_MethodName
                    std::string method_name = type_name + "_" + method.name;
                    current_func_ = method_name;
                    current_impl_type_ = type_name;  // Track impl self type for 'this' access
                    locals_.clear();
                    block_terminated_ = false;

                    // Determine return type
                    std::string ret_type = "void";
                    if (method.return_type.has_value()) {
                        ret_type = llvm_type_ptr(*method.return_type);
                    }
                    current_ret_type_ = ret_type;

                    // Build parameter list (including 'this')
                    std::string params;
                    std::string param_types;
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        if (i > 0) {
                            params += ", ";
                            param_types += ", ";
                        }
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern && method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                        } else {
                            param_name = "_anon";
                        }
                        // Substitute 'This' type with the actual impl type
                        if (param_name == "this" && param_type.find("This") != std::string::npos) {
                            param_type = "ptr";  // 'this' is always a pointer to the struct
                        }
                        params += param_type + " %" + param_name;
                        param_types += param_type;
                    }

                    // Register function
                    std::string func_type = ret_type + " (" + param_types + ")";
                    functions_[method_name] = FuncInfo{
                        "@tml_" + method_name,
                        func_type,
                        ret_type
                    };

                    // Generate function
                    emit_line("");
                    emit_line("define internal " + ret_type + " @tml_" + method_name + "(" + params + ") #0 {");
                    emit_line("entry:");

                    // Register params in locals
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        std::string param_type = llvm_type_ptr(method.params[i].type);
                        std::string param_name;
                        if (method.params[i].pattern && method.params[i].pattern->is<parser::IdentPattern>()) {
                            param_name = method.params[i].pattern->as<parser::IdentPattern>().name;
                        } else {
                            param_name = "_anon";
                        }
                        // Substitute 'This' type with ptr for 'this' param
                        if (param_name == "this" && param_type.find("This") != std::string::npos) {
                            param_type = "ptr";
                        }
                        std::string alloca_reg = fresh_reg();
                        emit_line("  " + alloca_reg + " = alloca " + param_type);
                        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
                        locals_[param_name] = VarInfo{alloca_reg, param_type, nullptr};
                    }

                    // Generate body
                    if (method.body.has_value()) {
                        gen_block(*method.body);
                        if (!block_terminated_) {
                            if (ret_type == "void") {
                                emit_line("  ret void");
                            } else {
                                emit_line("  ret " + ret_type + " 0");
                            }
                        }
                    } else {
                        if (ret_type == "void") {
                            emit_line("  ret void");
                        } else {
                            emit_line("  ret " + ret_type + " 0");
                        }
                    }
                    emit_line("}");
                    current_impl_type_.clear();  // Clear impl type context
                }

                // Generate default implementations for missing methods
                if (impl.trait_path && !impl.trait_path->segments.empty()) {
                    std::string trait_name = impl.trait_path->segments.back();
                    auto trait_it = trait_decls_.find(trait_name);
                    if (trait_it != trait_decls_.end()) {
                        const auto* trait_decl = trait_it->second;

                        // Collect method names that impl provides
                        std::set<std::string> impl_method_names;
                        for (const auto& m : impl.methods) {
                            impl_method_names.insert(m.name);
                        }

                        // Generate default implementations for missing methods
                        for (const auto& trait_method : trait_decl->methods) {
                            // Skip if impl provides this method
                            if (impl_method_names.count(trait_method.name) > 0) continue;

                            // Skip if trait method has no default implementation
                            if (!trait_method.body.has_value()) continue;

                            // Generate default implementation with type substitution
                            std::string method_name = type_name + "_" + trait_method.name;
                            current_func_ = method_name;
                            current_impl_type_ = type_name;
                            locals_.clear();
                            block_terminated_ = false;

                            // Determine return type
                            std::string ret_type = "void";
                            if (trait_method.return_type.has_value()) {
                                ret_type = llvm_type_ptr(*trait_method.return_type);
                                // Substitute 'This' with actual type
                                if (ret_type.find("This") != std::string::npos) {
                                    ret_type = "%struct." + type_name;
                                }
                            }
                            current_ret_type_ = ret_type;

                            // Build parameter list
                            std::string params;
                            std::string param_types;
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                if (i > 0) {
                                    params += ", ";
                                    param_types += ", ";
                                }
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern && trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
                                } else {
                                    param_name = "_anon";
                                }
                                // Substitute 'This' type with ptr for 'this' param
                                if (param_name == "this" && param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                }
                                params += param_type + " %" + param_name;
                                param_types += param_type;
                            }

                            // Register function
                            std::string func_type = ret_type + " (" + param_types + ")";
                            functions_[method_name] = FuncInfo{
                                "@tml_" + method_name,
                                func_type,
                                ret_type
                            };

                            // Generate function
                            emit_line("");
                            emit_line("; Default implementation from behavior " + trait_name);
                            emit_line("define internal " + ret_type + " @tml_" + method_name + "(" + params + ") #0 {");
                            emit_line("entry:");

                            // Register params in locals
                            for (size_t i = 0; i < trait_method.params.size(); ++i) {
                                std::string param_type = llvm_type_ptr(trait_method.params[i].type);
                                std::string param_name;
                                if (trait_method.params[i].pattern && trait_method.params[i].pattern->is<parser::IdentPattern>()) {
                                    param_name = trait_method.params[i].pattern->as<parser::IdentPattern>().name;
                                } else {
                                    param_name = "_anon";
                                }

                                // Create semantic type for this parameter
                                types::TypePtr semantic_type = nullptr;
                                if (param_name == "this" && param_type.find("This") != std::string::npos) {
                                    param_type = "ptr";
                                    // Create semantic type as the concrete impl type
                                    semantic_type = std::make_shared<types::Type>();
                                    semantic_type->kind = types::NamedType{type_name, "", {}};
                                }

                                std::string alloca_reg = fresh_reg();
                                emit_line("  " + alloca_reg + " = alloca " + param_type);
                                emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
                                locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type};
                            }

                            // Generate body
                            gen_block(*trait_method.body);
                            if (!block_terminated_) {
                                if (ret_type == "void") {
                                    emit_line("  ret void");
                                } else {
                                    emit_line("  ret " + ret_type + " 0");
                                }
                            }
                            emit_line("}");
                            current_impl_type_.clear();
                        }
                    }
                }
            }
        }
    }

    // Save function code
    func_output.str(output_.str());
    output_.str("");

    // Restore header output
    output_ << saved_output.str();

    // Now emit any generic instantiations discovered during function codegen
    // These will appear BEFORE function code
    generate_pending_instantiations();
    emit_line("");

    // Now append function code
    output_ << func_output.str();

    // Emit generated closure functions
    for (const auto& closure_func : module_functions_) {
        emit(closure_func);
    }

    // Emit vtables for trait objects (dyn dispatch)
    emit_vtables();

    // Emit string constants at the end (they were collected during codegen)
    emit_string_constants();

    // Collect test and benchmark functions (decorated with @test and @bench)
    std::vector<std::string> test_functions;
    std::vector<std::string> bench_functions;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            for (const auto& decorator : func.decorators) {
                if (decorator.name == "test") {
                    test_functions.push_back(func.name);
                    break;
                } else if (decorator.name == "bench") {
                    bench_functions.push_back(func.name);
                    break;
                }
            }
        }
    }

    // Generate main entry point
    bool has_user_main = false;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>() && decl->as<parser::FuncDecl>().name == "main") {
            has_user_main = true;
            break;
        }
    }

    if (!bench_functions.empty()) {
        // Generate benchmark runner main
        // Note: time functions are always declared in preamble
        emit_line("; Auto-generated benchmark runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        int bench_num = 0;
        std::string prev_block = "entry";
        for (const auto& bench_name : bench_functions) {
            std::string bench_fn = "@tml_" + bench_name;

            // Get start time
            std::string start_time = "%bench_start_" + std::to_string(bench_num);
            emit_line("  " + start_time + " = call i64 @time_us()");

            // Run benchmark 1000 iterations
            std::string iter_var = "%bench_iter_" + std::to_string(bench_num);
            std::string loop_header = "bench_loop_header_" + std::to_string(bench_num);
            std::string loop_body = "bench_loop_body_" + std::to_string(bench_num);
            std::string loop_end = "bench_loop_end_" + std::to_string(bench_num);

            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_header + ":");
            emit_line("  " + iter_var + " = phi i32 [ 0, %" + prev_block + " ], [ " + iter_var + "_next, %" + loop_body + " ]");
            std::string cmp_var = "%bench_cmp_" + std::to_string(bench_num);
            emit_line("  " + cmp_var + " = icmp slt i32 " + iter_var + ", 1000");
            emit_line("  br i1 " + cmp_var + ", label %" + loop_body + ", label %" + loop_end);
            emit_line("");
            emit_line(loop_body + ":");
            emit_line("  call void " + bench_fn + "()");
            emit_line("  " + iter_var + "_next = add i32 " + iter_var + ", 1");
            emit_line("  br label %" + loop_header);
            emit_line("");
            emit_line(loop_end + ":");

            // Get end time and calculate duration
            std::string end_time = "%bench_end_" + std::to_string(bench_num);
            std::string duration = "%bench_duration_" + std::to_string(bench_num);
            emit_line("  " + end_time + " = call i64 @time_us()");
            emit_line("  " + duration + " = sub i64 " + end_time + ", " + start_time);

            // Calculate average (duration / 1000)
            std::string avg_time = "%bench_avg_" + std::to_string(bench_num);
            emit_line("  " + avg_time + " = sdiv i64 " + duration + ", 1000");
            emit_line("");

            prev_block = loop_end;
            bench_num++;
        }

        emit_line("  ret i32 0");
        emit_line("}");
    } else if (!test_functions.empty()) {
        // Generate test runner main
        // @test functions return Unit (void) - assertions call exit(1) on failure
        // If a test function returns, it passed
        emit_line("; Auto-generated test runner");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");

        for (const auto& test_name : test_functions) {
            std::string test_fn = "@tml_" + test_name;
            // Call test function (void return) - if it returns, test passed
            // Assertions inside will call exit(1) on failure
            emit_line("  call void " + test_fn + "()");
        }

        // Print coverage report if enabled
        if (options_.coverage_enabled) {
            emit_line("  call void @print_coverage_report()");
        }

        // All tests passed (if we got here, no assertion failed)
        emit_line("  ret i32 0");
        emit_line("}");
    } else if (has_user_main) {
        // Standard main wrapper for user-defined main
        emit_line("; Entry point");
        emit_line("define i32 @main(i32 %argc, ptr %argv) {");
        emit_line("entry:");
        emit_line("  %ret = call i32 @tml_main()");
        emit_line("  ret i32 %ret");
        emit_line("}");
    }

    // Emit function attributes for optimization
    emit_line("");
    emit_line("; Function attributes for optimization");
    emit_line("attributes #0 = { nounwind mustprogress willreturn }");

    if (!errors_.empty()) {
        return errors_;
    }

    return output_.str();
}

// Infer print argument type from expression
auto LLVMIRGen::infer_print_type(const parser::Expr& expr) -> PrintArgType {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
            case lexer::TokenKind::IntLiteral: return PrintArgType::Int;
            case lexer::TokenKind::FloatLiteral: return PrintArgType::Float;
            case lexer::TokenKind::BoolLiteral: return PrintArgType::Bool;
            case lexer::TokenKind::StringLiteral: return PrintArgType::Str;
            default: return PrintArgType::Unknown;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
            case parser::BinaryOp::Add:
            case parser::BinaryOp::Sub:
            case parser::BinaryOp::Mul:
            case parser::BinaryOp::Div:
            case parser::BinaryOp::Mod:
                // Check if operands are float
                if (infer_print_type(*bin.left) == PrintArgType::Float ||
                    infer_print_type(*bin.right) == PrintArgType::Float) {
                    return PrintArgType::Float;
                }
                return PrintArgType::Int;
            case parser::BinaryOp::Eq:
            case parser::BinaryOp::Ne:
            case parser::BinaryOp::Lt:
            case parser::BinaryOp::Gt:
            case parser::BinaryOp::Le:
            case parser::BinaryOp::Ge:
            case parser::BinaryOp::And:
            case parser::BinaryOp::Or:
                return PrintArgType::Bool;
            default:
                return PrintArgType::Int;
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        if (un.op == parser::UnaryOp::Not) return PrintArgType::Bool;
        if (un.op == parser::UnaryOp::Neg) {
            // Check if operand is float
            if (infer_print_type(*un.operand) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        // For identifiers, we need to check the variable type
        // For now, default to Unknown (will be checked by caller)
        return PrintArgType::Unknown;
    }
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        // Check for known I64-returning functions
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& fn_name = call.callee->as<parser::IdentExpr>().name;
            if (fn_name == "time_us" || fn_name == "time_ns") {
                return PrintArgType::I64;
            }
        }
        return PrintArgType::Int; // Assume functions return int
    }
    return PrintArgType::Unknown;
}

// ============ Vtable Support ============

void LLVMIRGen::register_impl(const parser::ImplDecl* impl) {
    pending_impls_.push_back(impl);

    // Eagerly populate behavior_method_order_ for dyn dispatch
    if (impl->trait_path && !impl->trait_path->segments.empty()) {
        std::string behavior_name = impl->trait_path->segments.back();
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            auto behavior_def = env_.lookup_behavior(behavior_name);
            if (behavior_def) {
                std::vector<std::string> methods;
                for (const auto& m : behavior_def->methods) {
                    methods.push_back(m.name);
                }
                behavior_method_order_[behavior_name] = methods;

                // Also eagerly register vtable name
                // Get type name
                std::string type_name;
                if (impl->self_type->kind.index() == 0) {
                    const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
                    if (!named.path.segments.empty()) {
                        type_name = named.path.segments.back();
                    }
                }
                if (!type_name.empty()) {
                    std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
                    std::string key = type_name + "::" + behavior_name;
                    vtables_[key] = vtable_name;
                }
            }
        }
    }
}

void LLVMIRGen::emit_dyn_type(const std::string& behavior_name) {
    if (emitted_dyn_types_.count(behavior_name)) return;
    emitted_dyn_types_.insert(behavior_name);

    // Emit the dyn type as a fat pointer struct: { data_ptr, vtable_ptr }
    emit_line("%dyn." + behavior_name + " = type { ptr, ptr }");
}

auto LLVMIRGen::get_vtable(const std::string& type_name, const std::string& behavior_name) -> std::string {
    std::string key = type_name + "::" + behavior_name;
    auto it = vtables_.find(key);
    if (it != vtables_.end()) {
        return it->second;
    }
    return "";  // No vtable found
}

void LLVMIRGen::emit_vtables() {
    // For each registered impl block, generate a vtable
    for (const auto* impl : pending_impls_) {
        if (!impl->trait_path) continue;  // Skip inherent impls

        // Get the type name and behavior name
        std::string type_name;
        if (impl->self_type->kind.index() == 0) {  // NamedType
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }

        std::string behavior_name;
        if (!impl->trait_path->segments.empty()) {
            behavior_name = impl->trait_path->segments.back();
        }

        if (type_name.empty() || behavior_name.empty()) continue;

        // Emit the dyn type for this behavior
        emit_dyn_type(behavior_name);

        // Get behavior method order
        auto behavior_def = env_.lookup_behavior(behavior_name);
        if (!behavior_def) continue;

        // Build vtable type: array of function pointers
        std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0) vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0) vtable_value += ", ";
            const auto& method = behavior_def->methods[i];
            vtable_value += "ptr @tml_" + type_name + "_" + method.name;
        }
        vtable_value += " }";

        // Emit vtable global constant
        emit_line(vtable_name + " = internal constant " + vtable_type + " " + vtable_value);

        // Register vtable
        std::string key = type_name + "::" + behavior_name;
        vtables_[key] = vtable_name;

        // Store method order for this behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            std::vector<std::string> methods;
            for (const auto& m : behavior_def->methods) {
                methods.push_back(m.name);
            }
            behavior_method_order_[behavior_name] = methods;
        }
    }
}

} // namespace tml::codegen