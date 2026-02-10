//! # LLVM IR Generator - Module Index
//!
//! This file documents the structure of the LLVM IR code generator.
//! The implementation is split into modular components for maintainability.
//!
//! ## Architecture Overview
//!
//! The generator produces LLVM IR text (.ll format) which is then compiled
//! to native code via clang/LLVM. This approach provides portability and
//! leverages LLVM's optimization passes.
//!
//! ## Module Organization
//
// This file has been split into modular components:
//
// Core utilities (core/):
//   - core/utils.cpp:    Constructor, fresh_reg, emit, emit_line, report_error
//   - core/types.cpp:    Type conversion, mangling, resolve_parser_type_with_subs, unify_types
//   - core/generic.cpp:  Generic instantiation (generate_pending_instantiations)
//   - core/runtime.cpp:  Runtime declarations, module imports, string constants
//   - core/dyn.cpp:      Dynamic dispatch and vtables
//   - core/generate.cpp: Main generate() function, infer_print_type
//
// Expression codegen (expr/):
//   - expr/infer.cpp:       Type inference (infer_expr_type)
//   - expr/struct.cpp:      Struct expressions (gen_struct_expr, gen_field)
//   - expr/print.cpp:       Format print (gen_format_print)
//   - expr/collections.cpp: Arrays and paths (gen_array, gen_index, gen_path)
//   - expr/method.cpp:      Method calls (gen_method_call, gen_call_args)
//
// Builtin codegen (builtins/):
//   - builtins/print.cpp:       Print functions (gen_print_call, gen_println_call)
//   - builtins/math.cpp:        Math functions (sqrt, pow, abs, etc.)
//   - builtins/time.cpp:        Time functions (time_ms, sleep_ms, elapsed_ms)
//   - builtins/memory.cpp:      Memory functions (mem_alloc, mem_copy, mem_eq)
//   - builtins/atomic.cpp:      Atomic counter functions
//   - builtins/sync.cpp:        Sync primitives (mutex, waitgroup, channel)
//   - builtins/string.cpp:      String functions (str_len, str_concat, etc.)
//   - builtins/collections.cpp: List, HashMap, Buffer functions
//   - builtins/io.cpp:          File I/O functions
//
// Other files:
//   - llvm_ir_gen_decl.cpp:    Struct, enum, function declarations
//   - llvm_ir_gen_stmt.cpp:    Let statements, expression statements
//   - llvm_ir_gen_expr.cpp:    Literals, identifiers, binary/unary ops
//   - llvm_ir_gen_control.cpp: If, block, loop, while, for, return
//   - llvm_ir_gen_builtins.cpp: Builtin function call dispatcher
//
// All implementation is now in the modular files listed above.
// This file serves as documentation of the module structure.
