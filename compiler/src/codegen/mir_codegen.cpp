TML_MODULE("compiler")

//! # MIR-based LLVM IR Code Generator
//!
//! This file generates LLVM IR directly from MIR (Mid-level IR).
//!
//! ## Advantages of MIR-based Codegen
//!
//! MIR is already in SSA form, which maps naturally to LLVM IR:
//! - No need for SSA construction during codegen
//! - Direct mapping from MIR values to LLVM registers
//! - Simplified control flow handling
//!
//! ## Generation Pipeline
//!
//! | Phase            | Method              | Output                |
//! |------------------|---------------------|-----------------------|
//! | Preamble         | `emit_preamble`     | Target triple, attrs  |
//! | Type definitions | `emit_type_defs`    | Struct/enum layouts   |
//! | Functions        | `emit_function`     | Function definitions  |
//! | Basic blocks     | `emit_basic_block`  | Labels and terminators|
//! | Instructions     | `emit_instruction`  | LLVM instructions     |
//!
//! ## Value Mapping
//!
//! `value_regs_` maps MIR value IDs to LLVM register names (%t0, %t1, etc.).
//!
//! ## Code Organization
//!
//! The implementation is split across multiple files:
//! - mir_codegen.cpp: Core generation (this file)
//! - mir/instructions.cpp: Instruction emission
//! - mir/terminators.cpp: Terminator emission
//! - mir/types.cpp: Type conversion
//! - mir/helpers.cpp: Helper methods

#include "codegen/mir_codegen.hpp"

#include "codegen/target.hpp"
#include "version_generated.hpp"

#include <sstream>

namespace tml::codegen {

MirCodegen::MirCodegen(MirCodegenOptions options) : options_(std::move(options)) {}

void MirCodegen::emit(const std::string& s) {
    output_ << s;
}

void MirCodegen::emitln(const std::string& s) {
    output_ << s << "\n";
}

void MirCodegen::emit_comment(const std::string& s) {
    if (options_.emit_comments) {
        emitln("; " + s);
    }
}

auto MirCodegen::new_temp() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto MirCodegen::generate(const mir::Module& module) -> std::string {
    output_.str("");
    output_.clear();
    temp_counter_ = 0;
    spill_counter_ = 0;
    value_regs_.clear();
    value_types_.clear();
    struct_field_types_.clear();
    value_spill_allocas_.clear();
    block_labels_.clear();
    emitted_types_.clear();
    string_constants_.clear();
    value_string_contents_.clear();
    used_enum_types_.clear();

    // First pass: collect string constants, enum types, and generic enum defs
    generic_enum_defs_.clear();
    for (const auto& func : module.functions) {
        // Collect generic enum types from function signatures
        collect_enum_types_from_type(func.return_type);
        for (const auto& param : func.params) {
            collect_enum_types_from_type(param.type);
        }

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* const_inst = std::get_if<mir::ConstantInst>(&inst.inst)) {
                    if (auto* str_const = std::get_if<mir::ConstString>(&const_inst->value)) {
                        if (string_constants_.find(str_const->value) == string_constants_.end()) {
                            std::string global_name =
                                "@.str." + std::to_string(string_constants_.size());
                            string_constants_[str_const->value] = global_name;
                        }
                    }
                }
                // Collect enum types from EnumInitInst (for imported enums)
                if (auto* enum_inst = std::get_if<mir::EnumInitInst>(&inst.inst)) {
                    used_enum_types_.insert(enum_inst->enum_name);
                }
                // Collect generic enum types from instruction result types
                if (inst.type) {
                    collect_enum_types_from_type(inst.type);
                }
            }
        }
    }

    emit_preamble();

    // Emit string constants after preamble
    for (const auto& [value, name] : string_constants_) {
        std::string escaped;
        for (char c : value) {
            if (c == '\n') {
                escaped += "\\0A";
            } else if (c == '\r') {
                escaped += "\\0D";
            } else if (c == '"') {
                escaped += "\\22";
            } else if (c == '\\') {
                escaped += "\\5C";
            } else if (c == '\0') {
                escaped += "\\00";
            } else {
                escaped += c;
            }
        }
        size_t len = value.size() + 1; // +1 for null terminator
        emitln(name + " = private constant [" + std::to_string(len) + " x i8] c\"" + escaped +
               "\\00\"");
    }
    if (!string_constants_.empty()) {
        emitln();
    }

    emit_type_defs(module);

    // Collect sret functions (those with uses_sret flag set by RVO pass)
    sret_functions_.clear();
    for (const auto& func : module.functions) {
        if (func.uses_sret && func.original_return_type) {
            sret_functions_[func.name] = mir_type_to_llvm(func.original_return_type);
        }
    }

    // Collect declared parameter types for all functions (for array-to-slice coercion)
    // Also convert this/self struct params to pointer types to match the actual
    // LLVM IR signature (where struct this/self is always passed as ptr).
    func_param_types_.clear();
    for (const auto& func : module.functions) {
        std::vector<mir::MirTypePtr> param_types;
        for (const auto& p : func.params) {
            if ((p.name == "this" || p.name == "self") && p.type) {
                std::string llvm_ty = mir_type_to_llvm(p.type);
                if (llvm_ty.starts_with("%struct.") || llvm_ty.starts_with("%enum.") ||
                    llvm_ty.starts_with("%class.") || llvm_ty.starts_with("%union.")) {
                    param_types.push_back(mir::make_ptr_type());
                    continue;
                }
            }
            param_types.push_back(p.type);
        }
        func_param_types_[func.name] = std::move(param_types);
    }

    // Emit functions
    for (const auto& func : module.functions) {
        emit_function(func);
    }

    // Emit entry point wrappers.
    // The user's `main` function is emitted as `tml_main` (see emit_function),
    // and these wrappers provide the public entry symbols.
    if (options_.generate_exe_main) {
        emit_main_wrapper(module);
    } else if (!options_.test_entry_name.empty()) {
        emit_test_entry_wrapper(module);
    }

    // Module identification metadata
    emitln();
    emitln("!llvm.ident = !{!0}");
    emitln("!0 = !{!\"tml version " + std::string(tml::VERSION) + "\"}");

    return output_.str();
}

auto MirCodegen::generate_cgu(const mir::Module& module,
                              const std::vector<size_t>& function_indices) -> std::string {
    output_.str("");
    output_.clear();
    temp_counter_ = 0;
    spill_counter_ = 0;
    value_regs_.clear();
    value_types_.clear();
    struct_field_types_.clear();
    value_spill_allocas_.clear();
    block_labels_.clear();
    emitted_types_.clear();
    string_constants_.clear();
    value_string_contents_.clear();
    used_enum_types_.clear();

    // Build index set for O(1) lookup
    std::unordered_set<size_t> included(function_indices.begin(), function_indices.end());

    // First pass: collect string constants, enum types, and generic enum defs
    // (same as generate() — all CGUs need the complete set)
    generic_enum_defs_.clear();
    for (const auto& func : module.functions) {
        // Collect generic enum types from function signatures
        collect_enum_types_from_type(func.return_type);
        for (const auto& param : func.params) {
            collect_enum_types_from_type(param.type);
        }

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* const_inst = std::get_if<mir::ConstantInst>(&inst.inst)) {
                    if (auto* str_const = std::get_if<mir::ConstString>(&const_inst->value)) {
                        if (string_constants_.find(str_const->value) == string_constants_.end()) {
                            std::string global_name =
                                "@.str." + std::to_string(string_constants_.size());
                            string_constants_[str_const->value] = global_name;
                        }
                    }
                }
                if (auto* enum_inst = std::get_if<mir::EnumInitInst>(&inst.inst)) {
                    used_enum_types_.insert(enum_inst->enum_name);
                }
                if (inst.type) {
                    collect_enum_types_from_type(inst.type);
                }
            }
        }
    }

    emit_preamble();

    // Emit string constants after preamble
    for (const auto& [value, name] : string_constants_) {
        std::string escaped;
        for (char c : value) {
            if (c == '\n') {
                escaped += "\\0A";
            } else if (c == '\r') {
                escaped += "\\0D";
            } else if (c == '"') {
                escaped += "\\22";
            } else if (c == '\\') {
                escaped += "\\5C";
            } else if (c == '\0') {
                escaped += "\\00";
            } else {
                escaped += c;
            }
        }
        size_t len = value.size() + 1;
        emitln(name + " = private constant [" + std::to_string(len) + " x i8] c\"" + escaped +
               "\\00\"");
    }
    if (!string_constants_.empty()) {
        emitln();
    }

    emit_type_defs(module);

    // Collect sret functions from ALL functions (same as generate())
    sret_functions_.clear();
    for (const auto& func : module.functions) {
        if (func.uses_sret && func.original_return_type) {
            sret_functions_[func.name] = mir_type_to_llvm(func.original_return_type);
        }
    }

    // Collect declared parameter types for all functions (for array-to-slice coercion)
    // Also convert this/self struct params to pointer types to match the actual
    // LLVM IR signature (where struct this/self is always passed as ptr).
    func_param_types_.clear();
    for (const auto& func : module.functions) {
        std::vector<mir::MirTypePtr> param_types;
        for (const auto& p : func.params) {
            if ((p.name == "this" || p.name == "self") && p.type) {
                std::string llvm_ty = mir_type_to_llvm(p.type);
                if (llvm_ty.starts_with("%struct.") || llvm_ty.starts_with("%enum.") ||
                    llvm_ty.starts_with("%class.") || llvm_ty.starts_with("%union.")) {
                    param_types.push_back(mir::make_ptr_type());
                    continue;
                }
            }
            param_types.push_back(p.type);
        }
        func_param_types_[func.name] = std::move(param_types);
    }

    // Emit functions: define for included, declare for others
    for (size_t i = 0; i < module.functions.size(); ++i) {
        if (included.count(i)) {
            emit_function(module.functions[i]);
        } else {
            emit_function_declaration(module.functions[i]);
        }
    }

    // Emit entry point wrappers for the CGU that contains the `main` function.
    {
        bool this_cgu_has_main = false;
        for (size_t idx : function_indices) {
            if (module.functions[idx].name == "main") {
                this_cgu_has_main = true;
                break;
            }
        }
        if (this_cgu_has_main) {
            if (options_.generate_exe_main) {
                emit_main_wrapper(module);
            } else if (!options_.test_entry_name.empty()) {
                emit_test_entry_wrapper(module);
            }
        }
    }

    // Module identification metadata
    emitln();
    emitln("!llvm.ident = !{!0}");
    emitln("!0 = !{!\"tml version " + std::string(tml::VERSION) + "\"}");

    return output_.str();
}

void MirCodegen::emit_function_declaration(const mir::Function& func) {
    std::string ret_type = mir_type_to_llvm(func.return_type);
    // Rename `main` based on entry mode:
    // - generate_exe_main: rename to tml_main (C entry wrapper calls it)
    // - test_entry_name: rename to e.g. tml_test_0 (dispatcher calls it)
    std::string decl_name = func.name;
    if (func.name == "main") {
        // Always rename main to tml_main; wrappers provide the public entry name
        if (options_.generate_exe_main || !options_.test_entry_name.empty())
            decl_name = "tml_main";
    }
    emit("declare " + ret_type + " @" + quote_func_name(decl_name) + "(");

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string param_type = mir_type_to_llvm(func.params[i].type);
        const auto& param_name = func.params[i].name;
        // Unit type maps to "void" but LLVM doesn't allow void as a parameter type.
        if (param_type == "void") {
            param_type = "{}";
        }
        // Method 'this'/'self' parameters for struct/enum types → ptr
        if ((param_name == "this" || param_name == "self") &&
            (param_type.starts_with("%struct.") || param_type.starts_with("%enum.") ||
             param_type.starts_with("%class.") || param_type.starts_with("%union."))) {
            param_type = "ptr";
        }
        if (func.uses_sret && i == 0 && func.original_return_type) {
            std::string orig_ret_type = mir_type_to_llvm(func.original_return_type);
            emit(param_type + " sret(" + orig_ret_type + ")");
        } else {
            emit(param_type);
        }
    }

    emitln(")");
    emitln();
}

void MirCodegen::emit_main_wrapper(const mir::Module& module) {
    // Find the `main` function to determine its return type.
    const mir::Function* main_func = nullptr;
    for (const auto& func : module.functions) {
        if (func.name == "main") {
            main_func = &func;
            break;
        }
    }
    if (!main_func) {
        return; // No main function in this module.
    }

    // Determine if user's main returns void or i32.
    std::string main_ret = mir_type_to_llvm(main_func->return_type);
    bool returns_void = (main_ret == "void");

    emitln("; C entry point — calls tml_main() generated from user's `main` function");
    emitln("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
    emitln("entry:");
    if (returns_void) {
        emitln("  call void @tml_main()");
        emitln("  ret i32 0");
    } else {
        emitln("  %ret = call i32 @tml_main()");
        emitln("  ret i32 %ret");
    }
    emitln("}");
    emitln();
}

void MirCodegen::emit_test_entry_wrapper(const mir::Module& module) {
    // Collect @test functions from the module.
    // Test files have @test annotated functions instead of main.
    std::vector<const mir::Function*> test_funcs;
    const mir::Function* main_func = nullptr;
    for (const auto& func : module.functions) {
        if (func.name == "main") {
            main_func = &func;
        }
        for (const auto& attr : func.attributes) {
            if (attr == "test") {
                test_funcs.push_back(&func);
                break;
            }
        }
    }

    if (test_funcs.empty() && !main_func) {
        return;
    }

    // Generate an i32-returning wrapper that the dispatcher can call.
    emitln("; Test entry wrapper — calls test functions, returns i32 for dispatcher");
    emitln("define dllexport i32 @" + quote_func_name(options_.test_entry_name) + "() {");
    emitln("entry:");

    if (!test_funcs.empty()) {
        // Call each @test function sequentially
        for (const auto* tf : test_funcs) {
            std::string ret_type = mir_type_to_llvm(tf->return_type);
            if (ret_type == "void") {
                emitln("  call void @" + quote_func_name(tf->name) + "()");
            } else {
                emitln("  call " + ret_type + " @" + quote_func_name(tf->name) + "()");
            }
        }
    } else if (main_func) {
        // Fallback: call main renamed to tml_main
        std::string main_ret = mir_type_to_llvm(main_func->return_type);
        if (main_ret == "void") {
            emitln("  call void @tml_main()");
        } else {
            emitln("  call i32 @tml_main()");
        }
    }

    emitln("  ret i32 0");
    emitln("}");
    emitln();
}

void MirCodegen::emit_preamble() {
    emit_comment("Generated by TML MIR Codegen");

    // Target datalayout computed from the target triple
    auto target = Target::from_triple(options_.target_triple);
    if (target) {
        emitln("target datalayout = \"" + target->to_data_layout() + "\"");
    }

    emitln("target triple = \"" + options_.target_triple + "\"");
    emitln();

    // Compiler identification embedded in the binary
    std::string ident = "tml version " + std::string(tml::VERSION);
    emitln("$__tml_ident = comdat any");
    emitln("@__tml_ident = linkonce_odr constant [" + std::to_string(ident.size() + 1) +
           " x i8] c\"" + ident + "\\00\", comdat, align 1");
    emitln("@llvm.used = appending global [1 x ptr] [ptr @__tml_ident], section \"llvm.metadata\"");
    emitln();

    // Declare printf, println, print, and abort for print builtins
    emitln("declare i32 @printf(ptr, ...)");
    emitln("declare void @print(ptr)");
    emitln("declare void @println(ptr)");
    emitln("declare void @abort() noreturn");
    // str_concat/_3/_4 — removed (Phase 49); time_ns — removed (Phase 49, 0 MIR callers)
    emitln("declare ptr @mem_alloc(i64)"); // Memory allocation for char-to-string
    emitln("declare i64 @strlen(ptr)");
    emitln("declare ptr @malloc(i64)");
    emitln("declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");
    emitln();

    // str_concat_opt: null-safe string concatenation (inlined from runtime.cpp)
    emitln("@.str.empty = private constant [1 x i8] c\"\\00\"");
    emitln("define internal ptr @str_concat_opt(ptr %a, ptr %b) {");
    emitln("entry:");
    emitln("  %a_null = icmp eq ptr %a, null");
    emitln("  %a_safe = select i1 %a_null, ptr @.str.empty, ptr %a");
    emitln("  %b_null = icmp eq ptr %b, null");
    emitln("  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b");
    emitln("  %len_a = call i64 @strlen(ptr %a_safe)");
    emitln("  %len_b = call i64 @strlen(ptr %b_safe)");
    emitln("  %total = add i64 %len_a, %len_b");
    emitln("  %alloc = add i64 %total, 1");
    emitln("  %buf = call ptr @malloc(i64 %alloc)");
    emitln("  call void @llvm.memcpy.p0.p0.i64(ptr %buf, ptr %a_safe, i64 %len_a, i1 false)");
    emitln("  %dst = getelementptr i8, ptr %buf, i64 %len_a");
    emitln("  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b_safe, i64 %len_b, i1 false)");
    emitln("  %end = getelementptr i8, ptr %buf, i64 %total");
    emitln("  store i8 0, ptr %end");
    emitln("  ret ptr %buf");
    emitln("}");
    emitln();

    // Black box functions (prevent optimization)
    emitln("declare i32 @black_box_i32(i32)");
    emitln("declare i64 @black_box_i64(i64)");
    emitln("declare double @black_box_f64(double)");
    emitln();

    // String format constants
    // %d\n\0 = 4 chars, %lld\n\0 = 6 chars, %f\n\0 = 4 chars, %s\n\0 = 4 chars
    emitln("@.str.int = private constant [4 x i8] c\"%d\\0A\\00\"");
    emitln("@.str.long = private constant [6 x i8] c\"%lld\\0A\\00\"");
    emitln("@.str.float = private constant [4 x i8] c\"%f\\0A\\00\"");
    emitln("@.str.str = private constant [4 x i8] c\"%s\\0A\\00\"");
    emitln("@.str.bool.true = private constant [5 x i8] c\"true\\00\"");
    emitln("@.str.bool.false = private constant [6 x i8] c\"false\\00\"");
    emitln(
        "@.str.sq = private constant [2 x i8] c\"'\\00\""); // Single quote for Char::debug_string
    emitln(
        "@.str.dq = private constant [2 x i8] c\"\\22\\00\""); // Double quote for Str::debug_string
    emitln("@.str.assert = private constant [18 x i8] c\"assertion failed\\0A\\00\"");
    emitln();

    // digit_pairs lookup table removed — was only used by Text V8-style optimizations

    // Assert implementation
    emitln("define internal void @assert(i1 %cond) {");
    emitln("entry:");
    emitln("    br i1 %cond, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [18 x i8], ptr @.str.assert, i32 0, i32 0");
    emitln("    call i32 @printf(ptr %msg)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for i64
    emitln(
        "@.str.assert_eq = private constant [32 x i8] c\"assert_eq failed: %lld != %lld\\0A\\00\"");
    emitln("define internal void @assert_eq(i64 %a, i64 %b) {");
    emitln("entry:");
    emitln("    %cmp = icmp eq i64 %a, %b");
    emitln("    br i1 %cmp, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [32 x i8], ptr @.str.assert_eq, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, i64 %a, i64 %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for i32
    emitln(
        "@.str.assert_eq_i32 = private constant [28 x i8] c\"assert_eq failed: %d != %d\\0A\\00\"");
    emitln("define internal void @assert_eq_i32(i32 %a, i32 %b) {");
    emitln("entry:");
    emitln("    %cmp = icmp eq i32 %a, %b");
    emitln("    br i1 %cmp, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [28 x i8], ptr @.str.assert_eq_i32, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, i32 %a, i32 %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for strings (ptr) — uses strcmp for content comparison
    emitln("declare i32 @strcmp(ptr, ptr)");
    emitln(
        "@.str.assert_eq_str = private constant [28 x i8] c\"assert_eq failed: %s != %s\\0A\\00\"");
    emitln("define internal void @assert_eq_str(ptr %a, ptr %b) {");
    emitln("entry:");
    emitln("    %cmp = call i32 @strcmp(ptr %a, ptr %b)");
    emitln("    %eq = icmp eq i32 %cmp, 0");
    emitln("    br i1 %eq, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [28 x i8], ptr @.str.assert_eq_str, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, ptr %a, ptr %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Drop functions (no-ops for simple types) - alwaysinline for zero overhead
    emitln("define internal void @drop_Ptr(ptr %p) alwaysinline {");
    emitln("entry:");
    emitln("    ret void");
    emitln("}");
    emitln();

    emitln("define internal void @drop_F64(double %v) alwaysinline {");
    emitln("entry:");
    emitln("    ret void");
    emitln("}");
    emitln();
}

void MirCodegen::emit_type_defs(const mir::Module& module) {
    // Emit struct definitions
    for (const auto& s : module.structs) {
        emit_struct_def(s);
    }

    // Emit enum definitions (local enums)
    for (const auto& e : module.enums) {
        emit_enum_def(e);
    }

    // Emit definitions for imported enums used in EnumInitInst
    // These are enums not defined in the current module but used via imports
    // Note: Use %struct. prefix to be consistent with AST-based codegen
    for (const auto& enum_name : used_enum_types_) {
        std::string type_name = "%struct." + enum_name;
        if (!emitted_types_.count(type_name)) {
            // Emit a simple enum type (just tag) for imported enums
            // Enums without payloads like Ordering use { i32 }
            emitln(type_name + " = type { i32 }");
            emitted_types_.insert(type_name);
        }
    }

    // Emit definitions for generic enum types collected from function signatures
    // These are enums like Maybe[Str], Maybe[I32], Outcome[I32, Str], etc.
    for (const auto& [mangled_name, payload_size] : generic_enum_defs_) {
        std::string type_name = "%struct." + mangled_name;
        if (!emitted_types_.count(type_name)) {
            if (payload_size > 0) {
                emitln(type_name + " = type { i32, [" + std::to_string(payload_size) + " x i8] }");
            } else {
                emitln(type_name + " = type { i32 }");
            }
            emitted_types_.insert(type_name);
        }
    }

    if (!module.structs.empty() || !module.enums.empty() || !used_enum_types_.empty() ||
        !generic_enum_defs_.empty()) {
        emitln();
    }

    // Emit drop functions for struct types (no-ops, just for RAII compatibility)
    // Use alwaysinline for zero overhead
    for (const auto& s : module.structs) {
        std::string type_name = "%struct." + s.name;
        emitln("define internal void @drop_" + s.name + "(" + type_name + " %v) alwaysinline {");
        emitln("entry:");
        emitln("    ret void");
        emitln("}");
        emitln();
    }
}

void MirCodegen::emit_struct_def(const mir::StructDef& s) {
    std::string type_name = "%struct." + s.name;
    if (emitted_types_.count(type_name)) {
        return;
    }
    emitted_types_.insert(type_name);

    // Store field types for later use in struct initialization coercion
    std::vector<std::string> field_types;

    emit(type_name + " = type { ");
    for (size_t i = 0; i < s.fields.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string field_type = mir_type_to_llvm(s.fields[i].type);
        emit(field_type);
        field_types.push_back(field_type);
    }
    emitln(" }");

    struct_field_types_[s.name] = std::move(field_types);
}

void MirCodegen::emit_enum_def(const mir::EnumDef& e) {
    // Enums are represented as tagged unions
    // { i32 tag, [max_payload_size x i8] payload }
    // Use %struct. prefix to be consistent with AST-based codegen
    std::string type_name = "%struct." + e.name;
    if (emitted_types_.count(type_name)) {
        return;
    }
    emitted_types_.insert(type_name);

    // Calculate max payload size
    size_t max_payload_size = 0;
    bool has_payload = false;
    for (const auto& v : e.variants) {
        size_t payload_size = 0;
        for (const auto& t : v.payload_types) {
            has_payload = true;
            // Estimate size based on type
            if (t->is_integer()) {
                payload_size += t->bit_width() / 8;
            } else if (t->is_float()) {
                payload_size += t->bit_width() / 8;
            } else if (t->is_bool()) {
                payload_size += 1;
            } else if (std::holds_alternative<mir::MirPointerType>(t->kind)) {
                payload_size += 8; // 64-bit pointer
            } else if (auto* p = std::get_if<mir::MirPrimitiveType>(&t->kind);
                       p && p->kind == mir::PrimitiveType::Str) {
                payload_size += 8; // String pointer
            } else {
                payload_size += 8; // Default
            }
        }
        max_payload_size = std::max(max_payload_size, payload_size);
    }

    // For simple enums without payloads (like Ordering), use just { i32 }
    if (!has_payload) {
        emitln(type_name + " = type { i32 }");
    } else {
        // Minimum 8 bytes for alignment
        if (max_payload_size < 8) {
            max_payload_size = 8;
        }
        emitln(type_name + " = type { i32, [" + std::to_string(max_payload_size) + " x i8] }");
    }
}

void MirCodegen::emit_function(const mir::Function& func) {
    current_func_ = func.name;
    value_regs_.clear();
    block_labels_.clear();
    block_exit_labels_.clear();
    value_types_.clear();      // Clear type tracking for new function
    bounds_check_counter_ = 0; // Reset per-function (see pre-scan below)

    // Setup block labels - use block ID, not index
    for (const auto& blk : func.blocks) {
        block_labels_[blk.id] = blk.name;
    }

    // Pre-scan: populate block_exit_labels_ before any block is emitted.
    // Bounds check injection in emit_instruction() splits MIR blocks into
    // multiple LLVM blocks (block -> bc.panic.N + bc.ok.N). Phi nodes in
    // earlier blocks reference later blocks by ID, so they need to know the
    // correct exit label (bc.ok.N) before those blocks are processed.
    // We use bounds_check_counter_ (separate from temp_counter_) so the scan
    // predicts labels accurately without simulating all temp_counter_ uses.
    {
        int bc_scan = 0;
        for (const auto& blk : func.blocks) {
            block_exit_labels_[blk.id] = blk.name; // default: entry label
            for (const auto& inst : blk.instructions) {
                std::visit(
                    [&](const auto& i) {
                        using T = std::decay_t<decltype(i)>;
                        if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                            if (i.needs_bounds_check && i.known_array_size >= 0 &&
                                !i.indices.empty()) {
                                // This GEP will emit bc.panic.N + bc.ok.N and update exit label
                                std::string ok = "bc.ok." + std::to_string(bc_scan++);
                                block_exit_labels_[blk.id] = ok;
                            }
                        }
                    },
                    inst.inst);
            }
        }
    }
    // Reset bounds_check_counter_ so actual emission matches the pre-scan values
    bounds_check_counter_ = 0;

    // Find fallback label for missing block targets
    // Prefer first block with a return terminator, otherwise use last block
    fallback_label_.clear();
    for (const auto& blk : func.blocks) {
        if (blk.terminator.has_value()) {
            if (std::holds_alternative<mir::ReturnTerm>(*blk.terminator)) {
                fallback_label_ = blk.name;
                break;
            }
        }
    }
    // If no return block found, use the last block
    if (fallback_label_.empty() && !func.blocks.empty()) {
        fallback_label_ = func.blocks.back().name;
    }

    // Setup parameter registers and track parameter info for indirect calls
    param_info_.clear();
    for (const auto& param : func.params) {
        value_regs_[param.value_id] = "%" + param.name;
        // Also store parameter types for correct type tracking
        if (param.type) {
            value_types_[param.value_id] = mir_type_to_llvm(param.type);
            // Track parameter info for function pointer indirect calls
            param_info_[param.name] = {param.value_id, param.type};
        }
    }

    // Function signature
    // In suite mode (force_internal_linkage), all user-defined functions get
    // internal linkage to prevent duplicate symbol errors when multiple test
    // files in the same suite define the same function names (e.g., main).
    // The test entry wrapper (tml_test_N) is generated separately with
    // dllexport linkage — user's main() is just another internal function.
    std::string linkage = "define";
    if (options_.dll_export && func.is_public) {
        linkage = "define dllexport";
    } else if (options_.force_internal_linkage) {
        linkage = "define internal";
    }

    // Add inline hints for small functions to help LLVM optimizer
    // When coverage is enabled, skip inlining so functions can be instrumented
    std::string inline_attr;
    if (!options_.coverage_enabled) {
        size_t total_instructions = 0;
        for (const auto& blk : func.blocks) {
            total_instructions += blk.instructions.size();
        }

        // Check if this is an iterator method that should always inline
        // Iterator methods are critical for zero-cost abstraction in for loops
        bool is_iterator_method = func.name.find("Iter__next") != std::string::npos ||
                                  func.name.find("__into_iter") != std::string::npos ||
                                  func.name.find("ArrayIter__") != std::string::npos ||
                                  func.name.find("SliceIter__") != std::string::npos ||
                                  func.name.find("Chunks__next") != std::string::npos ||
                                  func.name.find("Windows__next") != std::string::npos ||
                                  func.name.find("ChunksExact__next") != std::string::npos;

        // Small functions (<=10 instructions, single block) get inlinehint
        // drop_ functions and iterator methods get alwaysinline
        if (func.name.rfind("drop_", 0) == 0 || is_iterator_method) {
            inline_attr = " alwaysinline";
        } else if (total_instructions <= 10 && func.blocks.size() <= 2) {
            inline_attr = " inlinehint";
        }
    }

    std::string ret_type = mir_type_to_llvm(func.return_type);
    // Rename `main` based on entry mode:
    // - generate_exe_main: rename to tml_main (C entry wrapper calls it)
    // - test_entry_name: rename to e.g. tml_test_0 (dispatcher calls it)
    std::string emit_name = func.name;
    if (func.name == "main") {
        // Always rename main to tml_main; wrappers provide the public entry name
        if (options_.generate_exe_main || !options_.test_entry_name.empty())
            emit_name = "tml_main";
    }
    emit(linkage + " " + ret_type + " @" + quote_func_name(emit_name) + "(");

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string param_type = mir_type_to_llvm(func.params[i].type);
        const auto& param_name = func.params[i].name;
        // Unit type maps to "void" but LLVM doesn't allow void as a parameter type.
        // Use "{}" (empty struct, zero-sized) as the data representation.
        if (param_type == "void") {
            param_type = "{}";
        }
        // Method 'this'/'self' parameters for struct/enum types must be passed
        // as ptr (pointer), not by value. The function body uses GEP instructions
        // that expect a pointer base, so the parameter type must match.
        if ((param_name == "this" || param_name == "self") &&
            (param_type.starts_with("%struct.") || param_type.starts_with("%enum.") ||
             param_type.starts_with("%class.") || param_type.starts_with("%union."))) {
            param_type = "ptr";
            // Update value_types_ so instructions correctly see this as ptr
            value_types_[func.params[i].value_id] = "ptr";
        }
        // If this function uses sret, the first parameter gets the sret attribute
        if (func.uses_sret && i == 0 && func.original_return_type) {
            std::string orig_ret_type = mir_type_to_llvm(func.original_return_type);
            emit(param_type + " sret(" + orig_ret_type + ") %" + param_name);
        } else {
            emit(param_type + " %" + param_name);
        }
    }

    emitln(")" + inline_attr + " {");

    // Emit basic blocks
    for (const auto& block : func.blocks) {
        emit_block(block);
    }

    emitln("}");
    emitln();
}

void MirCodegen::emit_block(const mir::BasicBlock& block) {
    emitln(block.name + ":");

    // Track current block ID for exit label updates (bounds check injection etc.)
    // NOTE: block_exit_labels_[block.id] was pre-populated in emit_function() pre-scan.
    // We do NOT reset it here to avoid overwriting the pre-scanned exit label.
    current_block_id_ = block.id;

    // Emit instructions
    for (const auto& inst : block.instructions) {
        emit_instruction(inst);
    }

    // Emit terminator
    if (block.terminator.has_value()) {
        emit_terminator(*block.terminator);
    }
}

} // namespace tml::codegen
