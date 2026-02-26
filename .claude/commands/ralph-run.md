---
name: /ralph-run
id: ralph-run
category: Ralph
description: Execute Ralph autonomous iteration loop for a task
---

**Usage**
```
/ralph-run --task <task-id> [options]
```

**Flags**
- `--task <id>` (REQUIRED) - Task ID to run (e.g., `parallel-test-execution`)
- `--max-iterations <n>` (optional) - Override max iterations (default: from rulebook.json, usually 10)
- `--pause-on-error` (optional) - Pause loop on first error instead of continuing
- `--dry-run` (optional) - Simulate loop without executing changes
- `--verbose` (optional) - Show iteration details

**What It Does**
Ralph autonomous loop:
1. **Iteration 1**: Analyze problem, design Phase 1
2. **Iteration 2**: Implement Phase 1, test, debug
3. **Iteration 3**: Implement Phase 2, integrate
4. **Iteration N**: Continue until task complete or max iterations reached

Each iteration:
- ✅ Fresh context (avoids AI exhaustion/hallucination drift)
- ✅ Validates compilation (type-check, lint, test)
- ✅ Tracks progress in `.rulebook/ralph/history.json`
- ✅ Can pause/resume across sessions

**Examples**
```bash
# Run with default max iterations (from rulebook.json)
/ralph-run --task parallel-test-execution

# Run with custom max iterations
/ralph-run --task coverage-migration --max-iterations 3 --verbose

# Dry run to see what would happen
/ralph-run --task parallel-test-execution --dry-run

# Pause on error for debugging
/ralph-run --task parallel-test-execution --pause-on-error
```

**Success Criteria**
Ralph will run until:
- ✅ Task reaches "completed" status, OR
- ⏳ Max iterations reached, OR
- ⚠️ --pause-on-error triggered on failure

Check status with `/ralph-status`

**Related Commands**
- `/ralph-init` - Initialize task (generates PRD)
- `/ralph-status` - Check current status
- `/ralph-history` - View iteration history
- `/ralph-pause` - Pause loop gracefully
- `/ralph-resume` - Resume from pause

**References**
- [.rulebook/ralph.json](../../.rulebook/ralph.json)
- [Rulebook Ralph Documentation](https://github.com/hivellm/rulebook#ralph)
