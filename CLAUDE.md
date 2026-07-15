<!-- RULEBOOK:START v7.0.0 — DO NOT EDIT BY HAND. Regenerated on `rulebook update`.
     Put project-specific content in AGENTS.override.md or CLAUDE.local.md.
     Anything outside the RULEBOOK:START/END sentinels is preserved across updates. -->

# CLAUDE.md

Managed by [@hivehub/rulebook](https://github.com/hivellm/rulebook) — few rules, all deliberate.

## Project-specific overrides (user-owned, survives `rulebook update`, wins on conflict)
@AGENTS.override.md

## Commands
- Before each commit: type-check + lint + the tests covering what changed.
- Before push / PR / task archive: the FULL quality gate (type-check → lint →
  full test suite), all green. Never bypass hooks — what the project wired
  into pre-commit/pre-push is the floor.
- Diagnostic-first: run the type-checker before the test suite; it is the faster signal.

## Values
1. Complete implementations — no stubs, no TODO markers left behind; finish, or say concretely why you can't.
2. Root causes, not workarounds — diagnose before changing code; never guess at bug causes.
3. Surgical diffs — touch only what the task needs; match existing style.
4. Simplicity first — the least code that solves the problem; no unrequested abstractions or features.
5. Fix forward — never discard uncommitted work.
6. State assumptions — if interpretations diverge, say so instead of picking silently.

## Git safety (requires explicit user authorization)
`reset --hard` · `checkout -- .` / `restore .` · `clean -f` · `push --force` ·
`rebase` on shared branches · `stash` · `branch -D` · switching a shared checkout
with changes you did not author. Yours autonomously: status/diff/log/add/commit,
branches you create (create/switch/merge), `git worktree`, PRs via `gh`.

## Orchestration
Subagents, parallel dispatch, and teams are your call — fan out freely when work is
parallel or context-heavy; work directly when it isn't. Rulebook never blocks or
mandates orchestration.

## Rulebook (on demand — no ceremony for small fixes)
- Multi-session or multi-phase work: track via the `rulebook` MCP (`rulebook_task`).
  Checklist order = dependencies; independent items may run in parallel.
- Optional session context: `rulebook_session`. Learned something non-obvious?
  `rulebook_memory`.
- Project specs live in `.rulebook/specs/` — read a spec when the work touches its area.
- Analyses live in `docs/analysis/<slug>/` — numbered files, one theme per file.
- Long session? `/compact <focus>` at a task boundary (~60% context). After
  `rulebook_task {action:"archive"}`, `/clear` is free — state lives in `.rulebook/`.

<!-- RULEBOOK:END -->

## Delegation & Parallelism (HIGH PRECEDENCE — apply every turn)

**Default to delegation.** Main conversation = analyze, plan, dispatch, report. Implementation, research, and verification belong in agents/teams. Working directly in main is the exception, not the default.

### When to spawn an agent (proactive triggers)

Spawn an agent BEFORE writing code yourself if ANY of these apply:

- **Codebase exploration > 3 queries** → `Explore` (read-only) or `researcher` (haiku).
- **Codegen / IR / type bug** → `codegen-debugger`, `mir-expert`, `thir-expert`, `hir-expert`, or `compiler-optimizer` depending on layer.
- **Library / stdlib work** → `tml-library-engineer`.
- **C → TML migration** → `c-to-tml-migrator`.
- **Multi-file refactor (3+ files)** → decompose, dispatch each cluster to `implementer` (sonnet).
- **Test failures, coverage regression** → `test-coverage-guardian` proactively after any compiler/lib change.
- **Spec / docs sync after implementation** → `spec-engineer`, `docs-writer`.
- **Architecture decision, ADR** → `architect` (opus).
- **Build failures** → `build-engineer` or `zig-expert`.
- **Quality audit, tech debt** → `qa-code-analyst`.
- **Status, prioritization, delegation orchestration** → `project-manager`.
- **Deep root-cause when 1 fix already failed** → `deep-analysis-reviewer` (BEFORE attempting fix #2).
- **Rust IR comparison** → `rust-reference`.

If a task matches an agent's description, USE IT. Don't re-implement what the agent does in main.

### Parallelism is mandatory when work is independent

- **Send multiple Agent tool calls in ONE message** when the work is independent (different files, different concerns, different layers). Sequential dispatch wastes wall-clock time.
- **2+ background agents MUST use a Team** (`team_name`) — standalone background `Agent` calls without a team can't communicate via `SendMessage` and the PreToolUse hook will deny them.
- **Team-lead pattern** for orchestrated multi-step work: spawn `team-lead` (background) which creates the team and dispatches members. Use this for any task with 3+ parallel concerns.
- **Foreground agents** are fine standalone — but still launch independent ones in parallel within a single message.

### Create new skills/agents when the pattern repeats

If you find yourself doing the same multi-step workflow ≥ 2× in the same session, OR you notice a recurring pattern that doesn't have a skill/agent yet:

1. **Skills** (`/<name>`) — for repeatable user-invocable workflows. Create via the Skill tool / settings. Examples: a new `/migrate-c-batch` for batch C→TML migration, `/audit-codegen-layer` for layer-specific IR review.
2. **Agents** — for specialized expertise that benefits from a focused system prompt. Add to `.claude/agents/` (project) or user-level. Use when an existing agent's description doesn't fit and the role would be reused.
3. **Teams** — pre-defined agent compositions for recurring multi-agent workflows.

Propose new skills/agents to the user when you spot the opportunity — don't just absorb the pattern silently.

### Anti-patterns (forbidden)

- ❌ Doing 5 sequential `Read` + `Edit` in main when an `implementer` agent could batch them.
- ❌ Running `mcp__tml__test` in main, parsing output, then `mcp__tml__emit-ir`, then analyzing — use `/investigate` skill or `test-coverage-guardian` agent.
- ❌ Spawning 3 background agents without a team (hook will deny).
- ❌ Sequential `Agent` calls when the work is independent (must batch in one message).
- ❌ Re-doing research a subagent already did — read its result, don't duplicate.

### Skill > MCP tool > Bash (preference order)

When dispatching, pick the highest-level abstraction that fits:

1. **Skill** (`/check`, `/test`, `/commit`, `/investigate`, `/compare-ir`, …) — multi-step workflow.
2. **Direct MCP tool** (`mcp__tml__*`, `mcp__rulebook__*`) — single-step, no orchestration overhead.
3. **Built-in tools** (Read, Grep, Edit) — only when no skill/MCP fits.
4. **Bash** — last resort.

This order is enforced because skills bundle conventions (e.g. `/test` already redirects output, `/precommit` already chains format+lint+tests). Bypassing them re-creates work the project already automated.
