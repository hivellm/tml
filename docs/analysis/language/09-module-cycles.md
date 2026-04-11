# F09: Module Circular Import Resolution

**Priority**: Medium
**Impact**: AST module architecture
**Complexity**: High (module resolver)

## Problem

Related types must be split across files to avoid import cycles:

```tml
//! As of phase0p 9.3 (cross-module cyclic imports verified), the canonical
//! Expr-family definitions live HERE rather than in `ast/nodes.tml`.
//! `nodes.tml` still holds Type/Pattern/Module types and imports `Expr` via
//! `use compiler::ast::exprs::Expr`.
```

The AST naturally has `Expr`, `Decl`, `Pattern`, `Type` all referencing each
other. Splitting them into separate files creates documentation overhead and
import chains of 80+ lines.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/ast/exprs.tml` | 1-6 | Cycle-avoidance doc comment |
| `compiler-tml/src/ast/decls.tml` | 1-6 | Same pattern |
| `compiler-tml/src/parser/parse_expr.tml` | 8-89 | 89 lines of imports |

## Proposal

### A. Allow mutual imports between sibling modules

Modules within the same package should be allowed to import each other:

```tml
// ast/exprs.tml
use compiler::ast::decls::Decl  // OK even though decls imports Expr

// ast/decls.tml
use compiler::ast::exprs::Expr  // OK, sibling module
```

Implementation: Two-pass module resolution (collect declarations first, resolve types second).

### B. Glob imports

```tml
use compiler::ast::exprs::*     // Import all public items
use compiler::ast::{Expr, Decl, Pattern}  // Multi-import
```

## C++ Compiler Changes

1. **Module resolver**: Two-pass approach — declare first, resolve second
2. **Cycle detection**: Allow cycles within the same package, reject cross-package
3. **Multi-import syntax**: Parse `use mod::{A, B, C}` as sugar for individual imports
