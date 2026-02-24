# Proposal: Self-Hosting TML Compiler

## Status
- **Created**: 2026-02-13
- **Updated**: 2026-02-24
- **Status**: Proposed
- **Priority**: Strategic (Long-term)

## Why

The TML compiler is currently written in ~238,000 lines of C++. Every mature language eventually rewrites its compiler in itself — Rust did it (OCaml → Rust), Go did it (C → Go), and TML must do it too. Self-hosting proves the language is capable of systems-level programming and eliminates the C++ build dependency.

**This task is backend-agnostic**: it works whether TML targets LLVM, Cranelift, or both. The backend is accessed via FFI (`@extern`), so swapping backends doesn't affect the compiler source.

### Current state

| Component | Language | Lines | Status |
|-----------|----------|------:|--------|
| Compiler (lexer→codegen) | C++ | ~158,900 | Production |
| Compiler headers | C++ | ~78,900 | Production |
| C Runtime | C | ~19,100 | Partially unnecessary (see migrate-runtime-to-tml) |
| Standard Library | TML | ~137,300 | 84.7% test coverage (4,095/4,836 functions) |
| **Total** | | **~394,200** | |

### What self-hosting means

```
Stage 0: C++ compiler compiles TML code (TODAY)
Stage 1: C++ compiler compiles the TML compiler written in TML
Stage 2: TML compiler compiles itself (SELF-HOSTING)
Stage 3: C++ compiler can be retired (BOOTSTRAP COMPLETE)
```

After Stage 2, the only role of the C++ compiler is bootstrapping: compiling the TML compiler from source on a fresh machine. Most users would use a pre-built TML compiler binary.

### Prerequisites (must be done BEFORE self-hosting)

1. **`migrate-runtime-to-tml`** — Stdlib must be pure TML (collections, strings, formatting)
2. **Language features** — Closures with captures (DONE), recursive enums (BLOCKED), default behavior method dispatch (BUG), generic combinators on Maybe/Outcome (BLOCKED)
3. **File I/O** — Working file reading/writing for source files (DONE)
4. **Stable LLVM C API bindings** — TML must be able to call LLVM via `@extern("c")`

## The compiler in numbers

### What must be rewritten (by component)

| Component | C++ Lines | Difficulty | Rewrite Priority |
|-----------|----------:|------------|-----------------|
| **Lexer** | 3,183 | Easy | Stage 1 (first) |
| **Preprocessor** | 1,168 | Easy | Stage 1 |
| **Parser** | 9,384 | Medium | Stage 1 |
| **Type Checker** | 20,205 | Very Hard | Stage 2 |
| **Borrow Checker** | 6,579 | Very Hard | Stage 3 (can defer) |
| **HIR** | 14,295 | Medium-Hard | Stage 2 |
| **MIR + 49 passes** | 36,951 | Very Hard | Stage 3 |
| **LLVM Codegen** | 52,907 | Very Hard | Stage 2 |
| **Query System** | 2,736 | Hard | Stage 3 (optimization) |
| **CLI/Builder** | 26,975 | Medium | Stage 2 |
| **Tester** | 12,899 | Medium | Stage 3 |
| **Formatter** | 1,169 | Easy | Stage 4 (optional) |
| **Linter** | 1,347 | Easy | Stage 4 (optional) |
| **Documentation** | 4,303 | Medium | Stage 4 (optional) |
| **Backend (LLVM+LLD)** | 1,421 | Hard | Stage 2 |
| **TOTAL** | **~195,500** | | |

### External dependencies

| Dependency | How TML accesses it | Replaceable? |
|------------|-------------------|--------------|
| **LLVM C API** | `@extern("c")` FFI to llvm-c/*.h | No — needed for IR generation |
| **LLD** | `@extern("c")` FFI to lld API | No — needed for linking |
| **libc** (malloc, fopen, etc.) | `@extern("c")` via `mem_alloc`, file I/O | No — OS interface |
| **C++ STL** (vector, map, string) | Replaced by TML List, HashMap, Str | Yes — this IS the point |
| **OpenSSL / BCrypt** | `@extern("c")` FFI | No — crypto stays as FFI |

### What DOESN'T get rewritten

- **LLVM itself** — stays as C++ library, accessed via FFI
- **LLD linker** — stays as C++ library, accessed via FFI
- **OS syscalls** — accessed via `@extern("c")` to libc
- **Crypto libraries** — accessed via `@extern("c")` to OpenSSL/BCrypt

## Bootstrap strategy

### Stage 0: Preparation (current → pre-Stage 1)

Before writing a single line of compiler-in-TML, the language must be ready:

1. Complete `migrate-runtime-to-tml` — collections, strings, formatting in pure TML
2. Write LLVM C API bindings as TML `@extern("c")` declarations
3. Write LLD bindings as TML `@extern("c")` declarations
4. Ensure file I/O works reliably (read source files, write object files)
5. Ensure HashMap with string keys works (symbol tables need this)
6. Ensure recursive data structures work (AST nodes are recursive)

### Stage 1: Frontend in TML (Lexer + Parser)

The easiest components. They are self-contained and have clear input/output:

- **Input**: Source text (Str / `ref [U8]`)
- **Output**: Token stream → AST data structures

Validation: TML lexer/parser must produce identical output to C++ lexer/parser on the full test suite.

### Stage 2: Middle-end in TML (Type Checker + HIR + Codegen)

The core of the compiler. This is where most complexity lives:

- Type checker needs: HashMap (symbol tables), recursive enums (types), pattern matching
- HIR needs: tree transformations, monomorphization
- Codegen needs: LLVM C API FFI, string building (for IR names)

### Stage 3: Backend + Optimizations in TML (MIR + Borrow Checker)

The most complex parts, but also the most deferrable:

- MIR 49 optimization passes can be ported one at a time
- Borrow checker can be simplified initially (NLL only, no Polonius)
- Query system is an optimization, not a requirement

### Stage 4: Tooling in TML (CLI, Tester, Formatter, Linter)

Non-critical but needed for a complete compiler:

- CLI argument parsing
- Test runner
- Code formatter
- Linter

### The bootstrap moment

When Stage 2 is complete, the TML compiler can compile a subset of TML. At some point during Stage 3, it becomes capable of compiling itself. This is the **bootstrap moment**:

```
1. C++ compiler compiles TML compiler → produces tml_stage1.exe
2. tml_stage1.exe compiles TML compiler → produces tml_stage2.exe
3. tml_stage2.exe compiles TML compiler → produces tml_stage3.exe
4. Verify: tml_stage2.exe == tml_stage3.exe (byte-identical)
5. If identical → self-hosting achieved
```

## Impact

- **Eliminates C++ dependency** for compiler development
- **Proves TML is systems-capable** — the ultimate dogfooding test
- **Simplifies contribution** — one language to learn, not two
- **Improves optimization** — compiler can use TML-specific optimizations
- **Enables compiler plugins** — written in TML, loaded at compile time

## Risks

- **Language maturity**: If TML has bugs in generics, closures, or memory management, they will surface during self-hosting. This is actually a feature — self-hosting is the best stress test.
- **Performance**: TML compiler may initially be slower than C++. Acceptable if within 2-3x.
- **Scope creep**: Must resist adding language features "for the compiler." The compiler should work with existing TML.
- **LLVM API stability**: LLVM C API changes between versions. Need version pinning.

## Known Blockers (as of 2026-02-24)

| Blocker | Severity | Status | Impact |
|---------|----------|--------|--------|
| Recursive enums (`Heap[Self]` in variants) | CRITICAL | Not implemented | Cannot represent AST, Type, CFG nodes |
| LLVM C API bindings (~500 functions) | CRITICAL | Not started | Cannot emit code |
| Default behavior method dispatch bug | HIGH | Returns `()` instead of correct type | Blocks Visitor pattern, iterator accumulators |
| Generic combinators (`map[U]` on Maybe) | MEDIUM | Blocked by generic codegen | Ergonomics for compiler code |
| BTreeMap genericity (I64-only) | MEDIUM | Compiler limitation | No ordered symbol tables |
| Tuple return codegen | LOW | Broken | Workaround: use structs |
| Const generics codegen | LOW | Incomplete | Workaround: use `List[T]` instead of `[T; N]` |
| Inline modules (`mod foo { ... }`) | LOW | Parser doesn't support | Workaround: file-based modules |

## Dependencies

- `migrate-runtime-to-tml` (MUST complete first)
- Stable LLVM C API FFI bindings
- Working file I/O in TML
- Working HashMap[Str, T] (symbol tables)
- Working recursive enums (AST node types)
- Closures with variable capture (useful but not strictly required)

## Success Criteria

- TML compiler written in TML compiles the full test suite identically to C++ compiler
- Bootstrap chain produces byte-identical output at Stage 3
- Compiler performance within 3x of C++ version
- All 10,000+ tests pass
- C++ compiler relegated to bootstrap-only role
