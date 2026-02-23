TML_MODULE("codegen_x86")

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

#include "codegen/llvm/llvm_ir_gen.hpp"

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
    // TypeInfo layout: { i64, ptr, %struct.TypeKind, i64, i64, i64, i64 }
    // TypeKind is a struct wrapper: { i32 }
    std::string def = typeinfo_name + " = private constant %struct.TypeInfo { ";
    def += "i64 " + std::to_string(type_id) + ", ";                        // id
    def += "ptr " + name_const + ", ";                                     // name
    def += "%struct.TypeKind { i32 " + std::to_string(type_kind) + " }, "; // kind (Struct = 0)
    def += "i64 0, ";                                                      // size (placeholder)
    def += "i64 0, ";                                                      // align (placeholder)
    def += "i64 " + std::to_string(field_count) + ", ";                    // field_count
    def += "i64 0";                                                        // variant_count
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
    // TypeInfo layout: { i64, ptr, %struct.TypeKind, i64, i64, i64, i64 }
    // TypeKind is a struct wrapper: { i32 }
    std::string def = typeinfo_name + " = private constant %struct.TypeInfo { ";
    def += "i64 " + std::to_string(type_id) + ", ";                        // id
    def += "ptr " + name_const + ", ";                                     // name
    def += "%struct.TypeKind { i32 " + std::to_string(type_kind) + " }, "; // kind (Enum = 1)
    def += "i64 0, ";                                                      // size (placeholder)
    def += "i64 0, ";                                                      // align (placeholder)
    def += "i64 0, ";                                                      // field_count
    def += "i64 " + std::to_string(variant_count);                         // variant_count
    def += " }";

    type_defs_buffer_ << def << "\n";

    // Generate the impl Reflect for T (type_info and runtime_type_info)
    gen_derive_reflect_impl(type_name, typeinfo_name);

    // Generate enum-specific methods: variant_name and variant_tag
    gen_derive_reflect_enum_methods(e, type_name);
}

/// Generate variant_name() and variant_tag() methods for an enum
/// NOTE: We emit to type_defs_buffer_ (not output_) to ensure the enum type
/// is defined before these functions reference it in GEP instructions.
void LLVMIRGen::gen_derive_reflect_enum_methods(const parser::EnumDecl& e,
                                                const std::string& type_name) {
    // Add suite prefix for test-local types
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string llvm_type = "%struct." + type_name;

    // Generate: func EnumName::variant_name(this) -> Str
    std::string variant_name_func = "@tml_" + suite_prefix + type_name + "_variant_name";
    if (generated_functions_.count(variant_name_func) == 0) {
        generated_functions_.insert(variant_name_func);

        // Create string constants for each variant name
        std::vector<std::string> variant_name_consts;
        for (const auto& variant : e.variants) {
            variant_name_consts.push_back(add_string_literal(variant.name));
        }

        // Emit to type_defs_buffer_ to ensure type is defined before use
        type_defs_buffer_ << "; impl Reflect for " << type_name << " - variant_name()\n";
        type_defs_buffer_ << "define internal ptr " << variant_name_func << "(ptr %this) {\n";
        type_defs_buffer_ << "entry:\n";
        // Load the discriminant tag (first field of enum struct)
        type_defs_buffer_ << "  %tag_ptr = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 0\n";
        type_defs_buffer_ << "  %tag = load i32, ptr %tag_ptr\n";

        // Generate switch statement to return the variant name
        type_defs_buffer_ << "  switch i32 %tag, label %default [\n";
        for (size_t i = 0; i < e.variants.size(); ++i) {
            type_defs_buffer_ << "    i32 " << i << ", label %variant_" << i << "\n";
        }
        type_defs_buffer_ << "  ]\n";

        // Generate labels for each variant
        for (size_t i = 0; i < e.variants.size(); ++i) {
            type_defs_buffer_ << "variant_" << i << ":\n";
            type_defs_buffer_ << "  ret ptr " << variant_name_consts[i] << "\n";
        }

        // Default case (should never happen)
        type_defs_buffer_ << "default:\n";
        type_defs_buffer_ << "  ret ptr " << add_string_literal("unknown") << "\n";
        type_defs_buffer_ << "}\n\n";
    }

    // Generate: func EnumName::variant_tag(this) -> I64
    std::string variant_tag_func = "@tml_" + suite_prefix + type_name + "_variant_tag";
    if (generated_functions_.count(variant_tag_func) == 0) {
        generated_functions_.insert(variant_tag_func);

        // Emit to type_defs_buffer_ to ensure type is defined before use
        type_defs_buffer_ << "; impl Reflect for " << type_name << " - variant_tag()\n";
        type_defs_buffer_ << "define internal i64 " << variant_tag_func << "(ptr %this) {\n";
        type_defs_buffer_ << "entry:\n";
        // Load the discriminant tag and sign-extend to i64
        type_defs_buffer_ << "  %tag_ptr = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 0\n";
        type_defs_buffer_ << "  %tag = load i32, ptr %tag_ptr\n";
        type_defs_buffer_ << "  %tag64 = sext i32 %tag to i64\n";
        type_defs_buffer_ << "  ret i64 %tag64\n";
        type_defs_buffer_ << "}\n\n";
    }
}

/// Generate impl Reflect for T with type_info() and runtime_type_info() methods
void LLVMIRGen::gen_derive_reflect_impl(const std::string& type_name,
                                        const std::string& typeinfo_name) {
    // Add suite prefix for test-local types (same as gen_func_decl does)
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    // Generate: func T::type_info() -> ref TypeInfo (static method)
    std::string static_func_name = "@tml_" + suite_prefix + type_name + "_type_info";
    if (generated_functions_.count(static_func_name) == 0) {
        generated_functions_.insert(static_func_name);

        emit_line("; impl Reflect for " + type_name + " - static type_info()");
        emit_line("define ptr " + static_func_name + "() {");
        emit_line("entry:");
        emit_line("  ret ptr " + typeinfo_name);
        emit_line("}");
        emit_line("");
    }

    // Generate: func T::runtime_type_info(ref this) -> ref TypeInfo (instance method)
    std::string instance_func_name = "@tml_" + suite_prefix + type_name + "_runtime_type_info";
    if (generated_functions_.count(instance_func_name) == 0) {
        generated_functions_.insert(instance_func_name);

        emit_line("; impl Reflect for " + type_name + " - instance runtime_type_info()");
        emit_line("define ptr " + instance_func_name + "(ptr %this) {");
        emit_line("entry:");
        emit_line("  ret ptr " + typeinfo_name);
        emit_line("}");
        emit_line("");
    }
}

} // namespace tml::codegen
