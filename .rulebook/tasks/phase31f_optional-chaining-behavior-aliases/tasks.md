## 1. Optional chaining
- [ ] 1.1 std/json/types.tml -- get_path_string, get_path_i64 with ?.
- [ ] 1.2 compiler-tml/types/imports.tml -- module lookup chains with ?.
- [ ] 1.3 compiler-tml/types/module.tml -- module lookup chains with ?.
- [ ] 1.4 Scan remaining files for nested when on Maybe with single method call

## 2. Behavior aliases
- [ ] 2.1 Define common aliases in core (Copyable = Duplicate + PartialEq, Hashable = Hash + PartialEq)
- [ ] 2.2 Apply aliases in generic bounds across core where pattern repeats
- [ ] 2.3 Apply aliases in std where pattern repeats

## 3. Tail (mandatory -- enforced by rulebook v5.3.0)
- [ ] 3.1 Update CHANGELOG.md
- [ ] 3.2 Run /check on all modified files
- [ ] 3.3 Run tests on affected suites and confirm they pass
