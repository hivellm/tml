---
name: gen_path unsigned flag bug
description: gen_path() in collections.cpp didn't set last_expr_is_unsigned_, causing stale zext for signed constants like I32::MIN
type: project
---

`gen_path()` at `compiler/src/codegen/llvm/expr/collections.cpp:407-435` returns constant values
from `global_constants_` without setting `last_expr_is_unsigned_`. The stale value from a previous
expression could be `true`, causing `zext` instead of `sext` when the constant is later extended
to a wider type (e.g., in `assert_eq` which promotes i32 to i64 for comparison).

**Why:** `last_expr_is_unsigned_` is a sticky state variable that many codegen functions set but
`gen_path` missed. The pattern of "set last_expr_type_ but forget last_expr_is_unsigned_" could
exist in other codegen functions too.

**How to apply:** When debugging incorrect `zext`/`sext` in IR output, check whether the source
expression's codegen function properly sets `last_expr_is_unsigned_`. This flag propagates through
`assert_eq`, cast expressions, and other places that widen integers.
