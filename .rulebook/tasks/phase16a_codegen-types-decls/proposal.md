# Proposal: Codegen Types & Declarations — Rewrite in TML

## Why

The MIR codegen subsystem is the final C++ layer standing between an optimized MirModule and the
LLVM IR text that LLVM compiles to native code. Its entry point (`mir_codegen.cpp`, 1,622 LOC) and
type emission layer (`mir_types.cpp`, `llvm_types.cpp`, 1,207 LOC) are the foundation on which all
instruction and call emission depends. Types that are laid out incorrectly corrupt every instruction
that reads or writes a value of that type. Porting the type and declaration layer first establishes
a verified foundation before tackling instructions and calls in phases 16b and 16c.

## What Changes

The C++ type emission code in `compiler/src/codegen/mir_codegen.cpp`, `mir/mir_types.cpp`, and
`llvm/core/llvm_types.cpp` is replaced by a TML implementation in `compiler-tml/src/codegen/`.
Function signature emission from `llvm/decl/func.cpp` (1,351 LOC) and impl/vtable emission from
`llvm/decl/impl.cpp` (1,336 LOC) are also ported here, since they depend only on the type layer.

### Architecture

```
compiler-tml/src/codegen/
  mod.tml          — re-exports Codegen, CodegenConfig, emit_module()
  config.tml       — CodegenConfig: target triple, data layout, opt level
  types.tml        — LlvmType enum: I1..I64, F32/F64, Ptr, Struct, Array, Func, Void
  layout.tml       — LayoutComputer: size/alignment/field-offsets per MirType
  emit_type.tml    — emit_type(MirType) -> Text: MIR type → LLVM IR type string
  emit_func.tml    — emit_func_decl(MirFunc) -> Text: define/declare line + sret/byval
  emit_module.tml  — emit_module(MirModule) -> Text: complete LLVM IR file
```

### Key Design Decisions

- **Text output via template literals** — all IR emission uses TML template literals
  (`` `define fastcc i64 @{name}({params}) {` ``) rather than string concatenation. This matches
  how the C++ code builds IR and keeps emission code readable and diffable.
- **Type layout must be byte-for-byte identical to C++** — the `LayoutComputer` in `layout.tml`
  replicates the exact field-padding rules from `llvm_types.cpp`. Any divergence corrupts sret
  slot sizes, GEP indices, and struct constructor IR. Tests assert field offsets directly.
- **Opaque pointer model** — the TML codegen targets LLVM 15+ opaque pointers. All pointer types
  emit as `"ptr"` regardless of pointee type. This simplifies the type layer significantly
  compared to the typed-pointer LLVM IR the legacy codegen sometimes emits.
- **Named struct deduplication** — each struct name is emitted as a `%struct.Name = type { ... }`
  definition exactly once at the top of the module. A `HashMap[Str, Bool]` tracks already-emitted
  structs to prevent duplicate definitions, which are LLVM IR errors.
- **sret for large return types** — structs larger than 16 bytes use the sret convention: the
  caller allocates a stack slot and passes its address as the first argument annotated
  `ptr sret(%struct.T) align 8`. The callee writes the result there and returns void. The
  `emit_func_decl` function computes this from the layout, matching `func.cpp` exactly.
- **Runtime declarations on demand** — instead of emitting all 500+ runtime function declarations
  unconditionally (as the C++ legacy codegen does), the TML emitter tracks which extern functions
  the module actually calls and emits only those `declare` lines. This reduces IR file size and
  speeds up LLVM parsing.

## Impact

- Affected code: `compiler/src/codegen/mir_codegen.cpp`, `mir/mir_types.cpp`,
  `llvm/core/llvm_types.cpp`, `llvm/decl/func.cpp`, `llvm/decl/impl.cpp` (all replaced)
- Affected phases: 16b (instructions call `emit_type`), 16c (calls use sret/byval decisions)
- Breaking change: NO — IR-diff testing ensures identical type strings and function signatures
- User benefit: self-hosting progress; type layout logic is inspectable and modifiable in TML

## Success Criteria

The TML type emitter produces LLVM IR struct definitions and function declaration lines that are
character-identical to C++ codegen output for all stdlib modules. The `LayoutComputer` produces
field offsets that match C++ for all 40+ named struct types in the stdlib. IR-diff on 5 stdlib
modules shows zero differences in the declarations section.

## Dependencies

- **Requires**: phase15d (MirModule with MirType, MirFunc available in TML)
- **Blocks**: phase16b (instructions need `emit_type`), phase16c (calls need sret decisions)
- **Risk**: Medium — type layout errors are silent but fatal; mitigated by per-struct layout
  unit tests that assert field offsets before any full-module IR-diff testing begins.
