## 1. Core library
- [ ] 1.1 core/types/option.tml -- flatten nested when in zip, unzip, transpose, one_of
- [ ] 1.2 core/types/result.tml -- flatten nested when in map_ok, map_err, transpose
- [ ] 1.3 core/async/task.tml -- flatten Poll equality, waker handling

## 2. Std library
- [ ] 2.1 std/types.tml -- flatten filter(), or_else() patterns
- [ ] 2.2 std/json/types.tml -- flatten get(), key_at(), get_path_* patterns
- [ ] 2.3 std/thread/mod.tml -- flatten result unwrapping
- [ ] 2.4 std/sync/mpsc.tml -- flatten channel receive patterns

## 3. Compiler-tml
- [ ] 3.1 compiler-tml/types/imports.tml -- flatten module resolution chains
- [ ] 3.2 compiler-tml/types/register.tml -- flatten symbol dispatch
- [ ] 3.3 compiler-tml/types/checker/check_expr.tml -- flatten symbol resolution

## 4. Tail (mandatory -- enforced by rulebook v5.3.0)
- [ ] 4.1 Update CHANGELOG.md
- [ ] 4.2 Run /check on all modified files
- [ ] 4.3 Run tests on affected suites and confirm they pass
