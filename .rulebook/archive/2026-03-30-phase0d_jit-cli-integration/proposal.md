# Proposal: phase0d_jit-cli-integration

## Why
The JIT engine (phase0b) and runtime symbols (phase0c) are infrastructure. This task wires them into the user-facing CLI so `tml run --jit hello.tml` and `tml script hello.tml` work end-to-end. This is what users will actually interact with.

## What Changes
- Add `--jit` flag to `tml run` command
- Add `tml script` command (alias for `tml run --jit`)
- Wire `builder_run.cpp` to branch after IR generation: if `--jit`, use JitEngine instead of LLVMBackend+LLD
- Add shebang support (`#!/usr/bin/env tml script`)

## Execution Flow
```
tml run --jit hello.tml
  → compile_via_queries()           [existing — unchanged]
  → result.llvm_ir                  [existing — the IR text string]
  → JitEngine::create()             [NEW — phase0b]
  → engine.addModule(result.llvm_ir)[NEW — phase0b]
  → engine.registerRuntime()        [NEW — phase0c]
  → engine.executeMain(args)        [NEW — phase0b]
  → return exit_code
```

vs current:
```
tml run hello.tml
  → compile_via_queries()
  → LLVMBackend::compile_ir_to_object() → .obj file
  → LLD::link() → .exe file
  → std::system(exe_path) → subprocess
  → return exit_code
```

## Impact
- Affected specs: CLI spec (new `--jit` flag, new `script` subcommand)
- Affected code: `cmd_run.hpp`, `builder_run.cpp`, `main.cpp` (add script command)
- Breaking change: NO (new flag, existing behavior unchanged)
- User benefit: 2-4x faster `tml run`, scripting support, no LLD failures

## Dependencies
- Requires: phase0c (runtime symbols resolve correctly)
