---
name: commit
description: Stage and commit changes with a well-crafted conventional commit message. Use when the user says "commit", "comite", or asks to save changes.
user-invocable: true
allowed-tools: Bash(git *), Read, Grep, Glob
argument-hint: [optional commit message override]
---

## Commit Workflow

Follow this exact sequence:

### 1. Assess Changes

Run these in parallel:
- `git status -u` (never use `-uall`)
- `git diff --cached --stat` to see staged changes
- `git diff --stat` to see unstaged changes
- `git log --oneline -5` to see recent commit style

### 2. Stage Files

- If there are unstaged changes that should be committed, stage them with `git add <specific-files>`
- NEVER use `git add -A` or `git add .`
- NEVER commit `.env`, credentials, or secrets
- If changes span multiple logical units, ask the user if they want separate commits

### 3. Format Check

Before committing, run formatting on any C++ files that were modified:
- Check if any `.cpp` or `.hpp` files are staged
- If so, the pre-commit hook will check formatting automatically

### 4. Create Commit

Use conventional commit format matching the project style:

```
<type>(<scope>): <description>

[optional body]

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `perf`, `chore`
**Scopes**: `std`, `core`, `compiler`, `codegen`, `mcp`, `cli`, `test`, `docs`

If `$ARGUMENTS` is provided, use it as the commit message body/description.

ALWAYS use HEREDOC format:
```bash
git commit -m "$(cat <<'EOF'
type(scope): description

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

### 5. If Pre-commit Hook Fails

- Fix formatting issues (use `mcp__tml__format` tool)
- Re-stage the fixed files
- Create a NEW commit (NEVER amend)

### 6. Verify

Run `git status` after commit to confirm clean state.