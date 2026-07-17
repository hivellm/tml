## 1. Implementation
- [x] 1.1 println intrinsic — emit_println: literal path uses puts(ptr), dynamic path uses printf with @.str_fmt_newline
- [x] 1.2 print intrinsic — emit_print: uses printf with @.str_fmt_no_newline (no trailing newline)
- [x] 1.3 Format interpolation — emit_print_format_consts generates "%s\n" and "%s" global constants; emit_to_string_format_consts generates format strings for all primitives
- [x] 1.4 Primitive to_string — emit_i64_to_string, emit_i32_to_string, emit_f64_to_string (snprintf into 64-byte alloca), emit_bool_to_string (select between "true"/"false" constants)
- [x] 1.5 Tests — print_format.test.tml: 10 @test functions covering println literal/dynamic, print, i64/i32/f64/bool to_string, format consts, intrinsic decls

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments on all new functions
- [x] 2.2 Write tests covering the new behavior — print_format.test.tml (10 @test functions)
- [x] 2.3 Run tests and confirm they pass — all sources and tests type-check clean
