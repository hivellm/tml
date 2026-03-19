# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⛔ MANDATORY: Consult Language Reference Before Implementing ⛔

**Before writing ANY new TML code, you MUST read [docs/readme.md](docs/readme.md).**

This is a HARD REQUIREMENT. The TML standard library has **500+ types, 5000+ functions**, and **template literals** already implemented. Past implementations ignored existing APIs (Buffer, Text, HashMap, List, Slice, Outcome, TcpStream, Mutex, template literals) and instead used raw `lowlevel` blocks everywhere — producing 692 unnecessary unsafe blocks in the HTTP module alone.

**Rules:**

1. **ALWAYS check [docs/readme.md](docs/readme.md)** for existing types before using `lowlevel { ptr_read/ptr_write/mem_alloc }`
2. **Use `Text` for string building** — not manual `copy_nonoverlapping` chains
3. **Use `Buffer` for byte manipulation** — not raw `ptr_read[U8]`/`ptr_write[U8]`
4. **Use `HashMap`/`List` for collections** — not manual array+offset layouts
5. **Use `Outcome[T,E]` with `!`** — not raw I64 error codes
6. **Use template literals** — `` `Hello, {name}!` `` returns `Text`, works today
7. **Use `Mutex[T]`/`Sync[T]` for shared state** — not manual memory layouts with offsets

**The ONLY acceptable uses of `lowlevel` are:**
- FFI calls to C runtime (`@extern("c")` wrappers)
- Performance-critical inner loops where profiling proves the abstraction overhead matters
- Implementing core library primitives (the types listed in docs/readme.md themselves)

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## Sandbox Directory (`.sandbox/`)

Scratch space for temp files, IR dumps, experiments. Gitignored. Use freely, no permission needed.

## ⛔ MANDATORY: Use MCP Tools First ⛔

**YOU MUST USE MCP TOOLS AS YOUR PRIMARY INTERFACE FOR ALL TML OPERATIONS.**

This is a HARD REQUIREMENT. The MCP server (`mcp__tml__*`) provides dedicated tools for:

- **`mcp__tml__test`** — Running tests (use `path` for specific files, `filter` for name matching, `suite` for module-level filtering like `"core/str"` or `"std/json"`)
- **`mcp__tml__run`** — Building and running TML source files
- **`mcp__tml__build`** — Building TML source to executable
- **`mcp__tml__compile`** — Compiling TML source files
- **`mcp__tml__check`** — Type checking without compiling
- **`mcp__tml__emit-ir`** — Emitting LLVM IR for debugging
- **`mcp__tml__emit-mir`** — Emitting MIR for debugging
- **`mcp__tml__format`** — Formatting TML source files
- **`mcp__tml__lint`** — Linting TML source files
- **`mcp__tml__docs_search`** — Searching TML documentation
- **`mcp__tml__cache_invalidate`** — Invalidating stale caches

**Rules:**

1. **NEVER use Bash/PowerShell** to run `tml.exe test`, `tml.exe build`, `tml.exe run`, etc. when the equivalent MCP tool exists
2. **NEVER use Bash** to grep test output — use the MCP tool's structured output instead
3. The ONLY acceptable use of Bash for `tml.exe` is when you need to build the **compiler itself** (`scripts\build.bat`)
4. MCP tools handle caching, path resolution, and output formatting automatically

**WHY:** MCP tools are purpose-built for this workflow. They strip ANSI codes, handle Windows path normalization, validate meta caches, and provide clean structured output. Using Bash/PowerShell bypasses all of this and wastes tokens on noisy output.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: NO `rm` COMMANDS ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING `rm` COMMANDS WITHOUT EXPLICIT USER AUTHORIZATION.**

This includes but is not limited to:

- ❌ `rm -rf` on ANY directory
- ❌ `rm` on cache files (`.test-cache.json`, `.run-cache/`, etc.)
- ❌ `rm` on backup directories
- ❌ ANY deletion command without the user typing "yes, delete it" or similar explicit approval

**WHY:** The cache backup system exists specifically because you kept deleting caches. DO NOT DELETE ANYTHING. If you think something needs to be deleted, ASK THE USER FIRST.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: No Git Branch Manipulation Without Authorization ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING ANY GIT COMMAND THAT ALTERS BRANCH STATE, HISTORY, OR STASHED WORK WITHOUT EXPLICIT USER AUTHORIZATION.**

This includes but is not limited to:

- ❌ `git stash` / `git stash pop` / `git stash drop`
- ❌ `git rebase` (any form — interactive, onto, autosquash)
- ❌ `git reset` (soft, mixed, or hard)
- ❌ `git checkout -- .` / `git restore .` (discarding changes)
- ❌ `git revert` (creating revert commits)
- ❌ `git cherry-pick`
- ❌ `git merge` (merging branches)
- ❌ `git branch -D` / `git branch -d` (deleting branches)
- ❌ `git push --force` / `git push --force-with-lease`
- ❌ `git clean -f` / `git clean -fd`
- ❌ `git checkout <branch>` (switching branches)
- ❌ ANY command that rewrites history, moves HEAD, discards uncommitted work, or changes the current branch

**The ONLY git commands allowed without explicit authorization are:**

- ✅ `git status` — viewing state
- ✅ `git diff` — viewing changes
- ✅ `git log` — viewing history
- ✅ `git add` — staging files (when committing with user approval)
- ✅ `git commit` — creating commits (when user asks to commit)
- ✅ `git blame` — viewing authorship

**If you believe a branch-altering operation is needed, you MUST:**

1. Explain what you want to do and WHY
2. Show the exact command you would run
3. Wait for the user to explicitly say "yes", "do it", "go ahead", or similar approval
4. Only then execute the command

**WHY:** Unauthorized branch manipulation has destroyed in-progress work, lost uncommitted changes, and created merge nightmares. The cost of asking is zero. The cost of unauthorized manipulation can be hours of lost work.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: Never Run Tests Multiple Times to Filter Output ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING THE TEST SUITE MULTIPLE TIMES TO GREP/FILTER DIFFERENT PARTS OF THE OUTPUT.**

The test suite takes significant time and CPU. Running it once to get results and then running it AGAIN just to grep for a different pattern is **unacceptable waste of processing and time**.

**Rules:**

1. **Run the test suite ONCE** — save or read the full output
2. **NEVER pipe test output through grep** and then re-run to pipe through a different grep
3. **NEVER run tests just to get a summary** if you already ran them and have the output
4. If you need specific data from test output, read the log file or scroll through the existing output
5. Use `mcp__tml__test` with `structured: true` to get parsed results in a single call
6. If the MCP structured output doesn't have what you need, run ONCE via Bash and redirect to a file in `.sandbox/`, then read that file as many times as needed

**WRONG (wastes 2x-5x processing time):**

```bash
# ❌ Run tests, grep for failures
tml test --no-cache 2>&1 | grep FAIL
# ❌ Run tests AGAIN, grep for timing
tml test --no-cache 2>&1 | grep -E "Slowest|Profile"
# ❌ Run tests AGAIN, grep for summary
tml test --no-cache 2>&1 | grep -E "passed|failed"
```

**CORRECT (run once, read many):**

```bash
# ✅ Run once, save output
tml test --no-cache 2>&1 > .sandbox/test_output.log
# ✅ Read the file for whatever you need
grep FAIL .sandbox/test_output.log
grep Profile .sandbox/test_output.log
```

**WHY:** Each test run recompiles ALL test suites and executes ALL tests. This takes minutes of CPU time. Running it 3 times to grep 3 different patterns wastes 2/3 of the total processing time for zero benefit.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ MANDATORY: Analyze Before Executing ⛔

**YOU MUST ANALYZE PROJECT PATTERNS AND CONVENTIONS BEFORE EXECUTING ANY TASK.**

This is a HARD REQUIREMENT because rushing to execute tasks without analysis leads to:

- ❌ Wrong folder structures (e.g., creating `archived/` instead of using existing `archive/`)
- ❌ Wrong naming conventions (e.g., missing date prefixes like `YYYY-MM-DD-task-name`)
- ❌ Wrong file formats (e.g., not following test patterns)
- ❌ Wasted tokens fixing your own mistakes
- ❌ Frustrated users

**BEFORE executing any task that involves project conventions, you MUST:**

1. **Check existing examples first** - Look at how similar tasks were done before

   ```bash
   # Example: Before archiving a task, check the archive folder structure
   ls rulebook/tasks/archive/
   # Learn: Tasks are named YYYY-MM-DD-task-name
   ```

2. **Read relevant documentation** - Check RULEBOOK.md, AGENTS.md, or related docs

3. **Identify patterns** - Note naming conventions, folder structures, file formats

4. **Only then execute** - After understanding the correct approach

**Examples of tasks that REQUIRE analysis first:**

- Archiving tasks → Check `archive/` folder naming pattern
- Creating tests → Check existing test file patterns (`use test`, return types, etc.)
- Creating tasks → Check `rulebook/RULEBOOK.md` for format
- Adding new modules → Check existing module structures

**WHY:** Executing quickly without analysis causes MORE errors, which requires MORE fixes, which wastes MORE tokens and time. Taking 30 seconds to analyze saves minutes of corrections.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: No Shortcuts, Stubs, Placeholders, or Simplified Logic ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM TAKING SHORTCUTS TO DELIVER RESULTS FASTER.**

Response time is NOT important. What matters is the QUALITY of the final result. When given a task, you MUST find the correct way to implement it and deliver a proper, complete implementation — regardless of complexity.

**This is NON-NEGOTIABLE. You MUST follow these rules:**

1. **NEVER simplify logic** to make implementation easier or faster
2. **NEVER add TODO/FIXME/HACK comments** as placeholders for unfinished work
3. **NEVER create stubs** — implement the real functionality
4. **NEVER create placeholder implementations** that "work for now" but aren't correct
5. **NEVER alter existing logic** to avoid dealing with complexity
6. **NEVER reduce scope** of what was requested to deliver something quicker
7. **NEVER skip edge cases** or error handling that the correct implementation requires
8. **NEVER deliver partial implementations** claiming "the rest can be added later"

**What you MUST do instead:**

- ✅ **Research the correct approach** — read existing code, understand patterns, find the right solution
- ✅ **Implement completely** — the full functionality as requested, with all edge cases
- ✅ **Take as long as needed** — there is no time pressure, only quality pressure
- ✅ **Ask for clarification** if the task is ambiguous, rather than guessing and delivering something wrong
- ✅ **Fix root causes** — never patch symptoms to make things "appear to work"

**WHY:** Quick, incomplete implementations create technical debt, hide bugs, and require rework. A proper implementation done once is always better than a quick hack that needs to be redone. The user explicitly values correctness and completeness over speed.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ MANDATORY: Minimize C and C++ Code ⛔

**The TML project is actively migrating away from C/C++ toward pure TML. You MUST NOT add new C or C++ code unless absolutely necessary.**

This is a HARD REQUIREMENT aligned with the project roadmap (see [docs/ROADMAP.md](docs/ROADMAP.md)).

### Three-Tier Rule for New Implementations

When implementing new functionality, follow this decision hierarchy:

1. **Pure TML** (STRONGLY PREFERRED) — Use TML's existing memory intrinsics (`ptr_read`, `ptr_write`, `ptr_offset`, `mem_alloc`, `mem_free`, `copy_nonoverlapping`) to implement algorithms directly in `.tml` files. This includes: string operations, collections, formatting, sorting, search algorithms, data structures, math utilities, parsers, serialization.

2. **`@extern("c")` FFI to existing libraries** (ACCEPTABLE) — When calling external system libraries (LLVM, OpenSSL/BCrypt, zlib, libc, OS APIs). Do NOT reimplement what these libraries already provide. Declare `@extern("c")` bindings in TML and call them.

3. **New C/C++ code** (LAST RESORT ONLY) — Only for functionality that genuinely cannot be expressed in TML or as FFI bindings. Examples: OS-level I/O (print, file read/write), panic/abort handlers, test harness DLL entry points.

### What This Means in Practice

**NEVER do this:**

- ❌ Add new `.c` files to `compiler/runtime/` for algorithms that TML can express
- ❌ Add new `lowlevel` blocks in `.tml` files that call C functions for pure logic (string manipulation, collection operations, math formatting)
- ❌ Create C wrapper functions when `@extern("c")` to an existing library suffices
- ❌ Add new C++ code to the compiler for features that could be implemented as TML library code
- ❌ Use the C runtime as a shortcut instead of implementing properly in TML

**ALWAYS do this:**

- ✅ Implement new algorithms in pure TML using memory intrinsics
- ✅ Use `@extern("c")` for system APIs, crypto, compression, networking
- ✅ Keep `compiler/runtime/core/essential.c` as the ONLY essential C runtime (I/O, panic, test harness)
- ✅ When fixing a bug in existing C runtime code, consider if it's an opportunity to migrate that function to TML

### Current C Code That MUST NOT Grow

| Location                            | Purpose                  | Status                         |
| ----------------------------------- | ------------------------ | ------------------------------ |
| `compiler/runtime/core/essential.c` | I/O, panic, test harness | KEEP — essential               |
| `compiler/runtime/memory/mem.c`     | malloc/free wrappers     | KEEP — OS interface            |
| `compiler/runtime/collections/`     | List, HashMap, Buffer    | MIGRATE — do not add code here |
| `compiler/runtime/text/`            | String/Text algorithms   | MIGRATE — do not add code here |
| `compiler/runtime/math/`            | Number formatting        | MIGRATE — do not add code here |
| `compiler/runtime/search/`          | BM25, HNSW, distance     | MIGRATE — do not add code here |
| `lib/std/runtime/`                  | Duplicate C files        | MIGRATE — do not add code here |
| `lib/test/runtime/`                 | Coverage tracking        | KEEP — lock-free atomics       |

**WHY:** The project is on a path to self-hosting (compiler rewritten in TML). Every new line of C/C++ code is debt that must be rewritten later. Pure TML implementations serve double duty: they work today AND they prepare for self-hosting.

**See also:** [ROADMAP.md](docs/ROADMAP.md) Phase 4 (Runtime Migration), Phase 6 (Self-Hosting)

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## Project Overview

**TML (To Machine Language)** is a programming language designed for LLM code generation and analysis. Contains: compiler (`/compiler/`), standard library (`/lib/`), and language spec (`/docs/`). Source files use `.tml` extension.

## Build Commands

**⚠️⚠️⚠️ CRITICAL: NEVER USE CMAKE DIRECTLY! ⚠️⚠️⚠️**

**THIS IS A HARD REQUIREMENT - NO EXCEPTIONS:**

- ❌ NEVER run `cmake --build`
- ❌ NEVER run `cmake -B`
- ❌ NEVER run any direct cmake commands
- ❌ NEVER use powershell/cmd to call cmake

**WHY:** Direct cmake calls CORRUPT the build directory, cause silent failures, break incremental compilation, and waste time. The build scripts handle critical environment setup that cmake alone cannot.

**ENFORCED:** The CMakeLists.txt has a build token check that will FAIL with a fatal error if you try to use cmake directly. Only the build scripts pass the required token.

**ALWAYS use the provided scripts:**

```bash
scripts\build.bat              # Debug build (default, monolithic ~100MB)
scripts\build.bat release      # Release build
scripts\build.bat --clean      # Clean build
scripts\build.bat --tests      # Also build C++ unit tests (tml_tests.exe)
scripts\build.bat --modular    # Modular build (thin launcher + plugin DLLs)
```

**⚠️ EXACT BUILD COMMAND (MANDATORY) ⚠️**

```bash
cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat" 2>&1
```

### Modular Build (Plugin Architecture)

`--modular` produces thin launcher + plugin DLLs. Key files: `plugin/abi.h` (pure C ABI), `plugin/loader.hpp`, `src/launcher/main_launcher.cpp`, `src/plugin/*_plugin.cpp`.

## Test Commands

**⚠️ USE MCP TOOLS — NOT BASH — FOR ALL TEST OPERATIONS ⚠️**

The new test system (`compiler/src/testing/`) uses a subprocess-based architecture (Go model):
- Each test suite compiles to an EXE and runs as a subprocess
- NDJSON protocol streams results from subprocess to coordinator
- Coverage via `TML_COVERAGE_FILE` env var — no LLVM profiling, no hangs

**Via MCP (MANDATORY):**
```
mcp__tml__test                                     # full suite
mcp__tml__test with suite="core/str"               # core/str → lib/core/tests/str/
mcp__tml__test with suite="std/json"               # std/json → lib/std/tests/json/
mcp__tml__test with path="lib/core/tests/str/basic.test.tml"  # single file
mcp__tml__test with coverage=true, no_cache=true   # coverage run (no hangs)
mcp__tml__test with structured=true                # parsed JSON results
```

**Bash fallback (only when MCP tool times out — run ONCE, save to file):**
```bash
cd f:/Node/hivellm/tml && build/debug/bin/tml.exe test --profile --verbose --no-cache --coverage 2>&1
```

**CRITICAL: NEVER DELETE TEST CACHES!** (`build/debug/.new-test-cache.json`, `build/debug/.incr-cache/`). They auto-invalidate on source changes.

Output: `build/debug/bin/tml.exe` (debug), `build/release/bin/tml.exe` (release), `build/debug/bin/tml_tests.exe` (C++ unit tests).

## Key Design Decisions

TML syntax optimized for LLM comprehension — keywords over symbols:

| Rust | TML | Reason |
|------|-----|--------|
| `<T>` | `[T]` | `<` conflicts with comparison |
| `\|x\| expr` | `do(x) expr` | `\|` conflicts with OR |
| `&&` `\|\|` `!` | `and` `or` `not` | Keywords clearer |
| `fn` / `match` | `func` / `when` | More explicit |
| `for`/`while`/`loop` | `loop` unified | Single keyword |
| `&T` / `&mut T` | `ref T` / `mut ref T` | Words over symbols |
| `trait` | `behavior` | Self-documenting |
| `Option`/`Result` | `Maybe`/`Outcome` | Intent clear |
| `Some`/`None` | `Just`/`Nothing` | Self-documenting |
| `unsafe` | `lowlevel` | Less scary, accurate |
| Lifetimes `'a` | Always inferred | No syntax noise |

## Project Structure

```
tml/
├── compiler/           # C++ compiler implementation
│   ├── src/            # lexer/, parser/, types/, borrow/, hir/, mir/, codegen/,
│   │                   # query/, backend/, cli/ (commands/, builder/),
│   │                   # testing/ (new test system), format/, plugin/, launcher/
│   ├── include/        # Headers (plugin/abi.h, query/, codegen/, testing/)
│   ├── runtime/        # Essential C runtime (essential.c, mem.c)
│   └── tests/          # C++ unit tests (process, protocol, dispatcher, cache)
├── lib/                # TML standard libraries
│   ├── core/           # Core (alloc, iter, slice, str, fmt, error)
│   ├── std/            # Std (collections, file, json, crypto)
│   └── test/           # Test framework (assert_eq, etc.)
├── docs/               # Language spec (01-OVERVIEW through 14-EXAMPLES)
├── scripts/            # Build scripts (build.bat, test.bat, clean.bat)
└── build/              # Build output (debug/, release/)
```

## Compilation Architecture

Query-based demand-driven pipeline (like rustc):

```
Source → QueryContext → ReadSource → Tokenize → Parse → Typecheck
       → Borrowcheck → HirLower → MirBuild → CodegenUnit → LLVM → .obj → LLD → .exe
```

Key: memoized queries, incremental compilation (fingerprints in `.incr-cache/incr.bin`), embedded LLVM + LLD (in-process, no subprocesses).

## Conditional Compilation

TML supports `#if`/`#elif`/`#endif`/`#ifdef`/`#ifndef` directives. Predefined symbols: `WINDOWS`, `LINUX`, `MACOS`, `X86_64`, `ARM64`, `DEBUG`, `RELEASE`, `TEST`, etc.

## MANDATORY: Rust-as-Reference IR Methodology

**When fixing codegen bugs or optimizing the TML compiler's LLVM IR output, you MUST use Rust as the reference implementation.**

This is a HARD REQUIREMENT. The TML compiler aims to produce IR of the same quality as `rustc`. Rust's IR is the gold standard for correctness, safety, and optimization.

### Workflow (MUST follow for every codegen task)

1. **Write equivalent code in BOTH languages:**
   - Create `.sandbox/temp_<feature>.rs` (Rust version)
   - Create `.sandbox/temp_<feature>.tml` (TML version with equivalent semantics)
   - Both files must exercise the EXACT same pattern (same struct, same methods, same calls)

2. **Generate IR from both compilers:**
   ```bash
   # Rust IR (debug)
   rustc --edition 2021 --emit=llvm-ir -C opt-level=0 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_debug.ll

   # Rust IR (release)
   rustc --edition 2021 --emit=llvm-ir -C opt-level=3 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_release.ll

   # TML IR (debug)
   tml build .sandbox/temp_<feature>.tml --emit-ir --legacy
   # Then copy: cp build/debug/temp_<feature>.ll .sandbox/temp_<feature>_tml_debug.ll

   # TML IR (release)
   tml build .sandbox/temp_<feature>.tml --emit-ir --legacy --release
   # Then copy: cp build/debug/temp_<feature>.ll .sandbox/temp_<feature>_tml_release.ll
   ```

3. **Compare function-by-function:**
   - Instruction count (TML must not exceed 2x Rust for equivalent logic)
   - Type layouts (struct/enum sizes should match)
   - Alloca count (TML should not have allocas that Rust avoids)
   - Safety features (overflow checks, null checks)
   - Call overhead (unnecessary wrappers, extra indirection)

4. **Fix the TML codegen** to match or exceed Rust's quality, then verify with the test suite.

### Key Optimization Targets (from IR comparison)

| Issue | Current TML | Rust Reference | Priority |
|-------|-------------|---------------|----------|
| `Maybe[I32]` layout | 16 bytes `{ i32, [1 x i64] }` | 8 bytes `{ i32, i32 }` | HIGH |
| Struct constructors | alloca+store+load (10 instr) | `insertvalue` (3 instr) | HIGH |
| Runtime declarations | 500+ lines unconditionally | Only what's used | MEDIUM |
| Integer arithmetic | `add nsw` (UB on overflow) | Checked with panic | MEDIUM |
| Exception handling | None | `invoke` + `cleanuppad` | LOW |

### Rulebook Task (Living Document)

Full task details: `rulebook/tasks/optimize-codegen-like-rust/`

**This task is incremental and deferred.** Whenever you discover a codegen inefficiency during ANY work, you MUST update the task's `tasks.md` with the new finding.

**VIOLATION OF THIS METHODOLOGY IS UNACCEPTABLE when working on codegen.**

## Important Development Rules

**NEVER simplify or comment out tests!** Fix the compiler/library, not the test.

### MANDATORY: No Test Circumvention

**This is NON-NEGOTIABLE. You MUST follow these rules:**

1. **NEVER move tests to `pending/` folders** - All tests must live in the main `tests/` directory
2. **NEVER create placeholder implementations** - Implement the actual functionality
3. **NEVER simplify test assertions** - Fix the code, not the test
4. **NEVER create stubs** - Write real implementations
5. **NEVER comment out failing tests** - Fix the underlying issue
6. **NEVER skip tests** - Every test must pass

When a test fails:
- Investigate the root cause in the compiler or library
- Implement the missing codegen, type checking, or runtime functionality
- Keep working until the test passes
- Do NOT invent creative ways to bypass the test

If a test reveals a bug that requires significant work:
- Create a task in `rulebook/tasks/` to track the fix
- Fix the bug properly, don't defer it
- The test stays in place and must pass before committing

### MANDATORY: Incremental Test Development

**This is NON-NEGOTIABLE. You MUST follow this workflow when writing tests:**

1. **Write tests incrementally** - Create 1-3 tests at a time, NOT entire test files at once
2. **Test immediately after writing** - Run the individual test file before moving to the next
3. **Fix errors before proceeding** - If a test fails, fix it before writing more tests
4. **Use individual test execution** - NEVER run full test suite when developing tests

**Correct workflow:**
```bash
# Write 1-3 tests in a file
# Run ONLY that specific test file:
tml test path/to/specific.test.tml
# OR use MCP tool: mcp__tml__test with path parameter
# Fix any errors, then write more tests. Repeat.
```

**Coverage Updates:** After completing a block of tests, run `tml test --coverage`.

## Rulebook Integration

Uses [@hivehub/rulebook](https://www.npmjs.com/package/@hivehub/rulebook) v3.2.0+ for task management, persistent memory, and Ralph (autonomous AI iteration loops).

**Key rules:** Read AGENTS.md first. Use Rulebook tasks for features. Validate before commit. Use Ralph for complex multi-iteration tasks.

### Persistent Memory (MANDATORY)

**You MUST actively use memory to preserve context** via `mcp__rulebook__rulebook_memory_*` tools.

- **Save** on: architectural decisions, bugfixes, discoveries, features, errors, session summaries
- **Search** at: session start, when working on previously-touched code, when needing past context

### tasks.md Format

All `rulebook/tasks/*/tasks.md` must be **simple checklists only** — no prose, no code examples, no root cause analysis. Use `proposal.md` for detailed documentation.

## File Editing Best Practices

Read and edit files **sequentially** (Read file1 → Edit file1 → Read file2 → Edit file2). Never batch parallel reads before edits.

## ⛔ MANDATORY: Agent Delegation & Model Optimization ⛔

**The main conversation (opus) serves ONLY for coordination, planning, and user communication. ALL substantial work MUST be delegated to specialized agents with cost-appropriate models.**

### Model Assignment by Complexity

| Model | Cost | Use For |
|-------|------|---------|
| **opus** | $$$ | Compiler codegen bugs, deep analysis, code review of C++ core |
| **sonnet** | $$ | Tests, TML library code, build system, specs, task management |
| **haiku** | $ | Codebase exploration, documentation, research, file searches |

### Agent → Model Mapping

| Agent | Model | Rationale |
|-------|-------|-----------|
| `codegen-debugger` | opus | Traces values through compilation pipeline |
| `deep-analysis-reviewer` | opus | Root cause analysis of complex bugs |
| `compiler-optimizer` | opus | LLVM IR quality optimization |
| `tml-library-engineer` | opus | Core/std library in TML (complex type system) |
| `test-coverage-guardian` | sonnet | Test diagnosis follows established patterns |
| `build-engineer` | haiku | Build scripts are mechanical/repetitive |
| `c-to-tml-migrator` | sonnet | Migration follows clear patterns |
| `spec-engineer` | sonnet | Documentation with technical accuracy |
| `project-manager` | sonnet | Task tracking and coordination |
| `researcher` | haiku | Read-only codebase exploration |
| `implementer` | sonnet | Code following established patterns |
| `tester` | sonnet | Test writing follows patterns |
| `team-lead` | sonnet | Coordination and delegation |
| `qa-code-analyst` | sonnet | Code quality analysis |

### Delegation Rules

1. **NEVER write substantial code directly in the main conversation** — delegate to the appropriate agent
2. **NEVER do broad codebase exploration in the main conversation** — delegate to `researcher` (haiku, ~20x cheaper)
3. **After implementing code, launch in parallel:**
   - `test-coverage-guardian` to verify no regressions
   - `spec-engineer` to update docs if needed
4. **The main conversation handles ONLY:**
   - Understanding user intent
   - Choosing which agent(s) to dispatch
   - Reporting results back to the user
   - Quick, targeted edits (< 5 lines)

### ⛔ MANDATORY: Use Teams for Multi-Agent Work ⛔

**When launching 2 or more agents that need to work in parallel, you MUST use Teams (via `team-lead` agent or `TeamCreate`) instead of spawning individual agents.**

This is a HARD REQUIREMENT. Teams provide:
- **Named agents** addressable via `SendMessage({to: name})`
- **Coordinated execution** with shared context and handoff
- **Resource efficiency** — avoids duplicate work between agents
- **Visibility** — the user can see all agents and their status

**Rules:**

1. **2+ parallel agents = MUST use a Team** — never spawn multiple independent agents when a team can coordinate them
2. **Use `team-lead` (sonnet)** as the orchestrator — it dispatches to specialists and aggregates results
3. **Name each agent** in the team for clear identification (e.g., `name: "tester"`, `name: "reviewer"`)
4. **Single agent = no team needed** — only use teams for genuinely parallel multi-agent work

**WRONG:**
```
# ❌ Spawning 3 independent agents without coordination
Agent(prompt="run tests", subagent_type="tester")
Agent(prompt="review code", subagent_type="code-reviewer")
Agent(prompt="update docs", subagent_type="docs-writer")
```

**CORRECT:**
```
# ✅ Team-led coordinated execution
Agent(prompt="Coordinate: (1) run tests, (2) review code, (3) update docs",
      subagent_type="team-lead", name="coordinator")
```

**WHY:** Independent agents duplicate research, compete for resources, and produce inconsistent results. Teams share context, avoid duplicate work, and produce coherent output. The overhead of team setup is negligible compared to the waste of uncoordinated parallel agents.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**