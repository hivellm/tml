# C/C++ Compiler Strategy: How TML Becomes a Complete Toolchain

**Date**: 2026-04-05
**Scope**: Strategy for adding C and C++ compilation to the TML toolchain
**Goal**: `tml cc main.c` and `tml c++ main.cpp` work natively

---

## 1. Why TML Needs a C/C++ Compiler

- **Bootstrap independence**: The TML compiler currently needs a C++ compiler to build itself (for the 1,593 LOC LLVM/LLD shim and the initial C++ compiler). With a native C/C++ frontend, TML can compile its own shim.
- **Unified toolchain**: Developers using TML for systems programming will inevitably need to compile C libraries (OpenSSL, zlib, SQLite). Today they need MSVC/GCC/Clang alongside TML. A `tml cc` eliminates this.
- **Zig's proof of concept**: Zig proved that bundling a C/C++ compiler is a massive developer experience win. `zig cc` is Zig's most popular feature. TML should follow this model.
- **Cross-compilation**: A self-contained toolchain makes cross-compilation trivial (like Zig targeting Linux from Windows).
- **Distribution simplicity**: One download, one binary, compiles everything.

---

## 2. Four Strategy Options

### Option A: Embed Clang (Zig's Approach)

**What**: Ship a patched version of Clang as `tml cc` / `tml c++`.

| Aspect | Detail |
|--------|--------|
| **Effort** | 2–3 months integration |
| **C support** | Full C23 |
| **C++ support** | Full C++23 |
| **Binary size** | +80–100 MB (Clang + LLVM for C/C++) |
| **Independence** | None — still depends on LLVM |
| **Maintenance** | Must track Clang releases |

**How it works**:
- Link Clang's frontend library into tml.exe
- Route `tml cc` to Clang's driver
- Route `tml c++` to Clang's C++ driver
- Bundle libc headers (like Zig bundles musl + Windows CRT headers)

**Pros**: Immediate, battle-tested, full standard compliance, same approach Zig uses
**Cons**: Adds 80MB+ to binary, defeats independence goal, maintenance burden
**Verdict**: Good INTERIM solution for Phase 1, but not the end goal

---

### Option B: Build C Frontend from Scratch

**What**: Write a C17-compliant frontend in TML that emits MIR.

| Aspect | Detail |
|--------|--------|
| **Effort** | 12–18 months |
| **C support** | C17 (pragmatic subset of C23) |
| **C++ support** | None |
| **Binary size** | +1–2 MB |
| **Independence** | Full (for C) |
| **Maintenance** | Self-maintained |

**Architecture**:
```
C source → Preprocessor → C Lexer → C Parser → C AST → C Type Checker → MIR → Backend → Linker → binary
```
Reuses TML's existing MIR pipeline, backend, and linker. Only the frontend is new.

**Components**:

| Component | LOC | Complexity | Notes |
|-----------|-----|-----------|-------|
| C Preprocessor | 3,800 | High | #include, #define, #if, token pasting |
| C Lexer | 1,200 | Low | Simpler than TML lexer (fewer keywords) |
| C Parser | 6,000 | Medium | Declarations, expressions, statements |
| C Type Checker | 5,000 | High | Promotion rules, implicit conversions |
| C → MIR Lowering | 4,200 | High | Map C constructs to MIR instructions |
| **Total** | **~20,200** | **High** | |

**Prior art**:
- chibicc: 5K LOC C, educational, compiles most real C programs
- TCC (Tiny C Compiler): 15K LOC C, production-quality, Linux kernel boots with it
- cparser (from libFirm): 15K LOC C, full C11 support
- lcc: 12K LOC C, retargetable, textbook compiler

**Key insight**: A C compiler is MUCH simpler than a C++ compiler. The entire C17 spec can be implemented in ~20K LOC. The preprocessor is the hardest part.

**Pros**: Full independence, tiny binary addition, shared optimizations with TML
**Cons**: 12–18 months work, must handle C's edge cases (K&R compatibility, VLAs, complex.h)
**Verdict**: THE long-term goal — TML owns its C compilation

---

### Option C: Hybrid — Clang Frontend + TML Backend

**What**: Use Clang to parse C/C++, produce LLVM IR, convert LLVM IR → TML MIR, use TML backend.

| Aspect | Detail |
|--------|--------|
| **Effort** | 6–12 months |
| **C/C++ support** | Full (via Clang) |
| **Independence** | Partial (still needs Clang for parsing) |
| **Key challenge** | LLVM IR → MIR conversion is complex and lossy |

**Pros**: Leverages Clang's C++ parsing (the hardest problem in the space)
**Cons**: Still needs Clang, IR conversion adds complexity and potential for subtle bugs, two moving targets to track
**Verdict**: Technically interesting but combines the worst of both worlds

---

### Option D: C Frontend + C++ via Source-to-Source Translation

**What**: Build a C frontend (Option B), then for C++, transpile C++ → C first.

| Aspect | Detail |
|--------|--------|
| **Effort** | C: 12–18 months, C++ transpiler: 6–12 months additional |
| **C support** | Full C17 |
| **C++ support** | Limited (depends on transpiler quality) |
| **Prior art** | Cfront (original C++ compiled to C), Compcert |

**Pros**: C frontend is clean and complete; C++ support comes incrementally
**Cons**: C++ transpilation is lossy — exceptions, templates, RTTI are hard to translate cleanly
**Verdict**: Interesting for basic C++ but not viable for modern C++ codebases (LLVM itself, for instance)

---

## 3. Recommended Strategy: Phased Approach

| Phase | Timeline | What | Result |
|-------|----------|------|--------|
| **Phase 1** | Immediate | Ship `tml cc` as Clang wrapper | C/C++ compilation works TODAY |
| **Phase 2** | After self-hosting | Build C17 frontend from scratch | Full C independence |
| **Phase 3** | After C frontend | Build C++ subset (classes, templates) | Most C++ libraries compile |
| **Phase 4** | Long-term | Full C++20 support | Complete toolchain |

**Phase 1 (NOW)**: Zig's approach — bundle Clang, ship it. Gives users `tml cc` immediately while the real work happens in the background.

**Phase 2 (ERA 4)**: The real work — build a proper C frontend in TML. This is achievable: chibicc proves a C compiler can be 5K LOC; the target of ~20K LOC covers production quality including proper preprocessor, VLAs, designated initializers, and C11 atomics.

**Phase 3 (ERA 4+)**: Add C++ support incrementally. Start with the subset needed to compile the LLVM/LLD C++ shim (1,593 LOC). That is a small and well-defined C++ dialect. Expand coverage from there guided by what real users need.

**Phase 4**: Full C++20 is aspirational and almost certainly community-driven. The core team focuses on C + the C++ subset required for bootstrap independence. Full C++ standard compliance is a multi-year community effort.

---

## 4. C Preprocessor Deep Dive

The preprocessor is the hardest part of a C compiler. It must run before any parsing occurs, handles its own mini-language, and has notoriously subtle edge cases baked in by decades of legacy.

### 4.1 #include Resolution

- Search paths: `-I` flags, system paths, relative paths from current file
- Quote includes (`"header.h"`) vs angle includes (`<stdio.h>`)
- Include guards (`#ifndef HEADER_H` ... `#define HEADER_H` ... `#endif`)
- `#pragma once` (non-standard but universally supported and preferred)
- Recursive includes with cycle detection to prevent infinite expansion
- Computed includes (`#include MACRO_THAT_EXPANDS_TO_FILENAME`)

### 4.2 Macro Expansion

- Object-like macros: `#define PI 3.14159`
- Function-like macros: `#define MAX(a,b) ((a)>(b)?(a):(b))`
- Variadic macros: `#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)`
- Token pasting: `#define CONCAT(a,b) a##b`
- Stringification: `#define STR(x) #x`
- Rescanning and further replacement after expansion
- Blue paint algorithm (painting tokens to prevent infinite self-referential recursion)
- Argument prescan rules (arguments expanded before substitution unless in `#` or `##` context)

### 4.3 Conditional Compilation

- `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`
- Expression evaluation in `#if` (integer arithmetic, `defined()`, `__has_include()`)
- Nested conditionals with correct balancing
- Skipping tokens efficiently in false branches (without full tokenization)

### 4.4 Predefined Macros

The preprocessor must define platform macros before any user code runs:

| Macro | Value (example) | Purpose |
|-------|----------------|---------|
| `__FILE__` | `"main.c"` | Current filename |
| `__LINE__` | `42` | Current line number |
| `__DATE__` | `"Apr  5 2026"` | Build date |
| `__TIME__` | `"14:32:00"` | Build time |
| `__STDC__` | `1` | Hosted C implementation |
| `__STDC_VERSION__` | `201710L` | C17 |
| `_WIN32` | `1` | Windows target |
| `__linux__` | `1` | Linux target |
| `__x86_64__` | `1` | x86-64 architecture |

### 4.5 Estimated Complexity

| Feature | LOC | Difficulty |
|---------|-----|-----------|
| Tokenizer | 500 | Low |
| Include handling | 800 | Medium |
| Macro expansion | 1,200 | High |
| Conditional compilation | 600 | Medium |
| Predefined macros | 300 | Low |
| Error recovery | 400 | Medium |
| **Total** | **~3,800** | **High** |

---

## 5. C → MIR Lowering

TML's MIR is already a fairly low-level SSA IR. Most C constructs map directly:

| C Construct | MIR Mapping | Notes |
|------------|-------------|-------|
| `int x = 5;` | `Alloc(i32)` + `Store(5)` | Direct alloca + store |
| `x + y` | `BinOp(Add, x, y)` | Identical to TML |
| `if (x) {...}` | `Branch(x, bb1, bb2)` | Identical to TML |
| `while (x) {...}` | `Jump(loop_header)` + back-edge | Same as TML `loop` |
| `for (i=0; i<n; i++) {...}` | `Alloc(i32)` + `Store(0)` + loop | Desugar to while |
| `struct S { int a; float b; }` | Named aggregate type | Same as TML struct |
| `int *p = &x;` | `AddrOf(x)` | Same as TML `ref` |
| `*p = 10;` | `Store(10, *p)` | Deref store |
| `arr[i]` | `GEP(arr, i)` | GetElementPtr |
| `func(a, b)` | `Call(func, [a, b])` | Same as TML |
| `return x;` | `Return(x)` | Same as TML |
| `switch (x) {...}` | `SwitchInt(x, targets)` | TML uses `when` |
| `goto label;` | `Jump(bb_label)` | Requires label → BB mapping |
| `(int)x` | `Cast(i32, x)` | Explicit cast |
| `union U { int a; float b; }` | Overlapping fields at offset 0 | Needs union MIR support |
| `int arr[N]` | `Alloc([N x i32])` | Fixed array allocation |
| `int arr[]` (VLA) | `Alloc(N * sizeof(i32))` | Dynamic stack alloc |
| `setjmp/longjmp` | Runtime call + frame saves | Complex — needs special support |

### 5.1 Implicit Type Conversion

C's implicit conversion rules are the largest source of added complexity vs. TML. TML is explicit — every type conversion is written by the programmer. C promotes types silently:

```c
char c = 42;
int i = c;         // char → int (sign extension)
float f = i;       // int → float (widening)
double d = f + 1;  // float + int → double (promotion)
```

The C type checker must insert conversion MIR instructions at every such site. This requires:
1. Tracking the "usual arithmetic conversions" rules (C17 §6.3.1.8)
2. Integer promotion (types smaller than int promoted before arithmetic)
3. Implicit pointer-to-integer casts in conditions (`if (ptr)` → `if (ptr != NULL)`)
4. Array decay (`int arr[]` → `int *` in most contexts)
5. Function-to-pointer decay (`func` → `&func` in most contexts)

### 5.2 The `union` Problem

Unions are the most complex C construct to lower to MIR. A union allocates one chunk of memory shared by all fields. In LLVM IR terms, a union is an alloca of `max(sizeof(members))` bytes, with each member access casting the pointer to the right type.

TML's MIR currently has no concept of overlapping fields. Adding union support requires either:
- A `Union` MIR type with explicit byte-cast loads/stores
- Lowering unions to `[N x i8]` with bitcasts at every access site

The second option is cleaner and what Clang does.

### 5.3 `setjmp` / `longjmp`

These C functions require saving and restoring the call stack frame. They interact deeply with the platform ABI (stack layout, callee-saved registers). The approach:

1. `setjmp(env)` → save all callee-saved registers + stack pointer into `env` (a platform-specific struct), return 0
2. `longjmp(env, val)` → restore registers + stack pointer from `env`, jump to the return address saved in `env`

This is implementable as runtime C functions — the C frontend itself does not need to special-case it beyond ensuring the `jmp_buf` type has the right size and alignment for the target platform.

---

## 6. Bundled System Headers

Like Zig, TML would bundle system headers to eliminate the need for a separately-installed SDK:

| Platform | Headers | Source | License |
|----------|---------|--------|---------|
| Windows | CRT headers (`<stdio.h>`, `<stdlib.h>`, `<windows.h>`, etc.) | Windows SDK (with MS license) or musl port | Needs careful licensing |
| Linux | musl libc headers | musl-libc project | MIT |
| macOS | macOS SDK headers | Apple SDK | Apple license |
| Freestanding | `<stdint.h>`, `<stddef.h>`, `<stdarg.h>` | Self-contained (no OS dependency) | Public domain |

### 6.1 Header Bundling Strategy

1. For Linux targets: bundle musl headers unconditionally. musl is MIT licensed, small (~500KB of headers), and covers the full C standard library interface.
2. For Windows targets: two options
   - Bundle a redistributable subset of the Windows SDK headers (permitted under Microsoft's terms for distribution)
   - Alternatively, use LLVM's MinGW headers (open source, covers Win32 API)
3. For macOS targets: Apple's SDK headers have a non-redistribution clause. This is Zig's main licensing challenge too. The practical solution is to detect a local Xcode install or provide documented instructions.

### 6.2 Why Bundling Matters

The current pain of C development on most systems:
1. Install compiler (Clang, GCC, MSVC)
2. Install platform SDK separately (Windows SDK, Xcode, build-essential)
3. Configure include paths
4. Configure library paths
5. Deal with version mismatches

With TML:
1. Download `tml.exe`
2. Run `tml cc main.c`

That is the entire workflow.

---

## 7. Cross-Compilation

With a native C frontend + TML's backend + LLD linker, cross-compilation becomes straightforward:

```bash
# From Windows, targeting Linux x86-64
tml cc --target=x86_64-linux-musl main.c -o main

# From Windows, targeting macOS ARM64
tml cc --target=aarch64-apple-macos main.c -o main

# From Linux, targeting Windows x86-64
tml cc --target=x86_64-pc-windows-msvc main.c -o main.exe

# From anything, targeting a bare-metal embedded system
tml cc --target=thumbv7em-none-eabi main.c -o firmware.elf
```

TML bundles the libc headers for all supported targets. No sysroot installation, no cross-toolchain setup.

### 7.1 What Makes This Work

The ability to cross-compile depends on three components all being target-aware:

| Component | Target-aware? | Notes |
|-----------|--------------|-------|
| C Frontend | Yes | Predefined macros differ per target |
| MIR Backend | Yes | TML already supports multiple targets via LLVM |
| LLD Linker | Yes | LLD already handles all major target formats |
| libc Headers | Yes | Bundled per-target |
| libc Runtime | No (needed at link time) | User provides target libc or use musl |

The libc runtime (the compiled `.a` file, not just headers) is needed at link time. For Linux musl targets, TML can bundle the precompiled musl `.a` files (one per target architecture). This is exactly what Zig does and is the feature that makes `zig cc` special.

---

## 8. C++ Strategy Detail

C++ is a fundamentally harder problem than C. The key complexity drivers:

### 8.1 C++ Complexity vs. C

| Feature | C | C++ | Why Hard |
|---------|---|-----|----------|
| Namespaces | No | Yes | Name mangling, lookup rules |
| Classes | No (structs only) | Yes | vtables, constructors, destructors |
| Inheritance | No | Yes | vtable layout, virtual dispatch |
| Templates | No | Yes | Full Turing-complete meta-programming |
| Exceptions | No | Yes | Stack unwinding, RTTI |
| Overloading | No | Yes | Name mangling, overload resolution |
| `auto` inference | No | Yes | Complex deduction rules |
| Concepts (C++20) | No | Yes | Constraint satisfaction |
| Coroutines (C++20) | No | Yes | Coroutine frame layout, resumption |
| Ranges (C++20) | No | Yes | Library feature, but complex |

Templates alone are Turing-complete and account for a substantial fraction of Clang's complexity. The LLVM codebase itself makes heavy use of templates — this is why the TML C++ shim (1,593 LOC) uses mostly simple C++.

### 8.2 Minimum Viable C++ Subset

To compile the TML LLVM/LLD shim, TML needs:

- Basic classes with member functions
- Inheritance (single, no virtual needed for the shim)
- Standard library includes (`<string>`, `<vector>`, `<memory>`) — but only the parts the shim uses
- `new` / `delete`
- References (`&`)
- `const` methods
- Namespaces

This is roughly "C with classes" — the original Cfront subset. Implementable in 6–12 months after the C frontend is solid.

### 8.3 The Template Problem

Full C++ templates (including partial specialization, SFINAE, concepts) require a complete template instantiation engine. This is the single hardest part of any C++ compiler. Options:

1. **Implement full templates**: Required for LLVM/Clang compatibility. 12–18 months of work on top of the C++ base.
2. **Implement template basics**: Class templates and function templates without SFINAE. Covers 80% of user code. ~6 months.
3. **Defer templates entirely**: Only support the C++ subset without templates. Covers a surprising amount of real-world C++ code that avoids templates by convention.

For bootstrap independence (compiling the shim), Option 3 is sufficient. The TML LLVM shim does not use templates.

---

## 9. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| C preprocessor edge cases | High (60%) | Medium | Extensive test suite using GCC/Clang torture tests |
| C standard compliance gaps (VLAs, complex.h) | Medium (40%) | Low | Focus on C17 pragmatic subset, document unsupported features |
| C++ complexity explosion during Phase 3 | High (70%) | High | Hard scope limit: only the subset needed to compile the shim |
| Header bundling licensing (Windows) | Low (10%) | Medium | Use MinGW headers (open source) as primary Windows path |
| Cross-compilation sysroot complexity | Medium (30%) | Medium | Follow Zig's approach: bundle precompiled musl per target |
| Community expects full C++23 | High (80%) | Low | Clear roadmap communication: C first, C++ subset next, full C++ is community |
| ABI mismatch with platform libc | Medium (35%) | High | Test against MSVC ABI (Windows) and System V ABI (Linux/macOS) explicitly |
| Preprocessor recursion depth limits | Medium (30%) | Low | Cap at 200 levels (matches GCC default), emit warning |

---

## 10. Implementation Roadmap Detail

### 10.1 Phase 1: Clang Wrapper (0–3 months)

Tasks:
1. Add `tml cc` CLI subcommand
2. Detect or download Clang binaries (same LLVM version TML ships with)
3. Forward all arguments to Clang, passing TML's bundled include paths
4. Add `tml c++` subcommand similarly
5. Bundle musl headers for Linux cross-compilation
6. Document `tml cc` usage

Success criteria: `tml cc hello.c -o hello` works on all three platforms.

### 10.2 Phase 2: C Frontend in TML (12–18 months, starts after self-hosting)

Tasks:
1. C preprocessor (month 1–3)
   - Tokenizer
   - Macro expansion with blue-paint algorithm
   - Include resolution with search paths
   - Conditional compilation
2. C Lexer (month 3–4)
   - All C17 tokens
   - Integer/float literal parsing (hex, octal, binary, suffixes)
   - String escape sequences
3. C Parser (month 4–8)
   - Declarations (function, variable, struct, union, enum, typedef)
   - Expressions (full C precedence table)
   - Statements (if, while, for, do, switch, goto, return)
4. C Type Checker (month 8–12)
   - Type environment and scope tracking
   - Implicit conversion insertion
   - Usual arithmetic conversions
   - Pointer arithmetic rules
5. C → MIR Lowering (month 12–15)
   - All constructs from section 5
   - Union support in MIR
6. Integration and test (month 15–18)
   - Test against chibicc test suite
   - Test against SQLite (compiles with no extensions, ~150K LOC C)
   - Test against zlib
   - Test against musl libc itself

Success criteria: `tml cc` (native frontend) compiles SQLite with identical binary output to Clang.

### 10.3 Phase 3: C++ Subset (12–24 months after C frontend)

Tasks:
1. C++ lexer extensions (new keywords: `class`, `namespace`, `template`, etc.)
2. Basic class support (member functions, constructors, destructors)
3. Single inheritance with vtable generation
4. Name mangling (Itanium ABI for Linux/macOS, MSVC ABI for Windows)
5. `new` / `delete` (maps to malloc/free + constructor/destructor calls)
6. References (maps to pointers with non-null constraint)
7. Basic templates (class templates and function templates, no SFINAE)
8. Standard library headers for what the shim uses

Success criteria: `tml c++` (native frontend) compiles the TML LLVM/LLD shim (1,593 LOC) with identical behavior.

---

## 11. Summary

| Strategy | Effort | Independence | Recommendation |
|----------|--------|-------------|----------------|
| Embed Clang | 2–3 months | None | Do first (interim) |
| C frontend from scratch | 12–18 months | Full (C) | The real goal |
| C++ subset frontend | 12–24 months | Full (C/C++) | After C frontend |
| Full C++20 | Years | Full | Community-driven |

**The path**: Clang wrapper now → C frontend in TML → C++ subset → full C++20 eventually.

The C frontend is achievable. chibicc is 5K LOC and Rui Ueyama built it as an educational project in a few months. TCC is 15K LOC and powers the Linux kernel in some embedded configurations. TML's C frontend at ~20K LOC sits comfortably in this space with production quality in mind.

The C++ subset is bounded: the only hard requirement is compiling TML's own LLVM/LLD shim, which is 1,593 LOC of straightforward C++ without heavy template metaprogramming. That is a well-scoped target that can be reached incrementally.

Full C++23 compliance is not a core team goal. It is a multi-year community effort — which is exactly what happened with Rust's C/C++ interop story. The core team's job is to build the foundation that makes it possible.
