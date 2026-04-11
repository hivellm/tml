# JIT Execution for TML — Feasibility Analysis

**Date:** 2026-03-30
**Version:** TML 0.2.7 | LLVM 23.0.0 (trunk)
**Author:** Architecture Analysis

## 1. Motivation

TML is currently a fully compiled language. Every execution — whether `tml run`, `tml test`, or a hypothetical `tml script` — requires the full pipeline:

```
Source → Parse → Typecheck → Borrow → HIR → MIR → LLVM IR → .obj → LLD Link → .exe → subprocess
```

This creates friction in three scenarios:

1. **Rapid iteration** — editing a file and running it takes seconds due to linking I/O
2. **Testing** — each test suite compiles to a separate EXE, each must be linked
3. **Scripting** — TML cannot be used as a script language (no `#!/usr/bin/env tml`)

The question: can we add an **in-process JIT execution mode** that skips object files and linking entirely?

## 2. Current Architecture

### 2.1 Pipeline Flow

```
compile_via_queries()                    [builder_run.cpp:94]
  → QueryContext::codegen_unit()         [query_context.cpp]
    → ReadSource → Tokenize → Parse → Typecheck → Borrowcheck
    → HirLower → MirBuild → MirCodegen
    → Result: LLVM IR text (std::string)

LLVMBackend::compile_ir_to_object()     [llvm_backend.cpp:129]
  → LLVMParseIRInContext()              (IR text → LLVMModuleRef)
  → LLVMTargetMachineEmitToFile()       (module → .obj file)

LLD::link()                              [lld_linker.cpp]
  → .obj + runtime.obj + system libs → .exe

std::system(exe_path)                    [builder_run.cpp:401]
  → Execute as subprocess
```

### 2.2 Current LLVM Libraries Linked

TML already links **70+ LLVM static libraries** (~350MB .lib total) including:

- **X86 + AArch64 targets** (codegen, asm parser, disassembler)
- **Optimization passes** (LLVMPasses, LLVMipo, LLVMVectorize, LLVMInstCombine, etc.)
- **Core** (LLVMCore, LLVMIRReader, LLVMAsmParser, LLVMBitWriter, etc.)
- **Object/MC** (LLVMObject, LLVMMC, LLVMMCParser, LLVMMCDisassembler)
- **Debug info** (DWARF, CodeView, PDB)

### 2.3 Current Binary Sizes

| Component | Size |
|-----------|------|
| `tml.exe` (monolithic, debug) | ~100MB |
| `tml.exe` (launcher, modular) | 774KB |
| `tml_compiler.dll` | ~104MB |
| `tml_codegen_x86.dll` | ~78MB |

### 2.4 C Runtime Dependencies

TML executables link against C runtime objects:

| File | Purpose | JIT Implication |
|------|---------|-----------------|
| `essential.c` | `tml_print`, `tml_panic`, test harness | Must be loadable in-process |
| `mem.c` | `tml_alloc`, `tml_free` (malloc wrappers) | Must be loadable in-process |
| Collections C files | List, HashMap, Buffer | Must be loadable in-process |
| Net/Crypto C files | Sockets, TLS, crypto | Must be loadable in-process |

## 3. LLVM ORC JIT — The Candidate

### 3.1 What Is ORC JIT?

LLVM's **ORC (On-Request Compilation)** JIT framework compiles LLVM IR to native code **in memory** without producing object files or invoking a linker. It's the same technology behind:

- **`lli`** — LLVM's IR interpreter/JIT executor
- **Clang REPL** / **`clang-repl`** — C++ REPL
- **Julia** — compiles Julia code via ORC JIT
- **Swift Playgrounds** — JIT execution of Swift code

ORC JIT v2 (current) uses a **lazy compilation model** with `JITDylib` symbol tables, `LLJIT` convenience layer, and platform-specific features (COFF on Windows, ELF on Linux).

### 3.2 Available Libraries (Already Compiled)

The TML LLVM installation at `src/llvm-install/lib/` **already includes** all required ORC JIT libraries:

| Library | Size | Purpose |
|---------|------|---------|
| `LLVMOrcJIT.lib` | 21.1 MB | Core ORC framework (LLJIT, ExecutionSession, layers) |
| `LLVMJITLink.lib` | 9.8 MB | Object linking in memory (replaces LLD for JIT) |
| `LLVMOrcTargetProcess.lib` | 2.2 MB | In-process execution support |
| `LLVMOrcShared.lib` | 0.3 MB | Shared utilities |
| `LLVMOrcDebugging.lib` | 1.5 MB | Debug info for JIT'd code |
| `LLVMExecutionEngine.lib` | 0.6 MB | Base execution engine |
| `LLVMRuntimeDyld.lib` | 2.3 MB | Legacy dynamic loader (fallback) |
| `LLVMMCJIT.lib` | 0.3 MB | Legacy MCJIT (fallback) |
| **Total** | **~38 MB** | Additional static lib size |

**Key finding: zero new compilation needed.** The libs exist, we just need to link them.

### 3.3 Binary Size Impact

Current `tml.exe` (monolithic): ~100MB.
Adding ORC JIT libs: +38MB static → estimated **+15-25MB** after dead code elimination.
New estimated size: **~115-125MB** (monolithic) — a 15-25% increase.

In **modular build**, JIT would go into `tml_compiler.dll` (currently ~104MB → ~120MB).

### 3.4 Integration Point

The JIT path branches **after IR generation**, replacing the object+link+subprocess path:

```
                          compile_via_queries()
                                │
                          LLVM IR text (std::string)
                                │
                    ┌───────────┴───────────┐
                    │                       │
              [--jit mode]            [default mode]
                    │                       │
           LLJIT::addIRModule()    LLVMBackend::compile_ir_to_object()
                    │                       │
           Register C runtime       LLD::link() → .exe
           symbols in-process              │
                    │               std::system(exe)
           LLJIT::lookup("main")
                    │
           Execute in-process
```

## 4. Implementation Design

### 4.1 Core JIT Engine (`compiler/src/backend/jit_engine.cpp`)

```cpp
// Pseudocode — actual implementation would follow this structure

class TmlJitEngine {
    std::unique_ptr<llvm::orc::LLJIT> jit_;

public:
    // Initialize with host target
    static Expected<TmlJitEngine> create() {
        auto builder = LLJITBuilder();
        // Use in-process target (host machine)
        auto jit = builder.create();
        return TmlJitEngine(std::move(jit));
    }

    // Register C runtime symbols (tml_print, tml_alloc, etc.)
    Error registerRuntimeSymbols() {
        auto& dylib = jit_->getMainJITDylib();
        // Map each C runtime function to its in-process address
        return dylib.define(absoluteSymbols({
            {"tml_print",     pointerToJITTargetAddress(&tml_print)},
            {"tml_panic",     pointerToJITTargetAddress(&tml_panic)},
            {"tml_alloc",     pointerToJITTargetAddress(&tml_alloc)},
            {"tml_free",      pointerToJITTargetAddress(&tml_free)},
            // ... all @extern("c") functions
        }));
    }

    // Add compiled IR module
    Error addModule(StringRef ir_text) {
        auto ctx = std::make_unique<LLVMContext>();
        auto module = parseIR(MemoryBufferRef(ir_text, "tml_module"), err, *ctx);
        return jit_->addIRModule(ThreadSafeModule(std::move(module), std::move(ctx)));
    }

    // Execute main function
    Expected<int> executeMain(ArrayRef<std::string> args) {
        auto main_sym = jit_->lookup("main");
        if (!main_sym) return main_sym.takeError();

        auto main_fn = main_sym->toPtr<int(int, char**)>();
        return main_fn(args.size(), args.data());
    }
};
```

### 4.2 C Runtime Integration Strategy

This is the **highest risk** aspect. TML code calls C runtime functions via `@extern("c")`. In compiled mode, these are resolved by the linker. In JIT mode, we must register them manually.

**Approach: Compile C runtime into host process**

The C runtime files (`essential.c`, `mem.c`, collections, etc.) are already compiled as `.obj` files during the build process. Two options:

1. **Static link into tml.exe** — compile all C runtime `.c` files directly into the compiler binary. The JIT engine then uses `DynamicLibrarySearchGenerator::GetForCurrentProcess()` to resolve symbols from the host process. This is the simplest approach.

2. **Load as shared library** — compile C runtime into a `tml_runtime.dll` that the JIT engine loads via `EPCDynamicLibrarySearchGenerator`. More complex but avoids bloating `tml.exe`.

**Recommendation: Option 1 (static link).** The C runtime is ~50KB of code. Adding it to `tml.exe` has negligible size impact, and `GetForCurrentProcess()` resolves everything automatically — zero manual symbol registration needed.

### 4.3 Symbol Resolution Chain

```
JIT'd TML code calls tml_print()
  → ORC symbol lookup in JITDylib
  → Not found in IR modules
  → Falls through to DynamicLibrarySearchGenerator
  → Finds tml_print in host process (statically linked)
  → Resolved ✓
```

For system libraries (kernel32, ucrt, ws2_32):
```
JIT'd TML code calls CreateFileW() [via C runtime]
  → C runtime wrapper is in host process
  → Wrapper calls CreateFileW() normally
  → Works because host process already links these
  → Resolved ✓
```

### 4.4 CLI Integration

```bash
# Current (compiled, ~2-5s)
tml run hello.tml

# New JIT mode (proposed, ~200-500ms)
tml run --jit hello.tml          # explicit JIT
tml script hello.tml             # alias for --jit mode

# Testing with JIT (proposed)
tml test --jit                   # JIT all test suites instead of compiling EXEs
tml test --jit --suite core/str  # JIT single suite
```

### 4.5 Shebang Support

With JIT mode, TML files could be used as scripts:

```tml
#!/usr/bin/env tml script

func main() {
    print("Hello from TML script!")
}
```

## 5. Performance Analysis

### 5.1 Current Execution Latency (Compiled Mode)

| Phase | Time (cold) | Time (cached) |
|-------|-------------|---------------|
| Parse + Typecheck + Borrow | ~50-100ms | ~5-10ms (GREEN) |
| HIR → MIR → LLVM IR | ~50-150ms | ~5-10ms (GREEN) |
| LLVM IR → .obj (backend) | ~100-300ms | ~100-300ms |
| LLD linking | ~200-500ms | ~200-500ms |
| Subprocess launch | ~10-30ms | ~10-30ms |
| **Total** | **~400-1100ms** | **~320-850ms** |

### 5.2 Projected JIT Latency

| Phase | Time (cold) | Time (cached) |
|-------|-------------|---------------|
| Parse + Typecheck + Borrow | ~50-100ms | ~5-10ms (GREEN) |
| HIR → MIR → LLVM IR | ~50-150ms | ~5-10ms (GREEN) |
| IR parse + JIT compile | ~50-200ms | ~50-200ms |
| Symbol resolution | ~1-5ms | ~1-5ms |
| Execute in-process | ~0ms | ~0ms |
| **Total** | **~150-460ms** | **~60-220ms** |

**Improvement: 2-4x faster** — primarily by eliminating LLD linking and subprocess overhead.

### 5.3 Test Suite Impact

Current test execution (1659 tests):
- Each suite compiles to separate EXE → linking overhead per suite
- NDJSON subprocess protocol overhead
- Total: ~5-10 minutes (cached), ~10-15 minutes (cold)

With JIT:
- No linking per suite — IR → JIT directly
- No subprocess overhead — execute in-process
- **Estimated improvement: 30-50% faster** for the full suite
- Individual suite: ~2-3x faster

### 5.4 Memory Considerations

| Mode | Memory Pattern |
|------|---------------|
| Compiled (current) | Subprocess per suite → isolated memory, clean after exit |
| JIT (proposed) | In-process → all suites share address space |

**Risk:** Memory leaks in one test suite could affect others when running in-process. Mitigation: JIT mode could still spawn subprocesses for `tml test` (just using JIT inside each subprocess instead of LLD linking).

## 6. Comparison with Alternatives

### 6.1 Custom VM (Lua/Java-style)

| Aspect | LLVM ORC JIT | Custom VM |
|--------|-------------|-----------|
| Implementation effort | 4-6 weeks | 12-24 weeks |
| Semantic fidelity | 100% (same IR) | Must reimplement everything |
| Performance | Native speed | 10-100x slower |
| `lowlevel` support | Full (same codegen) | Must emulate ptr ops |
| Borrow checker | Already done (same pipeline) | Must reimplement or skip |
| Generics/monomorphization | Already done | Must reimplement |
| C FFI | Automatic (host process) | Complex bridging |
| Debugging | Full LLVM debug info | Custom debugger needed |
| Maintenance | Zero (same pipeline) | Every feature needs dual impl |
| Binary size impact | +15-25MB | +1-5MB |
| Behavioral divergence risk | Zero | High |

**Verdict: JIT wins on every dimension except binary size.** A custom VM would only make sense if TML were a much simpler language (like Lua) where reimplementation is trivial.

### 6.2 MIR Interpreter

| Aspect | LLVM ORC JIT | MIR Interpreter |
|--------|-------------|-----------------|
| Implementation effort | 4-6 weeks | 8-12 weeks |
| Performance | Native speed | ~10-50x slower |
| LLVM dependency | Required | Not needed |
| Startup latency | ~50-200ms JIT compile | ~0ms (interpret directly) |
| `lowlevel` support | Full | Must emulate every intrinsic |
| C FFI | Automatic | Manual bridging |

**Verdict:** MIR interpreter has faster startup for tiny programs but much slower execution. Only useful for a REPL where sub-10ms response is needed. For `tml run` and `tml test`, JIT is superior.

### 6.3 Incremental Linking

| Aspect | LLVM ORC JIT | Incremental Linking |
|--------|-------------|-------------------|
| Implementation effort | 4-6 weeks | 8-16 weeks |
| Performance gain | 2-4x | 1.5-2x |
| Complexity | Moderate | Very high (link cache, patching) |
| Platform support | LLVM handles it | Must implement per-platform |

**Verdict:** JIT is simpler, faster, and more portable than incremental linking.

## 7. Implementation Roadmap

### Phase 1: Core JIT Engine (2-3 weeks)

1. Add ORC JIT libraries to `CMakeLists.txt` (link `LLVMOrcJIT`, `LLVMJITLink`, etc.)
2. Create `compiler/src/backend/jit_engine.hpp/.cpp` wrapping `LLJIT`
3. Static-link C runtime into `tml.exe` for symbol resolution
4. Implement `JitEngine::create()` + `addModule()` + `executeMain()`
5. Wire into `tml run --jit` CLI path
6. Test: `tml run --jit hello.tml` produces correct output

### Phase 2: C Runtime & FFI (1-2 weeks)

1. Verify all `@extern("c")` symbols resolve from host process
2. Handle platform-specific symbols (Windows: `ucrt`, `kernel32`)
3. Test: programs using I/O, allocations, collections work under JIT
4. Test: programs using `lowlevel` blocks work under JIT

### Phase 3: Test Integration (1-2 weeks)

1. Add `--jit` flag to `tml test` command
2. Each test suite: compile to IR → JIT execute (instead of EXE + subprocess)
3. Adapt NDJSON protocol for in-process execution (or keep subprocess + JIT inside)
4. Coverage tracking compatibility
5. Test: full test suite passes with `--jit`

### Phase 4: Script Mode (1 week)

1. Add `tml script` command (alias for `tml run --jit`)
2. Shebang support (`#!/usr/bin/env tml script`)
3. Implicit `main()` detection (auto-wrap top-level code)
4. File watcher mode: `tml script --watch file.tml` (re-JIT on save)

### Phase 5: Optimization (ongoing)

1. Lazy compilation (JIT functions on first call, not upfront)
2. Module caching (cache JIT'd code across runs)
3. Concurrent compilation (JIT modules in parallel)
4. Profile-guided: auto-select JIT vs compiled based on program size

**Total estimated effort: 4-8 weeks** (varies with platform edge cases)

## 8. Risks and Mitigations

### 8.1 Windows SEH (Structured Exception Handling)

**Risk:** TML's `panic()` mechanism must work correctly when JIT'd code panics.

**Mitigation:** LLVM ORC on Windows uses COFF format and supports SEH. The `COFFPlatform` class (present in our LLVM 23 source) handles `.pdata`/`.xdata` registration for JIT'd code. `tml_panic` calls `exit()` which works in-process.

### 8.2 Address Space Conflicts

**Risk:** JIT'd code runs in the compiler's address space. A crash in JIT'd code crashes the compiler.

**Mitigation:** For `tml run --jit`, this is acceptable (same behavior as any interpreter). For `tml test --jit`, use subprocess-per-suite with JIT inside each subprocess (eliminates linking but preserves isolation).

### 8.3 Global State

**Risk:** Multiple JIT'd modules sharing global state (static variables, singletons).

**Mitigation:** Each `tml run --jit` invocation creates a fresh `LLJIT` instance. Globals are isolated per instance.

### 8.4 Debug Info

**Risk:** Debugging JIT'd code is harder than debugging compiled executables.

**Mitigation:** `LLVMOrcDebugging.lib` supports GDB JIT interface and Windows debugger integration. Not critical for v1 — primary use case is rapid iteration, not debugging.

## 9. Decision Matrix

| Criterion | Weight | Custom VM | MIR Interp | ORC JIT | Incr. Link |
|-----------|--------|-----------|-----------|---------|------------|
| Implementation effort | 25% | 2/10 | 4/10 | **8/10** | 3/10 |
| Semantic fidelity | 25% | 3/10 | 6/10 | **10/10** | 10/10 |
| Performance | 20% | 3/10 | 4/10 | **9/10** | 7/10 |
| Maintenance burden | 15% | 2/10 | 4/10 | **9/10** | 5/10 |
| Binary size impact | 10% | **9/10** | **9/10** | 6/10 | 8/10 |
| Scripting UX | 5% | 8/10 | 8/10 | **8/10** | 3/10 |
| **Weighted Score** | | **3.3** | **4.9** | **8.8** | **6.2** |

## 10. Recommendation

**Implement LLVM ORC JIT as the `tml run --jit` / `tml script` mode.**

Reasons:
1. **Zero semantic divergence** — uses the exact same compilation pipeline through IR
2. **Minimal effort** — ORC libraries already compiled and available (~38MB .lib)
3. **2-4x faster execution** — eliminates linking and subprocess overhead
4. **Enables scripting** — TML becomes usable for automation, prototyping, REPL
5. **Test speedup** — 30-50% improvement for full test suite
6. **No maintenance burden** — adding a new language feature works in JIT automatically (same codegen)

The only trade-off is +15-25MB binary size, which is acceptable for a compiler that's already ~100MB. The JIT mode would be **opt-in** (not default) — `tml build` and `tml run` continue to use the compiled path. `tml run --jit` and `tml script` use the JIT path.

A custom VM is **not recommended** because TML's type system (generics, borrow checker, `lowlevel` blocks, C FFI) would require reimplementing the entire compiler's semantic analysis in interpreter form — a 3-6 month effort with guaranteed behavioral divergence.

## 11. Open Questions

1. **Should `tml run` default to JIT?** — Could auto-select based on program size (JIT for small, compiled for large)
2. **REPL support?** — ORC JIT's lazy compilation enables an interactive REPL (`tml repl`)
3. **Hot reload?** — With JIT, could support `tml watch file.tml` that re-JITs on file changes without restarting
4. **Cross-compilation?** — JIT is host-only. `tml build --target` always uses the compiled path
5. **Optimization level for JIT?** — Default to O0 for fastest JIT compile, or O1 for balanced?
