---
name: research
description: Explore the codebase to understand patterns, find implementations, or gather context. Use when the user says "research", "pesquisa", "find", "explore", "where is", "how does X work", or needs to understand existing code before making changes.
user-invocable: true
argument-hint: "<what to research — e.g. 'how generics work', 'find all uses of mem_alloc', 'where is fmt::Display implemented'>"
---

**Delegation**: Use the Agent tool to dispatch a `researcher` agent with `model: haiku` for this task. Research is read-only and does not need an expensive model.

## Research Workflow

### 1. Understand the Question

Parse `$ARGUMENTS` to determine what the user wants to know:
- **"where is X"** → Find file/function location
- **"how does X work"** → Trace through implementation
- **"find all uses of X"** → Search for references
- **"what pattern does X follow"** → Analyze conventions

### 2. Search Strategy

Use a combination of:
- `Glob` for finding files by name pattern
- `Grep` for finding code by content
- `Read` for understanding specific implementations
- `mcp__tml__docs_search` for documented APIs

### 3. Report

Present findings concisely:
- File locations with line numbers
- Key code snippets (keep short)
- Patterns and conventions identified
- Relationships between components
