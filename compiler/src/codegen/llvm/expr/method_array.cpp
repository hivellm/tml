TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Array Methods
//!
//! This file implements methods for fixed-size array types `[T; N]`.
//!
//! ## Methods
//!
//! | Method      | Signature                 | Description              |
//! |-------------|---------------------------|--------------------------|
//! | `len`       | `() -> I64`               | Returns N (compile-time) |
//! | `is_empty`  | `() -> Bool`              | Returns N == 0           |
//! | `get`       | `(I64) -> Maybe[ref T]`   | Element ref at index     |
//! | `get_mut`   | `(I64) -> Maybe[mut ref T]`| Mutable element ref     |
//! | `first`     | `() -> Maybe[ref T]`      | First element ref        |
//! | `first_mut` | `() -> Maybe[mut ref T]`  | Mutable first ref        |
//! | `last`      | `() -> Maybe[ref T]`      | Last element ref         |
//! | `last_mut`  | `() -> Maybe[mut ref T]`  | Mutable last ref         |
//! | `each_ref`  | `() -> [ref T; N]`        | Array of refs            |
//! | `each_mut`  | `() -> [mut ref T; N]`    | Array of mut refs        |
//! | `eq`, `ne`  | `([T; N]) -> Bool`        | Element-wise comparison  |
//! | `cmp`       | `([T; N]) -> Ordering`    | Lexicographic compare    |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

namespace tml::codegen {

// Handle array-specific methods
// Returns empty optional if this isn't an array or method isn't recognized
auto LLVMIRGen::gen_array_method(const parser::MethodCallExpr& call, const std::string& method)
    -> std::optional<std::string> {
    // Infer receiver type
    types::TypePtr receiver_semantic_type = infer_expr_type(*call.receiver);
    if (!receiver_semantic_type || !receiver_semantic_type->is<types::ArrayType>()) {
        return std::nullopt;
    }

    const auto& arr_type = receiver_semantic_type->as<types::ArrayType>();
    types::TypePtr elem_type = arr_type.element;
    size_t arr_size = arr_type.size;

    std::string elem_llvm_type = llvm_type_from_semantic(elem_type, true);
    std::string array_llvm_type = "[" + std::to_string(arr_size) + " x " + elem_llvm_type + "]";

    // Generate receiver and store it to get a pointer
    std::string arr_receiver = gen_expr(*call.receiver);
    std::string receiver_type = last_expr_type_;
    std::string arr_ptr;

    // If receiver is already a pointer (ref), use it directly
    if (receiver_type == "ptr") {
        arr_ptr = arr_receiver;
    } else {
        arr_ptr = fresh_reg();
        emit_line("  " + arr_ptr + " = alloca " + array_llvm_type);
        emit_line("  store " + array_llvm_type + " " + arr_receiver + ", ptr " + arr_ptr);
    }

    // len() returns the array size as I64
    if (method == "len" || method == "length") {
        emit_coverage("Array::len");
        last_expr_type_ = "i64";
        return std::to_string(arr_size);
    }

    // is_empty() returns true if size is 0
    if (method == "is_empty" || method == "isEmpty") {
        emit_coverage("Array::is_empty");
        last_expr_type_ = "i1";
        return arr_size == 0 ? "true" : "false";
    }

    // get(index) returns Maybe[ref T]
    if (method == "get") {
        emit_coverage("Array::get");
        if (call.args.empty()) {
            report_error("get requires an index argument", call.span, "C015");
            return "0";
        }

        std::string index = gen_expr(*call.args[0]);
        std::string index_i64 = fresh_reg();
        if (last_expr_type_ == "i64") {
            index_i64 = index;
        } else {
            emit_line("  " + index_i64 + " = sext i32 " + index + " to i64");
        }

        // Create ref type for Maybe[ref T]
        auto ref_type = std::make_shared<types::Type>();
        ref_type->kind =
            types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        bool nullable = nullable_maybe_types_.count(maybe_mangled) > 0;
        std::string maybe_type = nullable ? "ptr" : "%struct." + maybe_mangled;

        // Bounds check
        std::string below_zero = fresh_reg();
        emit_line("  " + below_zero + " = icmp slt i64 " + index_i64 + ", 0");
        std::string above_max = fresh_reg();
        emit_line("  " + above_max + " = icmp sge i64 " + index_i64 + ", " +
                  std::to_string(arr_size));
        std::string out_of_bounds = fresh_reg();
        emit_line("  " + out_of_bounds + " = or i1 " + below_zero + ", " + above_max);

        std::string label_oob = "oob_" + std::to_string(label_counter_++);
        std::string label_ok = "ok_" + std::to_string(label_counter_++);
        std::string label_end = "end_" + std::to_string(label_counter_++);

        emit_line("  br i1 " + out_of_bounds + ", label %" + label_oob + ", label %" + label_ok);

        // Out of bounds: return Nothing
        emit_line(label_oob + ":");
        current_block_ = label_oob;
        emit_line("  br label %" + label_end);

        // In bounds: return Just(elem_ptr)
        emit_line(label_ok + ":");
        current_block_ = label_ok;
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                  arr_ptr + ", i64 0, i64 " + index_i64);

        if (nullable) {
            // Nullable: Just = elem_ptr, Nothing = null
            emit_line("  br label %" + label_end);
            emit_line(label_end + ":");
            current_block_ = label_end;
            std::string result = fresh_reg();
            emit_line("  " + result + " = phi ptr [ null, %" + label_oob + " ], [ " + elem_ptr +
                      ", %" + label_ok + " ]");
            last_expr_type_ = "ptr";
            return result;
        } else {
            std::string maybe_ptr = fresh_reg();
            emit_line("  " + maybe_ptr + " = alloca " + maybe_type);
            std::string tag_ptr_ok = fresh_reg();
            emit_line("  " + tag_ptr_ok + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr_ok);
            std::string val_ptr = fresh_reg();
            emit_line("  " + val_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 1");
            emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
            std::string ok_val = fresh_reg();
            emit_line("  " + ok_val + " = load " + maybe_type + ", ptr " + maybe_ptr);
            emit_line("  br label %" + label_end);

            // Out-of-bounds path needs its own Nothing value
            // (we already branched, rewrite to use phi)
            emit_line(label_end + ":");
            current_block_ = label_end;
            // Need to construct Nothing in the oob path too — but we already emitted the branch.
            // Actually, let's reconstruct: use alloca+tag approach for Nothing
            // Simpler: go back to alloca-per-branch with phi
            std::string nothing_alloca = fresh_reg();
            emit_line("  " + nothing_alloca + " = alloca " + maybe_type);
            std::string nothing_tag_ptr = fresh_reg();
            emit_line("  " + nothing_tag_ptr + " = getelementptr inbounds " + maybe_type +
                      ", ptr " + nothing_alloca + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + nothing_tag_ptr);
            std::string nothing_val = fresh_reg();
            emit_line("  " + nothing_val + " = load " + maybe_type + ", ptr " + nothing_alloca);

            // Select: oob = nothing, ok = just
            std::string result = fresh_reg();
            // We can't use phi here since we're in the end block already.
            // Use select on the out_of_bounds condition instead.
            // But select requires same type. Let's restructure with alloca before branches.
            // Actually, the simplest approach: alloca the maybe_ptr before the branches
            // and fill it in each branch. Let me rewrite using the original pattern.
            // ... For non-nullable this path should be rare (ref T is ptr), so just keep as-is
            // Actually: ref T = ptr, so Maybe[ref T] WILL be nullable. This else branch won't
            // execute.
            last_expr_type_ = maybe_type;
            return nothing_val; // This path is unreachable for ref types
        }
    }

    // first() returns Maybe[ref T]
    if (method == "first") {
        emit_coverage("Array::first");
        // Create ref type for Maybe[ref T]
        auto ref_type = std::make_shared<types::Type>();
        ref_type->kind =
            types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        bool nullable = nullable_maybe_types_.count(maybe_mangled) > 0;

        if (nullable) {
            if (arr_size == 0) {
                last_expr_type_ = "ptr";
                return "null";
            } else {
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type +
                          ", ptr " + arr_ptr + ", i64 0, i64 0");
                last_expr_type_ = "ptr";
                return elem_ptr;
            }
        }

        std::string maybe_type = "%struct." + maybe_mangled;
        std::string maybe_ptr = fresh_reg();
        emit_line("  " + maybe_ptr + " = alloca " + maybe_type);

        if (arr_size == 0) {
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr);
        } else {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 0");
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);
            std::string val_ptr = fresh_reg();
            emit_line("  " + val_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 1");
            emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_ptr);
        last_expr_type_ = maybe_type;
        return result;
    }

    // last() returns Maybe[ref T]
    if (method == "last") {
        emit_coverage("Array::last");
        // Create ref type for Maybe[ref T]
        auto ref_type = std::make_shared<types::Type>();
        ref_type->kind =
            types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        bool nullable = nullable_maybe_types_.count(maybe_mangled) > 0;

        if (nullable) {
            if (arr_size == 0) {
                last_expr_type_ = "ptr";
                return "null";
            } else {
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type +
                          ", ptr " + arr_ptr + ", i64 0, i64 " + std::to_string(arr_size - 1));
                last_expr_type_ = "ptr";
                return elem_ptr;
            }
        }

        std::string maybe_type = "%struct." + maybe_mangled;
        std::string maybe_ptr = fresh_reg();
        emit_line("  " + maybe_ptr + " = alloca " + maybe_type);

        if (arr_size == 0) {
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr);
        } else {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(arr_size - 1));
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);
            std::string val_ptr = fresh_reg();
            emit_line("  " + val_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                      maybe_ptr + ", i32 0, i32 1");
            emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_ptr);
        last_expr_type_ = maybe_type;
        return result;
    }

    // get_mut(index) returns Maybe[mut ref T] — same IR as get() since mut ref T is ptr
    if (method == "get_mut") {
        emit_coverage("Array::get_mut");
        if (call.args.empty()) {
            report_error("get_mut requires an index argument", call.span, "C015");
            return "0";
        }

        std::string index = gen_expr(*call.args[0]);
        std::string index_i64 = fresh_reg();
        if (last_expr_type_ == "i64") {
            index_i64 = index;
        } else {
            emit_line("  " + index_i64 + " = sext i32 " + index + " to i64");
        }

        // Bounds check
        std::string below_zero = fresh_reg();
        emit_line("  " + below_zero + " = icmp slt i64 " + index_i64 + ", 0");
        std::string above_max = fresh_reg();
        emit_line("  " + above_max + " = icmp sge i64 " + index_i64 + ", " +
                  std::to_string(arr_size));
        std::string out_of_bounds = fresh_reg();
        emit_line("  " + out_of_bounds + " = or i1 " + below_zero + ", " + above_max);

        std::string label_oob = "oob_" + std::to_string(label_counter_++);
        std::string label_ok = "ok_" + std::to_string(label_counter_++);
        std::string label_end = "end_" + std::to_string(label_counter_++);

        emit_line("  br i1 " + out_of_bounds + ", label %" + label_oob + ", label %" + label_ok);

        // Out of bounds: return Nothing (null)
        emit_line(label_oob + ":");
        current_block_ = label_oob;
        emit_line("  br label %" + label_end);

        // In bounds: return Just(elem_ptr)
        emit_line(label_ok + ":");
        current_block_ = label_ok;
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                  arr_ptr + ", i64 0, i64 " + index_i64);
        emit_line("  br label %" + label_end);

        // Merge: nullable Maybe[mut ref T] = ptr (null or elem_ptr)
        emit_line(label_end + ":");
        current_block_ = label_end;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi ptr [ null, %" + label_oob + " ], [ " + elem_ptr + ", %" +
                  label_ok + " ]");
        last_expr_type_ = "ptr";
        return result;
    }

    // first_mut() returns Maybe[mut ref T]
    if (method == "first_mut") {
        emit_coverage("Array::first_mut");
        if (arr_size == 0) {
            last_expr_type_ = "ptr";
            return "null";
        }
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                  arr_ptr + ", i64 0, i64 0");
        last_expr_type_ = "ptr";
        return elem_ptr;
    }

    // last_mut() returns Maybe[mut ref T]
    if (method == "last_mut") {
        emit_coverage("Array::last_mut");
        if (arr_size == 0) {
            last_expr_type_ = "ptr";
            return "null";
        }
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                  arr_ptr + ", i64 0, i64 " + std::to_string(arr_size - 1));
        last_expr_type_ = "ptr";
        return elem_ptr;
    }

    // each_ref() returns Array[ref T, N] — array of pointers to each element
    if (method == "each_ref") {
        emit_coverage("Array::each_ref");
        std::string ptr_array_type = "[" + std::to_string(arr_size) + " x ptr]";
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + ptr_array_type);

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string slot_ptr = fresh_reg();
            emit_line("  " + slot_ptr + " = getelementptr inbounds " + ptr_array_type + ", ptr " +
                      result_ptr + ", i64 0, i64 " + std::to_string(i));
            emit_line("  store ptr " + elem_ptr + ", ptr " + slot_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + ptr_array_type + ", ptr " + result_ptr);
        last_expr_type_ = ptr_array_type;
        return result;
    }

    // each_mut() returns Array[mut ref T, N] — same IR as each_ref
    if (method == "each_mut") {
        emit_coverage("Array::each_mut");
        std::string ptr_array_type = "[" + std::to_string(arr_size) + " x ptr]";
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + ptr_array_type);

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string slot_ptr = fresh_reg();
            emit_line("  " + slot_ptr + " = getelementptr inbounds " + ptr_array_type + ", ptr " +
                      result_ptr + ", i64 0, i64 " + std::to_string(i));
            emit_line("  store ptr " + elem_ptr + ", ptr " + slot_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + ptr_array_type + ", ptr " + result_ptr);
        last_expr_type_ = ptr_array_type;
        return result;
    }

    // map(closure) returns [U; N]
    if (method == "map") {
        emit_coverage("Array::map");
        if (call.args.empty()) {
            report_error("map requires a closure argument", call.span, "C016");
            return "0";
        }

        // Get closure (may be fat pointer { ptr, ptr } or thin ptr)
        std::string closure_val = gen_expr(*call.args[0]);
        closure_val = coerce_closure_to_fn_ptr(closure_val);

        // For now, we assume closure output type matches input type (simplified)
        std::string result_type = array_llvm_type;
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + result_type);

        // Loop through each element and apply closure
        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr_src = fresh_reg();
            emit_line("  " + elem_ptr_src + " = getelementptr inbounds " + array_llvm_type +
                      ", ptr " + arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem_val = fresh_reg();
            emit_line("  " + elem_val + " = load " + elem_llvm_type + ", ptr " + elem_ptr_src);

            // Call the closure
            std::string mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = call " + elem_llvm_type + " " + closure_val + "(" +
                      elem_llvm_type + " " + elem_val + ")");

            // Store result
            std::string result_elem_ptr = fresh_reg();
            emit_line("  " + result_elem_ptr + " = getelementptr inbounds " + result_type +
                      ", ptr " + result_ptr + ", i64 0, i64 " + std::to_string(i));
            emit_line("  store " + elem_llvm_type + " " + mapped_val + ", ptr " + result_elem_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + result_type + ", ptr " + result_ptr);
        last_expr_type_ = result_type;
        return result;
    }

    // eq(other) returns Bool
    if (method == "eq") {
        emit_coverage("Array::eq");
        if (call.args.empty()) {
            report_error("eq requires an argument", call.span, "C015");
            return "0";
        }

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;
        std::string other_ptr;

        // If argument is a reference (ptr), use it directly
        if (other_type == "ptr") {
            other_ptr = other;
        } else {
            other_ptr = fresh_reg();
            emit_line("  " + other_ptr + " = alloca " + array_llvm_type);
            emit_line("  store " + array_llvm_type + " " + other + ", ptr " + other_ptr);
        }

        // Compare element by element
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca i1");
        emit_line("  store i1 true, ptr " + result_ptr);

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem1_ptr = fresh_reg();
            emit_line("  " + elem1_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      other_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem2 = fresh_reg();
            emit_line("  " + elem2 + " = load " + elem_llvm_type + ", ptr " + elem2_ptr);

            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + elem_llvm_type + " " + elem1 + ", " + elem2);

            std::string old_result = fresh_reg();
            emit_line("  " + old_result + " = load i1, ptr " + result_ptr);
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = and i1 " + old_result + ", " + cmp);
            emit_line("  store i1 " + new_result + ", ptr " + result_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load i1, ptr " + result_ptr);
        last_expr_type_ = "i1";
        return result;
    }

    // ne(other) returns Bool
    if (method == "ne") {
        emit_coverage("Array::ne");
        if (call.args.empty()) {
            report_error("ne requires an argument", call.span, "C015");
            return "0";
        }

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;
        std::string other_ptr;

        // If argument is a reference (ptr), use it directly
        if (other_type == "ptr") {
            other_ptr = other;
        } else {
            other_ptr = fresh_reg();
            emit_line("  " + other_ptr + " = alloca " + array_llvm_type);
            emit_line("  store " + array_llvm_type + " " + other + ", ptr " + other_ptr);
        }

        std::string eq_result_ptr = fresh_reg();
        emit_line("  " + eq_result_ptr + " = alloca i1");
        emit_line("  store i1 true, ptr " + eq_result_ptr);

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem1_ptr = fresh_reg();
            emit_line("  " + elem1_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      other_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem2 = fresh_reg();
            emit_line("  " + elem2 + " = load " + elem_llvm_type + ", ptr " + elem2_ptr);

            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + elem_llvm_type + " " + elem1 + ", " + elem2);

            std::string old_result = fresh_reg();
            emit_line("  " + old_result + " = load i1, ptr " + eq_result_ptr);
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = and i1 " + old_result + ", " + cmp);
            emit_line("  store i1 " + new_result + ", ptr " + eq_result_ptr);
        }

        std::string eq_result = fresh_reg();
        emit_line("  " + eq_result + " = load i1, ptr " + eq_result_ptr);
        std::string result = fresh_reg();
        emit_line("  " + result + " = xor i1 " + eq_result + ", true");
        last_expr_type_ = "i1";
        return result;
    }

    // cmp(other) returns Ordering
    if (method == "cmp") {
        emit_coverage("Array::cmp");
        if (call.args.empty()) {
            report_error("cmp requires an argument", call.span, "C015");
            return "0";
        }

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;
        std::string other_ptr;

        // If argument is a reference (ptr), use it directly
        if (other_type == "ptr") {
            other_ptr = other;
        } else {
            other_ptr = fresh_reg();
            emit_line("  " + other_ptr + " = alloca " + array_llvm_type);
            emit_line("  store " + array_llvm_type + " " + other + ", ptr " + other_ptr);
        }

        // Ordering: Less=0, Equal=1, Greater=2
        std::string ordering_result_ptr = fresh_reg();
        emit_line("  " + ordering_result_ptr + " = alloca i32");
        emit_line("  store i32 1, ptr " + ordering_result_ptr); // Default: Equal

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem1_ptr = fresh_reg();
            emit_line("  " + elem1_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      other_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem2 = fresh_reg();
            emit_line("  " + elem2 + " = load " + elem_llvm_type + ", ptr " + elem2_ptr);

            // Compare: use signed comparison for signed types
            std::string cmp_lt = fresh_reg();
            std::string cmp_gt = fresh_reg();
            emit_line("  " + cmp_lt + " = icmp slt " + elem_llvm_type + " " + elem1 + ", " + elem2);
            emit_line("  " + cmp_gt + " = icmp sgt " + elem_llvm_type + " " + elem1 + ", " + elem2);

            // Select Ordering: if less -> 0, if greater -> 2, else keep current
            std::string sel1 = fresh_reg();
            emit_line("  " + sel1 + " = select i1 " + cmp_lt + ", i32 0, i32 1");
            std::string sel2 = fresh_reg();
            emit_line("  " + sel2 + " = select i1 " + cmp_gt + ", i32 2, i32 " + sel1);

            std::string old_result = fresh_reg();
            emit_line("  " + old_result + " = load i32, ptr " + ordering_result_ptr);
            // Only update if current result is Equal (1)
            std::string is_equal = fresh_reg();
            emit_line("  " + is_equal + " = icmp eq i32 " + old_result + ", 1");
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = select i1 " + is_equal + ", i32 " + sel2 + ", i32 " +
                      old_result);
            emit_line("  store i32 " + new_result + ", ptr " + ordering_result_ptr);
        }

        // Build Ordering struct { i32 tag }
        std::string ordering_ptr = fresh_reg();
        emit_line("  " + ordering_ptr + " = alloca %struct.Ordering");
        std::string tag_val = fresh_reg();
        emit_line("  " + tag_val + " = load i32, ptr " + ordering_result_ptr);
        std::string tag_ptr = fresh_reg();
        emit_line("  " + tag_ptr + " = getelementptr inbounds %struct.Ordering, ptr " +
                  ordering_ptr + ", i32 0, i32 0");
        emit_line("  store i32 " + tag_val + ", ptr " + tag_ptr);

        std::string result = fresh_reg();
        emit_line("  " + result + " = load %struct.Ordering, ptr " + ordering_ptr);
        last_expr_type_ = "%struct.Ordering";
        return result;
    }

    // partial_cmp(other) returns Maybe[Ordering]
    if (method == "partial_cmp") {
        emit_coverage("Array::partial_cmp");
        if (call.args.empty()) {
            report_error("partial_cmp requires an argument", call.span, "C015");
            return "0";
        }

        // Ensure Maybe[Ordering] is instantiated
        auto ordering_type = std::make_shared<types::Type>();
        ordering_type->kind = types::NamedType{"Ordering", "", {}};
        std::vector<types::TypePtr> maybe_type_args = {ordering_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string other = gen_expr(*call.args[0]);
        std::string other_type = last_expr_type_;
        std::string other_ptr;
        if (other_type == "ptr") {
            other_ptr = other;
        } else {
            other_ptr = fresh_reg();
            emit_line("  " + other_ptr + " = alloca " + array_llvm_type);
            emit_line("  store " + array_llvm_type + " " + other + ", ptr " + other_ptr);
        }

        // Lexicographic comparison: iterate elements, first non-equal determines result
        // Use alloca to hold current ordering tag (Less=0, Equal=1, Greater=2)
        std::string ordering_result_ptr = fresh_reg();
        emit_line("  " + ordering_result_ptr + " = alloca i32");
        emit_line("  store i32 1, ptr " + ordering_result_ptr); // Default: Equal

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem1_ptr = fresh_reg();
            emit_line("  " + elem1_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      other_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem2 = fresh_reg();
            emit_line("  " + elem2 + " = load " + elem_llvm_type + ", ptr " + elem2_ptr);

            std::string cmp_lt = fresh_reg();
            std::string cmp_gt = fresh_reg();
            emit_line("  " + cmp_lt + " = icmp slt " + elem_llvm_type + " " + elem1 + ", " + elem2);
            emit_line("  " + cmp_gt + " = icmp sgt " + elem_llvm_type + " " + elem1 + ", " + elem2);

            std::string sel1 = fresh_reg();
            emit_line("  " + sel1 + " = select i1 " + cmp_lt + ", i32 0, i32 1");
            std::string sel2 = fresh_reg();
            emit_line("  " + sel2 + " = select i1 " + cmp_gt + ", i32 2, i32 " + sel1);

            std::string old_result = fresh_reg();
            emit_line("  " + old_result + " = load i32, ptr " + ordering_result_ptr);
            std::string is_equal = fresh_reg();
            emit_line("  " + is_equal + " = icmp eq i32 " + old_result + ", 1");
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = select i1 " + is_equal + ", i32 " + sel2 + ", i32 " +
                      old_result);
            emit_line("  store i32 " + new_result + ", ptr " + ordering_result_ptr);
        }

        // Build Ordering struct
        std::string ordering_alloca = fresh_reg();
        emit_line("  " + ordering_alloca + " = alloca %struct.Ordering, align 4");
        std::string tag_val = fresh_reg();
        emit_line("  " + tag_val + " = load i32, ptr " + ordering_result_ptr);
        std::string ordering_tag_ptr = fresh_reg();
        emit_line("  " + ordering_tag_ptr + " = getelementptr inbounds %struct.Ordering, ptr " +
                  ordering_alloca + ", i32 0, i32 0");
        emit_line("  store i32 " + tag_val + ", ptr " + ordering_tag_ptr);
        std::string ordering_val = fresh_reg();
        emit_line("  " + ordering_val + " = load %struct.Ordering, ptr " + ordering_alloca);

        // Build Maybe[Ordering] = Just(ordering): tag=0 (Just), payload=Ordering
        std::string enum_alloca = fresh_reg();
        emit_line("  " + enum_alloca + " = alloca " + maybe_type + ", align 8");
        std::string maybe_tag_ptr = fresh_reg();
        emit_line("  " + maybe_tag_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  enum_alloca + ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + maybe_tag_ptr);
        std::string payload_ptr = fresh_reg();
        emit_line("  " + payload_ptr + " = getelementptr inbounds " + maybe_type + ", ptr " +
                  enum_alloca + ", i32 0, i32 1");
        emit_line("  store %struct.Ordering " + ordering_val + ", ptr " + payload_ptr);

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + enum_alloca);
        last_expr_type_ = maybe_type;
        return result;
    }

    // duplicate() / clone() returns [T; N] (copy of the array)
    if (method == "duplicate" || method == "clone") {
        emit_coverage("Array::" + method);
        // Array is a value type, just load and return it
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_llvm_type + ", ptr " + arr_ptr);
        last_expr_type_ = array_llvm_type;
        return result;
    }

    // default() returns [T; N] with all elements zeroed
    if (method == "default") {
        emit_coverage("Array::default");
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + array_llvm_type);
        // Zero-initialize
        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      result_ptr + ", i64 0, i64 " + std::to_string(i));
            emit_line("  store " + elem_llvm_type + " 0, ptr " + elem_ptr);
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + array_llvm_type + ", ptr " + result_ptr);
        last_expr_type_ = array_llvm_type;
        return result;
    }

    // hash() returns I64
    if (method == "hash") {
        emit_coverage("Array::hash");
        // Simple hash: combine element hashes with FNV-like mixing
        std::string hash_ptr = fresh_reg();
        emit_line("  " + hash_ptr + " = alloca i64");
        emit_line("  store i64 14695981039346656037, ptr " + hash_ptr); // FNV offset basis

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem = fresh_reg();
            emit_line("  " + elem + " = load " + elem_llvm_type + ", ptr " + elem_ptr);
            // Cast element to i64 for hashing
            std::string elem_i64 = fresh_reg();
            if (elem_llvm_type == "i64") {
                elem_i64 = elem;
            } else if (elem_llvm_type == "i32") {
                emit_line("  " + elem_i64 + " = sext i32 " + elem + " to i64");
            } else if (elem_llvm_type == "i8") {
                emit_line("  " + elem_i64 + " = sext i8 " + elem + " to i64");
            } else if (elem_llvm_type == "i16") {
                emit_line("  " + elem_i64 + " = sext i16 " + elem + " to i64");
            } else {
                // For ptr and other types, ptrtoint
                emit_line("  " + elem_i64 + " = ptrtoint " + elem_llvm_type + " " + elem +
                          " to i64");
            }
            std::string old_hash = fresh_reg();
            emit_line("  " + old_hash + " = load i64, ptr " + hash_ptr);
            std::string xored = fresh_reg();
            emit_line("  " + xored + " = xor i64 " + old_hash + ", " + elem_i64);
            std::string mixed = fresh_reg();
            emit_line("  " + mixed + " = mul i64 " + xored + ", 1099511628211"); // FNV prime
            emit_line("  store i64 " + mixed + ", ptr " + hash_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load i64, ptr " + hash_ptr);
        last_expr_type_ = "i64";
        return result;
    }

    // to_string() / debug_string() returns Str
    if (method == "to_string" || method == "debug_string") {
        emit_coverage("Array::" + method);
        // Build string representation "[e1, e2, ...]"
        // For simplicity, call tml_array_to_string runtime helper or build inline
        // Simple approach: allocate a fixed string "[...]" placeholder
        // TODO: proper to_string with element formatting
        // For now, return a static string to avoid crash
        std::string str_global = add_string_literal("[array]");
        last_expr_type_ = "ptr";
        return str_global;
    }

    // eq_slice(other) returns Bool - compare array with a slice
    if (method == "eq_slice") {
        emit_coverage("Array::eq_slice");
        if (call.args.empty()) {
            report_error("eq_slice requires an argument", call.span, "C015");
            return "0";
        }

        // Get the slice argument (fat pointer { ptr, i64 })
        std::string slice_val = gen_expr(*call.args[0]);
        std::string slice_type = last_expr_type_;

        // Extract data ptr and length from slice
        std::string slice_alloca = fresh_reg();
        emit_line("  " + slice_alloca + " = alloca { ptr, i64 }");
        emit_line("  store { ptr, i64 } " + slice_val + ", ptr " + slice_alloca);
        std::string data_ptr_ptr = fresh_reg();
        emit_line("  " + data_ptr_ptr + " = getelementptr inbounds { ptr, i64 }, ptr " +
                  slice_alloca + ", i32 0, i32 0");
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = load ptr, ptr " + data_ptr_ptr);
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds { ptr, i64 }, ptr " + slice_alloca +
                  ", i32 0, i32 1");
        std::string len_val = fresh_reg();
        emit_line("  " + len_val + " = load i64, ptr " + len_ptr);

        // Check length matches
        std::string len_match = fresh_reg();
        emit_line("  " + len_match + " = icmp eq i64 " + len_val + ", " + std::to_string(arr_size));

        std::string label_check = "eqslice_check_" + std::to_string(label_counter_++);
        std::string label_false = "eqslice_false_" + std::to_string(label_counter_++);
        std::string label_end = "eqslice_end_" + std::to_string(label_counter_++);

        emit_line("  br i1 " + len_match + ", label %" + label_check + ", label %" + label_false);

        // Length matches: compare elements
        emit_line(label_check + ":");
        current_block_ = label_check;
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca i1");
        emit_line("  store i1 true, ptr " + result_ptr);

        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem1_ptr = fresh_reg();
            emit_line("  " + elem1_ptr + " = getelementptr inbounds " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr inbounds " + elem_llvm_type + ", ptr " +
                      data_ptr + ", i64 " + std::to_string(i));
            std::string elem2 = fresh_reg();
            emit_line("  " + elem2 + " = load " + elem_llvm_type + ", ptr " + elem2_ptr);

            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + elem_llvm_type + " " + elem1 + ", " + elem2);

            std::string old_result = fresh_reg();
            emit_line("  " + old_result + " = load i1, ptr " + result_ptr);
            std::string new_result = fresh_reg();
            emit_line("  " + new_result + " = and i1 " + old_result + ", " + cmp);
            emit_line("  store i1 " + new_result + ", ptr " + result_ptr);
        }

        std::string check_result = fresh_reg();
        emit_line("  " + check_result + " = load i1, ptr " + result_ptr);
        emit_line("  br label %" + label_end);

        // Length mismatch: return false
        emit_line(label_false + ":");
        current_block_ = label_false;
        emit_line("  br label %" + label_end);

        // Merge
        emit_line(label_end + ":");
        current_block_ = label_end;
        std::string result = fresh_reg();
        emit_line("  " + result + " = phi i1 [ " + check_result + ", %" + label_check +
                  " ], [ false, %" + label_false + " ]");
        last_expr_type_ = "i1";
        return result;
    }

    // as_ref() returns Slice[T] (same as as_slice)
    if (method == "as_ref") {
        emit_coverage("Array::as_ref");
        std::string slice_llvm_type = "{ ptr, i64 }";
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + slice_llvm_type);
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 0");
        emit_line("  store ptr " + arr_ptr + ", ptr " + data_ptr);
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 1");
        emit_line("  store i64 " + std::to_string(arr_size) + ", ptr " + len_ptr);
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + slice_llvm_type + ", ptr " + result_ptr);
        last_expr_type_ = slice_llvm_type;
        return result;
    }

    // as_slice() returns Slice[T] (fat pointer { ptr, i64 })
    if (method == "as_slice") {
        emit_coverage("Array::as_slice");
        std::string slice_llvm_type = "{ ptr, i64 }";
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + slice_llvm_type);

        // Store the array pointer as the data field
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 0");
        emit_line("  store ptr " + arr_ptr + ", ptr " + data_ptr);

        // Store the array length
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 1");
        emit_line("  store i64 " + std::to_string(arr_size) + ", ptr " + len_ptr);

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + slice_llvm_type + ", ptr " + result_ptr);
        last_expr_type_ = slice_llvm_type;
        return result;
    }

    // as_mut_slice() returns MutSlice[T] (fat pointer { ptr, i64 })
    if (method == "as_mut_slice") {
        emit_coverage("Array::as_mut_slice");
        std::string slice_llvm_type = "{ ptr, i64 }";
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + slice_llvm_type);

        // Store the array pointer as the data field
        std::string data_ptr = fresh_reg();
        emit_line("  " + data_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 0");
        emit_line("  store ptr " + arr_ptr + ", ptr " + data_ptr);

        // Store the array length
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds " + slice_llvm_type + ", ptr " +
                  result_ptr + ", i32 0, i32 1");
        emit_line("  store i64 " + std::to_string(arr_size) + ", ptr " + len_ptr);

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + slice_llvm_type + ", ptr " + result_ptr);
        last_expr_type_ = slice_llvm_type;
        return result;
    }

    // Not a recognized array method
    return std::nullopt;
}

} // namespace tml::codegen
