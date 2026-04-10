<!-- OVERRIDE:START -->
# TML Project — Overrides (Highest Precedence)

These rules override `AGENTS.md` and generic rulebook guidance. Only TML-specific items live here; universal rules stay in `AGENTS.md` / `/.rulebook/specs/`.

---

## T0. NEVER mark a task as "blocked" — FIX IT (ABSOLUTE RULE)

**NEVER write "BLOCKED", "blocker", or "blocked by" in tasks.md or any status update.** If something is broken, FIX IT. Do not stop. Do not summarize. Do not write a report about what's broken and wait for the next session. The task is not blocked — YOU haven't finished fixing it yet.

- If a codegen bug prevents the frontend from working → **fix the codegen bug NOW**, in this session.
- If a type mismatch crashes at runtime → **trace the root cause and fix it NOW**.
- If the fix reveals another bug → **fix that one too**. Keep going until it works.
- If you've tried 2 approaches and both failed → try a THIRD approach. Research deeper. Read the IR. Add debug prints. Trace the exact byte. Do NOT stop and label it "blocked".
- The only acceptable output is **working code** or an explicit user decision to pause.
- Writing "Phase 3 blocked by codegen bug" is FORBIDDEN. Instead: fix the codegen bug, then continue Phase 3.
- This rule overrides the "fail twice → escalate" rule from AGENTS.md. You escalate to the USER only if you genuinely cannot figure out the root cause after exhaustive investigation — not after 2 attempts.

---

## T1. Build & Toolchain

- **NEVER run `cmake` directly.** Always use `scripts\build.bat` — CMakeLists.txt fails with a fatal error otherwise.
- Build command:
  ```bash
  cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat" 2>&1
  ```
- `tml_compiler.dll` (~104 MB) = ALL compiler code **including TML→IR**. `tml_codegen_x86.dll` (~78 MB) = only IR→obj + LLD linker.
  → For codegen bugs, rebuild **`tml_compiler_plugin`**, NOT `tml_codegen_x86_plugin`.
- Incremental Link Cache (`.ilk`) can serve stale code — delete it if strings are missing from the DLL.
- Zig CC is the preferred C/C++ toolchain (ADR-007), not MSVC.

---

## T2. MCP Tools First (mandatory)

**Never use `Bash` to run `tml.exe test/build/run` when an MCP tool or skill exists.**  
**Preference order: skill (`/name`) → direct MCP tool → Bash** (see T9/T10).

### MCP Tool Reference

| Task | MCP Tool | Skill shortcut |
|------|----------|----------------|
| Type check | `mcp__tml__check` (≈10× faster) | `/check` |
| Run tests | `mcp__tml__test` (`suite=`, `path=`, `structured=true`) | `/test` |
| Debug failure | `mcp__tml__test` + `debug_layers=true` | `/investigate` |
| LLVM IR | `mcp__tml__emit-ir` | `/emit-ir` |
| MIR | `mcp__tml__emit-mir` | `/emit-mir` |
| Format source | `mcp__tml__format` | `/format` |
| Compile binary | `mcp__tml__compile` | `/compile` |
| API docs | `mcp__tml__docs_{search,list,get,resolve}` | — |
| Memory leaks | `mcp__tml__debug(file, check_leaks=true)` | `/debug` |
| Crash backtrace | `mcp__tml__debug(file, backtrace=true)` | `/debug` |
| Build compiler | `mcp__tml__project_build` | `/build-compiler` |
| Tasks | `mcp__rulebook__rulebook_task_*` | `/tasks`, `/task-create` |
| Knowledge/learn | `mcp__rulebook__rulebook_knowledge_add`, `rulebook_learn_capture` | — |
| Session end | `mcp__rulebook__rulebook_session_end` | `/handoff` |

**Diagnostic-first order**: `/check` → fix → `/test`. Never run test before check.

**Never run tests multiple times to filter.** Run once, redirect to `.sandbox/test_output.log`, re-read the file.

**Never run the full test suite via MCP** — only specific suites/files.

---

## T3. TML Language Reference (mandatory before coding — BLOCKING)

**BEFORE writing any `.tml` code that uses a type, function, or module you did not author in this session, you MUST consult the documentation first.** This is not a suggestion — guessing at API shapes is the #1 source of wasted iterations (wrong argument count, wrong return type, wrong method name, wrong module path, inventing non-existent variants). Every minute spent in docs saves 10 minutes of failed edits.

### Lookup order (fastest → slowest)

1. **`mcp__tml__docs_search/list/get/resolve`** — primary interface. Covers `core`, `std`, `test`, and `compiler-tml`. 565 modules / 5132 items indexed.
2. **`tml doc <symbol>`** — CLI fallback when MCP is unavailable (`./build/debug/bin/tml.exe doc List`, `tml doc HashMap::get`, `tml doc core::str::split`).
3. **`docs/docs.json`** — raw JSON index (8.4 MB) regenerated via `tml doc --all --format=json --output=docs`. Grep it directly when you need to enumerate (e.g. "what methods does `BinaryWriter` have", "which variants does `SerialError` actually have"). Regenerate if stale.
4. **Source files (`lib/core/src/*.tml`, `lib/std/src/*.tml`, `compiler-tml/src/*.tml`)** — **last resort**, and only when you need to **modify** them, not to **use** them. Reading source to understand an API wastes tokens on impl details and often yields a misleading picture (private helpers, deprecated paths).

### Forbidden patterns

- ❌ Writing `use compiler::token::SomeName` without verifying `SomeName` exists in docs first.
- ❌ Pattern-matching on enum variants you have not looked up (e.g. writing `SerialError::BadMagic` when the real variant is `InvalidMagic` — this happened in phase13a and cost ~30min).
- ❌ Calling `x.method(a, b)` without verifying the arity and parameter types in docs.
- ❌ Assuming a C++ subsystem's API shape carries over to its TML counterpart. They drift.

### When in doubt

If `docs_search` does not return the symbol you expect, the symbol probably does not exist — STOP and re-search with variations before inventing code. Do NOT fall back to "reading a similar file and hoping". Either the API exists (find it in docs) or it needs to be created (open a task).

### Rust → TML quick reference

| Rust | TML |
|------|-----|
| `<T>` | `[T]` |
| `\|x\| expr` | `do(x) expr` |
| `&&` `\|\|` `!` | `and` `or` `not` |
| `fn` / `match` | `func` / `when` |
| `for`/`while`/`loop` | `loop` (unified) |
| `trait` | `behavior` |
| `Option`/`Result` | `Maybe`/`Outcome` |
| `Some`/`None` | `Just`/`Nothing` |
| `unsafe` | `lowlevel` |
| `let Some(x) = e else {}` | `let Just(x) = e else {}` |
| `x?.m()` optional chaining | `expr?.method()` |

### Syntax rules
- Use `let-else` for flat unwrapping instead of nested `when` cascades.
- Use `?.` for chained Maybe calls.
- `impl Behavior for Type` (not `impl Type with Behavior`).
- Enum variants: `Start(I64)` (no field names).
- Template literals `` `Hello, {name}!` `` return `Text`.

---

## T4. Implementation Discipline

- **No baby-stepping. Execute the full task end-to-end.** When given a task, complete every phase and every item before stopping. Do NOT pause between phases to ask "continue?", "want me to proceed?", "should I do phase N+1?". The only valid stops are: (a) genuine design ambiguity that requires a user decision, (b) destructive-op authorization, (c) the fail-twice rule, (d) the task is 100% complete (all items `[x]`, mandatory tail done).
- **Incremental quality, not incremental delivery**: still compile/check after each file and fix errors before moving on — but keep moving on. Incremental means small verified steps inside one continuous run, NOT stopping to confirm with the user between steps.
- **Rust-as-Reference IR**: when fixing codegen bugs, write equivalent `.rs` + `.tml`, compile both to LLVM IR, compare function-by-function. TML should not exceed ~2× Rust instruction count.
- **Analyze before executing**: check existing examples/conventions before restructuring.
- **If 2–3 fix attempts fail on the same error**, delete the broken code, re-analyze, pick a different approach (fail-twice rule). This is the ONLY mid-task escalation trigger besides design decisions.

### Minimize C/C++ code
1. **Pure TML** (preferred) — memory intrinsics in `.tml`.
2. `@extern("c")` FFI to existing libs — acceptable.
3. New C/C++ code — last resort only.

Do NOT add code to: `runtime/collections/`, `runtime/text/`, `runtime/math/`, `runtime/search/`, `lib/std/runtime/`.

---

## T5. THIR→MIR Single Path (since phase12a)

Legacy HIR→MIR was removed. All MIR fixes go in:
- `compiler/src/mir/thir_mir_builder.cpp` + `thir_mir_builder_expr.cpp`
- Method dispatch for MIR path → `emit_call_inst` in `instructions.cpp` (**NOT** `MethodCallInst`)

---

## T6. Known Critical Gotchas

- `base` is a reserved keyword (`KwBase`) — never use as a variable name.
- Integer literals `0`/`1` infer as `I32` — annotate `:I64` in index code.
- `bool`/`i1` struct fields must be `I64` to avoid layout bugs.
- `as RawPtr` in `lowlevel` blocks causes LLVM alloca error — remove; use pointer types directly.
- `type_implements` has false positives (workaround: `safe_types` whitelist for primitives only).
- Parser `skip_newlines()` bug was fixed (commit `a68f4c4f`) — only skip `Newline` tokens, never `DocComment`.

---

## T7. Testing & Quality

- PostgreSQL tests MUST use the `@test` framework — no standalone `main()` hacks.
- Use SIMD in tensor/numeric code — never scalar-only loops.
- All native TML packages use **Apache-2.0** license.
- Always increment `CHANGELOG.md` version (semver) — current baseline see STATE.md.
- HTTP must be a real generic implementation using existing TML APIs (Buffer, Text, HashMap) — no benchmark hacks, no `lowlevel` everywhere.

---

## T8. Active ADRs

- ADR-001 In-process LLVM vs Subprocess
- ADR-002 Query-based Incremental Compilation (Red-Green)
- ADR-003 Pure C ABI for Plugin Interface
- ADR-004 NDJSON Subprocess Test Protocol (Go model)
- ADR-005 Dual Codegen Paths: Legacy AST→IR + MIR→IR
- ADR-006 TML-First Runtime (migrating from C/C++)
- ADR-007 Zig CC as Preferred Toolchain
- ADR-008 LL(1) Grammar with Single-Token Lookahead

---

## T9. Delegation Discipline

- Main conversation = analysis, planning, dispatch, reporting. **Never implement code directly in main.**
- **Dispatch decision order (strict):**
  1. MCP tool (`mcp__tml__*`, `mcp__rulebook__*`) — single-step, no overhead
  2. Skill (`/check`, `/commit`, `/test`, …) — multi-step workflow, invoke via `Skill` tool
  3. Built-in tools (Read, Grep, Edit, Bash) — only when no MCP/skill covers it
  4. Agent spawn — last resort for long or parallel work
- 2+ parallel agents MUST use a Team. Every team member needs a `name` for `SendMessage`.
- After launching agents, actively monitor output — never go passive.

---

## T10. Skills Catalog (invoke with `/skill-name` or `Skill(skill: "name")`)

Skills accept trigger words in **English and Portuguese**. Always prefer a skill over raw Bash.

### Code Quality
| Skill | When to use |
|-------|-------------|
| `/check` | Type-check a `.tml` file without compiling — first step before any test |
| `/lint` | Lint TML source for style issues |
| `/format` | Auto-format `.tml` files |
| `/precommit` | format + lint + affected tests — run before every commit |

### Testing
| Skill | When to use |
|-------|-------------|
| `/test` | Run tests: specific file, suite, or filtered set (`path=`, `suite=`) |
| `/write-tests` | Generate new tests for a module or feature |
| `/coverage` | Run tests with coverage report |
| `/affected-tests` | Detect which suites are affected by recent changes |
| `/list-suites` | Enumerate all available test suites |
| `/slow-tests` | Profile and explain slow compilation/test runs |
| `/investigate` | Deep-dive a failing test: run → emit IR → analyze error |
| `/parallel-test-execution` | Run multiple test suites in parallel |

### Build & Compile
| Skill | When to use |
|-------|-------------|
| `/build-compiler` | Rebuild TML compiler from C++ sources |
| `/build-smart` | Incremental smart build — only rebuilds what changed |
| `/build-fix` | Fix a broken build automatically |
| `/compile` | Compile a single `.tml` file to binary |
| `/verify` | Build compiler + run targeted tests end-to-end |
| `/cache-invalidate` | Invalidate stale incremental cache for specific files |

### Codegen & IR
| Skill | When to use |
|-------|-------------|
| `/emit-ir` | Emit LLVM IR for a `.tml` source file |
| `/emit-mir` | Emit MIR for a `.tml` source file |
| `/compare-ir` | Rust-as-Reference: compile equivalent `.rs`+`.tml`, compare IR side-by-side |
| `/fix-codegen` | Systematic codegen bug fix workflow |
| `/optimize-ir` | Improve LLVM IR quality for a specific pattern |

### Tasks & Planning
| Skill | When to use |
|-------|-------------|
| `/tasks` | List active Rulebook tasks with status |
| `/task-create` | Create a new tracked task (runs `rulebook_task_create`) |
| `/task-archive` | Archive a completed task (runs `rulebook_task_archive`) |

### Analysis & Review
| Skill | When to use |
|-------|-------------|
| `/research` | Explore codebase and gather context on a topic |
| `/investigate` | Root-cause analysis of a failing test |
| `/review` | Deep code review of recent changes or specific files |
| `/review-pr` | Review current branch vs main |
| `/status` | Project health dashboard: tests, coverage, active tasks, build |
| `/qa` | Quality audit of a module — creates improvement tasks |
| `/analysis` | Structured analysis of a problem or code area |
| `/perf` | Performance profiling and optimization analysis |
| `/slow-tests` | Identify slow test suites and compilation bottlenecks |

### Git & Session
| Skill | When to use |
|-------|-------------|
| `/commit` | Stage + commit with conventional commit message |
| `/handoff` | Save session state before `/clear` at context limit |

### Docs & Explanation
| Skill | When to use |
|-------|-------------|
| `/docs` | Generate or update documentation |
| `/explain` | Explain a piece of code, error, or concept |
| `/stdlib-architecture` | Inject deep context about TML stdlib structure before library work |

### Debug
| Skill | When to use |
|-------|-------------|
| `/debug` | Systematic debugging workflow for bugs and test failures |
| `/fix-codegen` | Fix incorrect LLVM IR generation |
<!-- OVERRIDE:END -->
