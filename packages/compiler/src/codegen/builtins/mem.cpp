// LLVM IR generator - Memory builtin functions
// Handles: alloc, dealloc, mem_alloc, mem_free, mem_copy, mem_move,
//          mem_set, mem_zero, mem_compare, mem_eq, read_i32, write_i32, ptr_offset

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_mem(const std::string& fn_name,
                                    const parser::CallExpr& call) -> std::optional<std::string> {

    // Memory allocation: alloc(size) -> ptr
    // Always inline as malloc call (registered as builtin for type checking)
    if (fn_name == "alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            // Convert i32 size to i64 for malloc
            std::string size64 = fresh_reg();
            emit_line("  " + size64 + " = sext i32 " + size + " to i64");
            emit_line("  " + result + " = call ptr @malloc(i64 " + size64 + ")");
            return result;
        }
        return "null";
    }

    // Memory deallocation: dealloc(ptr)
    // Always inline as free call (registered as builtin for type checking)
    if (fn_name == "dealloc") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @free(ptr " + ptr + ")");
        }
        return "0";
    }

    // mem_alloc(size: I64) -> *Unit
    if (fn_name == "mem_alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc(i64 " + size + ")");
            return result;
        }
        return "null";
    }

    // mem_alloc_zeroed(size: I64) -> *Unit
    if (fn_name == "mem_alloc_zeroed") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc_zeroed(i64 " + size + ")");
            return result;
        }
        return "null";
    }

    // mem_realloc(ptr: *Unit, new_size: I64) -> *Unit
    if (fn_name == "mem_realloc") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_realloc(ptr " + ptr + ", i64 " + size +
                      ")");
            return result;
        }
        return "null";
    }

    // mem_free(ptr: *Unit) -> Unit
    if (fn_name == "mem_free") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @mem_free(ptr " + ptr + ")");
        }
        return "";
    }

    // mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_copy") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_copy(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_move") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_move(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
    if (fn_name == "mem_set") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_set(ptr " + ptr + ", i32 " + val + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_zero(ptr: *Unit, size: I64) -> Unit
    if (fn_name == "mem_zero") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            emit_line("  call void @mem_zero(ptr " + ptr + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
    if (fn_name == "mem_compare") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_compare(ptr " + a + ", ptr " + b +
                      ", i64 " + size + ")");
            return result;
        }
        return "0";
    }

    // mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool (I32)
    if (fn_name == "mem_eq") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_eq(ptr " + a + ", ptr " + b + ", i64 " +
                      size + ")");
            return result;
        }
        return "0";
    }

    // Read from memory: read_i32(ptr) -> I32
    if (fn_name == "read_i32" && !env_.lookup_func("read_i32").has_value()) {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + ptr);
            return result;
        }
        return "0";
    }

    // Write to memory: write_i32(ptr, value)
    if (fn_name == "write_i32" && !env_.lookup_func("write_i32").has_value()) {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  store i32 " + val + ", ptr " + ptr);
        }
        return "0";
    }

    // Pointer offset: ptr_offset(ptr, offset) -> ptr
    if (fn_name == "ptr_offset" && !env_.lookup_func("ptr_offset").has_value()) {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr i32, ptr " + ptr + ", i32 " + offset);
            return result;
        }
        return "null";
    }

    return std::nullopt;
}

} // namespace tml::codegen
