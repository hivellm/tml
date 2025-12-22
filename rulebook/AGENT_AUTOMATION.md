<!-- AGENT_AUTOMATION:START -->
# Agent Automation Rules

**CRITICAL**: Mandatory workflow that AI agents MUST execute after EVERY implementation.

## Workflow Overview

After completing ANY feature, bug fix, or code change, execute this workflow in order:

### Step 1: Quality Checks (MANDATORY)

Run these checks in order - ALL must pass:

```bash
1. Type check (if applicable)
2. Lint (MUST pass with ZERO warnings)
3. Format code
4. Run ALL tests (MUST pass 100%)
5. Verify coverage meets threshold (default 95%)
```

**Language-specific commands**: See your language template (TYPESCRIPT, RUST, PYTHON, etc.) for exact commands.

**IF ANY CHECK FAILS:**
- ❌ STOP immediately
- ❌ DO NOT proceed
- ❌ DO NOT commit
- ✅ Fix the issue first
- ✅ Re-run ALL checks

### Step 2: Security & Dependency Audits

```bash
# Check for vulnerabilities (language-specific)
# Check for outdated dependencies (informational)
# Find unused dependencies (optional)
```

**Language-specific commands**: See your language template for audit commands.

**IF VULNERABILITIES FOUND:**
- ✅ Attempt automatic fix
- ✅ Document if auto-fix fails
- ✅ Include in Step 5 report
- ❌ Never ignore critical/high vulnerabilities without user approval

### Step 3: Update OpenSpec Tasks

If `openspec/` directory exists:

```bash
# Mark completed tasks as [DONE]
# Update in-progress tasks
# Add new tasks if discovered
# Update progress percentages
# Document deviations or blockers
```

### Step 4: Update Documentation

```bash
# Update ROADMAP.md (if feature is milestone)
# Update CHANGELOG.md (conventional commits format)
# Update feature specs (if implementation differs)
# Update README.md (if public API changed)
```

**⚠️ CRITICAL FOR TML COMPILER WORK:**

If working on the TML compiler (packages/compiler, parser, type checker, codegen, etc.):

**YOU MUST follow [rulebook/DOCUMENTATION.md](./DOCUMENTATION.md) STRICTLY**

This is **NON-NEGOTIABLE**. Every compiler change requires documentation updates:

✅ **MANDATORY Documentation Updates:**
- `docs/03-GRAMMAR.md` - if syntax changed
- `docs/02-LEXICAL.md` - if tokens/keywords added
- `docs/rfcs/` - mark RFCs as implemented
- `docs/user/appendix-03-builtins.md` - if builtins added
- `docs/specs/` - if semantics/types/IR changed

**Checklist before committing compiler changes:**
- [ ] Grammar updated with new syntax?
- [ ] Tokens/keywords added to LEXICAL.md?
- [ ] RFC (if exists) marked as implemented?
- [ ] User docs have examples?
- [ ] Builtins documented?
- [ ] Specs reflect semantic changes?

**Rule of Gold:** If not documented, it's not implemented.

See [DOCUMENTATION.md](./DOCUMENTATION.md) for complete guidelines.

### Step 5: Git Commit

**ONLY after ALL above steps pass:**

**⚠️ CRITICAL: All commit messages MUST be in English**

```bash
git add .
git commit -m "<type>(<scope>): <description>

- Detailed change 1
- Detailed change 2
- Tests: [describe coverage]
- Coverage: X% (threshold: 95%)

Closes #<issue> (if applicable)"
```

**Commit Types**: `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`

**Language Requirement**: Commit messages must be written in English. Never use Portuguese, Spanish, or any other language.

### Step 6: Report to User

```
✅ Implementation Complete

📝 Changes:
- [List main changes]

🧪 Quality Checks:
- ✅ Type check: Passed
- ✅ Linting: Passed (0 warnings)
- ✅ Formatting: Applied
- ✅ Tests: X/X passed (100%)
- ✅ Coverage: X% (threshold: 95%)

🔒 Security:
- ✅ No vulnerabilities

📊 OpenSpec:
- ✅ Tasks updated
- ✅ Progress: X% → Y%

📚 Documentation:
- ✅ CHANGELOG.md updated
- ✅ [other docs updated]

💾 Git:
- ✅ Committed: <commit message>
- ✅ Hash: <commit hash>

📋 Next Steps:
- [ ] Review changes
- [ ] Push to remote (if ready)
```

## Automation Exceptions

Skip steps ONLY when:

1. **Exploratory Code**: User says "experimental", "draft", "try"
   - Still run quality checks
   - Don't commit

2. **User Explicitly Requests**: User says "skip tests", "no commit"
   - Only skip requested step
   - Warn about skipped steps

3. **Emergency Hotfix**: Critical production bug
   - Run minimal checks
   - Document technical debt

**In ALL other cases: Execute complete workflow**

## Error Recovery

If workflow fails 3+ times:

```bash
1. Create backup branch
2. Reset to last stable commit
3. Report to user with error details
4. Request guidance or try alternative approach
```

## Best Practices

### DO's ✅
- ALWAYS run complete workflow
- ALWAYS update OpenSpec and documentation
- ALWAYS use conventional commits
- ALWAYS report summary to user
- ASK before skipping steps

### DON'Ts ❌
- NEVER skip quality checks without permission
- NEVER commit failing tests
- NEVER commit linting errors
- NEVER skip documentation updates
- NEVER assume user wants to skip automation
- NEVER commit debug code or secrets

## Summary

**Complete workflow after EVERY implementation:**

1. ✅ Quality checks (type, lint, format, test, coverage)
2. ✅ Security audit
3. ✅ Update OpenSpec tasks
4. ✅ Update documentation
5. ✅ Git commit (conventional format)
6. ✅ Report summary to user

**Only skip with explicit user permission and document why.**

<!-- AGENT_AUTOMATION:END -->