# Analysis: core-std-ergonomics-audit

> Created by `/analysis core-std-ergonomics-audit` on 2026-04-11
> Analysis ID: `core-std-ergonomics-audit`

## Index

| File | Purpose |
|------|---------|
| [findings.md](./findings.md) | Numbered findings (F-001 ... F-010) |
| [execution-plan.md](./execution-plan.md) | Phased implementation plan |
| [manifest.json](./manifest.json) | Analysis metadata |

## Executive summary

Comprehensive audit of `lib/core/src/` (200 files), `lib/std/src/` (338 files), and `compiler-tml/src/` (39 files) for opportunities to apply the 10 language ergonomics features shipped in phase 30 (for-in loops, pattern guards, struct update syntax, @auto directives, @repr, destructuring let, let-else, behavior aliases, closure type inference, optional chaining).

**Top 5 findings:**

1. **F-001 — Manual index loops**: 250+ instances across all three libraries. `for i in 0 to N` eliminates ~500 lines of boilerplate.
2. **F-002 — Nested when for Maybe**: 80+ instances. `let-else` and pattern guards flatten deeply nested code.
3. **F-003 — Manual Duplicate/PartialEq impls**: 50+ manual impls on simple data structs replaceable by `@auto(duplicate, equal)`.
4. **F-004 — Missing @repr on small enums**: 15+ enums with <256 variants could use `@repr(U8)`.
5. **F-005 — Destructuring let opportunities**: 70+ field extraction patterns improvable with destructuring.

## Methodology

- **Agents used**: 3 parallel Explore agents (core, std, compiler-tml)
- **Scope**: All `.tml` source files in `lib/core/src/`, `lib/std/src/`, `compiler-tml/src/`
- **Exclusions**: Test files, C/C++ runtime, build scripts

## Conclusion

The phase 30 language features have significant applicability across the entire TML codebase. The highest-ROI refactors are for-in loops (mechanical, safe, 250+ sites), let-else unwrapping (reduces nesting depth by 1-3 levels in 80+ sites), and @auto directives (eliminates ~50 manual trait impls). Work should be organized by library (core, std, compiler-tml) and by feature type within each, with for-in loops first (lowest risk, highest count) and behavior aliases last (lowest count).
