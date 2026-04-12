# Legacy LLVM Codegen — Retirement Log

This file tracks which C++ legacy codegen files have been retired (coverage
proven by MIR codegen path) vs which need TML ports.

## Retirement Categories

### RETIRE: Covered by MIR codegen path (phases 16a–16c)

These files implement AST-level codegen patterns that the MIR pipeline handles
at the MIR instruction level. They don't need TML equivalents because the
THIR→MIR builder generates the correct MIR instructions, and phases 16a–16c
emit the corresponding LLVM IR.

| C++ File | Covered By | Notes |
|----------|-----------|-------|
| `llvm/expr/binary.cpp` | 16b emit_inst Binary | Int/float arithmetic, comparison |
| `llvm/expr/binary_ops.cpp` | 16b emit_inst Binary | Operator overloading dispatch |
| `llvm/expr/unary.cpp` | 16b emit_inst Unary | Negation, not, bitwise not |
| `llvm/expr/cast.cpp` | 16b emit_inst Cast | sext/trunc/fpext/sitofp/etc |
| `llvm/expr/struct_field.cpp` | 16b emit_inst GEP | Struct field access via GEP |
| `llvm/expr/tuple.cpp` | 16b emit_inst TupleInit/ExtractValue | Tuple construction and access |
| `llvm/expr/llvm_struct_expr.cpp` | 16b emit_inst StructInit | Struct construction via insertvalue |
| `llvm/expr/call.cpp` | 16c emit_call | Direct function calls |
| `llvm/expr/call_user.cpp` | 16c emit_call | User-defined function calls |
| `llvm/expr/call_primitive.cpp` | 16c emit_call | Primitive method calls |
| `llvm/expr/call_generic_func.cpp` | 16c emit_generic | Generic function instantiation |
| `llvm/expr/call_generic_struct.cpp` | 16c emit_generic | Generic struct method calls |
| `llvm/expr/call_indirect.cpp` | 16c emit_method vtable | Indirect/virtual calls |
| `llvm/expr/method.cpp` | 16c emit_method | Method dispatch entry point |
| `llvm/expr/method_impl.cpp` | 16c emit_method inherent | Inherent impl dispatch |
| `llvm/expr/method_impl_module.cpp` | 16c emit_method | Module-level method resolution |
| `llvm/expr/method_static.cpp` | 16c emit_method inherent | Static method calls |
| `llvm/expr/method_static_dispatch.cpp` | 16c emit_method | Static dispatch resolution |
| `llvm/expr/method_dyn.cpp` | 16c emit_method vtable | Dynamic dispatch |
| `llvm/expr/method_generic.cpp` | 16c emit_generic | Generic method instantiation |
| `llvm/expr/method_maybe.cpp` | 16c emit_call try_operator | Maybe unwrap/map |
| `llvm/expr/method_outcome.cpp` | 16c emit_call try_operator | Outcome unwrap/? |
| `llvm/expr/method_primitive.cpp` | 16c emit_method inherent | Primitive type methods |
| `llvm/expr/method_primitive_ext.cpp` | 16c emit_method | Extended primitive methods |
| `llvm/expr/method_prim_behavior.cpp` | 16c emit_method behavior | Primitive behavior impls |
| `llvm/expr/method_collection.cpp` | 16c emit_method inherent | List/HashMap methods |
| `llvm/expr/method_array.cpp` | 16c emit_method inherent | Array methods |
| `llvm/expr/method_slice.cpp` | 16c emit_method inherent | Slice methods |
| `llvm/expr/method_class.cpp` | 16c emit_method inherent | Class methods |
| `llvm/expr/try.cpp` | 16c emit_call try_operator | Try operator (?) |
| `llvm/expr/print.cpp` | 16c emit_call | Print/println calls |
| `llvm/expr/closure.cpp` | 16b emit_inst ClosureInit | Closure construction |
| `llvm/expr/await.cpp` | 16b emit_inst Await | Async/await patterns |
| `llvm/expr/collections.cpp` | 16c emit_call | Collection constructor calls |
| `llvm/expr/core.cpp` | 16b emit_inst (various) | Expression codegen dispatch |
| `llvm/expr/infer.cpp` | N/A | Type inference (not codegen) |
| `llvm/expr/infer_methods.cpp` | N/A | Method inference (not codegen) |
| `llvm/expr/infer_types.cpp` | N/A | Type inference (not codegen) |
| `llvm/expr/call_class.cpp` | 16c emit_method | Class constructor calls |
| `llvm/expr/call_enum.cpp` | 16b emit_inst EnumInit | Enum variant construction |
| `llvm/control/if.cpp` | 16b emit_terminator CondBranch | If/else control flow |
| `llvm/control/loop.cpp` | 16b emit_terminator Branch | Loop control flow |
| `llvm/control/return.cpp` | 16b emit_terminator Return | Return statements |
| `llvm/control/when.cpp` | 16b emit_terminator Switch | Pattern matching |
| `llvm/core/llvm_types.cpp` | 16a emit_type | Type emission |
| `llvm/core/types_resolve.cpp` | 16a emit_type | Type resolution |
| `llvm/core/generic.cpp` | 16c emit_generic | Generic infrastructure |
| `llvm/core/generic_instantiate.cpp` | 16c emit_generic | Generic instantiation |
| `llvm/core/generic_instantiate_impl.cpp` | 16c emit_generic | Generic impl instantiation |
| `llvm/decl/func.cpp` | 16a emit_func | Function declarations |
| `llvm/decl/enum.cpp` | 16a emit_type enum | Enum type declarations |
| `llvm/decl/llvm_struct_decl.cpp` | 16a layout | Struct declarations |
| `llvm/decl/impl.cpp` | 16c emit_method | Impl block declarations |
| `llvm/llvm_ir_gen.cpp` | N/A | IR gen orchestration |
| `llvm/llvm_ir_gen_expr.cpp` | 16b emit_inst | Expression codegen |
| `llvm/llvm_ir_gen_stmt.cpp` | 16b emit_inst | Statement codegen |
| `llvm/llvm_codegen_backend.cpp` | N/A | Backend integration |
| `mir/instructions.cpp` | 16b emit_inst | MIR instruction emission |
| `mir/instructions_call.cpp` | 16c emit_call | MIR call emission |
| `mir/instructions_method.cpp` | 16c emit_method | MIR method emission |
| `mir/instructions_misc.cpp` | 16b emit_inst | Misc instruction emission |
| `mir/mir_types.cpp` | 16a emit_type | MIR type mapping |
| `mir/terminators.cpp` | 16b emit_terminator | Terminator emission |
| `mir/codegen_helpers.cpp` | 16a/16b helpers | Codegen utilities |
| `mir_codegen.cpp` | 16a emit_module | MIR codegen entry point |

### PORT: Needs TML equivalent (phases 16d.2–16d.6)

| C++ File | TML Target | Notes |
|----------|-----------|-------|
| `llvm/builtins/intrinsics.cpp` | emit_intrinsic.tml | LLVM intrinsic calls |
| `llvm/builtins/intrinsics_extended.cpp` | emit_intrinsic.tml | Extended intrinsics |
| `llvm/builtins/math.cpp` | emit_intrinsic.tml | Math intrinsics |
| `llvm/builtins/mem.cpp` | emit_intrinsic.tml | Memory intrinsics |
| `llvm/builtins/string.cpp` | emit_intrinsic.tml | String builtins |
| `llvm/builtins/atomic.cpp` | emit_intrinsic.tml | Atomic ops (partially in 16b) |
| `llvm/builtins/io.cpp` | emit_intrinsic.tml | I/O builtins |
| `llvm/builtins/sync.cpp` | emit_intrinsic.tml | Sync primitives |
| `llvm/builtins/time.cpp` | emit_intrinsic.tml | Time builtins |
| `llvm/builtins/async.cpp` | emit_intrinsic.tml | Async runtime |
| `llvm/builtins/assert.cpp` | emit_intrinsic.tml | Assert builtin |
| `llvm/builtins/intrinsics_simd_*.cpp` | emit_intrinsic.tml | SIMD intrinsics |
| `llvm/builtins/intrinsics_extended_*.cpp` | emit_intrinsic.tml | Reflection/dyncall |
| `llvm/core/drop.cpp` | emit_drop.tml | Drop glue |
| `llvm/core/dyn.cpp` | emit_method.tml (vtable) | Vtable emission (partially in 16c) |
| `llvm/derive/*.cpp` (11 files) | emit_derive.tml | Auto-derived impls |
| `llvm/llvm_ir_gen_stmt_let.cpp` | emit_let.tml | Let pattern codegen |
| `llvm/core/runtime.cpp` | runtime_decls.tml | Runtime declarations |
| `llvm/core/runtime_modules*.cpp` | runtime_decls.tml | Module runtime decls |

### DEFER: Low priority / experimental

| C++ File | Reason |
|----------|--------|
| `cranelift/cranelift_codegen_backend.cpp` | Experimental backend, not in CI |
| `llvm/core/debug_info.cpp` | Debug info emission (phase 21a) |
| `llvm/core/optimization_passes.cpp` | LLVM optimization pipeline config |
| `llvm/core/target.cpp` | Target machine configuration |
| `llvm/core/class_codegen*.cpp` | Class (OOP) codegen — rare in stdlib |
| `llvm/core/generate*.cpp` | Generation orchestration — stays in C++ |
| `codegen_backend.cpp` | Backend abstraction layer |
| `codegen_partitioner.cpp` | Code splitting — not needed for TML path |
| `c_header_gen.cpp` | C header generation — separate tool |
| `ir_emitter.cpp` | IR emitter abstraction |
| `abi.cpp` | ABI utilities (partially in 16c) |
| `cg_value.cpp` | Codegen value tracking |
| `intrinsic_table.cpp` | Intrinsic lookup table |
