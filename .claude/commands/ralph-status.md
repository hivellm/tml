---
name: /ralph-status
id: ralph-status
category: Ralph
description: Check Ralph loop status and current iteration progress
---

**Usage**
```
/ralph-status --task <task-id> [--detailed]
```

**Flags**
- `--task <id>` (REQUIRED) - Task ID to check status
- `--detailed` (optional) - Show detailed iteration breakdown

**What It Does**
Shows Ralph loop status:
- Current iteration number
- Current phase (if task has phases)
- Completion percentage
- Last successful iteration
- Any errors or blockers
- Time elapsed

**Examples**
```bash
# Quick status
/ralph-status --task parallel-test-execution

# Detailed breakdown with all iterations
/ralph-status --task parallel-test-execution --detailed

# Check coverage-migration task
/ralph-status --task coverage-migration
```

**Output Example**
```
Task: parallel-test-execution
Status: IN_PROGRESS
Current Iteration: 3 / 4
Current Phase: Phase 3 - True Parallel Polling
Completion: 30%
Last Success: Iteration 2 (2 hours ago)
Next Checkpoint: Phase 3 compilation
```

**Detailed Output**
With `--detailed`, shows:
- All iterations with timestamps
- Each iteration's accomplishments
- Any errors or rollbacks
- Blockers and mitigations

**Related Commands**
- `/ralph-run` - Execute the loop
- `/ralph-history` - View full iteration history
- `/ralph-pause` - Pause current loop
- `/ralph-resume` - Resume paused loop

**References**
- [.rulebook/ralph.json](../../.rulebook/ralph.json)
