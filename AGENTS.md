<!-- RULEBOOK:START -->
# Project Agent Directives

Detailed specs live in `/.rulebook/specs/`. **`AGENTS.override.md` takes precedence over this file.**

---

## 0. Critical Reading Order

1. `/.rulebook/specs/TIER1_PROHIBITIONS.md` — absolute prohibitions (highest precedence)
2. `AGENTS.override.md` — TML project-specific rules
3. `/.rulebook/specs/RULEBOOK.md` — task management
4. `/.rulebook/specs/QUALITY_ENFORCEMENT.md` — quality gates
5. `/.rulebook/PLANS.md` — session scratchpad

---

## 1. Absolute Prohibitions (Tier 1)

- **No shortcuts, stubs, TODOs, placeholders, or simplified logic.** Implement completely or explain concretely why impossible. Speed is irrelevant; quality is everything.
- **No destructive git ops** without explicit user authorization: `stash`, `rebase`, `reset --hard`, `checkout --`, `branch -D`, `push --force`, `clean -f`, `switch`. Allowed: `status`, `diff`, `log`, `blame`, `add`, `commit`.
- **No `rm`/deletion** without explicit "yes, delete it".
- **Never use `--no-verify`** or any flag that bypasses git hooks.
- **Never skip/comment/`.skip()` tests.** Fix the root cause, not the test.
- **Never guess.** State what you know, what you don't, research before implementing.
- **Sequential file editing.** Read f1 → Edit f1 → Read f2 → Edit f2. For 3+ files, decompose into 1–2 file sub-tasks.
- **No deferred tasks.** If an item is deferred, create a new rulebook task for it BEFORE archiving.
- **Follow task sequence exactly.** `tasks.md` is an ORDER, not a MENU — execute first unchecked item, never skip/reorder/cherry-pick. Phase N+1 cannot start before Phase N is 100% complete.
- **Temp files go in `/scripts/` only** and are deleted immediately after use.

---

## 2. Task Management (MANDATORY)

**Always use Rulebook MCP tools** — never `mkdir` + `Write` manually.

```
rulebook_task_create   rulebook_task_list      rulebook_task_show
rulebook_task_update   rulebook_task_validate  rulebook_task_archive
```

### Task structure (enforced by MCP)
```
.rulebook/tasks/<task-id>/
├── proposal.md       # Why (≥20 chars) + What Changes
├── tasks.md          # ONLY simple checklist: - [ ] item
├── design.md         # Optional: architecture decisions
└── specs/<module>/spec.md   # Requirements with SHALL/MUST
```

- **proposal.md** = Why + What Changes + Impact
- **tasks.md** = checklist ONLY (no explanations, no specs, no code)
- **specs/** = `### Requirement:` with SHALL/MUST + `#### Scenario:` (4 hashtags) with Given/When/Then
- Delta headers: `## ADDED Requirements`, `## MODIFIED Requirements`, `## REMOVED Requirements`, `## RENAMED Requirements`
- ❌ NEVER create README.md, PROCESS.md, or any other files in task dirs

### Task ID naming
Verb-led kebab-case, one capability per task: `add-auth`, `refactor-module`, `remove-legacy-x`.

### Workflow
1. Create task BEFORE any implementation.
2. Write proposal + tasks + specs; `rulebook_task_validate`.
3. Implement in the exact order listed. Mark `[x]` immediately.
4. Update `tasks.md` after every cycle; commit locally often.
5. `rulebook_task_archive` only when: all items `[x]`, tests pass, coverage ≥95%, docs updated, validation passes.

### Deferred items protocol
Archiving with any deferred item requires creating a new rulebook task for it first.

---

## 3. Quality Gates (all must pass before commit)

1. Type-check / compiler check (**diagnostic-first — before tests**)
2. Lint (zero warnings)
3. Format
4. All tests (100% pass)
5. Coverage ≥95%
6. Security/dependency audit

**Diagnostic-first**: type-check before tests — it's 5–10× faster and catches a different class of errors.

**Fail-twice rule**: if a fix fails twice with the same approach, STOP. Escalate via research, a specialist agent, or a Team. Never retry a third variation blindly.

Pre-commit hooks will block broken code — fix it, never bypass.

---

## 4. Commit Protocol

- Messages in **English only**, conventional format:
  `type(scope): description` — types: `feat|fix|docs|refactor|perf|test|build|ci|chore`
- Body lists: changes, test coverage, closes.
- Commit locally frequently; push to remote regularly.

---

## 5. Persistent Memory & Knowledge Base

**Memory** (`rulebook_memory_*`):
- Session start: `rulebook_memory_search` for relevant context.
- During work: save decisions, bugs, discoveries, patterns, gotchas.
- Session end: `rulebook_session_end` for summary.
- 3-layer search: `search` (compact) → `timeline` → `get` (full).

**Knowledge base** (before non-trivial work):
`rulebook_knowledge_list`, `rulebook_learn_list`, `rulebook_decision_list`.
After implementing, capture at least one entry: `knowledge_add`, `learn_capture`, or `decision_create` (ADR).

---

## 6. Dependency Architecture (DAG)

- **Foundation → Core → Features → Presentation** (dependencies flow down only).
- No circular deps. No lower-layer depending on higher-layer.
- Use interfaces between layers; keep the graph shallow.
- Validate before committing (`cargo check`, `tsc --noEmit`, `madge --circular`, `go vet`).

---

## 7. Multi-Agent / Teams

- **1 agent** → direct spawn.
- **2+ parallel agents** → MUST use a Team (`TeamCreate` or `team-lead`). Standalone parallel background agents are forbidden (they cannot `SendMessage`).
- Team lead assigns work; no two agents edit the same file concurrently.
- Always monitor running agents actively — never go passive.

---

## 8. Documentation Standards

**Root-level only**: `README.md`, `CHANGELOG.md`, `AGENTS.md`, `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`.
**All other docs in `/docs/`**.

| Commit type | Update |
|-------------|--------|
| `feat` | README, API docs, CHANGELOG "Added" |
| `fix` | Troubleshooting, CHANGELOG "Fixed" |
| `breaking` | CHANGELOG + migration guide |
| `perf` | Benchmarks, CHANGELOG "Performance" |
| `security` | SECURITY.md, CHANGELOG "Security" |

All docs in English. Lint with `markdownlint`, `markdown-link-check`, `codespell`.

---

## 9. Language & Module Specs

Language rules: `/.rulebook/specs/<LANGUAGE>.md`. Module patterns: `/.rulebook/specs/<MODULE>.md`.
This project is **C/C++ (compiler) + TML (stdlib) + Lua (tooling)** — see `AGENTS.override.md` for TML specifics.

**C/C++ quality commands** (must match CI):
```
clang-format --dry-run --Werror src/**/*.{cpp,hpp}
clang-tidy src/**/*.cpp -- -std=c++20
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 10. Decision Records (ADRs)

Active ADRs in `.rulebook/decisions/`:

- ADR-001 In-process LLVM vs Subprocess
- ADR-002 Query-based Incremental Compilation (Red-Green)
- ADR-003 Pure C ABI for Plugin Interface
- ADR-004 NDJSON Subprocess Test Protocol
- ADR-005 Dual Codegen Paths (Legacy AST→IR + MIR→IR)
- ADR-006 TML-First Runtime (migrating from C/C++)
- ADR-007 Zig CC as Preferred Toolchain
- ADR-008 LL(1) Grammar with Single-Token Lookahead

Never delete an ADR — supersede or deprecate.

---

## 11. Knowledge Highlights

**Patterns**: 30s codegen watchdog · content-addressable cache · `!` error propagation · fat-pointer closures · hoisted alloca · lazy monomorphization queues · on-demand runtime decls · SEH→C++ translation · stride-based generics · Swiss-table HashMap · user-defined decorators · vtable dedup.

**Anti-patterns**: heap-allocated closures · `return 0` error sentinels · LLVMIRGen god class · duplicated `parse_mangled_type_string` · legacy path forcing · string-based type encoding.

---

## 12. Token Optimization

- Lead with the answer/action. Skip preamble.
- Output code, not explanation. Comments over prose.
- Short reports (`✅ done`) instead of emoji tables.
- Only complex debugging/architecture warrants long explanations.

---

## References

- `/.rulebook/specs/{RULEBOOK,QUALITY_ENFORCEMENT,TIER1_PROHIBITIONS,GIT,TOKEN_OPTIMIZATION}.md`
- `AGENTS.override.md` — TML project-specific rules (highest precedence)
- `.rulebook/PLANS.md` — session scratchpad
<!-- RULEBOOK:END -->
