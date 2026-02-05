//! # LLVM IR Generator - @derive(Hash) Implementation
//!
//! This file implements the `@derive(Hash)` derive macro.
//! Hash generates: `func hash(this) -> I64`
//!
//! ## Generated Code Pattern
//!
//! For a struct like:
//! ```tml
//! @derive(Hash)
//! type Point {
//!     x: I32,
//!     y: I32
//! }
//! ```
//!
//! We generate:
//! ```llvm
//! define i64 @tml_Point_hash(ptr %this) {
//! entry:
//!   ; Start with FNV-1a offset basis
//!   %hash = 14695981039346656037
//!   ; Hash each field and combine
//!   %x_ptr = getelementptr %struct.Point, ptr %this, i32 0, i32 0
//!   %x_val = load i32, ptr %x_ptr
//!   %x_ext = sext i32 %x_val to i64
//!   %hash1 = xor i64 %hash, %x_ext
//!   %hash2 = mul i64 %hash1, 1099511628211  ; FNV prime
//!   ; ... similar for y field ...
//!   ret i64 %hash_final
//! }
//! ```

#include "codegen/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(Hash) decorator
static bool has_derive_hash(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Hash") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Hash) decorator
static bool has_derive_hash(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Hash") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if a type is a primitive that can be hashed directly
static bool is_primitive_hashable(const std::string& llvm_type) {
    // Integer types
    if (llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
        llvm_type == "i64" || llvm_type == "i128") {
        return true;
    }
    // Floating point types - convert to bits first
    if (llvm_type == "float" || llvm_type == "double") {
        return true;
    }
    // Pointers
    if (llvm_type == "ptr") {
        return true;
    }
    return false;
}

// FNV-1a 64-bit constants
static const uint64_t FNV64_OFFSET_BASIS = 14695981039346656037ULL;
static const uint64_t FNV64_PRIME = 1099511628211ULL;

// ============================================================================
// Hash Generation for Structs
// ============================================================================

/// Generate hash() method for a struct with @derive(Hash)
void LLVMIRGen::gen_derive_hash_struct(const parser::StructDecl& s) {
    if (!has_derive_hash(s)) {
        return;
    }

    // Skip generic structs - they need to be instantiated first
    if (!s.generics.empty()) {
        return;
    }

    std::string type_name = s.name;
    std::string llvm_type = "%struct." + type_name;

    // Add suite prefix for test-local types
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string func_name = "@tml_" + suite_prefix + type_name + "_hash";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Get field info for this struct
    auto fields_it = struct_fields_.find(type_name);
    if (fields_it == struct_fields_.end()) {
        return; // No field info, can't generate
    }
    const auto& fields = fields_it->second;

    // Emit function definition
    type_defs_buffer_ << "; @derive(Hash) for " << type_name << "\n";
    type_defs_buffer_ << "define i64 " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - return offset basis
        type_defs_buffer_ << "  ret i64 " << FNV64_OFFSET_BASIS << "\n";
        type_defs_buffer_ << "}\n\n";
        return;
    }

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    // Start with FNV-1a offset basis
    std::string current_hash = fresh_temp();
    type_defs_buffer_ << "  " << current_hash << " = add i64 0, " << FNV64_OFFSET_BASIS << "\n";

    // Hash each field and combine
    for (const auto& field : fields) {
        std::string field_ptr = fresh_temp();
        type_defs_buffer_ << "  " << field_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";

        std::string field_hash;
        if (is_primitive_hashable(field.llvm_type)) {
            // Primitive type - load and convert to i64
            std::string val = fresh_temp();
            type_defs_buffer_ << "  " << val << " = load " << field.llvm_type << ", ptr "
                              << field_ptr << "\n";

            // Convert to i64
            if (field.llvm_type == "i64") {
                field_hash = val;
            } else if (field.llvm_type == "i1") {
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = zext i1 " << val << " to i64\n";
            } else if (field.llvm_type == "i8" || field.llvm_type == "i16" ||
                       field.llvm_type == "i32") {
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = sext " << field.llvm_type << " "
                                  << val << " to i64\n";
            } else if (field.llvm_type == "i128") {
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = trunc i128 " << val << " to i64\n";
            } else if (field.llvm_type == "float") {
                // Convert float bits to i32, then extend to i64
                std::string bits = fresh_temp();
                type_defs_buffer_ << "  " << bits << " = bitcast float " << val << " to i32\n";
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = sext i32 " << bits << " to i64\n";
            } else if (field.llvm_type == "double") {
                // Convert double bits directly to i64
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = bitcast double " << val
                                  << " to i64\n";
            } else if (field.llvm_type == "ptr") {
                // Convert pointer to i64
                field_hash = fresh_temp();
                type_defs_buffer_ << "  " << field_hash << " = ptrtoint ptr " << val << " to i64\n";
            }
        } else {
            // Non-primitive type - call hash() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_hash_func = "@tml_" + suite_prefix + field_type_name + "_hash";
            field_hash = fresh_temp();
            type_defs_buffer_ << "  " << field_hash << " = call i64 " << field_hash_func << "(ptr "
                              << field_ptr << ")\n";
        }

        // Combine hash: hash = (hash ^ field_hash) * FNV_PRIME
        std::string xored = fresh_temp();
        type_defs_buffer_ << "  " << xored << " = xor i64 " << current_hash << ", " << field_hash
                          << "\n";
        std::string new_hash = fresh_temp();
        type_defs_buffer_ << "  " << new_hash << " = mul i64 " << xored << ", " << FNV64_PRIME
                          << "\n";
        current_hash = new_hash;
    }

    // Return final hash
    type_defs_buffer_ << "  ret i64 " << current_hash << "\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Hash Generation for Enums
// ============================================================================

/// Generate hash() method for an enum with @derive(Hash)
void LLVMIRGen::gen_derive_hash_enum(const parser::EnumDecl& e) {
    if (!has_derive_hash(e)) {
        return;
    }

    // Skip generic enums
    if (!e.generics.empty()) {
        return;
    }

    std::string type_name = e.name;
    std::string llvm_type = "%struct." + type_name;

    // Add suite prefix for test-local types
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }

    std::string func_name = "@tml_" + suite_prefix + type_name + "_hash";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // For simple enums (tag-only), just hash the tag
    // For complex enums, we'd need to switch on tag and hash payloads
    type_defs_buffer_ << "; @derive(Hash) for " << type_name << "\n";
    type_defs_buffer_ << "define i64 " << func_name << "(ptr %this) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load tag
    type_defs_buffer_ << "  %tag_ptr = getelementptr " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag = load i32, ptr %tag_ptr\n";
    type_defs_buffer_ << "  %tag_ext = sext i32 %tag to i64\n";

    // Simple hash: offset_basis ^ tag * prime
    type_defs_buffer_ << "  %hash1 = xor i64 " << FNV64_OFFSET_BASIS << ", %tag_ext\n";
    type_defs_buffer_ << "  %hash2 = mul i64 %hash1, " << FNV64_PRIME << "\n";
    type_defs_buffer_ << "  ret i64 %hash2\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
