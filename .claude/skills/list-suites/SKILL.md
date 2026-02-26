---
name: list-suites
description: List all available test suites with file and test counts. Use when user asks "what suites exist", "list suites", "quais suites", or needs to know available test modules.
user-invocable: true
allowed-tools: Bash(cd * && *)
argument-hint: no arguments needed
---

## List Suites

Run the TML compiler's `--list-suites` command to show all available test suite groups.

### Execute

```bash
cd /f/Node/hivellm/tml && build/debug/bin/tml.exe test --list-suites --verbose 2>&1
```

### Report

Format the output as a clean table for the user, stripping ANSI codes. Show:
- Library groups (core, std, compiler) with total counts
- Each module with its `--suite=` name and test counts
- Usage example at the end