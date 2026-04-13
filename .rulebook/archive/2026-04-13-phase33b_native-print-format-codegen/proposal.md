# Proposal: phase33b_native-print-format-codegen

## Why
`print` and `println` are the most fundamental output primitives in any language. Without format string codegen, no native TML program can produce visible output. Every hello-world, debug trace, and diagnostic message depends on these intrinsics being lowered correctly to libc `puts`/`printf` calls. The native backend currently has no implementation for these intrinsics, making it impossible to run or verify any program that produces output.

## What Changes
- `emit_intrinsic.tml`: add `println` branch that lowers to `puts(str_ptr)` for simple string literals and `printf("%s\n", str_ptr)` for format strings.
- `emit_intrinsic.tml`: add `print` branch that lowers to `printf("%s", str_ptr)` without a trailing newline.
- Format interpolation: resolve template literal segments at codegen time — static strings become `getelementptr` constants, dynamic slots (`{expr}`) call `to_string` on the inner expression and concatenate via a stack-allocated buffer.
- Primitive `to_string` helpers for `I32`, `I64`, `F64`, `Bool`: each lowers to `snprintf` into a 64-byte stack buffer and returns a `Str` pointing into it.
- New test file `compiler-tml/tests/codegen/print_format.test.tml` covering all five cases.

## Impact
- Affected specs: `compiler-tml/src/codegen/emit_intrinsic.tml`
- Affected code: `compiler-tml/src/codegen/emit_intrinsic.tml`, `compiler-tml/src/codegen/emit_expr.tml` (template literal expansion)
- Breaking change: NO
- User benefit: Native TML programs can produce output; all print-based tests become runnable on the native backend.
