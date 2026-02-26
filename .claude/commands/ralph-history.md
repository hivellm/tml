---
name: /ralph-history
id: ralph-history
category: Ralph
description: View Ralph iteration history and past work
---

**Usage**
```
/ralph-history --task <task-id> [options]
```

**Flags**
- `--task <id>` (REQUIRED) - Task ID to view history
- `--format <type>` (optional) - Output format: `json`, `table`, `markdown` (default: table)
- `--limit <n>` (optional) - Show last N iterations (default: all)

**What It Does**
Shows Ralph's iteration history:
- Each iteration's date/time
- What was accomplished
- Any errors or blockers
- Time spent
- Context used (tokens)
- Next steps planned

**Examples**
```bash
# View all iterations in table format
/ralph-history --task parallel-test-execution

# View last 3 iterations
/ralph-history --task parallel-test-execution --limit 3

# Export as markdown for documentation
/ralph-history --task parallel-test-execution --format markdown

# Export as JSON for scripting
/ralph-history --task coverage-migration --format json
```

**History Example (Table)**
```
Iteration  Date/Time              Status     Accomplishments                  Errors  Context
1          2026-02-26 10:00       COMPLETED  Design Phase 1, setup           none    150KB
2          2026-02-26 11:00       COMPLETED  Implement Phase 1, test         none    120KB
3          2026-02-26 12:30       IN_PROG    Implement Phase 2, integrate    none    130KB
```

**History Example (Markdown)**
```markdown
## Iteration 1: Analysis & Design
- Analyzed DLL vs subprocess architecture
- Designed Phase 1 implementation
- Created async subprocess launching functions
- Status: COMPLETED

## Iteration 2: Implementation & Test
- Implemented launch_subprocess_async()
- Implemented wait_for_subprocess()
- Added coverage file communication
- Status: COMPLETED
```

**Related Commands**
- `/ralph-status` - Check current status
- `/ralph-run` - Execute the loop
- `/ralph-pause` - Pause current loop
- `/ralph-resume` - Resume from pause

**References**
- [.rulebook/ralph/history.json](../../.rulebook/ralph/history.json)
