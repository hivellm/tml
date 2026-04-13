## 1. Implementation
- [ ] 1.1 Add `println` intrinsic lowering in `emit_intrinsic.tml`: bare string literal path lowers to `puts(ptr)`, format string path lowers to `printf("%s\n", ptr)`
- [ ] 1.2 Add `print` intrinsic lowering in `emit_intrinsic.tml`: lowers to `printf("%s", ptr)` with no trailing newline
- [ ] 1.3 Implement format interpolation expansion in `emit_expr.tml`: resolve template literal segments — static parts become `getelementptr` on global string constants, `{expr}` slots call `to_string` and concatenate into a stack buffer via `snprintf`
- [ ] 1.4 Add primitive `to_string` helpers for `I32`, `I64`, `F64`, and `Bool`: each lowers to `snprintf` into a 64-byte `alloca` buffer and returns a `Str` fat pointer
- [ ] 1.5 Write `compiler-tml/tests/codegen/print_format.test.tml` covering: `println` with literal, `print` with literal, interpolated `I64`, interpolated `Bool`, chained format with multiple slots

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
