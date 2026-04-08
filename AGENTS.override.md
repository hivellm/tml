<!-- OVERRIDE:START -->
# TML Project — Overrides (Highest Precedence)

These rules override `AGENTS.md` and generic rulebook guidance. Only TML-specific items live here; universal rules stay in `AGENTS.md` / `/.rulebook/specs/`.

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

**Never use `Bash` to run `tml.exe test/build/run` when an MCP tool exists.**

| Task | Tool |
|------|------|
| Type check | `mcp__tml__check` (≈10× faster than test) |
| Run tests | `mcp__tml__test` with `suite=`, `path=`, `structured=true` |
| Debug failure | `mcp__tml__test` + `debug_layers=true` (always on first failure) |
| LLVM IR | `mcp__tml__emit-ir` |
| MIR | `mcp__tml__emit-mir` |
| API docs | `mcp__tml__docs_{search,list,get,resolve}` |
| Memory leaks | `mcp__tml__debug(file, check_leaks=true)` |
| Crash backtrace | `mcp__tml__debug(file, backtrace=true)` |
| Build compiler | `mcp__tml__project_build` or `scripts\build.bat` |
| Tasks | `mcp__rulebook__rulebook_task_*` |

**Diagnostic-first order**: `check` → fix → `test`. Never run `test` before `check`.

**Never run tests multiple times to filter.** Run once, redirect to `.sandbox/test_output.log`, re-read the file.

**Never run the full test suite via MCP** — only specific suites/files.

---

## T3. TML Language Reference (mandatory before coding)

Before writing any `.tml` code:
- Use `mcp__tml__docs_search/list/get/resolve` — never read `lib/core/src/*.tml` or `lib/std/src/*.tml` to "understand" APIs (source wastes tokens on impl details).
- Source files only when you need to **modify** them, not to **use** them.

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
- Dispatch decision order: direct MCP tool → skill (`/commit`, `/test`, `/verify`, `/build-compiler`, …) → built-in tool → only then spawn an agent.
- 2+ parallel agents MUST use a Team. Every team member needs a `name` for `SendMessage`.
- After launching agents, actively monitor output — never go passive.
<!-- OVERRIDE:END -->
