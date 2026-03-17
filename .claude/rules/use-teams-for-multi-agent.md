---
description: When launching 2+ agents in parallel, MUST use Teams instead of independent agents
globs: *
alwaysApply: true
---

# MANDATORY: Use Teams for Multi-Agent Work

When launching 2 or more agents that need to work in parallel, you MUST use Teams (via `team-lead` agent or `TeamCreate`) instead of spawning individual independent agents.

## Rules

1. **2+ parallel agents = MUST use a Team** — never spawn multiple independent agents when a team can coordinate them
2. **Use `team-lead` (sonnet) as the orchestrator** — it dispatches to specialists and aggregates results
3. **Name each agent** in the team for clear identification (e.g., `name: "tester"`, `name: "reviewer"`)
4. **Single agent = no team needed** — only use teams for genuinely parallel multi-agent work

## Wrong

```
# Spawning 3 independent agents without coordination
Agent(prompt="run tests", subagent_type="tester")
Agent(prompt="review code", subagent_type="code-reviewer")
Agent(prompt="update docs", subagent_type="docs-writer")
```

## Correct

```
# Team-led coordinated execution
Agent(prompt="Coordinate: (1) run tests, (2) review code, (3) update docs",
      subagent_type="team-lead", name="coordinator")
```

## Why

Independent agents duplicate research, compete for resources, and produce inconsistent results. Teams share context via SendMessage, avoid duplicate work, and produce coherent output.
