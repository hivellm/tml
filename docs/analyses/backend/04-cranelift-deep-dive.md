# Cranelift Backend — Deep Dive Analysis

**Date**: 2026-04-05
**Scope**: Complete analysis of TML's Cranelift backend: current implementation, architecture,
MIR mapping, missing features, integration path, and effort estimate
**Source**: `compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp`,
`compiler/include/backend/cranelift_bridge.h`, Cranelift 0.110+ internals
**Purpose**: Inform Phase 6.3 (faster debug builds via Cranelift)

---

## 1. Current Implementation Status

TML has a partial Cranelift backend integrated but not yet production-ready. The implementation
spans three files:

| File | Role | Status |
|------|------|--------|
| `compiler/include/backend/cranelift_bridge.h` | C ABI between C++ and Rust Cranelift library | Complete |
| `compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp` | C++ wrapper implementing `ICodegenBackend` | Complete |
| Rust bridge library (not in C++ tree, separate crate) | Deserializes TML MIR binary, drives Cranelift | Partial |

### 1.1 What Is Already Implemented

The C++ side of the backend is complete. It correctly:

- Implements the `ICodegenBackend` interface via `CraneliftCodegenBackend`
- Serializes `mir::Module` to binary via `mir::serialize_binary()`
- Passes the binary blob to the Rust Cranelift bridge via C FFI
- Handles CGU (codegen unit) mode — compiling a subset of functions
- Writes resulting object bytes to a temp file atomically (thread-safe, unique filenames)
- Implements `generate_ir()` to emit Cranelift IR text without compilation
- Exposes `capabilities()` showing what the backend supports

The reported capabilities are:

```cpp
BackendCapabilities{
    .supports_mir = true,       // Can compile from MIR (not AST)
    .supports_ast = false,      // AST path not implemented
    .supports_generics = false, // Generic type instantiation not done
    .supports_debug_info = false, // No DWARF emission yet
    .supports_coverage = false,  // No coverage instrumentation
    .supports_cgu = true,        // CGU (parallel compilation units) works
    .max_optimization_level = 2, // Cranelift speed_and_size levels
}
```

### 1.2 What Is NOT Implemented

The Rust bridge library (the actual Cranelift integration) is partial. Based on the C ABI
header, it accepts serialized MIR and must:

1. Deserialize TML MIR binary format
2. Translate TML MIR instructions to Cranelift CLIF instructions
3. Handle type mapping (TML `I64` → Cranelift `i64`, `F64` → `f64`, structs → memory)
4. Emit object files via `cranelift-object`

The capabilities flags tell the story: `supports_generics = false` and `supports_debug_info = false`
mean that any TML code using generics (the entire standard library) cannot yet be compiled
by Cranelift. This rules out using Cranelift for the standard library or realistic programs.

### 1.3 Why This Matters

Cranelift is the planned backend for **fast debug builds** (Phase 6.3). LLVM with `-O0`
takes roughly 200-400ms per file due to startup and IR parsing overhead. Cranelift compiles
in 20-50ms per file — a 5-10x speedup. For iterative development (edit-compile-test loops),
this is the difference between a 2-second and 20-second cycle.

The gap to close: implement full generic type support and cover the MIR instruction set
completely. Debug info can be deferred until the correctness story is solid.

---

## 2. Cranelift Architecture Overview

Cranelift is a production-quality compiler backend maintained by the Bytecode Alliance,
originally developed for WebAssembly compilation in SpiderMonkey and now the primary
backend for Wasmtime (a WebAssembly runtime). It is written entirely in Rust.

### 2.1 Component Map

```
TML MIR (binary)
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│                  cranelift-frontend                      │
│  FunctionBuilder — construct CLIF values, blocks, insts │
│  Variable — SSA variable tracking (handles phi nodes)   │
│  InstBuilder — per-instruction construction helpers     │
└─────────────────────────────────────────────────────────┘
    │ CLIF (Cranelift Intermediate Format)
    ▼
┌─────────────────────────────────────────────────────────┐
│               cranelift-codegen (core)                   │
│  ISLE patterns — instruction selection rules             │
│  regalloc2 — backtracking register allocator            │
│  MachBuffer — machine instruction buffer                 │
│  MachInst — abstract machine instruction                 │
└─────────────────────────────────────────────────────────┘
    │ Target-specific MachInst variants
    ▼
┌─────────────────────────────────────────────────────────┐
│              cranelift-codegen/isa/x64                   │
│  X64Inst — concrete x86_64 instruction variants         │
│  X64Encoding — REX/VEX/ModRM/SIB byte encoding          │
│  X64ABICaller/X64ABICallee — calling convention impl    │
└─────────────────────────────────────────────────────────┘
    │ raw bytes
    ▼
┌─────────────────────────────────────────────────────────┐
│               cranelift-object                           │
│  ObjectProduct — wraps `object` crate (Gimli project)   │
│  add_function / define_function_bytes                   │
│  write_stream → COFF / ELF / Mach-O object bytes        │
└─────────────────────────────────────────────────────────┘
```

### 2.2 CLIF (Cranelift Intermediate Format)

CLIF is Cranelift's SSA IR. It is structurally similar to LLVM IR but significantly simpler:

- **Values** are typed SSA values (`v0: i64`, `v1: f32`, etc.)
- **Blocks** are basic blocks with explicit parameter lists (instead of phi nodes)
- **Instructions** produce zero or more result values
- **Types**: `i8`, `i16`, `i32`, `i64`, `i128`, `f32`, `f64`, `b1` (bool), vector types
- **No implicit side effects**: all memory operations are explicit `load`/`store`

Example CLIF for a simple function:

```
function %add_and_compare(i64, i64) -> b1 system_v {
block0(v0: i64, v1: i64):
    v2 = iadd v0, v1
    v3 = iconst.i64 0
    v4 = icmp sgt v2, v3
    return v4
}
```

Key differences from LLVM IR:
1. **Block parameters instead of phi nodes**: CLIF blocks take explicit arguments, so
   values from predecessor blocks are passed explicitly. No need for phi nodes.
2. **No named types**: All types are primitives or vectors. Structs are decomposed into
   multiple values or accessed via `load`/`store` with offsets.
3. **No module-level globals**: All data is passed via pointers from the caller.
4. **Explicit calling conventions**: `system_v`, `windows_fastcall`, `baldrdash_2020`, etc.

### 2.3 Instruction Selection — ISLE

Cranelift uses ISLE (Instruction Selection/Lowering Engine) — a domain-specific language
for pattern matching CLIF instructions to machine instructions. ISLE rules look like:

```lisp
;; Lower iadd i64 to x86_64 ADD
(rule (lower (iadd (fits_in_64 ty) x y))
      (x64_alu_rmi_r (OperandSize.Size64) (AluRmiROpcode.Add)
                     (put_in_reg x)
                     (RegMemImm.reg (put_in_reg y))))
```

There are ~3,000 ISLE rules for x86_64 and ~2,000 for AArch64. The ISLE compiler generates
Rust match code that selects the optimal instruction sequence for each CLIF pattern.

For TML's purposes, ISLE is opaque — it runs inside Cranelift and TML never interacts with it.

### 2.4 Register Allocation — regalloc2

Cranelift uses `regalloc2`, a backtracking register allocator. It processes the linear
order of instructions in a function and assigns physical registers to virtual registers.

Key characteristics:
- **Backtracking**: Can undo earlier decisions if a later constraint cannot be satisfied
- **Speed**: ~3-5x faster than graph-coloring allocators for typical functions
- **Quality**: Near-optimal for most patterns (comparable to LLVM's greedy allocator)
- **Spilling**: Automatically inserts load/store instructions for values that don't fit
  in registers

For TML, this means: write correct CLIF, and Cranelift handles all register allocation
automatically. TML's MIR-to-CLIF translation never assigns physical registers.

### 2.5 Object Output — cranelift-object

`cranelift-object` wraps the `object` crate (from the Gimli project) to write object files
in COFF, ELF, or Mach-O format. From Cranelift's perspective:

```rust
let mut module = ObjectModule::new(
    ObjectBuilder::new(
        isa,                              // x86_64 or aarch64
        "tml_output",                     // module name
        cranelift_module::default_libcall_names()
    )?
);

// For each function:
let func_id = module.declare_function("func_name", Linkage::Export, &sig)?;
module.define_function(func_id, &mut context)?;

// Finalize
let product = module.finish();
let object_bytes = product.emit()?;  // COFF on Windows, ELF on Linux
```

The resulting bytes are what `cranelift_compile_mir` returns via the C bridge.

---

## 3. MIR-to-CLIF Instruction Mapping

TML's MIR is SSA-form with basic blocks, typed values, and explicit terminators. CLIF is
also SSA-form. The mapping is mostly 1:1 for scalar operations. The complexity is in
aggregate types (structs, enums) and control flow.

### 3.1 Scalar Operations

| TML MIR Instruction | CLIF Equivalent | Notes |
|---------------------|-----------------|-------|
| `Assign(v, Const(42i64))` | `iconst.i64 42` | Direct constant |
| `Assign(v, Const(3.14f64))` | `f64const 0x400921fb` | Float as bit pattern |
| `Assign(v, Add(a, b))` — i64 | `iadd v_a, v_b` | No overflow check |
| `Assign(v, Add(a, b))` — i32 | `iadd.i32 v_a, v_b` | |
| `Assign(v, Sub(a, b))` | `isub v_a, v_b` | |
| `Assign(v, Mul(a, b))` | `imul v_a, v_b` | |
| `Assign(v, Div(a, b))` — signed | `sdiv v_a, v_b` | Traps on /0 |
| `Assign(v, Div(a, b))` — unsigned | `udiv v_a, v_b` | |
| `Assign(v, Rem(a, b))` — signed | `srem v_a, v_b` | |
| `Assign(v, FAdd(a, b))` | `fadd v_a, v_b` | |
| `Assign(v, FMul(a, b))` | `fmul v_a, v_b` | |
| `Assign(v, Neg(a))` — int | `ineg v_a` | |
| `Assign(v, Neg(a))` — float | `fneg v_a` | |
| `Assign(v, BitAnd(a, b))` | `band v_a, v_b` | |
| `Assign(v, BitOr(a, b))` | `bor v_a, v_b` | |
| `Assign(v, BitXor(a, b))` | `bxor v_a, v_b` | |
| `Assign(v, Shl(a, b))` | `ishl v_a, v_b` | |
| `Assign(v, Shr(a, b))` — signed | `sshr v_a, v_b` | |
| `Assign(v, Shr(a, b))` — unsigned | `ushr v_a, v_b` | |
| `Assign(v, Eq(a, b))` — int | `icmp eq v_a, v_b` → `bint` | i1 → i64 |
| `Assign(v, Lt(a, b))` — signed | `icmp slt v_a, v_b` | |
| `Assign(v, Lt(a, b))` — unsigned | `icmp ult v_a, v_b` | |

### 3.2 Memory Operations

| TML MIR Instruction | CLIF Equivalent | Notes |
|---------------------|-----------------|-------|
| `Alloca(v, ty)` | `stack_slot size align` + `stack_addr` | Returns pointer to stack slot |
| `Load(v, ptr)` | `load.TYPE flags ptr, 0` | Cranelift: explicit offset |
| `Store(ptr, val)` | `store flags val, ptr, 0` | |
| `GEP(v, base, offset)` — const | `iadd_imm base, offset_bytes` | |
| `GEP(v, base, index, stride)` | `imul_imm index, stride` → `iadd base, product` | Two instructions |

### 3.3 Control Flow

| TML MIR Terminator | CLIF Equivalent | Notes |
|--------------------|-----------------|-------|
| `Jump(block)` | `jump block()` | Unconditional branch |
| `Branch(cond, then_block, else_block)` | `brif cond, then_block(), else_block()` | |
| `Return(val)` | `return val` | |
| `Return(void)` | `return` | |
| `Unreachable` | `trap user0` | Cranelift: named trap code |
| `Switch(val, cases, default)` | `br_table val, default, [case...]` | For integer switch |

### 3.4 Function Calls

| TML MIR Instruction | CLIF Equivalent | Notes |
|--------------------|-----------------|-------|
| `Call(func, args)` — direct | `call func_ref(args...)` | func_ref from declare_function |
| `Call(func_ptr, args)` — indirect | `call_indirect sig_ref, func_ptr(args...)` | Needs sig_ref declared |
| `Call(extern_c_func, args)` | `call extern_func_ref(args...)` | extern declared via declare_func_in_data_segment |

Calling conventions are set at the function declaration level:
- `system_v` — SysV AMD64 (Linux, macOS)
- `windows_fastcall` — Windows x64 calling convention

TML uses Windows x64 convention on Windows and SysV on Linux — both are directly supported
by Cranelift.

### 3.5 Type Casting

| TML MIR Cast | CLIF Equivalent |
|--------------|-----------------|
| `I32 → I64` (sign extend) | `sextend.i64 v` |
| `I32 → I64` (zero extend) | `uextend.i64 v` |
| `I64 → I32` (truncate) | `ireduce.i32 v` |
| `I64 → F64` | `fcvt_from_sint.f64 v` |
| `F64 → I64` | `fcvt_to_sint_sat.i64 v` |
| `F32 → F64` | `fpromote.f64 v` |
| `F64 → F32` | `fdemote.f32 v` |
| `ptr → I64` | `bitcast.i64 v` |
| `I64 → ptr` | `bitcast.r64 v` (Cranelift reference type) |

### 3.6 Aggregate Types — The Hard Part

TML structs do not have a direct CLIF representation. CLIF only has scalar types and vectors.
This means every struct access becomes a sequence of loads and stores.

**Approach A: Memory layout (current Cranelift practice)**

All structs are passed and returned via pointers. Struct fields are accessed with explicit
offset calculations.

```
TML: let x = point.x   (where point: Point{x: I64, y: I64})

CLIF:
  ;; point is a pointer (i64 treated as pointer)
  v_x = load.i64 notrap aligned point, 0   ;; offset 0 = field x
```

**Approach B: Multiple return values (CLIF-specific advantage)**

CLIF natively supports functions returning multiple values. A small struct like `{i64, i64}`
can be returned as two values instead of via sret convention. This is better than LLVM for
small structs and reduces memory traffic.

```
;; Return struct {x: I64, y: I64} as two values
function %make_point(i64, i64) -> i64, i64 system_v {
block0(v_x: i64, v_y: i64):
    return v_x, v_y
}
```

For TML: structs with 2-3 fields of scalar types should use multiple return values. Larger
structs use the memory/pointer approach.

### 3.7 Enums (Tagged Unions)

TML enums with data (e.g., `Maybe[T]`) are represented in memory as a discriminant + payload.
In CLIF, reading an enum variant requires:
1. Load the discriminant (first field)
2. Compare discriminant value
3. If match, load payload fields at their offsets
4. Branch to the appropriate arm of a `when` expression

This is straightforward but verbose — each pattern match arm becomes a small CLIF block.

---

## 4. What Is Missing for Full TML Support

The `supports_generics = false` capability flag is the primary blocker. Here is the complete
list of missing features, ordered by impact:

### 4.1 Generic Type Instantiation (CRITICAL)

TML's generics are monomorphized during HIR lowering — by the time MIR is produced,
every generic instantiation is a concrete type. So `List[I32]` in MIR is just a struct
`{ptr: RawPtr, len: I64, cap: I64}` — no generic parameters remain.

The Cranelift bridge's `supports_generics = false` likely refers to one of:
- The Rust bridge not yet handling all struct type layouts correctly
- Pointer types in generic positions not being mapped to Cranelift's pointer types
- Multiple instantiations of the same function not being handled (duplicate CLIF function names)

Since MIR is already monomorphized, this is a bridge implementation gap, not a fundamental
architectural limitation. Estimated effort to close: 2-4 weeks.

### 4.2 Debug Information — DWARF/CodeView (NOT IMPLEMENTED)

The `supports_debug_info = false` flag means Cranelift-compiled binaries have no debug info.
Stack traces show function addresses, not names. Debuggers cannot step through source.

Cranelift does support DWARF emission via `cranelift-object` + `gimli` (the DWARF library).
The `cranelift-debug` crate provides DIE (Debug Info Entry) generation. However:
- `cranelift-debug` is experimental
- CodeView/PDB (Windows debugger format) is not yet supported in Cranelift's object writer
- DWARF on Windows is supported but Windows debuggers (WinDbg, VS) prefer PDB

For development builds, the priority is: correct output first, debug info later.
Estimated effort for basic DWARF: 3-4 weeks. PDB: 3+ months (see document 05).

### 4.3 Exception Handling / Panic Unwinding (NOT IMPLEMENTED)

TML's `panic()` currently unwinds the stack via Rust-style panic handling (which calls
`abort()` at the OS level). True unwinding (for `defer` semantics, if TML adds them) would
require:
- On Linux: DWARF-based unwinding via `.eh_frame` section
- On Windows: SEH (Structured Exception Handling) via `.pdata` and `.xdata` sections

Cranelift has basic support for `eh_frame` generation but not full SEH. For development
builds this is acceptable — panics abort cleanly. For production builds this must be
addressed before shipping Cranelift-compiled code to users.

### 4.4 Coverage Instrumentation (NOT IMPLEMENTED)

The `supports_coverage = false` flag means Cranelift-compiled test suites cannot collect
coverage data. This is a testing infrastructure gap, not a user-facing problem. TML's test
framework uses a custom coverage mechanism (not LLVM's `__llvm_profile_*`), so adding
Cranelift coverage support requires adding TML's custom instrumentation hooks to the CLIF
translation layer.

### 4.5 SIMD Intrinsics (PARTIAL)

Cranelift supports SIMD instructions via its vector type system (`i8x16`, `i32x4`, `f64x2`,
`i64x4`). However, TML's SIMD API maps to AVX2/SSE4.2 intrinsics that may not all have
direct CLIF equivalents.

Cranelift-supported SIMD operations:
- Integer arithmetic: `iadd`, `imul`, `ishl`, etc. on vector types
- Float arithmetic: `fadd`, `fmul`, etc. on `f32x4`, `f64x2`
- Shuffles: `shuffle`, `swizzle`
- Comparison: `icmp` on vectors
- Load/store: `vload`, `vstore`

Not directly supported (require lowering to x86 intrinsics via ISLE rules):
- `_mm256_*` AVX2 intrinsics beyond basic arithmetic
- AVX-512 instructions
- Crypto intrinsics (AES-NI, SHA-NI)

For TML programs that use `std::simd`, the Cranelift backend may produce slower code than
LLVM for SIMD-heavy operations. This is acceptable for development builds.

---

## 5. Integration Path Options

There are three ways to integrate MIR with Cranelift. The current implementation uses
Option C (binary serialize). Here is a complete evaluation.

### 5.1 Option A: MIR → CLIF Text → Cranelift Parse → Compile

Generate CLIF text from MIR (like the LLVM path generates LLVM IR text), then let Cranelift
parse it via `cranelift_reader::parse_functions()`.

```
TML MIR → std::string CLIF text → cranelift_reader → cranelift_codegen → object
```

**Pros**:
- Easy to debug (CLIF text is human-readable, like LLVM IR)
- Can use `generate_ir()` mode that already exists in the bridge
- No serialization format to maintain

**Cons**:
- Parsing CLIF text is an extra step (~5-10ms per function)
- String allocation overhead
- CLIF text format is not considered stable API by Cranelift maintainers

### 5.2 Option B: MIR → Cranelift Builder API Directly

In the Rust bridge, deserialize TML MIR binary and call `FunctionBuilder` directly to
construct CLIF values and instructions. No intermediate text representation.

```
TML MIR binary → Rust deserialization → FunctionBuilder API → cranelift_codegen → object
```

**Pros**:
- Fastest path (no parse overhead)
- Most correct (builder API validates as it builds)
- Better integration with Cranelift's type system

**Cons**:
- Most implementation work (must write Rust code that translates each MIR instruction)
- Changes to TML MIR require changes to the Rust bridge

This is the recommended long-term approach. The `FunctionBuilder` API is:

```rust
let mut builder = FunctionBuilder::new(&mut ctx.func, &mut func_ctx);
let block0 = builder.create_block();
builder.append_block_params_for_function_params(block0);
builder.switch_to_block(block0);
builder.seal_block(block0);

let v0 = builder.block_params(block0)[0];
let v1 = builder.ins().iconst(types::I64, 42);
let v2 = builder.ins().iadd(v0, v1);
builder.ins().return_(&[v2]);
builder.finalize();
```

### 5.3 Option C: MIR → Binary Serialize → Cranelift C API (Current)

Current approach: serialize TML MIR to a binary format using `mir::serialize_binary()`,
pass the bytes across the C FFI to the Rust bridge, which deserializes and compiles.

```
TML MIR → binary bytes → C FFI boundary → Rust deserialization → cranelift_codegen → object bytes → C FFI → temp .obj file
```

**Pros**:
- Clean boundary between C++ compiler and Rust Cranelift library
- Easy to test (can serialize MIR to file, inspect, pass to bridge separately)
- ABI-stable (binary format can be versioned)

**Cons**:
- Extra serialization/deserialization step
- Binary format must be kept in sync between C++ serializer and Rust deserializer
- Two copies of MIR data in memory simultaneously

### 5.4 Recommendation

**Option B for new code, Option C as the current stepping stone.**

The current Option C architecture is correct for getting the backend working. Once the
bridge is feature-complete (generics, full instruction set), migrate to Option B for
performance. The migration is internal to the Rust bridge library — the C API surface
(`cranelift_compile_mir`, `cranelift_compile_mir_cgu`) stays the same.

Option A (CLIF text) should be used for debugging only — the `generate_ir()` mode that
already exists serves this purpose.

---

## 6. Performance Characteristics

### 6.1 Compilation Speed

Cranelift's primary advantage is compilation speed at `O0` (no optimization). For typical
TML source files:

| Backend | Compilation time (100-function file) | Notes |
|---------|-------------------------------------|-------|
| LLVM O0 | ~200-400ms | IR parsing + O0 pipeline overhead |
| LLVM O2 | ~800ms-2s | Full optimization pipeline |
| Cranelift O0 | ~20-50ms | No optimization, fast register allocation |
| Cranelift O1 | ~40-80ms | speed_and_size level 1 |

The 5-10x speedup over LLVM O0 comes from:
- No IR parsing (Cranelift uses in-memory builder)
- Simpler register allocation (regalloc2 is very fast)
- Single-pass instruction selection via ISLE
- No global analysis passes (each function is independent)

Startup overhead is ~1ms (no LLVM context initialization, no target machine setup that
takes 50-100ms for LLVM). This makes Cranelift particularly good for projects with many
small source files.

### 6.2 Code Quality

Cranelift's code quality relative to LLVM:

| Code Pattern | Cranelift vs LLVM O1 | Notes |
|--------------|---------------------|-------|
| Integer arithmetic | ~90-95% | Near-optimal register use |
| Function calls | ~85-90% | No inlining at O0/O1 |
| Loop-heavy code | ~80-85% | No loop unrolling or vectorization |
| SIMD code | ~70-80% | Limited AVX2 pattern coverage |
| Branch-heavy code | ~85-90% | No branch prediction hints |
| Memory access patterns | ~85-90% | No alias analysis, no LICM |

For development builds (the target use case), 80-90% of LLVM O1 quality is acceptable.
The developer sees correct behavior, near-debug performance, and 5-10x faster builds.

### 6.3 Binary Size

Cranelift O0 produces slightly larger binaries than LLVM O0 due to:
- Less aggressive dead code elimination (Cranelift works per-function)
- No `alloca` promotion (all stack values stay on stack)
- Less constant folding across function boundaries

Typical increase: 5-15% larger binary vs LLVM O0. Not relevant for development builds.

---

## 7. Effort Estimate

To bring the Cranelift backend to production quality for development builds:

| Task | LOC Estimate | Duration | Priority |
|------|-------------|----------|----------|
| Complete MIR instruction mapping (all scalar ops) | 500 Rust | 2-3 weeks | CRITICAL |
| Struct/aggregate type layout (memory model) | 400 Rust | 2-3 weeks | CRITICAL |
| Generic type instantiation fix | 300 Rust | 1-2 weeks | CRITICAL |
| Function call translation (direct + indirect) | 300 Rust | 1-2 weeks | CRITICAL |
| Calling convention (Windows x64 + SysV) | 200 Rust | 1 week | CRITICAL |
| Switch/match translation (br_table) | 200 Rust | 1 week | HIGH |
| Extern function declarations (runtime calls) | 200 Rust | 1 week | CRITICAL |
| Test harness integration (verify object correctness) | 300 Rust | 1 week | HIGH |
| Basic DWARF debug info | 600 Rust | 3-4 weeks | MEDIUM |
| Coverage instrumentation hooks | 200 Rust | 1 week | LOW |
| SIMD intrinsics (AVX2 subset) | 400 Rust | 2-3 weeks | LOW |
| **Total (without DWARF/coverage/SIMD)** | **~2,400** | **~2-3 months** | |
| **Total (with DWARF, without SIMD)** | **~3,000** | **~3-4 months** | |

### 7.1 Implementation Sequence

The correct sequence to reach "Cranelift works for realistic TML programs":

1. **Week 1-2**: Fix generic struct type layouts. Start with the simplest case: structs
   with all-scalar fields. Verify that `List[I64]`, `HashMap[Str, I64]`, etc. produce
   correct CLIF type sequences.

2. **Week 2-3**: Complete the scalar instruction set. Map every MIR instruction variant
   to the correct CLIF instruction. Run TML's arithmetic test suite against Cranelift output.

3. **Week 3-4**: Fix calling conventions. Verify that functions with multiple arguments
   (especially mixed scalar/pointer), return values, and C FFI calls produce correct
   machine code. This is the highest-risk area (ABI bugs cause silent wrong results).

4. **Week 4-6**: Function calls to C runtime. Every TML program needs to call `essential.c`
   functions (print, panic, alloc). Verify these calls are generated correctly.

5. **Week 6-8**: Run the TML standard library test suite against Cranelift-compiled output.
   Identify and fix failures systematically.

6. **Week 8-12**: Add basic DWARF debug info (line numbers, function names). Not required
   for correctness but improves debuggability of Cranelift-compiled programs.

---

## 8. Risk Assessment

| Risk | Severity | Probability | Mitigation |
|------|----------|-------------|-----------|
| ABI mismatch on Windows x64 (struct passing) | HIGH | MEDIUM (40%) | Exhaustive test against LLVM output; compare with known-good calling convention tests |
| Cranelift version instability (API changes) | MEDIUM | LOW (20%) | Pin Cranelift version in Cargo.lock; upgrade deliberately |
| CLIF feature gaps (missing instruction for TML MIR op) | MEDIUM | LOW (15%) | All needed CLIF instructions exist; gap is in the bridge implementation, not Cranelift |
| Binary format desync (TML MIR serializer vs Rust deserializer) | MEDIUM | MEDIUM (35%) | Add version field + magic bytes to MIR binary format; fail loudly on mismatch |
| Cranelift code correctness bugs | LOW | LOW (10%) | Cranelift is production-tested in Wasmtime (compiled billions of Wasm functions); ABI bugs in TML's bridge are more likely than Cranelift bugs |
| Performance of Cranelift output (too slow for development) | LOW | LOW (10%) | Cranelift O0 is well-characterized; worst case 5x slower than LLVM O2, which is acceptable for development |

### 8.1 Confidence Assessment

**Confidence that Cranelift backend can reach production quality: HIGH (85%)**

Cranelift is more mature than TCC or any other alternative. It is used daily in Wasmtime to
compile real programs. The risk is entirely in TML's bridge implementation, not in Cranelift
itself. The implementation gap is 2-3 months of focused Rust development.

**Confidence that Cranelift will deliver 5-10x faster development builds: HIGH (90%)**

This is empirically measured across many Cranelift adopters. The compilation speed
improvement is Cranelift's primary design goal and is well-documented.

---

## 9. Integration with the Broader Backend Strategy

The Cranelift backend slots into Phase 6.3 of the TML roadmap. Its role:

```
Development flow (Phase 6.3+):
  tml build --debug    → Cranelift backend (fast, ~50ms/file)
  tml build --release  → LLVM backend (slow, ~500ms/file, optimized)
  tml test             → Cranelift backend (fast cycle for test iteration)

Self-hosting flow (Phase SH-5+):
  TML compiler (written in TML) → Cranelift backend → compiled fast
  Cranelift acts as "stage0" backend → bootstrap TML compiler
```

The Cranelift backend is not a replacement for LLVM — it is a development accelerator.
Release builds always use LLVM for optimal code quality. The investment in Cranelift pays
off across the entire project lifetime through faster edit-compile-test cycles.

---

*Related documents: [05-custom-backend-feasibility.md](./05-custom-backend-feasibility.md) | [compiler-selfhosting/00-executive-summary.md](../compiler-selfhosting/00-executive-summary.md)*
