# Tasks: Claude Code Skills for TML

**Status**: Completed (100%)
**Priority**: High

## Phase 1: Development & Debugging Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [x] 1.1 Create `/ir` skill — `emit-ir/SKILL.md` (pre-existing)
- [x] 1.2 Create `/compare-ir` skill — `compare-ir/SKILL.md`
- [x] 1.3 Create `/check` skill — `check/SKILL.md` (pre-existing)
- [x] 1.4 Create `/mir` skill — `emit-mir/SKILL.md` (pre-existing)

## Phase 2: Test & Coverage Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [x] 2.1 Create `/test` skill — `test/SKILL.md` (pre-existing)
- [x] 2.2 Create `/test-changed` skill — `affected-tests/SKILL.md` (pre-existing, has --run flag)
- [x] 2.3 Create `/test-suite` skill — `test-suite/SKILL.md` (pre-existing)
- [x] 2.4 Create `/coverage` skill — `coverage/SKILL.md` (pre-existing)
- [x] 2.5 Create `/coverage-gaps` skill — `coverage-gaps/SKILL.md`

## Phase 3: Build Skills

> **Priority**: High | **Dir**: `.claude/skills/`

- [x] 3.1 Create `/build` skill — `build-compiler/SKILL.md` (pre-existing)
- [x] 3.2 Create `/build-smart` skill — `build-smart/SKILL.md`
- [x] 3.3 Create `/rebuild` skill — `rebuild/SKILL.md`

## Phase 4: Task Management Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [x] 4.1 Create `/status` skill — `status/SKILL.md`
- [x] 4.2 Create `/task-create` skill — `task-create/SKILL.md`
- [x] 4.3 Create `/tasks` skill — `tasks/SKILL.md`
- [x] 4.4 Create `/task-archive` skill — `task-archive/SKILL.md`

## Phase 5: Documentation & Navigation Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [x] 5.1 Create `/doc` skill — `docs/SKILL.md` (pre-existing)
- [x] 5.2 Create `/lib-tree` skill — `structure/SKILL.md` (pre-existing)
- [x] 5.3 Create `/explain` skill — `explain/SKILL.md` (pre-existing)

## Phase 6: Quality & Workflow Skills

> **Priority**: Medium | **Dir**: `.claude/skills/`

- [x] 6.1 Create `/precommit` skill — `precommit/SKILL.md`
- [x] 6.2 Create `/tdd` skill — `tdd/SKILL.md`
- [x] 6.3 Create `/investigate` skill — `investigate/SKILL.md`
- [x] 6.4 Create `/migrate-c` skill — `migrate-c/SKILL.md`

## Phase 7: Integration & Documentation

> **Priority**: Low | **Dir**: `.claude/skills/`

- [x] 7.1 Skills auto-discovered by Claude Code via SKILL.md convention
- [x] 7.2 Each skill has description and argument-hint in frontmatter
- [x] 7.3 All 35 skills load correctly (22 pre-existing + 13 new)
- [x] 7.4 Composite skills reference correct tool chains
- [x] 7.5 Additional skills beyond plan: commit, compile, run, format, lint, list-suites, review-pr, slow-tests, artifacts, cache-invalidate, verify

## Validation

- [x] V.1 All skills recognized by Claude Code (confirmed in system-reminder listing)
- [x] V.2 `/compare-ir` automates Rust-as-Reference workflow
- [x] V.3 `/affected-tests --run` identifies and runs affected tests
- [x] V.4 `/coverage` displays per-module breakdown with sorting
- [x] V.5 `/status` aggregates test, coverage, task, and build data
- [x] V.6 `/precommit` chains format → lint → affected tests
- [x] V.7 `/task-create` uses rulebook MCP for proper structure
- [x] V.8 No skill conflicts with Claude Code built-in commands
