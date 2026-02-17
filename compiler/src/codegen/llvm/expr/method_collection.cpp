//! # LLVM IR Generator - Collection Methods
//!
//! This file implements instance methods for collection types.
//!
//! ## Buffer Methods
//!
//! `get`, `set`, `len`, `fill`
//!
//! Note: List and HashMap methods removed — now pure TML
//! (see lib/std/src/collections/list.tml, hashmap.tml)
//!
//! Methods delegate to runtime functions like `@buffer_len`.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_collection_method(const parser::MethodCallExpr& call,
                                      const std::string& receiver,
                                      const std::string& receiver_type_name,
                                      types::TypePtr receiver_type) -> std::optional<std::string> {
    const std::string& method = call.method;

    // Only handle Buffer (List and HashMap are now pure TML)
    if (receiver_type_name != "Buffer") {
        return std::nullopt;
    }

    // Extract handle from collection struct
    std::string struct_type = "%struct." + receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            struct_type = "%struct." + named.name;
            for (const auto& arg : named.type_args) {
                struct_type += "__" + mangle_type(arg);
            }
        }
    }

    // Handle different receiver types:
    // 1. If last_expr_type_ is a struct type, receiver is a loaded value - store to temp
    // 2. If last_expr_type_ is empty/ptr/other, receiver might be a pointer from GEP
    std::string handle;

    // First, ensure we have a valid receiver pointer
    std::string receiver_ptr;
    if (last_expr_type_.starts_with("%struct.")) {
        // Receiver is a loaded struct value - store to temp alloca
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + struct_type);
        emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
        receiver_ptr = tmp;
    } else {
        // Receiver might be a pointer (from GEP or other source)
        // Load the struct value first, then store to temp
        std::string loaded = fresh_reg();
        emit_line("  " + loaded + " = load " + struct_type + ", ptr " + receiver);
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + struct_type);
        emit_line("  store " + struct_type + " " + loaded + ", ptr " + tmp);
        receiver_ptr = tmp;
    }

    // Extract handle from the collection struct
    std::string handle_ptr = fresh_reg();
    emit_line("  " + handle_ptr + " = getelementptr " + struct_type + ", ptr " + receiver_ptr +
              ", i32 0, i32 0");
    handle = fresh_reg();
    emit_line("  " + handle + " = load ptr, ptr " + handle_ptr);

    // Note: List and HashMap methods removed — now pure TML

    // Buffer methods
    if (receiver_type_name == "Buffer") {
        if (method == "write_byte") {
            emit_coverage("Buffer::write_byte");
            if (call.args.empty()) {
                report_error("write_byte requires a value argument", call.span, "C008");
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_write_byte(ptr " + handle + ", i32 " + val + ")");
            return "void";
        }
        if (method == "read_byte") {
            emit_coverage("Buffer::read_byte");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + handle + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "write_i32") {
            emit_coverage("Buffer::write_i32");
            if (call.args.empty()) {
                report_error("write_i32 requires a value argument", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_write_i32(ptr " + handle + ", i32 " + val + ")");
            return "void";
        }
        if (method == "write_i64") {
            emit_coverage("Buffer::write_i64");
            if (call.args.empty()) {
                report_error("write_i64 requires a value argument", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string val_i64;
            if (val_type == "i64") {
                val_i64 = val;
            } else {
                val_i64 = fresh_reg();
                emit_line("  " + val_i64 + " = sext " + val_type + " " + val + " to i64");
            }
            emit_line("  call void @buffer_write_i64(ptr " + handle + ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "read_i32") {
            emit_coverage("Buffer::read_i32");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + handle + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_i64") {
            emit_coverage("Buffer::read_i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_i64(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "len" || method == "length") {
            emit_coverage("Buffer::len");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "capacity") {
            emit_coverage("Buffer::capacity");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_capacity(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "remaining") {
            emit_coverage("Buffer::remaining");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_remaining(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "clear") {
            emit_coverage("Buffer::clear");
            emit_line("  call void @buffer_clear(ptr " + handle + ")");
            return "void";
        }
        if (method == "reset_read") {
            emit_coverage("Buffer::reset_read");
            emit_line("  call void @buffer_reset_read(ptr " + handle + ")");
            return "void";
        }
        if (method == "destroy") {
            emit_coverage("Buffer::destroy");
            emit_line("  call void @buffer_destroy(ptr " + handle + ")");
            return "void";
        }

        // Index access methods (Node.js: buf[index])
        if (method == "get") {
            emit_coverage("Buffer::get");
            if (call.args.empty()) {
                report_error("get requires an index argument", call.span, "C008");
                return "0";
            }
            std::string idx = gen_expr(*call.args[0]);
            std::string idx_type = last_expr_type_;
            std::string idx_i64 = idx;
            if (idx_type == "i32") {
                idx_i64 = fresh_reg();
                emit_line("  " + idx_i64 + " = sext i32 " + idx + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_get(ptr " + handle + ", i64 " + idx_i64 +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "set") {
            emit_coverage("Buffer::set");
            if (call.args.size() < 2) {
                report_error("set requires index and value arguments", call.span, "C008");
                return "void";
            }
            std::string idx = gen_expr(*call.args[0]);
            std::string idx_type = last_expr_type_;
            std::string idx_i64 = idx;
            if (idx_type == "i32") {
                idx_i64 = fresh_reg();
                emit_line("  " + idx_i64 + " = sext i32 " + idx + " to i64");
            }
            std::string val = gen_expr(*call.args[1]);
            emit_line("  call void @buffer_set(ptr " + handle + ", i64 " + idx_i64 + ", i32 " +
                      val + ")");
            return "void";
        }

        // 8-bit integer read/write
        if (method == "write_u8") {
            emit_coverage("Buffer::write_u8");
            if (call.args.size() < 2) {
                report_error("write_u8 requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u8(ptr " + handle + ", i64 " + offset_i64 +
                      ", i32 " + val + ")");
            return "void";
        }
        if (method == "read_u8") {
            emit_coverage("Buffer::read_u8");
            if (call.args.empty()) {
                report_error("read_u8 requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_u8(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_i8") {
            emit_coverage("Buffer::read_i8");
            if (call.args.empty()) {
                report_error("read_i8 requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i8(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }

        // 16-bit integer read/write
        if (method == "write_u16_le") {
            emit_coverage("Buffer::write_u16_le");
            if (call.args.size() < 2) {
                report_error("write_u16_le requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u16_le(ptr " + handle + ", i64 " + offset_i64 +
                      ", i32 " + val + ")");
            return "void";
        }
        if (method == "write_u16_be") {
            emit_coverage("Buffer::write_u16_be");
            if (call.args.size() < 2) {
                report_error("write_u16_be requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u16_be(ptr " + handle + ", i64 " + offset_i64 +
                      ", i32 " + val + ")");
            return "void";
        }
        if (method == "read_u16_le") {
            emit_coverage("Buffer::read_u16_le");
            if (call.args.empty()) {
                report_error("read_u16_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_u16_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_u16_be") {
            emit_coverage("Buffer::read_u16_be");
            if (call.args.empty()) {
                report_error("read_u16_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_u16_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_i16_le") {
            emit_coverage("Buffer::read_i16_le");
            if (call.args.empty()) {
                report_error("read_i16_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i16_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_i16_be") {
            emit_coverage("Buffer::read_i16_be");
            if (call.args.empty()) {
                report_error("read_i16_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i16_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }

        // 32-bit integer read/write
        if (method == "write_u32_le") {
            emit_coverage("Buffer::write_u32_le");
            if (call.args.size() < 2) {
                report_error("write_u32_le requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string val_i64 = val;
            if (val_type == "i32") {
                val_i64 = fresh_reg();
                emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            }
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u32_le(ptr " + handle + ", i64 " + offset_i64 +
                      ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "write_u32_be") {
            emit_coverage("Buffer::write_u32_be");
            if (call.args.size() < 2) {
                report_error("write_u32_be requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string val_i64 = val;
            if (val_type == "i32") {
                val_i64 = fresh_reg();
                emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            }
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u32_be(ptr " + handle + ", i64 " + offset_i64 +
                      ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "read_u32_le") {
            emit_coverage("Buffer::read_u32_le");
            if (call.args.empty()) {
                report_error("read_u32_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_u32_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "read_u32_be") {
            emit_coverage("Buffer::read_u32_be");
            if (call.args.empty()) {
                report_error("read_u32_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_u32_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "read_i32_at") {
            emit_coverage("Buffer::read_i32_at");
            if (call.args.empty()) {
                report_error("read_i32_at requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "read_i32_be") {
            emit_coverage("Buffer::read_i32_be");
            if (call.args.empty()) {
                report_error("read_i32_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i32";
            return result;
        }

        // 64-bit integer read/write
        if (method == "write_u64_le") {
            emit_coverage("Buffer::write_u64_le");
            if (call.args.size() < 2) {
                report_error("write_u64_le requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string val_i64 = val;
            if (val_type == "i32") {
                val_i64 = fresh_reg();
                emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            }
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u64_le(ptr " + handle + ", i64 " + offset_i64 +
                      ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "write_u64_be") {
            emit_coverage("Buffer::write_u64_be");
            if (call.args.size() < 2) {
                report_error("write_u64_be requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;
            std::string val_i64 = val;
            if (val_type == "i32") {
                val_i64 = fresh_reg();
                emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            }
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_u64_be(ptr " + handle + ", i64 " + offset_i64 +
                      ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "read_u64_le") {
            emit_coverage("Buffer::read_u64_le");
            if (call.args.empty()) {
                report_error("read_u64_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_u64_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "read_u64_be") {
            emit_coverage("Buffer::read_u64_be");
            if (call.args.empty()) {
                report_error("read_u64_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_u64_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "read_i64_at") {
            emit_coverage("Buffer::read_i64_at");
            if (call.args.empty()) {
                report_error("read_i64_at requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_i64_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "read_i64_be") {
            emit_coverage("Buffer::read_i64_be");
            if (call.args.empty()) {
                report_error("read_i64_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_read_i64_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }

        // Float read/write
        if (method == "write_f32_le") {
            emit_coverage("Buffer::write_f32_le");
            if (call.args.size() < 2) {
                report_error("write_f32_le requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_f32_le(ptr " + handle + ", i64 " + offset_i64 +
                      ", float " + val + ")");
            return "void";
        }
        if (method == "write_f32_be") {
            emit_coverage("Buffer::write_f32_be");
            if (call.args.size() < 2) {
                report_error("write_f32_be requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_f32_be(ptr " + handle + ", i64 " + offset_i64 +
                      ", float " + val + ")");
            return "void";
        }
        if (method == "read_f32_le") {
            emit_coverage("Buffer::read_f32_le");
            if (call.args.empty()) {
                report_error("read_f32_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @buffer_read_f32_le(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "float";
            return result;
        }
        if (method == "read_f32_be") {
            emit_coverage("Buffer::read_f32_be");
            if (call.args.empty()) {
                report_error("read_f32_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @buffer_read_f32_be(ptr " + handle + ", i64 " +
                      offset_i64 + ")");
            last_expr_type_ = "float";
            return result;
        }
        if (method == "write_f64_le") {
            emit_coverage("Buffer::write_f64_le");
            if (call.args.size() < 2) {
                report_error("write_f64_le requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_f64_le(ptr " + handle + ", i64 " + offset_i64 +
                      ", double " + val + ")");
            return "void";
        }
        if (method == "write_f64_be") {
            emit_coverage("Buffer::write_f64_be");
            if (call.args.size() < 2) {
                report_error("write_f64_be requires value and offset arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  call void @buffer_write_f64_be(ptr " + handle + ", i64 " + offset_i64 +
                      ", double " + val + ")");
            return "void";
        }
        if (method == "read_f64_le") {
            emit_coverage("Buffer::read_f64_le");
            if (call.args.empty()) {
                report_error("read_f64_le requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @buffer_read_f64_le(ptr " + handle +
                      ", i64 " + offset_i64 + ")");
            last_expr_type_ = "double";
            return result;
        }
        if (method == "read_f64_be") {
            emit_coverage("Buffer::read_f64_be");
            if (call.args.empty()) {
                report_error("read_f64_be requires an offset argument", call.span, "C008");
                return "0";
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_type = last_expr_type_;
            std::string offset_i64 = offset;
            if (offset_type == "i32") {
                offset_i64 = fresh_reg();
                emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @buffer_read_f64_be(ptr " + handle +
                      ", i64 " + offset_i64 + ")");
            last_expr_type_ = "double";
            return result;
        }

        // Manipulation methods
        if (method == "fill") {
            emit_coverage("Buffer::fill");
            if (call.args.size() < 3) {
                report_error("fill requires value, start, and end arguments", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string start = gen_expr(*call.args[1]);
            std::string start_type = last_expr_type_;
            std::string start_i64 = start;
            if (start_type == "i32") {
                start_i64 = fresh_reg();
                emit_line("  " + start_i64 + " = sext i32 " + start + " to i64");
            }
            std::string end = gen_expr(*call.args[2]);
            std::string end_type = last_expr_type_;
            std::string end_i64 = end;
            if (end_type == "i32") {
                end_i64 = fresh_reg();
                emit_line("  " + end_i64 + " = sext i32 " + end + " to i64");
            }
            emit_line("  call void @buffer_fill(ptr " + handle + ", i32 " + val + ", i64 " +
                      start_i64 + ", i64 " + end_i64 + ")");
            return "void";
        }
        if (method == "fill_all") {
            emit_coverage("Buffer::fill_all");
            if (call.args.empty()) {
                report_error("fill_all requires a value argument", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            // Get length
            std::string len = fresh_reg();
            emit_line("  " + len + " = call i64 @buffer_len(ptr " + handle + ")");
            emit_line("  call void @buffer_fill(ptr " + handle + ", i32 " + val + ", i64 0, i64 " +
                      len + ")");
            return "void";
        }
        if (method == "copy_to") {
            emit_coverage("Buffer::copy_to");
            if (call.args.size() < 4) {
                report_error("copy_to requires target, target_start, source_start, source_end",
                             call.span, "C008");
                return "0";
            }
            // Get target buffer's handle
            std::string target_buf = gen_expr(*call.args[0]);
            // Extract handle from target buffer
            std::string target_handle_ptr = fresh_reg();
            emit_line("  " + target_handle_ptr + " = getelementptr %struct.Buffer, ptr " +
                      target_buf + ", i32 0, i32 0");
            std::string target_handle = fresh_reg();
            emit_line("  " + target_handle + " = load ptr, ptr " + target_handle_ptr);

            std::string target_start = gen_expr(*call.args[1]);
            std::string ts_type = last_expr_type_;
            std::string ts_i64 = target_start;
            if (ts_type == "i32") {
                ts_i64 = fresh_reg();
                emit_line("  " + ts_i64 + " = sext i32 " + target_start + " to i64");
            }
            std::string source_start = gen_expr(*call.args[2]);
            std::string ss_type = last_expr_type_;
            std::string ss_i64 = source_start;
            if (ss_type == "i32") {
                ss_i64 = fresh_reg();
                emit_line("  " + ss_i64 + " = sext i32 " + source_start + " to i64");
            }
            std::string source_end = gen_expr(*call.args[3]);
            std::string se_type = last_expr_type_;
            std::string se_i64 = source_end;
            if (se_type == "i32") {
                se_i64 = fresh_reg();
                emit_line("  " + se_i64 + " = sext i32 " + source_end + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_copy(ptr " + handle + ", ptr " +
                      target_handle + ", i64 " + ts_i64 + ", i64 " + ss_i64 + ", i64 " + se_i64 +
                      ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "slice") {
            emit_coverage("Buffer::slice");
            if (call.args.size() < 2) {
                report_error("slice requires start and end arguments", call.span, "C008");
                return "0";
            }
            std::string start = gen_expr(*call.args[0]);
            std::string start_type = last_expr_type_;
            std::string start_i64 = start;
            if (start_type == "i32") {
                start_i64 = fresh_reg();
                emit_line("  " + start_i64 + " = sext i32 " + start + " to i64");
            }
            std::string end = gen_expr(*call.args[1]);
            std::string end_type = last_expr_type_;
            std::string end_i64 = end;
            if (end_type == "i32") {
                end_i64 = fresh_reg();
                emit_line("  " + end_i64 + " = sext i32 " + end + " to i64");
            }
            std::string new_handle = fresh_reg();
            emit_line("  " + new_handle + " = call ptr @buffer_slice(ptr " + handle + ", i64 " +
                      start_i64 + ", i64 " + end_i64 + ")");
            // Return as Buffer struct
            std::string result = fresh_reg();
            emit_line("  " + result + " = insertvalue %struct.Buffer undef, ptr " + new_handle +
                      ", 0");
            last_expr_type_ = "%struct.Buffer";
            return result;
        }
        if (method == "duplicate") {
            emit_coverage("Buffer::duplicate");
            // duplicate() is slice(0, len()) - creates a copy of the entire buffer
            std::string len = fresh_reg();
            emit_line("  " + len + " = call i64 @buffer_len(ptr " + handle + ")");
            std::string new_handle = fresh_reg();
            emit_line("  " + new_handle + " = call ptr @buffer_slice(ptr " + handle +
                      ", i64 0, i64 " + len + ")");
            // Return as Buffer struct
            std::string result = fresh_reg();
            emit_line("  " + result + " = insertvalue %struct.Buffer undef, ptr " + new_handle +
                      ", 0");
            last_expr_type_ = "%struct.Buffer";
            return result;
        }
        if (method == "swap16") {
            emit_coverage("Buffer::swap16");
            emit_line("  call void @buffer_swap16(ptr " + handle + ")");
            return "void";
        }
        if (method == "swap32") {
            emit_coverage("Buffer::swap32");
            emit_line("  call void @buffer_swap32(ptr " + handle + ")");
            return "void";
        }
        if (method == "swap64") {
            emit_coverage("Buffer::swap64");
            emit_line("  call void @buffer_swap64(ptr " + handle + ")");
            return "void";
        }

        // Search and comparison methods
        if (method == "compare") {
            emit_coverage("Buffer::compare");
            if (call.args.empty()) {
                report_error("compare requires another buffer argument", call.span, "C008");
                return "0";
            }
            std::string other_buf = gen_expr(*call.args[0]);
            std::string other_handle_ptr = fresh_reg();
            emit_line("  " + other_handle_ptr + " = getelementptr %struct.Buffer, ptr " +
                      other_buf + ", i32 0, i32 0");
            std::string other_handle = fresh_reg();
            emit_line("  " + other_handle + " = load ptr, ptr " + other_handle_ptr);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_compare(ptr " + handle + ", ptr " +
                      other_handle + ")");
            last_expr_type_ = "i32";
            return result;
        }
        if (method == "equals") {
            emit_coverage("Buffer::equals");
            if (call.args.empty()) {
                report_error("equals requires another buffer argument", call.span, "C008");
                return "0";
            }
            std::string other_buf = gen_expr(*call.args[0]);
            std::string other_handle_ptr = fresh_reg();
            emit_line("  " + other_handle_ptr + " = getelementptr %struct.Buffer, ptr " +
                      other_buf + ", i32 0, i32 0");
            std::string other_handle = fresh_reg();
            emit_line("  " + other_handle + " = load ptr, ptr " + other_handle_ptr);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @buffer_equals(ptr " + handle + ", ptr " +
                      other_handle + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "index_of") {
            emit_coverage("Buffer::index_of");
            if (call.args.size() < 2) {
                report_error("index_of requires value and start arguments", call.span, "C008");
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string start = gen_expr(*call.args[1]);
            std::string start_type = last_expr_type_;
            std::string start_i64 = start;
            if (start_type == "i32") {
                start_i64 = fresh_reg();
                emit_line("  " + start_i64 + " = sext i32 " + start + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_index_of(ptr " + handle + ", i32 " +
                      val + ", i64 " + start_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "last_index_of") {
            emit_coverage("Buffer::last_index_of");
            if (call.args.size() < 2) {
                report_error("last_index_of requires value and start arguments", call.span, "C008");
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string start = gen_expr(*call.args[1]);
            std::string start_type = last_expr_type_;
            std::string start_i64 = start;
            if (start_type == "i32") {
                start_i64 = fresh_reg();
                emit_line("  " + start_i64 + " = sext i32 " + start + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_last_index_of(ptr " + handle + ", i32 " +
                      val + ", i64 " + start_i64 + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "includes") {
            emit_coverage("Buffer::includes");
            if (call.args.size() < 2) {
                report_error("includes requires value and start arguments", call.span, "C008");
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string start = gen_expr(*call.args[1]);
            std::string start_type = last_expr_type_;
            std::string start_i64 = start;
            if (start_type == "i32") {
                start_i64 = fresh_reg();
                emit_line("  " + start_i64 + " = sext i32 " + start + " to i64");
            }
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @buffer_includes(ptr " + handle + ", i32 " +
                      val + ", i64 " + start_i64 + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }

        // String conversion methods
        if (method == "to_hex") {
            emit_coverage("Buffer::to_hex");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_to_hex(ptr " + handle + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "to_string") {
            emit_coverage("Buffer::to_string");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_to_string(ptr " + handle + ")");
            last_expr_type_ = "ptr";
            return result;
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
