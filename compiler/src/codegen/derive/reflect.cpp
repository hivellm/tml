//! # LLVM IR Generator - @derive(Reflect) Implementation
//!
//! This file implements the `@derive(Reflect)` derive macro which generates:
//! 1. A static TypeInfo instance for the type
//! 2. An `impl Reflect for T` with `type_info()` method
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(Reflect)
//! type Person {
//!     name: Str,
//!     age: I32
//! }
//! ```
//!
//! We generate:
//! 1. Static TypeInfo: `@__typeinfo_Person = private constant %struct.TypeInfo { ... }`
//! 2. Impl: `func Person::type_info() -> ref TypeInfo` that returns the static TypeInfo

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Reflect) decorator
static bool has_derive_reflect(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    if (arg->as<parser::IdentExpr>().name == "Reflect") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Reflect) decorator
static bool has_derive_reflect(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    if (arg->as<parser::IdentExpr>().name == "Reflect") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// ============================================================================
// TypeInfo Generation
// ============================================================================

/// Generate static TypeInfo for a struct with @derive(Reflect)
void LLVMIRGen::gen_derive_reflect_struct(const parser::StructDecl& s) {
    if (!has_derive_reflect(s)) {
        return;
    }

    // Skip generic structs - they need to be instantiated first
    if (!s.generics.empty()) {
        return;
    }

    std::string type_name = s.name;
    std::string typeinfo_name = "@__typeinfo_" + type_name;

    // Skip if already generated
    if (generated_typeinfo_.count(type_name) > 0) {
        return;
    }
    generated_typeinfo_.insert(type_name);

    // Generate a type ID using FNV-1a hash
    uint64_t type_id = 14695981039346656037ULL;
    for (char c : type_name) {
        type_id ^= static_cast<uint64_t>(c);
        type_id *= 1099511628211ULL;
    }

    // Get the name as a string constant
    std::string name_const = add_string_literal(type_name);

    // Calculate size and alignment using LLVM intrinsics
    std::string llvm_type = "%struct." + type_name;

    // TypeKind::Struct = 0
    int type_kind = 0;

    // Field count
    size_t field_count = s.fields.size();

    // Note: %struct.TypeInfo is already defined in core::reflect.tml
    // and will be emitted by the normal struct codegen.
    // We just need to emit the static TypeInfo instance.

    // We need to compute size and alignment at runtime using GEP trick
    // For now, use placeholder values that will be filled in later
    // TODO: Compute actual size/align

    // Emit static TypeInfo as a global constant
    // The size and align will be computed by LLVM
    std::string def = typeinfo_name + " = private constant %struct.TypeInfo { ";
    def += "i64 " + std::to_string(type_id) + ", ";     // id
    def += "ptr " + name_const + ", ";                  // name
    def += "i32 " + std::to_string(type_kind) + ", ";   // kind (Struct = 0)
    def += "i64 0, ";                                   // size (placeholder)
    def += "i64 0, ";                                   // align (placeholder)
    def += "i64 " + std::to_string(field_count) + ", "; // field_count
    def += "i64 0";                                     // variant_count
    def += " }";

    type_defs_buffer_ << def << "\n";

    // Generate the impl Reflect for T
    gen_derive_reflect_impl(type_name, typeinfo_name);
}

/// Generate static TypeInfo for an enum with @derive(Reflect)
void LLVMIRGen::gen_derive_reflect_enum(const parser::EnumDecl& e) {
    if (!has_derive_reflect(e)) {
        return;
    }

    // Skip generic enums
    if (!e.generics.empty()) {
        return;
    }

    std::string type_name = e.name;
    std::string typeinfo_name = "@__typeinfo_" + type_name;

    // Skip if already generated
    if (generated_typeinfo_.count(type_name) > 0) {
        return;
    }
    generated_typeinfo_.insert(type_name);

    // Generate a type ID using FNV-1a hash
    uint64_t type_id = 14695981039346656037ULL;
    for (char c : type_name) {
        type_id ^= static_cast<uint64_t>(c);
        type_id *= 1099511628211ULL;
    }

    // Get the name as a string constant
    std::string name_const = add_string_literal(type_name);

    // TypeKind::Enum = 1
    int type_kind = 1;

    // Variant count
    size_t variant_count = e.variants.size();

    // Note: %struct.TypeInfo is already defined in core::reflect.tml

    // Emit static TypeInfo as a global constant
    std::string def = typeinfo_name + " = private constant %struct.TypeInfo { ";
    def += "i64 " + std::to_string(type_id) + ", ";   // id
    def += "ptr " + name_const + ", ";                // name
    def += "i32 " + std::to_string(type_kind) + ", "; // kind (Enum = 1)
    def += "i64 0, ";                                 // size (placeholder)
    def += "i64 0, ";                                 // align (placeholder)
    def += "i64 0, ";                                 // field_count
    def += "i64 " + std::to_string(variant_count);    // variant_count
    def += " }";

    type_defs_buffer_ << def << "\n";

    // Generate the impl Reflect for T
    gen_derive_reflect_impl(type_name, typeinfo_name);
}

/// Generate impl Reflect for T with type_info() method
void LLVMIRGen::gen_derive_reflect_impl(const std::string& type_name,
                                        const std::string& typeinfo_name) {
    // Generate: func T::type_info() -> ref TypeInfo
    // This returns a pointer to the static TypeInfo

    std::string func_name = "@tml_" + type_name + "_type_info";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Emit the function using emit_line (which outputs to output_)
    emit_line("; impl Reflect for " + type_name);
    emit_line("define ptr " + func_name + "() {");
    emit_line("entry:");
    emit_line("  ret ptr " + typeinfo_name);
    emit_line("}");
    emit_line("");
}

} // namespace tml::codegen
