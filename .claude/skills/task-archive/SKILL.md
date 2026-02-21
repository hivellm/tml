---
name: task-archive
description: Archive a completed Rulebook task. Use when the user says "archive task", "arquiva task", or wants to move a completed task to the archive.
user-invocable: true
allowed-tools: mcp__rulebook__rulebook_task_update, mcp__rulebook__rulebook_task_archive, mcp__rulebook__rulebook_task_validate, Read
argument-hint: <task-id> — the task to archive
---

## Task Archive Workflow

### 1. Get Task ID

Extract the task ID from `$ARGUMENTS`.

### 2. Validate

First, check the task exists and read its `tasks.md` to verify all items are complete.

If there are incomplete items (`- [ ]`), warn the user and ask if they want to proceed anyway.

### 3. Update Status

Use `mcp__rulebook__rulebook_task_update` to set status to "completed".

### 4. Archive

Use `mcp__rulebook__rulebook_task_archive` with the task ID.

### 5. Report

Confirm the task was archived to `rulebook/tasks/archive/YYYY-MM-DD-<id>/`.