---
name: /ralph-init
id: ralph-init
category: Ralph
description: Initialize Ralph and generate PRD from a Rulebook task
---

**Usage**
```
/ralph-init --task <task-id> [--verbose]
```

**Flags**
- `--task <id>` (REQUIRED) - Task ID to initialize (e.g., `parallel-test-execution`)
- `--verbose` (optional) - Show detailed PRD generation output

**What It Does**
- Reads task description from `.rulebook/ralph.json`
- Generates Product Requirements Document (PRD) with:
  - Problem statement
  - Solution approach
  - Implementation phases
  - Success criteria
  - Risk assessment
- Prepares Ralph loop for autonomous iteration
- Shows PRD for review before running

**Examples**
```bash
/ralph-init --task parallel-test-execution
/ralph-init --task coverage-migration --verbose
```

**Related Commands**
- `/ralph-run` - Execute the autonomous iteration loop
- `/ralph-status` - Check current loop status
- `/ralph-history` - View iteration history

**References**
- [.rulebook/ralph.json](../../.rulebook/ralph.json)
- [Rulebook Ralph Documentation](https://github.com/hivellm/rulebook#ralph)
