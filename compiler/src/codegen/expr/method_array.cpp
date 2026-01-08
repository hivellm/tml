//! # LLVM IR Generator - Array Methods
//!
//! This file implements methods for fixed-size array types `[T; N]`.
//!
//! ## Methods
//!
//! | Method     | Signature            | Description              |
//! |------------|----------------------|--------------------------|
//! | `len`      | `() -> I64`          | Returns N (compile-time) |
//! | `is_empty` | `() -> Bool`         | Returns N == 0           |
//! | `get`      | `(I64) -> T`         | Element at index         |
//! | `first`    | `() -> T`            | First element            |
//! | `last`     | `() -> T`            | Last element             |
//! | `eq`, `ne` | `([T; N]) -> Bool`   | Element-wise comparison  |
//! | `cmp`      | `([T; N]) -> Ordering`| Lexicographic compare   |

#include "codegen/llvm_ir_gen.hpp"
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
        last_expr_type_ = "i64";
        return std::to_string(arr_size);
    }

    // is_empty() returns true if size is 0
    if (method == "is_empty" || method == "isEmpty") {
        last_expr_type_ = "i1";
        return arr_size == 0 ? "true" : "false";
    }

    // get(index) returns Maybe[ref T]
    if (method == "get") {
        if (call.args.empty()) {
            report_error("get requires an index argument", call.span);
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
        ref_type->kind = types::RefType{false, elem_type};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        // Bounds check
        std::string below_zero = fresh_reg();
        emit_line("  " + below_zero + " = icmp slt i64 " + index_i64 + ", 0");
        std::string above_max = fresh_reg();
        emit_line("  " + above_max + " = icmp sge i64 " + index_i64 + ", " +
                  std::to_string(arr_size));
        std::string out_of_bounds = fresh_reg();
        emit_line("  " + out_of_bounds + " = or i1 " + below_zero + ", " + above_max);

        // Create Maybe struct
        std::string maybe_ptr = fresh_reg();
        emit_line("  " + maybe_ptr + " = alloca " + maybe_type);

        std::string label_oob = "oob_" + std::to_string(label_counter_++);
        std::string label_ok = "ok_" + std::to_string(label_counter_++);
        std::string label_end = "end_" + std::to_string(label_counter_++);

        emit_line("  br i1 " + out_of_bounds + ", label %" + label_oob + ", label %" + label_ok);

        // Out of bounds: return Nothing (tag = 1, second variant)
        emit_line(label_oob + ":");
        std::string tag_ptr_oob = fresh_reg();
        emit_line("  " + tag_ptr_oob + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                  ", i32 0, i32 0");
        emit_line("  store i32 1, ptr " + tag_ptr_oob);
        emit_line("  br label %" + label_end);

        // In bounds: return Just(ptr) (tag = 0, first variant)
        emit_line(label_ok + ":");
        std::string elem_ptr = fresh_reg();
        emit_line("  " + elem_ptr + " = getelementptr " + array_llvm_type + ", ptr " + arr_ptr +
                  ", i64 0, i64 " + index_i64);
        std::string tag_ptr_ok = fresh_reg();
        emit_line("  " + tag_ptr_ok + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                  ", i32 0, i32 0");
        emit_line("  store i32 0, ptr " + tag_ptr_ok);
        std::string val_ptr = fresh_reg();
        emit_line("  " + val_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                  ", i32 0, i32 1");
        emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
        emit_line("  br label %" + label_end);

        emit_line(label_end + ":");
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_ptr);

        last_expr_type_ = maybe_type;
        return result;
    }

    // first() returns Maybe[ref T]
    if (method == "first") {
        // Create ref type for Maybe[ref T]
        auto ref_type = std::make_shared<types::Type>();
        ref_type->kind = types::RefType{false, elem_type};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string maybe_ptr = fresh_reg();
        emit_line("  " + maybe_ptr + " = alloca " + maybe_type);

        if (arr_size == 0) {
            // Empty array: return Nothing (tag = 1, second variant)
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr);
        } else {
            // Non-empty: return Just(ptr to first element) (tag = 0, first variant)
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_llvm_type + ", ptr " + arr_ptr +
                      ", i64 0, i64 0");
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);
            std::string val_ptr = fresh_reg();
            emit_line("  " + val_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 1");
            emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_ptr);
        last_expr_type_ = maybe_type;
        return result;
    }

    // last() returns Maybe[ref T]
    if (method == "last") {
        // Create ref type for Maybe[ref T]
        auto ref_type = std::make_shared<types::Type>();
        ref_type->kind = types::RefType{false, elem_type};
        std::vector<types::TypePtr> maybe_type_args = {ref_type};
        std::string maybe_mangled = require_enum_instantiation("Maybe", maybe_type_args);
        std::string maybe_type = "%struct." + maybe_mangled;

        std::string maybe_ptr = fresh_reg();
        emit_line("  " + maybe_ptr + " = alloca " + maybe_type);

        if (arr_size == 0) {
            // Empty array: return Nothing (tag = 1, second variant)
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 0");
            emit_line("  store i32 1, ptr " + tag_ptr);
        } else {
            // Non-empty: return Just(ptr to last element) (tag = 0, first variant)
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr " + array_llvm_type + ", ptr " + arr_ptr +
                      ", i64 0, i64 " + std::to_string(arr_size - 1));
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);
            std::string val_ptr = fresh_reg();
            emit_line("  " + val_ptr + " = getelementptr " + maybe_type + ", ptr " + maybe_ptr +
                      ", i32 0, i32 1");
            emit_line("  store ptr " + elem_ptr + ", ptr " + val_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + maybe_type + ", ptr " + maybe_ptr);
        last_expr_type_ = maybe_type;
        return result;
    }

    // map(closure) returns [U; N]
    if (method == "map") {
        if (call.args.empty()) {
            report_error("map requires a closure argument", call.span);
            return "0";
        }

        // Get closure
        std::string closure_val = gen_expr(*call.args[0]);

        // For now, we assume closure output type matches input type (simplified)
        std::string result_type = array_llvm_type;
        std::string result_ptr = fresh_reg();
        emit_line("  " + result_ptr + " = alloca " + result_type);

        // Loop through each element and apply closure
        for (size_t i = 0; i < arr_size; ++i) {
            std::string elem_ptr_src = fresh_reg();
            emit_line("  " + elem_ptr_src + " = getelementptr " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem_val = fresh_reg();
            emit_line("  " + elem_val + " = load " + elem_llvm_type + ", ptr " + elem_ptr_src);

            // Call the closure
            std::string mapped_val = fresh_reg();
            emit_line("  " + mapped_val + " = call " + elem_llvm_type + " " + closure_val + "(" +
                      elem_llvm_type + " " + elem_val + ")");

            // Store result
            std::string result_elem_ptr = fresh_reg();
            emit_line("  " + result_elem_ptr + " = getelementptr " + result_type + ", ptr " +
                      result_ptr + ", i64 0, i64 " + std::to_string(i));
            emit_line("  store " + elem_llvm_type + " " + mapped_val + ", ptr " + result_elem_ptr);
        }

        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + result_type + ", ptr " + result_ptr);
        last_expr_type_ = result_type;
        return result;
    }

    // eq(other) returns Bool
    if (method == "eq") {
        if (call.args.empty()) {
            report_error("eq requires an argument", call.span);
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
            emit_line("  " + elem1_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
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
        if (call.args.empty()) {
            report_error("ne requires an argument", call.span);
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
            emit_line("  " + elem1_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
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
        if (call.args.empty()) {
            report_error("cmp requires an argument", call.span);
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
            emit_line("  " + elem1_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + std::to_string(i));
            std::string elem1 = fresh_reg();
            emit_line("  " + elem1 + " = load " + elem_llvm_type + ", ptr " + elem1_ptr);

            std::string elem2_ptr = fresh_reg();
            emit_line("  " + elem2_ptr + " = getelementptr " + array_llvm_type + ", ptr " +
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
        emit_line("  " + tag_ptr + " = getelementptr %struct.Ordering, ptr " + ordering_ptr +
                  ", i32 0, i32 0");
        emit_line("  store i32 " + tag_val + ", ptr " + tag_ptr);

        std::string result = fresh_reg();
        emit_line("  " + result + " = load %struct.Ordering, ptr " + ordering_ptr);
        last_expr_type_ = "%struct.Ordering";
        return result;
    }

    // Not a recognized array method
    return std::nullopt;
}

} // namespace tml::codegen
