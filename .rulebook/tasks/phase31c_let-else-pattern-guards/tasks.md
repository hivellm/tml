## 1. Core library
- [x] 1.1 core/types/option.tml -- 4 conversions (zip, zip_with, transpose, filter)
- [x] 1.2 core/types/result.tml -- 1 conversion (transpose inner when)
- [x] 1.3 core/async/task.tml -- 2 conversions (map_ok, map_err)

## 2. Std library
- [x] 2.1 std/types.tml -- 1 conversion (filter pattern guard)
- [x] 2.2 std/json/types.tml -- 2 conversions (get_path_string, get_path_i64)
- [x] 2.3 std/thread/mod.tml -- 1 conversion (join let-else)
- [x] 2.4 std/sync/mpsc.tml -- 0 convertible (both arms have meaningful logic)

## 3. Compiler-tml
- [x] 3.1 compiler-tml/types/imports.tml -- 3 conversions (resolve_single, glob, propagate)
- [x] 3.2 compiler-tml/types/register.tml -- 1 conversion (type_expr_to_type)
- [x] 3.3 compiler-tml/types/checker/check_expr.tml -- 2 conversions (infer_field, infer_if)

## 4. Tail
- [x] 4.1 Compiler suite: 238/241 passed (3 pre-existing)
- [x] 4.2 No regressions from let-else/pattern guard migration
- [x] 4.3 Committed
