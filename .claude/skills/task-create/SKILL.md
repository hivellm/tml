---
name: task-create
description: Create a new Rulebook task with proper template. Use when the user says "create task", "cria task", "nova task", or wants to start a new tracked work item.
user-invocable: true
allowed-tools: mcp__rulebook__rulebook_task_create, mcp__rulebook__rulebook_task_show, Write, Read
argument-hint: <task-id> — kebab-case identifier e.g. "fix-closure-capture"
---

## Task Create Workflow

### 1. Get Task ID

Extract the task ID from `$ARGUMENTS`. It must be kebab-case (e.g., `fix-closure-capture`).

If no ID provided, ask the user for one.

### 2. Create Task

Use `mcp__rulebook__rulebook_task_create` with the task ID.

### 3. Guide User

Tell the user:
- Task created at `rulebook/tasks/<id>/`
- They need to fill in `proposal.md` (why, what changes, impact, success criteria)
- And `tasks.md` (checklist of actionable items)

Offer to help write the proposal and tasks based on a description of what they want to accomplish.