# TML Compiler Documentation Guidelines

## CRITICAL RULE: Mandatory Documentation

**EVERY change or new feature in the compiler MUST be documented BEFORE or IMMEDIATELY after implementation.**

The consistency of the implementation depends on synchronization between code and documentation.

## Documents That MUST be Updated

### 1. Grammar (`docs/02-LEXICAL.md` and `docs/03-GRAMMAR.md`)

**When to update:**
- Add new tokens/keywords
- Modify parsing rules
- Add new syntactic constructs
- Change operator precedence

**Example:**
```
Implemented: const declarations
Update in:
  - docs/02-LEXICAL.md: add keyword 'const'
  - docs/03-GRAMMAR.md: add ConstDecl production rule
```

### 2. RFCs (`docs/rfcs/`)

**When to update:**
- Implement an existing RFC → mark as implemented
- Changes affecting design decisions → create new RFC or update existing
- Deviations from an approved RFC → document deviation and justification

**RFC Structure:**
```markdown
- Status: Draft | Review | Accepted | Implemented | Rejected
- Version: when it was implemented
- Deviations: if there were deviations from the original proposal
```

### 3. User Documentation (`docs/user/`)

**When to update:**
- Add builtin functions → update `appendix-03-builtins.md`
- New user-visible features → create guides
- Behavior changes → update existing guides

**Example:**
```
Implemented: panic() builtin
Update in:
  - docs/user/appendix-03-builtins.md:
    * Add panic(msg: Str) -> Never in Error Handling section
    * Include usage example
    * Document behavior (prints stderr, exit code 1)
```

### 4. Specs (`docs/specs/`)

**When to update (if necessary):**
- Type system changes → `docs/specs/04-TYPES.md`
- Semantics changes → `docs/specs/05-SEMANTICS.md`
- IR changes → `docs/specs/08-IR.md`
- New builtins → `docs/specs/13-BUILTINS.md`
- Module changes → `docs/specs/07-MODULES.md`

**Example:**
```
Implemented: const declarations
Update in:
  - docs/specs/05-SEMANTICS.md: global constants semantics
  - docs/specs/08-IR.md: how consts are represented in IR
```

## Mandatory Workflow

### For New Features

```
1. Plan implementation
   ↓
2. BEFORE coding: update docs/03-GRAMMAR.md with new syntax
   ↓
3. Implement in compiler
   ↓
4. IMMEDIATELY after implementing: update documentation
   ↓
5. Verify consistency:
   - Does grammar reflect parser?
   - Are RFCs marked as implemented?
   - Do user docs have examples?
   - Do specs cover semantics?
   ↓
6. Commit code + documentation TOGETHER
```

### For Changes to Existing Features

```
1. Identify documentation impact
   ↓
2. Update affected docs
   ↓
3. Make code change
   ↓
4. Verify consistency
   ↓
5. Commit code + docs TOGETHER
```

## Documentation Checklist

Before considering a feature "complete", verify:

- [ ] Grammar updated with new syntax?
- [ ] Tokens/keywords added to LEXICAL.md list?
- [ ] RFC (if exists) marked as implemented?
- [ ] User documentation has practical examples?
- [ ] Builtins documented in appendix-03-builtins.md?
- [ ] Specs reflect semantic behavior?
- [ ] INDEX.md updated if new concepts added?
- [ ] Examples in docs/14-EXAMPLES.md (if applicable)?

## Examples of Correct Commits

❌ **Wrong:**
```
commit: "Add const declarations"
files: src/parser/parser_decl.cpp, src/types/checker.cpp
```

✅ **Correct:**
```
commit: "Add const declarations with full documentation"
files:
  - src/parser/parser_decl.cpp
  - src/types/checker.cpp
  - src/codegen/llvm_ir_gen.cpp
  - docs/02-LEXICAL.md (added 'const' keyword)
  - docs/03-GRAMMAR.md (added ConstDecl production)
  - docs/specs/05-SEMANTICS.md (const semantics)
  - docs/user/appendix-03-builtins.md (if relevant)
```

## RFC Maintenance

### RFC States

```
Draft → Review → Accepted → Implemented
                         ↘ Rejected
```

**When implementing an RFC:**

1. Update `docs/rfcs/INDEX.md`:
   ```markdown
   - [RFC-XXXX](./RFC-XXXX-title.md) - **Implemented** in v1.0
   ```

2. Update the RFC itself:
   ```markdown
   # RFC-XXXX: Feature Name

   **Status**: Implemented
   **Version**: 1.0
   **Implementation Date**: 2025-12-22

   ## Implementation Notes

   - Deviations from proposal (if any)
   - Files changed
   - Related documentation updates
   ```

## Special Case: IR (Intermediate Representation)

IR changes are **CRITICAL** and require extra documentation:

**Mandatory updates:**
- `docs/specs/08-IR.md` - format and structure
- `docs/rfcs/ir/VERSION.md` - IR versioning
- `docs/rfcs/RFC-0007-IR.md` - if substantial change

**Example:**
```
Changed: const representation in IR
Update:
  1. docs/specs/08-IR.md: add (const name type value)
  2. docs/rfcs/ir/VERSION.md: increment minor version
  3. Document backward compatibility
```

## Penalties for Not Documenting

**IMPORTANT:** Undocumented code is considered **incomplete** and can:
- Cause inconsistencies between implementation and specification
- Make future maintenance difficult
- Break the contract with language users
- Generate subtle unexpected behavior bugs

**Golden Rule:** If it's not documented, it's not implemented.

## Verification Tools

### Manual Verification
```bash
# Before commit, verify:
git diff docs/
git diff src/

# Ask yourself:
# 1. Did I change grammar? → docs/03-GRAMMAR.md updated?
# 2. Did I add keyword? → docs/02-LEXICAL.md updated?
# 3. New builtin? → docs/user/appendix-03-builtins.md updated?
```

### PR/Commit Checklist
```markdown
## Documentation Updated

- [ ] Grammar (LEXICAL.md, GRAMMAR.md)
- [ ] RFCs marked as implemented
- [ ] User documentation
- [ ] Specs (if applicable)
- [ ] Examples added
- [ ] INDEX.md updated (if new concepts)
```

## Responsibilities

**Every developer/AI working on the compiler MUST:**
1. Read this document before making changes
2. Follow the mandatory workflow
3. Update documentation in EVERY relevant commit
4. Verify consistency before finalizing work

**There are no exceptions to this rule.**

---

**Last updated:** 2025-12-22
**Version:** 1.0
