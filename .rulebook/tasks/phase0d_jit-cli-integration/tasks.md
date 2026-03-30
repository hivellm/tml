# phase0d: JIT CLI Integration — 0/14 complete

## 1. `tml run --jit` Flag
- [ ] 1.1 Add `--jit` flag to `cmd_run.hpp` argument parser
- [ ] 1.2 Pass `jit` flag through to `builder_run.cpp` execution path
- [ ] 1.3 In `builder_run.cpp`: after `compile_via_queries()`, branch on `--jit` flag
- [ ] 1.4 JIT path: create JitEngine → addModule(result.llvm_ir) → executeMain()
- [ ] 1.5 Compiled path: unchanged (LLVMBackend → LLD → subprocess)

## 2. `tml script` Command
- [ ] 2.1 Add `script` subcommand to CLI dispatcher (same as `run --jit`)
- [ ] 2.2 Accept positional file argument: `tml script hello.tml`
- [ ] 2.3 Accept `--args` for passing arguments to the script
- [ ] 2.4 Shebang support: detect `#!/usr/bin/env tml script` and skip it during parse

## 3. Error Handling
- [ ] 3.1 If JIT not available (`TML_HAS_JIT=0`), print clear error: "JIT mode requires LLVM ORC libraries"
- [ ] 3.2 JIT compilation errors: display same error format as compiled mode
- [ ] 3.3 JIT runtime crash: catch signals (SIGSEGV, etc.) and print stack trace if possible

## 4. End-to-End Validation
- [ ] 4.1 `tml run --jit` on hello world — correct output, exit code 0
- [ ] 4.2 `tml script` on hello world — same result
- [ ] 4.3 `tml run --jit` on program with collections, I/O, template literals — all work
