# Proposal: Claude Code Skills for TML

## Status: PROPOSED

## Why

The TML project has 20 MCP tools that provide low-level compiler operations (build, test, emit-ir, docs search, etc.), but Claude Code lacks higher-level **skills** (slash commands) that compose these tools into the workflows developers actually perform. Every coding session involves repetitive multi-step patterns: emit IR and compare with Rust, run affected tests after a change, check coverage gaps, build the compiler then run a test, create a rulebook task with the correct template. Today these workflows require Claude Code to manually chain 3-5 tool calls each time, wasting tokens and introducing friction.

Skills bridge the gap between raw MCP tools and developer intent. A single `/compare-ir` skill replaces a 6-step workflow (write .rs, write .tml, run rustc, run tml emit-ir, read both files, compare). A `/test-changed` skill replaces manually calling `project/affected-tests` then `test`. The project already mandates specific workflows in CLAUDE.md (Rust-as-Reference IR methodology, incremental test development, pre-commit validation) — skills codify these mandates into executable shortcuts.

### Current State

- 20 MCP tools available but no Claude Code skills defined
- Common workflows require 3-6 manual tool calls per execution
- Mandatory workflows (IR comparison, incremental testing) are error-prone when done manually
- No project dashboard or status overview command
- Task creation requires knowing the exact rulebook template format

## What Changes

### Phase 1: Development & Debugging Skills

Core skills for the most frequent codegen and debugging workflows.

- `/ir <file> [func]` — Emit LLVM IR, optionally filtered to a single function
- `/compare-ir <feature>` — Full Rust-as-Reference workflow: generate equivalent .rs/.tml, compile both, compare IR side-by-side
- `/check <file>` — Quick type-check without compiling
- `/mir <file>` — Emit MIR for mid-level optimization debugging

### Phase 2: Test & Coverage Skills

Skills for test-driven development and coverage tracking.

- `/test <path|filter>` — Run individual test with verbose + structured output
- `/test-changed` — Detect git changes, run only affected tests
- `/test-suite` — Run full suite with `--coverage --profile --verbose`
- `/coverage [module]` — Show coverage stats, overall or filtered by module
- `/coverage-gaps` — List modules with lowest coverage, suggest next tests to write

### Phase 3: Build Skills

Shortcuts for compiler compilation and build management.

- `/build [debug|release]` — Build TML compiler via `scripts/build.bat`
- `/build-smart` — Detect changed files, build only necessary target (compiler vs mcp)
- `/rebuild` — Clean build with user confirmation

### Phase 4: Task Management Skills

Rulebook integration for task lifecycle.

- `/status` — Project dashboard: test count, coverage %, active tasks, build status
- `/task-create <id>` — Create task with pre-filled `proposal.md` + `tasks.md` templates
- `/tasks` — List active tasks with completion percentage
- `/task-archive <id>` — Archive completed task with correct date prefix

### Phase 5: Documentation & Navigation Skills

Quick access to TML documentation and library structure.

- `/doc <query>` — Hybrid search across TML documentation
- `/lib-tree [module]` — Show library module tree with function counts and coverage
- `/explain <code>` — Show detailed error code explanation

### Phase 6: Quality & Workflow Skills

Composite skills that enforce project conventions.

- `/precommit` — Run format + lint + affected tests before committing
- `/tdd <file>` — Test-driven development loop: write test, run, show error, await fix, repeat
- `/investigate <test>` — Run failing test, capture error, emit IR, suggest root cause
- `/migrate-c <file>` — Analyze C runtime file, suggest pure TML replacement

## Impact

- Affected specs: None (skills are tooling, not language features)
- Affected code: New `.claude/skills/` directory with skill definition files
- Breaking change: NO
- User benefit: 3-6x reduction in tool calls for common workflows, enforced project conventions, reduced token waste

## Dependencies

- MCP server tools (all 20 implemented and working)
- Claude Code skill system (available via `.claude/skills/` directory or equivalent)
- Existing `scripts/build.bat`, `scripts/format.bat`, `scripts/lint.bat`

## Success Criteria

1. All 25 skills defined and loadable by Claude Code
2. `/compare-ir` automates the full Rust-as-Reference IR methodology in a single command
3. `/test-changed` correctly maps git diff to affected test files and runs them
4. `/coverage` displays structured coverage data with module-level breakdown
5. `/status` shows a complete project health dashboard in one call
6. `/precommit` catches format, lint, and test issues before commit
7. `/task-create` generates correctly formatted rulebook task templates
8. Average tool calls per common workflow reduced from 4-5 to 1

## Out of Scope

- MCP server modifications (skills compose existing tools, not add new ones)
- VSCode extension integration (covered by `developer-tooling` task)
- CI/CD pipeline skills (covered by `improve-test-infrastructure` task)
- Package manager skills (covered by `package-manager` task)
