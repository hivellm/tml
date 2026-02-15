# Tasks: Claude Code Skills for TML

**Status**: Planning (0%)
**Priority**: High

## Phase 1: Development & Debugging Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [ ] 1.1 Create `/ir` skill — emit LLVM IR for a file, optional function filter via `mcp__tml__emit-ir`
- [ ] 1.2 Create `/compare-ir` skill — Rust-as-Reference workflow: write .rs/.tml in .sandbox/, compile both (rustc + tml emit-ir), read and compare IR output
- [ ] 1.3 Create `/check` skill — type-check a file without compiling via `mcp__tml__check`
- [ ] 1.4 Create `/mir` skill — emit MIR for a file via `mcp__tml__emit-mir`

## Phase 2: Test & Coverage Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [ ] 2.1 Create `/test` skill — run individual test file with `--verbose --structured` via `mcp__tml__test`
- [ ] 2.2 Create `/test-changed` skill — detect affected tests via `mcp__tml__project_affected-tests` then run them
- [ ] 2.3 Create `/test-suite` skill — run full suite with `--coverage --profile --verbose` flags
- [ ] 2.4 Create `/coverage` skill — show coverage stats via `mcp__tml__project_coverage`, accept optional module filter
- [ ] 2.5 Create `/coverage-gaps` skill — query coverage sorted by lowest, highlight modules below 20%

## Phase 3: Build Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [ ] 3.1 Create `/build` skill — build compiler via `mcp__tml__project_build`, accept debug/release mode
- [ ] 3.2 Create `/build-smart` skill — use git diff to detect changes, call `mcp__tml__project_build` with appropriate target (compiler/mcp/all)
- [ ] 3.3 Create `/rebuild` skill — clean build via `mcp__tml__project_build` with `clean: true`, require user confirmation

## Phase 4: Task Management Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [ ] 4.1 Create `/status` skill — aggregate: test count (via test --structured), coverage % (via project_coverage), active tasks (via glob rulebook/tasks/), build artifacts (via project_artifacts)
- [ ] 4.2 Create `/task-create` skill — create `rulebook/tasks/<id>/` with templated `proposal.md` + `tasks.md`
- [ ] 4.3 Create `/tasks` skill — list active tasks, parse each `tasks.md` for completion percentage
- [ ] 4.4 Create `/task-archive` skill — move task to `rulebook/tasks/archive/YYYY-MM-DD-<id>/`

## Phase 5: Documentation & Navigation Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [ ] 5.1 Create `/doc` skill — hybrid doc search via `mcp__tml__docs_search`
- [ ] 5.2 Create `/lib-tree` skill — show module tree via `mcp__tml__project_structure` with coverage overlay from `mcp__tml__project_coverage`
- [ ] 5.3 Create `/explain` skill — error code explanation via `mcp__tml__explain`

## Phase 6: Quality & Workflow Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [ ] 6.1 Create `/precommit` skill — sequential: format (mcp__tml__format) → lint (mcp__tml__lint) → affected tests (project_affected-tests --run)
- [ ] 6.2 Create `/tdd` skill — incremental test loop: run test file, display errors, prompt for fix, re-run until pass
- [ ] 6.3 Create `/investigate` skill — run failing test (structured), emit IR for test file, analyze error pattern, suggest root cause
- [ ] 6.4 Create `/migrate-c` skill — read C runtime file, analyze functions, suggest pure TML replacements using memory intrinsics

## Phase 7: Integration & Documentation

> **Priority**: Low | **Dir**: `.claude/skills/`

- [ ] 7.1 Add skill listing to CLAUDE.md so Claude Code discovers available skills
- [ ] 7.2 Write skill usage examples in each skill file's description
- [ ] 7.3 Verify all 25 skills load correctly in Claude Code sessions
- [ ] 7.4 Test skill composition (e.g., `/precommit` calling format → lint → test-changed)
- [ ] 7.5 Update `developer-tooling` task to reference skills integration

## Validation

- [ ] V.1 All 25 skills are recognized by Claude Code via `/` prefix
- [ ] V.2 `/compare-ir` completes full Rust-as-Reference workflow in single invocation
- [ ] V.3 `/test-changed` correctly identifies and runs only affected tests
- [ ] V.4 `/coverage` displays per-module breakdown with sorting
- [ ] V.5 `/status` aggregates test, coverage, task, and build data in one output
- [ ] V.6 `/precommit` catches formatting, lint, and test failures
- [ ] V.7 `/task-create` generates valid rulebook structure that passes `rulebook task validate`
- [ ] V.8 No skill conflicts with existing Claude Code built-in commands
