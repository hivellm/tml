// LLVM IR generator - Intrinsic functions
// Handles: @intrinsic decorated functions from core::intrinsics
// These are compiler-implemented primitives that generate inline LLVM IR

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_intrinsic(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Check if this function is marked as @intrinsic
    auto func_sig = env_.lookup_func(fn_name);
    if (!func_sig.has_value() || !func_sig->is_intrinsic) {
        return std::nullopt;
    }

    // ============================================================================
    // Arithmetic Intrinsics
    // ============================================================================

    // llvm_add[T](a: T, b: T) -> T
    if (fn_name == "llvm_add") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fadd " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = add " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_sub[T](a: T, b: T) -> T
    if (fn_name == "llvm_sub") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fsub " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = sub " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_mul[T](a: T, b: T) -> T
    if (fn_name == "llvm_mul") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fmul " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = mul " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_div[T](a: T, b: T) -> T
    if (fn_name == "llvm_div") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fdiv " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed division for now
                emit_line("  " + result + " = sdiv " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_rem[T](a: T, b: T) -> T
    if (fn_name == "llvm_rem") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = frem " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed remainder
                emit_line("  " + result + " = srem " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_neg[T](a: T) -> T
    if (fn_name == "llvm_neg") {
        if (!call.args.empty()) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fneg " + a_type + " " + a);
            } else {
                emit_line("  " + result + " = sub " + a_type + " 0, " + a);
            }
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Comparison Intrinsics
    // ============================================================================

    // llvm_eq[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_eq") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp oeq " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp eq " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_ne[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_ne") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp one " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp ne " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_lt[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_lt") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp olt " + a_type + " " + a + ", " + b);
            } else {
                // Default to signed comparison
                emit_line("  " + result + " = icmp slt " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_le[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_le") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp ole " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sle " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_gt[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_gt") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp ogt " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sgt " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // llvm_ge[T](a: T, b: T) -> Bool
    if (fn_name == "llvm_ge") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();

            if (a_type == "float" || a_type == "double") {
                emit_line("  " + result + " = fcmp oge " + a_type + " " + a + ", " + b);
            } else {
                emit_line("  " + result + " = icmp sge " + a_type + " " + a + ", " + b);
            }
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Bitwise Intrinsics
    // ============================================================================

    // llvm_and[T](a: T, b: T) -> T
    if (fn_name == "llvm_and") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_or[T](a: T, b: T) -> T
    if (fn_name == "llvm_or") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_xor[T](a: T, b: T) -> T
    if (fn_name == "llvm_xor") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_not[T](a: T) -> T
    if (fn_name == "llvm_not") {
        if (!call.args.empty()) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", -1");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_shl[T](a: T, b: T) -> T
    if (fn_name == "llvm_shl") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // llvm_shr[T](a: T, b: T) -> T (arithmetic shift right for signed)
    if (fn_name == "llvm_shr") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            // Default to arithmetic shift (preserves sign)
            emit_line("  " + result + " = ashr " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Memory Intrinsics
    // ============================================================================

    // ptr_read[T](ptr: Ptr[T]) -> T
    if (fn_name == "ptr_read") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);

            // Infer element type from function signature
            std::string elem_type = "i32"; // Default
            if (!func_sig->type_params.empty() && !func_sig->params.empty()) {
                // Try to infer from argument type
                types::TypePtr arg_type = infer_expr_type(*call.args[0]);
                if (arg_type->is<types::PtrType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::PtrType>().inner);
                }
            }

            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + ptr);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // ptr_write[T](ptr: Ptr[T], val: T)
    if (fn_name == "ptr_write") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;
            emit_line("  store " + val_type + " " + val + ", ptr " + ptr);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ptr_offset[T](ptr: Ptr[T], count: I64) -> Ptr[T]
    if (fn_name == "ptr_offset") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8"; // Default
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::PtrType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::PtrType>().inner);
            }

            std::string count = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + ptr + ", i64 " +
                      count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // ============================================================================
    // Slice Intrinsics
    // ============================================================================

    // slice_get[T](data: ref T, index: I64) -> ref T
    // Returns a reference to element at index
    if (fn_name == "slice_get") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_; // ptr

            // Infer element type from the first argument's semantic type
            std::string elem_type = "i8"; // Default
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);

            // GEP to compute address: data + index * sizeof(T)
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      index);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_get_mut[T](data: mut ref T, index: I64) -> mut ref T
    // Same as slice_get but for mutable references
    if (fn_name == "slice_get_mut") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::MutRefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      index);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_set[T](data: mut ref T, index: I64, value: T)
    // Sets element at index to value
    if (fn_name == "slice_set") {
        if (call.args.size() >= 3) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::MutRefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);
            std::string value = gen_expr(*call.args[2]);
            std::string value_type = last_expr_type_;

            // Compute address and store
            std::string addr = fresh_reg();
            emit_line("  " + addr + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      index);
            emit_line("  store " + value_type + " " + value + ", ptr " + addr);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // slice_offset[T](data: ref T, count: I64) -> ref T
    // Returns pointer offset by count elements
    if (fn_name == "slice_offset") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            } else if (arg_type->is<types::MutRefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_swap[T](data: mut ref T, a: I64, b: I64)
    // Swaps elements at indices a and b
    if (fn_name == "slice_swap") {
        if (call.args.size() >= 3) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::MutRefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string idx_a = gen_expr(*call.args[1]);
            std::string idx_b = gen_expr(*call.args[2]);

            // Compute addresses
            std::string addr_a = fresh_reg();
            std::string addr_b = fresh_reg();
            emit_line("  " + addr_a + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      idx_a);
            emit_line("  " + addr_b + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      idx_b);

            // Load both values
            std::string val_a = fresh_reg();
            std::string val_b = fresh_reg();
            emit_line("  " + val_a + " = load " + elem_type + ", ptr " + addr_a);
            emit_line("  " + val_b + " = load " + elem_type + ", ptr " + addr_b);

            // Store swapped
            emit_line("  store " + elem_type + " " + val_b + ", ptr " + addr_a);
            emit_line("  store " + elem_type + " " + val_a + ", ptr " + addr_b);

            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ============================================================================
    // Array Intrinsics
    // ============================================================================

    // array_as_ptr[T](data: T) -> ref T
    // Returns a pointer to the first element of an array (for creating slices)
    if (fn_name == "array_as_ptr") {
        if (!call.args.empty()) {
            // The argument should be an array field (like this.data)
            // We just need to get its address
            std::string arr = gen_expr(*call.args[0]);
            // For arrays in locals, gen_expr returns the alloca pointer
            // which is already what we need
            last_expr_type_ = "ptr";
            return arr;
        }
        return "null";
    }

    // array_as_mut_ptr[T](data: T) -> mut ref T
    // Same as array_as_ptr but for mutable references
    if (fn_name == "array_as_mut_ptr") {
        if (!call.args.empty()) {
            std::string arr = gen_expr(*call.args[0]);
            last_expr_type_ = "ptr";
            return arr;
        }
        return "null";
    }

    // array_offset_ptr[T](data: ref T, count: I64) -> ref T
    // Computes an offset pointer within an array
    if (fn_name == "array_offset_ptr") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type from the first argument
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                if (arg_type->is<types::RefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
                } else if (arg_type->is<types::MutRefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
                }
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // array_offset_mut_ptr[T](data: mut ref T, count: I64) -> mut ref T
    // Same as array_offset_ptr but for mutable references
    if (fn_name == "array_offset_mut_ptr") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                if (arg_type->is<types::MutRefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::MutRefType>().inner);
                } else if (arg_type->is<types::RefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
                }
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + elem_type + ", ptr " + data + ", i64 " +
                      count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // ============================================================================
    // Type Information Intrinsics
    // ============================================================================

    // size_of[T]() -> I64
    if (fn_name == "size_of") {
        // For now, use compile-time known sizes
        // This would need type argument resolution for full support
        std::string result = fresh_reg();
        // Emit a placeholder - in practice this would be resolved at compile time
        emit_line("  " + result + " = call i64 @llvm.sizeoftype(metadata token zeroinitializer)");
        last_expr_type_ = "i64";

        // Simplified: return a default size
        // In a full implementation, we'd resolve the type parameter
        last_expr_type_ = "i64";
        return "8"; // Placeholder
    }

    // align_of[T]() -> I64
    if (fn_name == "align_of") {
        last_expr_type_ = "i64";
        return "8"; // Placeholder - would need type resolution
    }

    // ============================================================================
    // Unsafe Conversions
    // ============================================================================

    // transmute[T, U](val: T) -> U
    if (fn_name == "transmute") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;

            // For now, just bitcast - in practice would validate sizes match
            std::string result = fresh_reg();
            // Bitcast requires same size, so we just return the value
            // A full implementation would validate types
            last_expr_type_ = val_type;
            return val;
        }
        return "0";
    }

    // cast[T, U](val: T) -> U
    if (fn_name == "cast") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            // Cast would need type argument resolution
            // For now, just return the value
            return val;
        }
        return "0";
    }

    // ============================================================================
    // Compiler Hints
    // ============================================================================

    // unreachable() -> Unit
    if (fn_name == "unreachable") {
        emit_line("  unreachable");
        block_terminated_ = true;
        last_expr_type_ = "void";
        return "0";
    }

    // assume(cond: Bool)
    if (fn_name == "assume") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            emit_line("  call void @llvm.assume(i1 " + cond + ")");
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // likely(cond: Bool) -> Bool
    if (fn_name == "likely") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @llvm.expect.i1(i1 " + cond + ", i1 true)");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // unlikely(cond: Bool) -> Bool
    if (fn_name == "unlikely") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @llvm.expect.i1(i1 " + cond + ", i1 false)");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // fence()
    if (fn_name == "fence") {
        emit_line("  fence seq_cst");
        last_expr_type_ = "void";
        return "0";
    }

    // ============================================================================
    // Bit Manipulation Intrinsics
    // ============================================================================

    // ctlz[T](val: T) -> T (count leading zeros)
    if (fn_name == "ctlz") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ctlz." + val_type + "(" +
                      val_type + " " + val + ", i1 false)");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // cttz[T](val: T) -> T (count trailing zeros)
    if (fn_name == "cttz") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.cttz." + val_type + "(" +
                      val_type + " " + val + ", i1 false)");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // ctpop[T](val: T) -> T (population count)
    if (fn_name == "ctpop") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ctpop." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // bswap[T](val: T) -> T (byte swap)
    if (fn_name == "bswap") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.bswap." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // bitreverse[T](val: T) -> T
    if (fn_name == "bitreverse") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.bitreverse." + val_type +
                      "(" + val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // Math Intrinsics
    // ============================================================================

    // sqrt[T](val: T) -> T
    if (fn_name == "sqrt" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.sqrt." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // sin[T](val: T) -> T
    if (fn_name == "sin" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.sin." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // cos[T](val: T) -> T
    if (fn_name == "cos" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.cos." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // log[T](val: T) -> T
    if (fn_name == "log" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.log." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // exp[T](val: T) -> T
    if (fn_name == "exp" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.exp." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // pow[T](base: T, exp: T) -> T
    if (fn_name == "pow" && func_sig->is_intrinsic) {
        if (call.args.size() >= 2) {
            std::string base = gen_expr(*call.args[0]);
            std::string base_type = last_expr_type_;
            std::string exp = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + base_type + " @llvm.pow." + base_type + "(" +
                      base_type + " " + base + ", " + base_type + " " + exp + ")");
            last_expr_type_ = base_type;
            return result;
        }
        return "1.0";
    }

    // floor[T](val: T) -> T
    if (fn_name == "floor" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.floor." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // ceil[T](val: T) -> T
    if (fn_name == "ceil" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.ceil." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // round[T](val: T) -> T
    if (fn_name == "round" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.round." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // trunc[T](val: T) -> T
    if (fn_name == "trunc" && func_sig->is_intrinsic) {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.trunc." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // fma[T](a: T, b: T, c: T) -> T (fused multiply-add)
    if (fn_name == "fma") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.fma." + a_type + "(" + a_type +
                      " " + a + ", " + a_type + " " + b + ", " + a_type + " " + c + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // minnum[T](a: T, b: T) -> T
    if (fn_name == "minnum") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.minnum." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // maxnum[T](a: T, b: T) -> T
    if (fn_name == "maxnum") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.maxnum." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // fabs[T](val: T) -> T
    if (fn_name == "fabs") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + val_type + " @llvm.fabs." + val_type + "(" +
                      val_type + " " + val + ")");
            last_expr_type_ = val_type;
            return result;
        }
        return "0.0";
    }

    // copysign[T](a: T, b: T) -> T
    if (fn_name == "copysign") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + a_type + " @llvm.copysign." + a_type + "(" +
                      a_type + " " + a + ", " + a_type + " " + b + ")");
            last_expr_type_ = a_type;
            return result;
        }
        return "0.0";
    }

    // Intrinsic not implemented - return nullopt to fall through
    return std::nullopt;
}

} // namespace tml::codegen
