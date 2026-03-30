# Proposal: phase0b_jit-engine-core

## Why
With ORC JIT libraries linked (phase0a), we need the actual JIT engine class that wraps LLVM's `LLJIT` API. This is the core component that takes LLVM IR text and executes it in-process, bypassing object file generation and LLD linking entirely.

## What Changes
- Create `compiler/include/backend/jit_engine.hpp` — public API
- Create `compiler/src/backend/jit_engine.cpp` — implementation wrapping `LLJIT`
- Core operations: `create()`, `addModule(ir_text)`, `lookup(symbol)`, `executeMain(args)`
- Uses LLVM C++ API (not C API) since ORC JIT has no C bindings

## Architecture
```
TmlJitEngine
  ├── LLJIT instance (manages JIT compilation)
  ├── JITDylib (symbol namespace)
  ├── IRCompileLayer (IR → native code in memory)
  └── ObjectLinkingLayer (resolves symbols without disk I/O)

Flow:
  IR text → LLVMContext::parseIR() → ThreadSafeModule → LLJIT::addIRModule()
  → LLJIT::lookup("main") → function pointer → call()
```

## Key Design Decisions
1. **Use `LLJIT` convenience class** — not raw `ExecutionSession` (simpler, sufficient)
2. **Use C++ API** — ORC JIT only has C++ bindings (OrcV2CBindings.cpp exists but is limited)
3. **One engine per execution** — create fresh LLJIT for each `tml run --jit` (avoids global state)
4. **Eager compilation** — compile all IR upfront (not lazy) for v1 simplicity

## Impact
- Affected specs: none
- Affected code: new files in `compiler/src/backend/`, `compiler/include/backend/`
- Breaking change: NO
- User benefit: Core infrastructure for JIT execution

## Dependencies
- Requires: phase0a (ORC JIT libraries linked)
