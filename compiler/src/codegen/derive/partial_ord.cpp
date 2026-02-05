//! # LLVM IR Generator - @derive(PartialOrd, Ord) Implementation
//!
//! This file implements the `@derive(PartialOrd)` and `@derive(Ord)` derive macros.
//! PartialOrd generates: `func partial_cmp(this, other: ref Self) -> Maybe[Ordering]`
//! Ord generates: `func cmp(this, other: ref Self) -> Ordering`
//!
//! ## Generated Code Pattern
//!
//! For lexicographic comparison, we compare fields in order.
//! If a field comparison returns Less or Greater, we return that immediately.
//! If Equal, we continue to the next field.

#include "codegen/llvm_ir_gen.hpp"
#include "derive/registry.hpp"

namespace tml::codegen {

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a struct has @derive(PartialOrd) decorator
static bool has_derive_partial_ord(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "PartialOrd") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if a struct has @derive(Ord) decorator
static bool has_derive_ord(const parser::StructDecl& s) {
    for (const auto& deco : s.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Ord") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(PartialOrd) decorator
static bool has_derive_partial_ord(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "PartialOrd") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if an enum has @derive(Ord) decorator
static bool has_derive_ord(const parser::EnumDecl& e) {
    for (const auto& deco : e.decorators) {
        if (deco.name == "derive") {
            for (const auto& arg : deco.args) {
                if (arg->is<parser::IdentExpr>()) {
                    const auto& name = arg->as<parser::IdentExpr>().name;
                    if (name == "Ord") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check if a type is a primitive that can be compared with icmp/fcmp
static bool is_primitive_comparable(const std::string& llvm_type) {
    // Integer types
    if (llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
        llvm_type == "i64" || llvm_type == "i128") {
        return true;
    }
    // Floating point types
    if (llvm_type == "float" || llvm_type == "double") {
        return true;
    }
    return false;
}

/// Check if a type is an integer type (for signed comparison)
static bool is_integer_type(const std::string& llvm_type) {
    return llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" || llvm_type == "i32" ||
           llvm_type == "i64" || llvm_type == "i128";
}

// ============================================================================
// Ord (cmp) Generation for Structs
// ============================================================================

/// Generate cmp() method for a struct with @derive(Ord)
void LLVMIRGen::gen_derive_ord_struct(const parser::StructDecl& s) {
    if (!has_derive_ord(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_cmp";

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

    // Emit function definition - returns Ordering
    type_defs_buffer_ << "; @derive(Ord) for " << type_name << "\n";
    type_defs_buffer_ << "define %struct.Ordering " << func_name << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - always equal
        type_defs_buffer_ << "  ret %struct.Ordering { i32 1 } ; Equal\n";
        type_defs_buffer_ << "}\n\n";
        return;
    }

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    int field_idx = 0;
    // Compare each field lexicographically
    for (const auto& field : fields) {
        std::string this_ptr = fresh_temp();
        std::string other_ptr = fresh_temp();

        type_defs_buffer_ << "  " << this_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";
        type_defs_buffer_ << "  " << other_ptr << " = getelementptr " << llvm_type
                          << ", ptr %other, i32 0, i32 " << field.index << "\n";

        if (is_primitive_comparable(field.llvm_type)) {
            // Primitive type - direct comparison
            std::string this_val = fresh_temp();
            std::string other_val = fresh_temp();
            type_defs_buffer_ << "  " << this_val << " = load " << field.llvm_type << ", ptr "
                              << this_ptr << "\n";
            type_defs_buffer_ << "  " << other_val << " = load " << field.llvm_type << ", ptr "
                              << other_ptr << "\n";

            std::string is_less = fresh_temp();
            std::string is_greater = fresh_temp();

            if (is_integer_type(field.llvm_type)) {
                // Signed integer comparison
                type_defs_buffer_ << "  " << is_less << " = icmp slt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
                type_defs_buffer_ << "  " << is_greater << " = icmp sgt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            } else {
                // Float comparison
                type_defs_buffer_ << "  " << is_less << " = fcmp olt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
                type_defs_buffer_ << "  " << is_greater << " = fcmp ogt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            }

            std::string label_less = "field" + std::to_string(field_idx) + "_less";
            std::string label_check_greater = "field" + std::to_string(field_idx) + "_check_gt";
            std::string label_greater = "field" + std::to_string(field_idx) + "_greater";
            std::string label_next = (field_idx < static_cast<int>(fields.size()) - 1)
                                         ? "field" + std::to_string(field_idx + 1)
                                         : "ret_equal";

            type_defs_buffer_ << "  br i1 " << is_less << ", label %" << label_less << ", label %"
                              << label_check_greater << "\n";
            type_defs_buffer_ << label_less << ":\n";
            type_defs_buffer_ << "  ret %struct.Ordering { i32 0 } ; Less\n";
            type_defs_buffer_ << label_check_greater << ":\n";
            type_defs_buffer_ << "  br i1 " << is_greater << ", label %" << label_greater
                              << ", label %" << label_next << "\n";
            type_defs_buffer_ << label_greater << ":\n";
            type_defs_buffer_ << "  ret %struct.Ordering { i32 2 } ; Greater\n";

            if (field_idx < static_cast<int>(fields.size()) - 1) {
                type_defs_buffer_ << label_next << ":\n";
            }
        } else {
            // Non-primitive type - call cmp() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_cmp_func = "@tml_" + suite_prefix + field_type_name + "_cmp";
            std::string cmp_result = fresh_temp();
            type_defs_buffer_ << "  " << cmp_result << " = call %struct.Ordering " << field_cmp_func
                              << "(ptr " << this_ptr << ", ptr " << other_ptr << ")\n";

            // Extract tag from Ordering
            std::string tag = fresh_temp();
            type_defs_buffer_ << "  " << tag << " = extractvalue %struct.Ordering " << cmp_result
                              << ", 0\n";

            // Check if not equal (tag != 1)
            std::string is_not_equal = fresh_temp();
            type_defs_buffer_ << "  " << is_not_equal << " = icmp ne i32 " << tag << ", 1\n";

            std::string label_not_equal = "field" + std::to_string(field_idx) + "_not_eq";
            std::string label_next = (field_idx < static_cast<int>(fields.size()) - 1)
                                         ? "field" + std::to_string(field_idx + 1)
                                         : "ret_equal";

            type_defs_buffer_ << "  br i1 " << is_not_equal << ", label %" << label_not_equal
                              << ", label %" << label_next << "\n";
            type_defs_buffer_ << label_not_equal << ":\n";
            type_defs_buffer_ << "  ret %struct.Ordering " << cmp_result << "\n";

            if (field_idx < static_cast<int>(fields.size()) - 1) {
                type_defs_buffer_ << label_next << ":\n";
            }
        }
        field_idx++;
    }

    // All fields equal
    type_defs_buffer_ << "ret_equal:\n";
    type_defs_buffer_ << "  ret %struct.Ordering { i32 1 } ; Equal\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// PartialOrd (partial_cmp) Generation for Structs
// ============================================================================

/// Generate partial_cmp() method for a struct with @derive(PartialOrd)
void LLVMIRGen::gen_derive_partial_ord_struct(const parser::StructDecl& s) {
    if (!has_derive_partial_ord(s)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_partial_cmp";

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

    // Ensure Maybe[Ordering] struct type is defined
    // Create an Ordering semantic type for the type argument
    auto ordering_type = std::make_shared<types::Type>();
    ordering_type->kind = types::NamedType{"Ordering", "", {}};
    std::vector<types::TypePtr> maybe_type_args = {ordering_type};
    std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
    std::string maybe_type = "%struct." + maybe_mangled;

    // Emit function definition - returns Maybe[Ordering]
    type_defs_buffer_ << "; @derive(PartialOrd) for " << type_name << "\n";
    type_defs_buffer_ << "define " << maybe_type << " " << func_name
                      << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    if (fields.empty()) {
        // Empty struct - always equal, return Just(Equal)
        type_defs_buffer_ << "  %ret = alloca " << maybe_type << "\n";
        type_defs_buffer_ << "  %tag_ptr = getelementptr " << maybe_type
                          << ", ptr %ret, i32 0, i32 0\n";
        type_defs_buffer_ << "  store i32 0, ptr %tag_ptr ; Just\n";
        type_defs_buffer_ << "  %payload_ptr = getelementptr " << maybe_type
                          << ", ptr %ret, i32 0, i32 1\n";
        type_defs_buffer_ << "  store i32 1, ptr %payload_ptr ; Equal\n";
        type_defs_buffer_ << "  %result = load " << maybe_type << ", ptr %ret\n";
        type_defs_buffer_ << "  ret " << maybe_type << " %result\n";
        type_defs_buffer_ << "}\n\n";
        return;
    }

    int temp_counter = 0;
    auto fresh_temp = [&temp_counter]() -> std::string {
        return "%t" + std::to_string(temp_counter++);
    };

    int field_idx = 0;
    // Compare each field lexicographically
    for (const auto& field : fields) {
        std::string this_ptr = fresh_temp();
        std::string other_ptr = fresh_temp();

        type_defs_buffer_ << "  " << this_ptr << " = getelementptr " << llvm_type
                          << ", ptr %this, i32 0, i32 " << field.index << "\n";
        type_defs_buffer_ << "  " << other_ptr << " = getelementptr " << llvm_type
                          << ", ptr %other, i32 0, i32 " << field.index << "\n";

        if (is_primitive_comparable(field.llvm_type)) {
            // Primitive type - direct comparison
            std::string this_val = fresh_temp();
            std::string other_val = fresh_temp();
            type_defs_buffer_ << "  " << this_val << " = load " << field.llvm_type << ", ptr "
                              << this_ptr << "\n";
            type_defs_buffer_ << "  " << other_val << " = load " << field.llvm_type << ", ptr "
                              << other_ptr << "\n";

            std::string is_less = fresh_temp();
            std::string is_greater = fresh_temp();

            if (is_integer_type(field.llvm_type)) {
                // Signed integer comparison
                type_defs_buffer_ << "  " << is_less << " = icmp slt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
                type_defs_buffer_ << "  " << is_greater << " = icmp sgt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            } else {
                // Float comparison (unordered for NaN handling)
                type_defs_buffer_ << "  " << is_less << " = fcmp olt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
                type_defs_buffer_ << "  " << is_greater << " = fcmp ogt " << field.llvm_type << " "
                                  << this_val << ", " << other_val << "\n";
            }

            std::string label_less = "field" + std::to_string(field_idx) + "_less";
            std::string label_check_greater = "field" + std::to_string(field_idx) + "_check_gt";
            std::string label_greater = "field" + std::to_string(field_idx) + "_greater";
            std::string label_next = (field_idx < static_cast<int>(fields.size()) - 1)
                                         ? "field" + std::to_string(field_idx + 1)
                                         : "ret_equal";

            type_defs_buffer_ << "  br i1 " << is_less << ", label %" << label_less << ", label %"
                              << label_check_greater << "\n";
            type_defs_buffer_ << label_less << ":\n";
            // Return Just(Less)
            type_defs_buffer_ << "  %less_ret" << field_idx << " = alloca " << maybe_type << "\n";
            type_defs_buffer_ << "  %less_tag" << field_idx << " = getelementptr " << maybe_type
                              << ", ptr %less_ret" << field_idx << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store i32 0, ptr %less_tag" << field_idx << "\n";
            type_defs_buffer_ << "  %less_payload" << field_idx << " = getelementptr " << maybe_type
                              << ", ptr %less_ret" << field_idx << ", i32 0, i32 1\n";
            type_defs_buffer_ << "  store i32 0, ptr %less_payload" << field_idx << "\n";
            type_defs_buffer_ << "  %less_result" << field_idx << " = load " << maybe_type
                              << ", ptr %less_ret" << field_idx << "\n";
            type_defs_buffer_ << "  ret " << maybe_type << " %less_result" << field_idx << "\n";

            type_defs_buffer_ << label_check_greater << ":\n";
            type_defs_buffer_ << "  br i1 " << is_greater << ", label %" << label_greater
                              << ", label %" << label_next << "\n";

            type_defs_buffer_ << label_greater << ":\n";
            // Return Just(Greater)
            type_defs_buffer_ << "  %greater_ret" << field_idx << " = alloca " << maybe_type
                              << "\n";
            type_defs_buffer_ << "  %greater_tag" << field_idx << " = getelementptr " << maybe_type
                              << ", ptr %greater_ret" << field_idx << ", i32 0, i32 0\n";
            type_defs_buffer_ << "  store i32 0, ptr %greater_tag" << field_idx << "\n";
            type_defs_buffer_ << "  %greater_payload" << field_idx << " = getelementptr "
                              << maybe_type << ", ptr %greater_ret" << field_idx
                              << ", i32 0, i32 1\n";
            type_defs_buffer_ << "  store i32 2, ptr %greater_payload" << field_idx << "\n";
            type_defs_buffer_ << "  %greater_result" << field_idx << " = load " << maybe_type
                              << ", ptr %greater_ret" << field_idx << "\n";
            type_defs_buffer_ << "  ret " << maybe_type << " %greater_result" << field_idx << "\n";

            if (field_idx < static_cast<int>(fields.size()) - 1) {
                type_defs_buffer_ << label_next << ":\n";
            }
        } else {
            // Non-primitive type - call partial_cmp() on the field
            std::string field_type_name;
            if (field.llvm_type.substr(0, 8) == "%struct.") {
                field_type_name = field.llvm_type.substr(8);
            } else {
                field_type_name = field.llvm_type;
            }

            std::string field_cmp_func = "@tml_" + suite_prefix + field_type_name + "_partial_cmp";
            std::string cmp_result = fresh_temp();
            type_defs_buffer_ << "  " << cmp_result << " = call " << maybe_type << " "
                              << field_cmp_func << "(ptr " << this_ptr << ", ptr " << other_ptr
                              << ")\n";

            // Extract tag from Maybe[Ordering] - check if Just
            std::string maybe_tag = fresh_temp();
            type_defs_buffer_ << "  " << maybe_tag << " = extractvalue " << maybe_type << " "
                              << cmp_result << ", 0\n";

            // Check if Nothing (tag == 1)
            std::string is_nothing = fresh_temp();
            type_defs_buffer_ << "  " << is_nothing << " = icmp eq i32 " << maybe_tag << ", 1\n";

            std::string label_nothing = "field" + std::to_string(field_idx) + "_nothing";
            std::string label_just = "field" + std::to_string(field_idx) + "_just";

            type_defs_buffer_ << "  br i1 " << is_nothing << ", label %" << label_nothing
                              << ", label %" << label_just << "\n";

            type_defs_buffer_ << label_nothing << ":\n";
            type_defs_buffer_ << "  ret " << maybe_type << " " << cmp_result << " ; Nothing\n";

            type_defs_buffer_ << label_just << ":\n";
            // Extract Ordering from payload and check if Equal
            // For simplicity, extract the payload as i32 (Ordering tag)
            std::string payload = fresh_temp();
            type_defs_buffer_ << "  " << payload << " = extractvalue " << maybe_type << " "
                              << cmp_result << ", 1\n";
            std::string ordering_tag = fresh_temp();
            type_defs_buffer_ << "  " << ordering_tag << " = extractvalue [1 x i64] " << payload
                              << ", 0\n";
            std::string ordering_i32 = fresh_temp();
            type_defs_buffer_ << "  " << ordering_i32 << " = trunc i64 " << ordering_tag
                              << " to i32\n";

            // Check if not Equal (ordering != 1)
            std::string is_not_equal = fresh_temp();
            type_defs_buffer_ << "  " << is_not_equal << " = icmp ne i32 " << ordering_i32
                              << ", 1\n";

            std::string label_not_equal = "field" + std::to_string(field_idx) + "_not_eq";
            std::string label_next = (field_idx < static_cast<int>(fields.size()) - 1)
                                         ? "field" + std::to_string(field_idx + 1)
                                         : "ret_equal";

            type_defs_buffer_ << "  br i1 " << is_not_equal << ", label %" << label_not_equal
                              << ", label %" << label_next << "\n";

            type_defs_buffer_ << label_not_equal << ":\n";
            type_defs_buffer_ << "  ret " << maybe_type << " " << cmp_result << "\n";

            if (field_idx < static_cast<int>(fields.size()) - 1) {
                type_defs_buffer_ << label_next << ":\n";
            }
        }
        field_idx++;
    }

    // All fields equal - return Just(Equal)
    type_defs_buffer_ << "ret_equal:\n";
    type_defs_buffer_ << "  %eq_ret = alloca " << maybe_type << "\n";
    type_defs_buffer_ << "  %eq_tag = getelementptr " << maybe_type
                      << ", ptr %eq_ret, i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 0, ptr %eq_tag ; Just\n";
    type_defs_buffer_ << "  %eq_payload = getelementptr " << maybe_type
                      << ", ptr %eq_ret, i32 0, i32 1\n";
    type_defs_buffer_ << "  store i32 1, ptr %eq_payload ; Equal\n";
    type_defs_buffer_ << "  %eq_result = load " << maybe_type << ", ptr %eq_ret\n";
    type_defs_buffer_ << "  ret " << maybe_type << " %eq_result\n";
    type_defs_buffer_ << "}\n\n";
}

// ============================================================================
// Enum Support
// ============================================================================

/// Generate cmp() method for an enum with @derive(Ord)
void LLVMIRGen::gen_derive_ord_enum(const parser::EnumDecl& e) {
    if (!has_derive_ord(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_cmp";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // For simple enums (tag-only), just compare tags
    type_defs_buffer_ << "; @derive(Ord) for " << type_name << "\n";
    type_defs_buffer_ << "define %struct.Ordering " << func_name << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load tags
    type_defs_buffer_ << "  %tag_this_ptr = getelementptr " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag_other_ptr = getelementptr " << llvm_type
                      << ", ptr %other, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag_this = load i32, ptr %tag_this_ptr\n";
    type_defs_buffer_ << "  %tag_other = load i32, ptr %tag_other_ptr\n";

    // Compare tags
    type_defs_buffer_ << "  %is_less = icmp slt i32 %tag_this, %tag_other\n";
    type_defs_buffer_ << "  %is_greater = icmp sgt i32 %tag_this, %tag_other\n";

    type_defs_buffer_ << "  br i1 %is_less, label %ret_less, label %check_greater\n";
    type_defs_buffer_ << "ret_less:\n";
    type_defs_buffer_ << "  ret %struct.Ordering { i32 0 } ; Less\n";
    type_defs_buffer_ << "check_greater:\n";
    type_defs_buffer_ << "  br i1 %is_greater, label %ret_greater, label %ret_equal\n";
    type_defs_buffer_ << "ret_greater:\n";
    type_defs_buffer_ << "  ret %struct.Ordering { i32 2 } ; Greater\n";
    type_defs_buffer_ << "ret_equal:\n";
    type_defs_buffer_ << "  ret %struct.Ordering { i32 1 } ; Equal\n";
    type_defs_buffer_ << "}\n\n";
}

/// Generate partial_cmp() method for an enum with @derive(PartialOrd)
void LLVMIRGen::gen_derive_partial_ord_enum(const parser::EnumDecl& e) {
    if (!has_derive_partial_ord(e)) {
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

    std::string func_name = "@tml_" + suite_prefix + type_name + "_partial_cmp";

    // Skip if already generated
    if (generated_functions_.count(func_name) > 0) {
        return;
    }
    generated_functions_.insert(func_name);

    // Ensure Maybe[Ordering] struct type is defined
    auto ordering_type = std::make_shared<types::Type>();
    ordering_type->kind = types::NamedType{"Ordering", "", {}};
    std::vector<types::TypePtr> maybe_type_args = {ordering_type};
    std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
    std::string maybe_type = "%struct." + maybe_mangled;

    // For simple enums (tag-only), just compare tags and wrap in Just
    type_defs_buffer_ << "; @derive(PartialOrd) for " << type_name << "\n";
    type_defs_buffer_ << "define " << maybe_type << " " << func_name
                      << "(ptr %this, ptr %other) {\n";
    type_defs_buffer_ << "entry:\n";

    // Load tags
    type_defs_buffer_ << "  %tag_this_ptr = getelementptr " << llvm_type
                      << ", ptr %this, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag_other_ptr = getelementptr " << llvm_type
                      << ", ptr %other, i32 0, i32 0\n";
    type_defs_buffer_ << "  %tag_this = load i32, ptr %tag_this_ptr\n";
    type_defs_buffer_ << "  %tag_other = load i32, ptr %tag_other_ptr\n";

    // Compare tags
    type_defs_buffer_ << "  %is_less = icmp slt i32 %tag_this, %tag_other\n";
    type_defs_buffer_ << "  %is_greater = icmp sgt i32 %tag_this, %tag_other\n";

    // Determine ordering value: 0=Less, 1=Equal, 2=Greater
    type_defs_buffer_ << "  %ord1 = select i1 %is_less, i32 0, i32 1\n";
    type_defs_buffer_ << "  %ordering = select i1 %is_greater, i32 2, i32 %ord1\n";

    // Build Just(ordering) result
    type_defs_buffer_ << "  %ret = alloca " << maybe_type << "\n";
    type_defs_buffer_ << "  %tag_ptr = getelementptr " << maybe_type
                      << ", ptr %ret, i32 0, i32 0\n";
    type_defs_buffer_ << "  store i32 0, ptr %tag_ptr ; Just\n";
    type_defs_buffer_ << "  %payload_ptr = getelementptr " << maybe_type
                      << ", ptr %ret, i32 0, i32 1\n";
    type_defs_buffer_ << "  store i32 %ordering, ptr %payload_ptr\n";
    type_defs_buffer_ << "  %result = load " << maybe_type << ", ptr %ret\n";
    type_defs_buffer_ << "  ret " << maybe_type << " %result\n";
    type_defs_buffer_ << "}\n\n";
}

} // namespace tml::codegen
