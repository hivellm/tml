TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Main Entry Point
//!
//! This file implements the main `generate()` code generation entry point.
//!
//! Related files:
//! - generate_cache.cpp: GlobalASTCache and GlobalLibraryIRCache implementations
//! - generate_support.cpp: Loop metadata, lifetime intrinsics, print type inference,
//!   namespace support, and library state capture

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"
#include "version_generated.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <set>

namespace {

/// Hoist non-entry-block alloca instructions into each function's entry block.
///
/// The text-based LLVM IR generator emits `alloca` instructions at the current
/// insertion point, which can be inside when-arms, try ok-blocks, or loop bodies.
/// LLVM treats non-entry-block allocas as dynamic stack allocations that adjust
/// the stack pointer at runtime. On Windows x64, many small dynamic allocas can
/// cumulatively exceed the 4KB guard page, causing STATUS_HEAP_CORRUPTION.
///
/// This pass moves all alloca lines to right after the `entry:` label so LLVM
/// sees them as static allocations and emits a single __chkstk probe if needed.
void hoist_allocas_to_entry(std::string& ir) {
    // Process each function: find "entry:\n" and collect allocas from the body.
    size_t search_from = 0;
    while (true) {
        // Find next function entry block
        auto entry_pos = ir.find("\nentry:\n", search_from);
        if (entry_pos == std::string::npos) {
            break;
        }
        // Position right after "entry:\n"
        size_t insert_pos = entry_pos + 8; // length of "\nentry:\n"

        // Find end of this function (closing brace at start of line)
        auto func_end = ir.find("\n}\n", insert_pos);
        if (func_end == std::string::npos) {
            // Last function might end with "}\n" at EOF
            func_end = ir.find("\n}", insert_pos);
            if (func_end == std::string::npos) {
                break;
            }
        }

        // Scan for alloca lines in the function body AFTER the entry block's
        // initial allocas. First skip existing entry-block allocas (contiguous
        // alloca lines right after entry:).
        size_t scan_from = insert_pos;
        while (scan_from < func_end) {
            auto line_end = ir.find('\n', scan_from);
            if (line_end == std::string::npos || line_end > func_end) {
                break;
            }
            std::string_view line(ir.data() + scan_from, line_end - scan_from);
            // Entry-block allocas, lifetime.start, and empty lines are part of prologue
            if (line.find("= alloca ") != std::string_view::npos ||
                line.find("llvm.lifetime.start") != std::string_view::npos ||
                line.find("store ") != std::string_view::npos || line.empty()) {
                scan_from = line_end + 1;
                continue;
            }
            break; // First non-alloca/store line = end of entry prologue
        }

        // Now collect non-entry allocas from scan_from to func_end
        std::string hoisted;
        size_t pos = scan_from;
        while (pos < func_end) {
            auto line_end = ir.find('\n', pos);
            if (line_end == std::string::npos || line_end > func_end) {
                break;
            }
            std::string_view line(ir.data() + pos, line_end - pos);
            if (line.find("= alloca ") != std::string_view::npos) {
                // Collect this alloca line
                hoisted += line;
                hoisted += '\n';
                // Remove it from the current position
                ir.erase(pos, line_end - pos + 1);
                func_end -= (line_end - pos + 1);
                // Don't advance pos — next line is now at current pos
                continue;
            }
            pos = line_end + 1;
        }

        // Insert collected allocas at the entry block
        if (!hoisted.empty()) {
            ir.insert(insert_pos, hoisted);
            // Adjust func_end for the insertion
            func_end += hoisted.size();
        }

        search_from = func_end;
    }
}

} // anonymous namespace

namespace tml::codegen {

// Helper: Parse a mangled type string back into a semantic type.
// Handles nested generics correctly by treating the entire suffix after
// the first "__" as a single (possibly nested) type argument.
// e.g., "Shared__PromiseState__I32" -> Shared[PromiseState[I32]]
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "U128") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U128};
        return t;
    }
    if (s == "I128") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I128};
        return t;
    }
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s == "Unit")
        return types::make_unit();

    if (s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
            return t;
        }
    }

    // Check if s is a numeric string (const generic value like "3")
    if (!s.empty() && (std::isdigit(s[0]) || (s[0] == '-' && s.size() > 1 && std::isdigit(s[1])))) {
        try {
            int64_t val = std::stoll(s);
            auto t = std::make_shared<types::Type>();
            t->kind = types::ConstGenericType{s, types::make_i64(), val};
            return t;
        } catch (...) {
            // Not a valid number, fall through
        }
    }

    // Nested generic: treat the entire suffix after the first "__" as a single
    // (possibly nested) type argument.  This is the KEY difference from the naive
    // splitting approach that breaks nested generics.
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

// Helper: Convert a parser::Type to a string for name mangling
// Used to extract behavior type parameters for impl method names
static std::string parser_type_to_string(const parser::Type& type) {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        std::string result = named.path.segments.empty() ? "" : named.path.segments.back();
        if (named.generics.has_value() && !named.generics->args.empty()) {
            result += "__";
            for (size_t i = 0; i < named.generics->args.size(); ++i) {
                if (i > 0)
                    result += "__";
                const auto& arg = named.generics->args[i];
                if (arg.is_type()) {
                    result += parser_type_to_string(*arg.as_type());
                }
            }
        }
        return result;
    } else if (type.is<parser::PtrType>()) {
        const auto& ptr = type.as<parser::PtrType>();
        std::string prefix = ptr.is_mut ? "mutptr_" : "ptr_";
        return prefix + parser_type_to_string(*ptr.inner);
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        std::string prefix = ref.is_mut ? "mutref_" : "ref_";
        return prefix + parser_type_to_string(*ref.inner);
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        return "Slice__" + parser_type_to_string(*slice.element);
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple = type.as<parser::TupleType>();
        std::string result = "Tuple";
        for (const auto& elem : tuple.elements) {
            result += "__" + parser_type_to_string(*elem);
        }
        return result;
    }
    return "";
}

// Helper: Get the LLVM type string for a constant's declared type
// For primitives like I32, I64, Bool, etc.
static std::string get_const_llvm_type(const parser::TypePtr& type) {
    if (!type)
        return "i64"; // Default fallback

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            const std::string& name = named.path.segments.back();
            // Map TML primitive types to LLVM types
            if (name == "I8" || name == "U8")
                return "i8";
            if (name == "I16" || name == "U16")
                return "i16";
            if (name == "I32" || name == "U32")
                return "i32";
            if (name == "I64" || name == "U64")
                return "i64";
            if (name == "I128" || name == "U128")
                return "i128";
            if (name == "Bool")
                return "i1";
            if (name == "Isize" || name == "Usize")
                return "i64";
            if (name == "Str")
                return "ptr";
        }
    } else if (type->is<parser::TupleType>()) {
        const auto& tuple = type->as<parser::TupleType>();
        if (tuple.elements.empty())
            return "{}";
        std::string result = "{ ";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += get_const_llvm_type(tuple.elements[i]);
        }
        result += " }";
        return result;
    }
    return "i64"; // Default for unknown types
}

/// Try to extract a compile-time constant scalar value from an expression.
/// Handles: LiteralExpr, CastExpr(LiteralExpr), UnaryExpr(-LiteralExpr),
/// and CastExpr(UnaryExpr(-LiteralExpr)).
/// Returns empty string if the expression is not a constant scalar.
static std::string try_extract_scalar_const(const parser::Expr* expr) {
    if (!expr)
        return "";

    // Unwrap cast expressions (e.g., "15 as U8")
    if (expr->is<parser::CastExpr>()) {
        const auto& cast = expr->as<parser::CastExpr>();
        if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
            expr = cast.expr.get();
        } else if (cast.expr && cast.expr->is<parser::UnaryExpr>()) {
            const auto& unary = cast.expr->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                const auto& lit = unary.operand->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    int64_t int_val = static_cast<int64_t>(lit.token.int_value().value);
                    return std::to_string(-int_val);
                }
            }
            return "";
        } else {
            return "";
        }
    }

    // Unary negation (e.g., -128)
    if (expr->is<parser::UnaryExpr>()) {
        const auto& unary = expr->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
            const auto& lit = unary.operand->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                int64_t int_val = static_cast<int64_t>(lit.token.int_value().value);
                return std::to_string(-int_val);
            }
        }
        return "";
    }

    // Direct literal
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
            return std::to_string(lit.token.int_value().value);
        } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
            return lit.token.bool_value() ? "1" : "0";
        } else if (lit.token.kind == lexer::TokenKind::NullLiteral) {
            return "null";
        } else if (lit.token.kind == lexer::TokenKind::StringLiteral) {
            // Use sentinel prefix to distinguish string values from LLVM values
            return "STR:" + std::string(lit.token.string_value().value);
        }
    }

    return "";
}

/// Try to extract a compile-time constant value (scalar or tuple) from an expression.
/// For tuples, returns the full LLVM aggregate constant (e.g., "{ i8 15, i8 1, i8 0 }").
/// For scalars, returns just the value (e.g., "42").
/// Also sets out_llvm_type to the corresponding LLVM type.
/// Returns empty string if the expression is not a compile-time constant.
static std::string try_extract_const_value(const parser::Expr* expr, const parser::TypePtr& type,
                                           std::string& out_llvm_type) {
    if (!expr)
        return "";

    // Handle tuple expressions
    if (expr->is<parser::TupleExpr>()) {
        const auto& tuple = expr->as<parser::TupleExpr>();
        if (tuple.elements.empty()) {
            out_llvm_type = "{}";
            return "zeroinitializer";
        }

        // Get element types from the declared type
        std::vector<std::string> elem_types;
        if (type && type->is<parser::TupleType>()) {
            const auto& tuple_type = type->as<parser::TupleType>();
            for (const auto& et : tuple_type.elements) {
                elem_types.push_back(get_const_llvm_type(et));
            }
        }

        // Extract each element value
        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const(tuple.elements[i].get());
            if (val.empty())
                return ""; // Non-constant element
            elem_values.push_back(val);
        }

        // If we don't have declared types, default each element to i64
        if (elem_types.size() != elem_values.size()) {
            elem_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i) {
                elem_types.push_back("i64");
            }
        }

        // Build LLVM type: { i8, i8, i8 }
        std::string llvm_type = "{ ";
        for (size_t i = 0; i < elem_types.size(); ++i) {
            if (i > 0)
                llvm_type += ", ";
            llvm_type += elem_types[i];
        }
        llvm_type += " }";
        out_llvm_type = llvm_type;

        // Build LLVM value: { i8 15, i8 1, i8 0 }
        std::string llvm_value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0)
                llvm_value += ", ";
            llvm_value += elem_types[i] + " " + elem_values[i];
        }
        llvm_value += " }";
        return llvm_value;
    }

    // Handle scalar expressions
    std::string scalar = try_extract_scalar_const(expr);
    if (!scalar.empty()) {
        out_llvm_type = get_const_llvm_type(type);
        return scalar;
    }

    return "";
}

auto LLVMIRGen::generate(const parser::Module& module)
    -> Result<std::string, std::vector<LLVMGenError>> {
    errors_.clear();
    output_.str("");
    type_defs_buffer_.str(""); // Clear type definitions buffer
    enum_drop_output_.str(""); // Clear enum drop function buffer
    generated_enum_drop_functions_.clear();
    string_literals_.clear();
    string_literal_dedup_.clear();
    current_type_subs_.clear(); // Clear type substitutions from previous compilation
    temp_counter_ = 0;
    label_counter_ = 0;

    // Register builtin enums
    // Ordering enum: Less=0, Equal=1, Greater=2
    enum_variants_["Ordering::Less"] = 0;
    enum_variants_["Ordering::Equal"] = 1;
    enum_variants_["Ordering::Greater"] = 2;

    // Register builtin generic enums: Maybe[T], Outcome[T, E]
    // These need to be stored in builtin_enum_decls_ to keep the AST alive
    {
        // Maybe[T] { Just(T), Nothing }
        auto maybe_decl = std::make_unique<parser::EnumDecl>();
        maybe_decl->name = "Maybe";
        maybe_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Just(T) variant
        parser::EnumVariant just_variant;
        just_variant.name = "Just";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> just_fields;
        just_fields.push_back(std::move(t_type));
        just_variant.tuple_fields = std::move(just_fields);
        maybe_decl->variants.push_back(std::move(just_variant));

        // Nothing variant
        parser::EnumVariant nothing_variant;
        nothing_variant.name = "Nothing";
        maybe_decl->variants.push_back(std::move(nothing_variant));

        pending_generic_enums_["Maybe"] = maybe_decl.get();
        builtin_enum_decls_.push_back(std::move(maybe_decl));
    }

    {
        // Outcome[T, E] { Ok(T), Err(E) }
        auto outcome_decl = std::make_unique<parser::EnumDecl>();
        outcome_decl->name = "Outcome";
        outcome_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});
        outcome_decl->generics.push_back(parser::GenericParam{
            "E", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Ok(T) variant
        parser::EnumVariant ok_variant;
        ok_variant.name = "Ok";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> ok_fields;
        ok_fields.push_back(std::move(t_type));
        ok_variant.tuple_fields = std::move(ok_fields);
        outcome_decl->variants.push_back(std::move(ok_variant));

        // Err(E) variant
        parser::EnumVariant err_variant;
        err_variant.name = "Err";
        auto e_type = std::make_unique<parser::Type>();
        e_type->kind = parser::NamedType{parser::TypePath{{"E"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> err_fields;
        err_fields.push_back(std::move(e_type));
        err_variant.tuple_fields = std::move(err_fields);
        outcome_decl->variants.push_back(std::move(err_variant));

        pending_generic_enums_["Outcome"] = outcome_decl.get();
        builtin_enum_decls_.push_back(std::move(outcome_decl));
    }

    {
        // Poll[T] { Ready(T), Pending }
        auto poll_decl = std::make_unique<parser::EnumDecl>();
        poll_decl->name = "Poll";
        poll_decl->generics.push_back(parser::GenericParam{
            "T", {}, false, false, std::nullopt, std::nullopt, std::nullopt, {}});

        // Ready(T) variant
        parser::EnumVariant ready_variant;
        ready_variant.name = "Ready";
        auto t_type = std::make_unique<parser::Type>();
        t_type->kind = parser::NamedType{parser::TypePath{{"T"}, {}}, std::nullopt, {}};
        std::vector<parser::TypePtr> ready_fields;
        ready_fields.push_back(std::move(t_type));
        ready_variant.tuple_fields = std::move(ready_fields);
        poll_decl->variants.push_back(std::move(ready_variant));

        // Pending variant
        parser::EnumVariant pending_variant;
        pending_variant.name = "Pending";
        poll_decl->variants.push_back(std::move(pending_variant));

        pending_generic_enums_["Poll"] = poll_decl.get();
        builtin_enum_decls_.push_back(std::move(poll_decl));
    }

    emit_header();
    emit_debug_info_header(); // Initialize debug info metadata
    emit_runtime_decls();
    emit_module_lowlevel_decls();

    // Save headers before generating imported module code
    std::string headers = output_.str();
    cached_preamble_headers_ = headers; // Save for capture_library_state()
    output_.str("");

    std::string imported_func_code;
    std::string imported_type_defs;

    // Pre-register local generic structs into pending_generic_structs_ BEFORE
    // library module registration/emission. This ensures that when library code
    // triggers require_struct_instantiation (e.g., for Node[I32]), the local
    // definition is used instead of a library struct with the same simple name
    // but different fields (e.g., local Node{value: T} vs library Node{value: Maybe[T]}).
    local_generic_struct_names_.clear();
    for (const auto& decl : module.decls) {
        if (decl->is<parser::StructDecl>()) {
            const auto& s = decl->as<parser::StructDecl>();
            if (!s.generics.empty()) {
                local_generic_struct_names_.insert(s.name);
                // Pre-register so require_struct_instantiation uses local fields
                pending_generic_structs_[s.name] = &s;
                struct_decls_[s.name] = &s;
            }
        }
    }

    if (options_.cached_library_state && options_.cached_library_state->valid) {
        // FAST PATH: Restore pre-computed library state instead of regenerating.
        // This skips emit_module_pure_tml_functions() entirely (~9 seconds for zlib).
        const auto& state = *options_.cached_library_state;

        // Type definitions are the same regardless of library_decls_only
        imported_type_defs = state.imported_type_defs;

        // For function IR: if library_decls_only is true, use pre-computed declarations.
        // If false, use the full definitions.
        if (options_.library_decls_only) {
            // Use pre-computed declarations extracted from full library IR
            // (contains define→declare conversions for TML functions defined in the shared lib)
            std::ostringstream func_code;
            func_code << state.imported_func_decls;

            // imported_func_decls already includes both:
            // 1. define→declare conversions for TML library functions
            // 2. FFI declare lines (brotli_*, zlib_*, etc.) NOT in preamble
            imported_func_code = func_code.str();
        } else {
            // Use full definitions (for coverage mode or library_ir_only)
            imported_func_code = state.imported_func_code;

            // When force_internal_linkage is set (suite mode workers), convert
            // library function definitions to internal linkage. The cached library
            // state was generated without force_internal_linkage (needed for shared
            // .obj in non-coverage mode), but suite workers need internal linkage
            // to avoid duplicate symbol errors when multiple .obj files in the same
            // suite each contain the same library function definitions.
            if (options_.force_internal_linkage && !imported_func_code.empty()) {
                std::string result;
                result.reserve(imported_func_code.size() + 4096);
                std::istringstream stream(imported_func_code);
                std::string line;
                while (std::getline(stream, line)) {
                    // Convert "define <type>" to "define internal <type>" for @tml_ functions
                    // but skip lines already marked internal/linkonce_odr
                    if (line.find("define ") != std::string::npos &&
                        line.find("@tml_") != std::string::npos &&
                        line.find("define internal ") == std::string::npos &&
                        line.find("define linkonce_odr ") == std::string::npos) {
                        auto pos = line.find("define ");
                        if (pos != std::string::npos) {
                            // Check if it's "define dllexport"
                            auto dpos = line.find("define dllexport ");
                            if (dpos != std::string::npos) {
                                line.replace(dpos, 17, "define internal ");
                            } else {
                                line.replace(pos, 7, "define internal ");
                            }
                        }
                    }
                    result += line + "\n";
                }
                imported_func_code = std::move(result);
            }

            // Restore string literals referenced by function definitions
            for (const auto& sl : state.string_literals) {
                string_literals_.push_back(sl);
            }
        }

        // Restore internal registries
        for (const auto& [k, v] : state.struct_types) {
            if (struct_types_.find(k) == struct_types_.end()) {
                struct_types_[k] = v;
                // Restore nullable_maybe_types_ for nullable Maybe types from cache
                // These were optimized to "ptr" during gen_enum_instantiation
                if (v == "ptr" && k.starts_with("Maybe__")) {
                    nullable_maybe_types_.insert(k);
                }
            }
            // Pre-populate instantiation guards so require_struct_instantiation()
            // and gen_enum_instantiation() return early without re-emitting type
            // definitions already present in the cached library IR.
            if (struct_instantiations_.find(k) == struct_instantiations_.end()) {
                struct_instantiations_[k] = GenericInstantiation{"", {}, k, true};
            }
            if (enum_instantiations_.find(k) == enum_instantiations_.end()) {
                enum_instantiations_[k] = GenericInstantiation{"", {}, k, true};
            }
        }
        for (const auto& k : state.union_types) {
            union_types_.insert(k);
        }
        for (const auto& [k, v] : state.enum_variants) {
            if (enum_variants_.find(k) == enum_variants_.end()) {
                enum_variants_[k] = v;
            }
        }
        for (const auto& [k, v] : state.global_constants) {
            if (global_constants_.find(k) == global_constants_.end()) {
                global_constants_[k] = ConstInfo{v.first, v.second};
            }
        }
        for (const auto& [struct_name, fields] : state.struct_fields) {
            if (struct_fields_.find(struct_name) == struct_fields_.end()) {
                std::vector<FieldInfo> fi;
                fi.reserve(fields.size());
                for (const auto& f : fields) {
                    fi.push_back(FieldInfo{f.name, f.index, f.llvm_type, f.semantic_type});
                }
                struct_fields_[struct_name] = std::move(fi);
            }
        }
        for (const auto& [k, v] : state.functions) {
            if (functions_.find(k) == functions_.end()) {
                functions_[k] =
                    FuncInfo{v.llvm_name, v.llvm_func_type, v.ret_type, v.param_types, v.is_extern};
            }
        }
        for (const auto& [k, v] : state.func_return_types) {
            if (func_return_types_.find(k) == func_return_types_.end()) {
                func_return_types_[k] = v;
            }
        }
        for (const auto& name : state.generated_functions) {
            generated_functions_.insert(name);
        }
        // Restore declared externals to prevent duplicate declarations
        // when user code has @extern functions with the same symbol names
        for (const auto& name : state.declared_externals) {
            declared_externals_.insert(name);
        }

        // Restore class types (class_name -> LLVM type name)
        for (const auto& [k, v] : state.class_types) {
            if (class_types_.find(k) == class_types_.end()) {
                class_types_[k] = v;
            }
        }

        // Restore class field info
        for (const auto& [class_name, fields] : state.class_fields) {
            if (class_fields_.find(class_name) == class_fields_.end()) {
                std::vector<ClassFieldInfo> fi;
                fi.reserve(fields.size());
                for (const auto& f : fields) {
                    ClassFieldInfo cfi;
                    cfi.name = f.name;
                    cfi.index = f.index;
                    cfi.llvm_type = f.llvm_type;
                    cfi.vis = static_cast<parser::MemberVisibility>(f.vis);
                    cfi.is_inherited = f.is_inherited;
                    for (const auto& step : f.inheritance_path) {
                        cfi.inheritance_path.push_back({step.class_name, step.index});
                    }
                    fi.push_back(std::move(cfi));
                }
                class_fields_[class_name] = std::move(fi);
            }
        }

        // Restore value classes
        for (const auto& name : state.value_classes) {
            value_classes_.insert(name);
        }

        // Restore emitted dyn types (prevents duplicate %dyn.X type definitions)
        for (const auto& name : state.emitted_dyn_types) {
            emitted_dyn_types_.insert(name);
        }

        // Restore SIMD type info (for @simd annotated structs)
        for (const auto& [name, info] : state.simd_types) {
            if (simd_types_.find(name) == simd_types_.end()) {
                simd_types_[name] = {info.element_llvm_type, info.lane_count};
            }
        }

        // Ensure trait-impl modules (core::default, core::cmp, core::clone, etc.) are
        // in the registry even in the fast path. These may not be loaded by the type
        // checker because they contain only trait impls for built-in types (Maybe, etc.)
        // and are not explicitly imported by user code.
        {
            static const std::vector<std::string> trait_impl_modules = {
                "core::default",
                "core::cmp",
                "core::clone",
            };
            if (env_.module_registry()) {
                auto reg = env_.module_registry();
                // Determine which modules need loading (must read before writing)
                std::vector<std::string> needs_loading;
                {
                    const auto& all_mods = reg->get_all_modules();
                    for (const auto& mod_path : trait_impl_modules) {
                        auto it = all_mods.find(mod_path);
                        bool has_source = it != all_mods.end() && !it->second.source_code.empty();
                        if (!has_source)
                            needs_loading.push_back(mod_path);
                    }
                }
                // Now register them (safe to modify registry here)
                for (const auto& mod_path : needs_loading) {
                    auto cached = types::GlobalModuleCache::instance().get(mod_path);
                    if (cached && !cached->source_code.empty()) {
                        reg->register_module(mod_path, std::move(*cached));
                    } else {
                        auto from_disk = types::load_module_from_cache(mod_path);
                        if (from_disk && !from_disk->source_code.empty()) {
                            reg->register_module(mod_path, std::move(*from_disk));
                        }
                    }
                }
            }
        }

        // Re-parse library module ASTs for pending generic registration.
        // We need the AST pointers to be valid for pending_generic_structs_ etc.
        // The GlobalASTCache already has these cached, so this is just pointer lookups.
        if (env_.module_registry()) {
            const auto& registry = env_.module_registry();
            const auto& all_modules = registry->get_all_modules();
            for (const auto& [mod_name, mod_info] : all_modules) {
                if (!mod_info.has_pure_tml_functions || mod_info.source_code.empty())
                    continue;
                if (!GlobalASTCache::should_cache(mod_name))
                    continue;
                const parser::Module* cached_ast = GlobalASTCache::instance().get(mod_name);
                if (!cached_ast)
                    continue;

                // Re-register generic structs/enums/funcs/impls from cached ASTs
                for (const auto& decl : cached_ast->decls) {
                    if (decl->is<parser::StructDecl>()) {
                        const auto& s = decl->as<parser::StructDecl>();
                        if (!s.generics.empty() &&
                            pending_generic_structs_.find(s.name) ==
                                pending_generic_structs_.end() &&
                            local_generic_struct_names_.find(s.name) ==
                                local_generic_struct_names_.end()) {
                            pending_generic_structs_[s.name] = &s;
                        }
                        if (struct_decls_.find(s.name) == struct_decls_.end()) {
                            struct_decls_[s.name] = &s;
                        }
                    } else if (decl->is<parser::EnumDecl>()) {
                        const auto& e = decl->as<parser::EnumDecl>();
                        if (!e.generics.empty() &&
                            pending_generic_enums_.find(e.name) == pending_generic_enums_.end()) {
                            pending_generic_enums_[e.name] = &e;
                        }
                    } else if (decl->is<parser::FuncDecl>()) {
                        const auto& func = decl->as<parser::FuncDecl>();
                        if (!func.generics.empty()) {
                            // Always append to the _all_ map for arity disambiguation
                            pending_generic_funcs_all_[func.name].push_back({&func, mod_name});
                            if (pending_generic_funcs_.find(func.name) ==
                                pending_generic_funcs_.end()) {
                                pending_generic_funcs_[func.name] = &func;
                                generic_func_modules_[func.name] = mod_name;
                            }
                        }
                    } else if (decl->is<parser::ImplDecl>()) {
                        const auto& impl = decl->as<parser::ImplDecl>();
                        if (!impl.generics.empty()) {
                            std::string type_name;
                            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                                type_name =
                                    impl.self_type->as<parser::NamedType>().path.segments.back();
                            } else if (impl.self_type && impl.self_type->is<parser::TupleType>()) {
                                const auto& tuple = impl.self_type->as<parser::TupleType>();
                                type_name = "Tuple" + std::to_string(tuple.elements.size());
                            }
                            if (!type_name.empty()) {
                                if (pending_generic_impls_.find(type_name) ==
                                    pending_generic_impls_.end()) {
                                    pending_generic_impls_[type_name] = &impl;
                                }
                                pending_generic_impls_all_[type_name].push_back(&impl);
                            }
                        }
                        // Register for vtable generation
                        register_impl(&impl);
                    } else if (decl->is<parser::TraitDecl>()) {
                        const auto& trait = decl->as<parser::TraitDecl>();
                        // Register by FQN to avoid short-name collisions (e.g. core::io::Write
                        // vs core::fmt::Write). Also register short name first-write-wins for
                        // legacy callers that look up by unqualified name.
                        std::string fqn = mod_name + "::" + trait.name;
                        trait_decls_[fqn] = &trait;
                        if (trait_decls_.find(trait.name) == trait_decls_.end()) {
                            trait_decls_[trait.name] = &trait;
                        }
                    }
                }
            }
        }

        // Restore loop metadata from library functions (needed for !N references in cached IR)
        if (!state.loop_metadata.empty()) {
            loop_metadata_ = state.loop_metadata;
            loop_metadata_counter_ = state.loop_metadata_counter;
        }

        TML_DEBUG_LN("[CODEGEN] Restored library state: "
                     << state.struct_types.size() << " struct types, " << state.functions.size()
                     << " functions, " << state.enum_variants.size() << " enum variants");
    } else {
        // SLOW PATH: Generate library IR from scratch
        emit_module_pure_tml_functions();

        imported_func_code = output_.str();
        output_.str("");

        imported_type_defs = type_defs_buffer_.str();

        // Save for capture_library_state() (used when library_ir_only=true)
        cached_imported_func_code_ = imported_func_code;
        cached_imported_type_defs_ = imported_type_defs;
    }

    // Now reassemble with types before functions
    output_ << headers;

    if (!imported_type_defs.empty()) {
        emit_line("; Generic types from imported modules");
        output_ << imported_type_defs;
    }
    type_defs_buffer_.str(""); // Clear for main module processing

    // Emit imported module functions AFTER their type dependencies
    // Scan for runtime refs since this bypasses emit_line()
    scan_for_runtime_refs(imported_func_code);
    output_ << imported_func_code;

    // In library_ir_only mode, we only want the library IR (headers + types + library functions).
    // Skip all user code generation. This is used to produce a shared library object that
    // can be linked into multiple test files.
    if (options_.library_ir_only) {
        return generate_library_only_ir(module);
    }

    generate_first_pass(module);

    // Buffer function code separately so we can emit type instantiations before functions
    std::stringstream func_output;
    std::stringstream saved_output;
    saved_output.str(output_.str()); // Save current output (headers, type defs, dyn types)
    output_.str("");                 // Clear for function code

    // Second pass: generate function bodies (in separate file)
    generate_function_bodies(module);

    // Save function code (non-generic functions)
    func_output.str(output_.str());
    output_.str("");
    // Generate pending generic instantiations (types go to type_defs_buffer_, funcs go to output_)
    {
        auto saved_lib = in_library_body_;
        in_library_body_ = true;
        generate_pending_instantiations();
        in_library_body_ = saved_lib;
    }

    // Save generic function code
    std::stringstream generic_func_output;
    generic_func_output.str(output_.str());
    output_.str("");

    // Now reassemble in correct order: headers + types + generic funcs + non-generic funcs
    // 1. Headers
    output_ << saved_output.str();

    // 2. Type definitions (from type_defs_buffer_) - MUST come before functions
    std::string type_defs = type_defs_buffer_.str();
    if (!type_defs.empty()) {
        emit_line("; Generic type instantiations");
        output_ << type_defs;
    }
    emit_line("");

    // 2b. Late-emitted @extern declarations discovered during function codegen
    //     These must appear at module level, not inline inside function bodies.
    if (!pending_late_extern_decls_.empty()) {
        emit_line("; Late-emitted @extern declarations");
        for (const auto& [sym, decl_text] : pending_late_extern_decls_) {
            emit_line(decl_text);
        }
        emit_line("");
        pending_late_extern_decls_.clear();
    }

    // 3. Generic functions (instantiated class constructors/methods) - MUST come before
    //    non-generic functions that call them, to ensure correct forward reference handling
    output_ << generic_func_output.str();

    // 4. Non-generic functions (including test functions that call generic class methods)
    output_ << func_output.str();

    // Emit generated closure functions
    for (const auto& closure_func : module_functions_) {
        scan_for_runtime_refs(closure_func);
        emit(closure_func);
    }

    // Emit vtables for trait objects (dyn dispatch)
    // Note: generate_default_method() called during emit_vtables() may generate new
    // generic type instantiations (e.g. Outcome__Unit__I64). These go to type_defs_buffer_.
    // We need to capture and prepend any new type defs before the functions.
    type_defs_buffer_.str(""); // Clear before vtable generation
    emit_vtables();
    {
        std::string vtable_type_defs = type_defs_buffer_.str();
        if (!vtable_type_defs.empty()) {
            // Prepend type defs to the output - they must appear before functions
            std::string current_output = output_.str();
            output_.str("");
            // Find the position after the "; Generic type instantiations" header
            // by looking for the first "define" or "@vtable" line
            auto define_pos = current_output.find("\ndefine ");
            if (define_pos == std::string::npos)
                define_pos = current_output.find("\n@vtable.");
            if (define_pos != std::string::npos) {
                output_ << current_output.substr(0, define_pos + 1);
                output_ << "; Additional generic type instantiations (from vtable generation)\n";
                output_ << vtable_type_defs;
                output_ << current_output.substr(define_pos + 1);
            } else {
                output_ << vtable_type_defs;
                output_ << current_output;
            }
            type_defs_buffer_.str("");
        }
    }

    // Emit definitions for library functions that were actually referenced
    // by user code, generic instantiations, or other library functions.
    // This replaces the `declare` stubs emitted during module scanning.
    if (options_.lazy_library_defs && !options_.library_ir_only && !options_.library_decls_only) {
        emit_referenced_library_definitions();
    }

    // In library_decls_only + lazy mode: emit `declare` for referenced functions.
    // Without this, lazy mode stores functions as pending but never emits them.
    if (options_.lazy_library_defs && options_.library_decls_only) {
        emit_referenced_library_declarations();
    }

    // Generate entry points (main, test harness, bench harness, HTTP routes)
    generate_main_and_test_harness(module);

    // Emit function attributes for optimization
    // When coverage is enabled, add noinline to prevent LLVM from inlining library functions
    emit_line("");
    emit_line("; Function attributes for optimization");
    // On Windows, add "probe-stack"="inline-asm" to ensure stack pages are
    // probed for functions with large frames (many struct allocas from when-arms,
    // try-operators, etc.). Without this, cumulative alloca adjustments can skip
    // past the 4KB guard page, causing STATUS_HEAP_CORRUPTION.
    const char* probe_attr =
#ifdef _WIN32
        " \"stack-probe-size\"=\"4096\"";
#else
        "";
#endif
    if (options_.coverage_enabled) {
        emit_line(std::string("attributes #0 = { nounwind noinline "
                              "\"target-features\"=\"+sse2,+sse4.2,+avx,+avx2,+fma\"") +
                  probe_attr + " }");
    } else {
        emit_line(std::string("attributes #0 = { nounwind "
                              "\"target-features\"=\"+sse2,+sse4.2,+avx,+avx2,+fma\"") +
                  probe_attr + " }");
    }

    // Emit loop metadata at the end
    emit_loop_metadata();

    // Emit debug info metadata at the end
    emit_debug_info_footer();

    // Emit module identification metadata
    {
        int ident_id = fresh_debug_id();
        emit_line("");
        emit_line("!llvm.ident = !{!" + std::to_string(ident_id) + "}");
        emit_line("!" + std::to_string(ident_id) + " = !{!\"tml version " +
                  std::string(tml::VERSION) + "\"}");
    }

    // Final sweep: scan the complete IR output for any runtime function references
    // that were missed by emit_line() auto-detection. This catches references emitted
    // via emit() (which doesn't scan) — notably, void call instructions in call_user.cpp
    // use emit() for the function name part, bypassing emit_line()'s auto-detection.
    // Also catches references from generate_pending_instantiations() which generates
    // library method bodies (e.g., Text::print calling @print) outside the lazy path.
    scan_for_runtime_refs(output_.str());

    // Append any deferred enum drop functions generated during codegen
    if (!enum_drop_output_.str().empty()) {
        output_ << enum_drop_output_.str();
        // Scan enum drop functions for their own runtime references
        // (e.g., @tml_str_free called within drop functions)
        scan_for_runtime_refs(enum_drop_output_.str());
    }

    // Finalize runtime declarations and splice into output
    finalize_runtime_decls();
    std::string final_output = output_.str();
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = final_output.find(placeholder);
        if (pos != std::string::npos) {
            final_output.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    // Update cached_preamble_headers_ with spliced declarations
    // so capture_library_state() gets the finalized preamble
    {
        const std::string placeholder = "; {{RUNTIME_DECLS_PLACEHOLDER}}\n";
        auto pos = cached_preamble_headers_.find(placeholder);
        if (pos != std::string::npos) {
            cached_preamble_headers_.replace(pos, placeholder.size(), deferred_runtime_decls_);
        }
    }

    if (!errors_.empty()) {
        return errors_;
    }

    // Post-process: hoist non-entry-block allocas into the entry block.
    // The text-based codegen emits allocas inline (e.g., inside when-arms,
    // try ok-blocks), producing dynamic stack allocations. On Windows x64,
    // many small dynamic allocas can cumulatively skip past the stack guard
    // page, causing STATUS_HEAP_CORRUPTION (0xC0000374). Moving all allocas
    // to the entry block lets LLVM compute the total static frame size and
    // insert __chkstk when needed.
    hoist_allocas_to_entry(final_output);

    return final_output;
}

} // namespace tml::codegen
