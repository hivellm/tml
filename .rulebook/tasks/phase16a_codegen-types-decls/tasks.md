# Tasks: Codegen Types & Declarations — Rewrite in TML

**Status**: Planned (0/25)
**Depends on**: phase15d (optimized MirModule available in TML)
**Blocks**: phase16b (instructions need type emission), phase16c (calls need ABI/type layer)
**Duration**: 4–6 weeks
**Risk**: Medium — type layouts must match C++ exactly; layout errors corrupt all downstream IR
**C++ reference**: ~8K LOC → ~5.2K TML

---

## Phase 1: Module & File Structure (3 items)

- [ ] 1.1 Create `compiler-tml/src/codegen/mod.tml` — module root, re-exports `Codegen`, `CodegenConfig`, `emit_module()`
- [ ] 1.2 Create `compiler-tml/src/codegen/types.tml` — `LlvmType` enum: `I1`, `I8`, `I16`, `I32`, `I64`, `F32`, `F64`, `Ptr`, `Struct(List[LlvmType])`, `Array(LlvmType, I64)`, `Func(List[LlvmType], LlvmType)`, `Void`
- [ ] 1.3 Create `compiler-tml/src/codegen/config.tml` — `CodegenConfig` struct: target triple, data layout string, optimize level, release flag

## Phase 2: Type Emission (6 items)

- [ ] 2.1 Create `compiler-tml/src/codegen/emit_type.tml` — `emit_type(t: MirType) -> Text` converting MIR types to LLVM IR type strings
- [ ] 2.2 Implement primitive types: `I64` → `"i64"`, `I32` → `"i32"`, `Bool` → `"i1"`, `F64` → `"double"`, `Unit` → `"{}"`, `Str` → `"ptr"`
- [ ] 2.3 Implement aggregate types: struct → `"%struct.Name"` named reference, tuple → `"{ i64, i64, ... }"` inline, array → `"[N x T]"`
- [ ] 2.4 Implement pointer and reference types: `Ref[T]` → `"ptr"`, `MutRef[T]` → `"ptr"`, raw pointer → `"ptr"` (opaque pointer model, LLVM 15+)
- [ ] 2.5 Implement function pointer types: `func(A, B) -> C` → `"ptr"` in opaque model; emit full signature only in function declarations
- [ ] 2.6 Implement Maybe/Outcome layout: `Maybe[T]` → `{ i32, T_padded }` matching C++ `maybe_layout()` byte-for-byte; `Outcome[T,E]` → `{ i32, union(T,E) }`

## Phase 3: Struct Layout Computation (4 items)

- [ ] 3.1 Create `compiler-tml/src/codegen/layout.tml` — `LayoutComputer` struct computing size/alignment for each `MirType`
- [ ] 3.2 Implement primitive sizes: I8=1, I16=2, I32=4, I64=8, F32=4, F64=8, Bool=1, pointer=8 (x86_64)
- [ ] 3.3 Implement struct layout: iterate fields, insert padding bytes to meet field alignment, record field offsets; total size rounded up to struct alignment
- [ ] 3.4 Emit named struct type definitions: `%struct.Foo = type { i64, i32, [4 x i8] }` — emit each struct exactly once, deduplicate by name

## Phase 4: Function Signature Emission (5 items)

- [ ] 4.1 Create `compiler-tml/src/codegen/emit_func.tml` — `emit_func_decl(f: MirFunc, cfg: CodegenConfig) -> Text` producing the `define`/`declare` line
- [ ] 4.2 Implement calling convention annotation: `cc` field on MirFunc → `fastcc`, `ccc`, `win64cc` strings prepended to `define`
- [ ] 4.3 Implement sret parameter: if return type is large struct, prepend `ptr sret(%struct.Name) align 8 %sret_slot` as first parameter
- [ ] 4.4 Implement byval parameter: struct args ≤ 16 bytes passed by value → `byval(%struct.Name) align 8` annotation
- [ ] 4.5 Implement function attributes: `nounwind`, `uwtable`, `alwaysinline`, `noinline` emitted from MirFunc attribute set

## Phase 5: Module-Level Declarations (4 items)

- [ ] 5.1 Create `compiler-tml/src/codegen/emit_module.tml` — `emit_module(m: MirModule, cfg: CodegenConfig) -> Text` producing complete LLVM IR text
- [ ] 5.2 Emit module header: `; ModuleID = 'file.tml'\nsource_filename = "..."\ntarget datalayout = "..."\ntarget triple = "..."\n`
- [ ] 5.3 Emit runtime declarations: `declare` lines for every `@extern("c")` function used in the module — only emit what the module actually uses (not all 500+ runtime functions)
- [ ] 5.4 Emit global constants and string literals: `@str.0 = private unnamed_addr constant [N x i8] c"...\00"` for each unique string in the module

## Phase 6: Differential Testing (3 items)

- [ ] 6.1 Create `compiler-tml/tests/codegen/types.test.tml` — unit tests: for each MirType variant, `emit_type(t)` must equal expected LLVM IR string
- [ ] 6.2 Create `compiler-tml/tests/codegen/layout.test.tml` — struct layout tests: compute layout of 10 stdlib structs, assert field offsets match C++ `llvm_types.cpp` output
- [ ] 6.3 IR-diff: compile 5 stdlib modules through TML type/decl emitter → compare struct definitions and function declarations against C++ codegen output line-by-line
