# ir-diff — LLVM IR Semantic Diff Tool

A standalone TML tool for comparing two LLVM IR text files for semantic
equivalence. Built to support the TML self-hosting effort by verifying
that the C++ frontend and TML-implemented compiler stages produce the
same IR for the same source program.

## Purpose

When porting compiler stages from C++ to TML, you need a way to verify
that the new implementation produces equivalent output. Byte-for-byte
diffs of LLVM IR are noisy: register names (`%0`, `%_tmp`), basic block
labels (`entry`, `bb1`), and debug metadata vary between runs even
when the underlying program is identical.

`ir-diff` solves this by:

1. **Parsing** the LLVM IR text into a structured representation
2. **Normalizing** register/label names to canonical sequential form
   (`%r0`, `%r1`, ..., `%b0`, `%b1`, ...)
3. **Stripping** debug metadata (`!dbg !N`) and trailing line comments
4. **Comparing** function-by-function, reporting the first differing
   instruction with surrounding context

Two IR files that are semantically identical (same instructions in the
same order, modulo naming) compare as equal regardless of whitespace,
naming choices, or debug info.

## Architecture

```
tools/ir_diff/
├── package.toml          Package manifest (lib + bin)
├── README.md             This file
├── src/
│   ├── mod.tml           Module root, re-exports
│   ├── types.tml         Shared data types (IrModule, IrFunction, ...)
│   ├── parser.tml        LLVM IR text → IrModule
│   ├── normalizer.tml    Canonical register/label renaming
│   ├── differ.tml        Function-by-function comparison
│   └── main.tml          CLI entry point
└── tests/
    ├── parser.test.tml         Parser unit tests
    ├── normalizer.test.tml     Normalizer unit tests
    ├── differ.test.tml         Differ unit tests with constructed IR
    ├── differ_minimal.test.tml Differ smoke test
    ├── norm_minimal.test.tml   Normalizer smoke test
    └── integration.test.tml    parse → diff pipeline tests
```

## Usage

### As a CLI

```sh
# Build the binary
tml build tools/ir_diff/src/main.tml

# Compare two IR files
build/debug/main.exe file1.ll file2.ll

# Summary mode (only print which functions differ)
build/debug/main.exe file1.ll file2.ll --summary

# Compare a single function
build/debug/main.exe file1.ll file2.ll --function my_function
```

Exit codes:

| Code | Meaning |
|------|---------|
| 0    | Files are semantically identical |
| 1    | Files differ (diff printed to stdout) |
| 2    | Usage error or file read failure |

Output format (default mode):

```
@@@ function_name @@@
- old line from file_a
+ new line from file_b
--- a: function_only_in_a
  (function exists only in first file)
+++ b: function_only_in_b
  (function exists only in second file)
```

### As a Library

```tml
use ir_diff::parser::parse
use ir_diff::normalizer::normalize
use ir_diff::differ::{diff, is_identical}

let ir_a = read_file_somehow("file_a.ll")
let ir_b = read_file_somehow("file_b.ll")

let mod_a = parse(ir_a)?
let mod_b = parse(ir_b)?

let norm_a = normalize(mod_a)
let norm_b = normalize(mod_b)

let result = diff(norm_a, norm_b)?
if is_identical(result) {
    // Files match
} else {
    // Inspect result.function_diffs / functions_only_in_a / functions_only_in_b
}
```

## What It Parses

`ir-diff` is **not a full LLVM IR parser**. It parses enough to support
semantic comparison of compiler frontend output:

- ✅ Function definitions: `define <ret> @<name>(<params>) <attrs> { ... }`
- ✅ Basic blocks with labels (including implicit `entry`)
- ✅ Instructions: opcode + operand list (uniform across all opcodes)
- ✅ Result registers (`%r = ...`)
- ✅ Global variables: `@name = <linkage> <ty> <init>`
- ✅ Trailing metadata stripping (`, !dbg !N`)
- ✅ Function pointer types in parameter lists (depth-aware paren walking)

It silently skips:

- Module flags (`!llvm.module.flags = ...`)
- Source filename declarations (`source_filename = ...`)
- Target triples (`target ...`)
- Attribute groups (`attributes #N = { ... }`)
- DWARF debug info contents (only metadata IDs are retained)
- Comments (`; ...`)

## What It Normalizes

- **Local registers**: `%0`, `%tmp`, `%retval.0` → `%r0`, `%r1`, ...
  in definition order
- **Function parameters**: `%x`, `%input` → `%a0`, `%a1`, ...
  in declaration order
- **Basic block labels**: `entry`, `bb1`, `loop.body` → `b0`, `b1`, ...
  in textual order
- **Trailing metadata**: stripped from raw lines
- **Trailing comments**: stripped from raw lines

Globals (`@name`) are **never** renamed — they are part of the program's
observable semantics (linking, exports, calls).

## Building

The package is a standalone TML crate. Build the CLI binary with:

```sh
tml build tools/ir_diff/src/main.tml
```

The output is `build/debug/main.exe` (Windows) or `build/debug/main`
(Unix). Use `--release` for an optimized build.

## Testing

Run all tests:

```sh
tml test --path tools/ir_diff/tests/parser.test.tml
tml test --path tools/ir_diff/tests/normalizer.test.tml
tml test --path tools/ir_diff/tests/differ.test.tml
tml test --path tools/ir_diff/tests/integration.test.tml
```

Or via the MCP test tool:

```
mcp__tml__test path="tools/ir_diff/tests/parser.test.tml"
```

## Known Limitations

1. **HashMap in tools/ packages**: The TML compiler has a codegen bug
   that causes `HashMap.len()` to fail at link time when used in
   packages outside `lib/std/`. The differ avoids HashMap by using a
   linear `List[I64]` index for matched-function tracking. The CLI
   skips the normalize step entirely; normalization is only available
   via the library API or the `normalizer.test.tml` tests.

2. **Cross-module field access on `DiffResult`**: A separate codegen
   bug causes direct field access (`result.functions_only_in_a.len()`)
   to return `()` when called from a different module than where
   `DiffResult` is defined. The differ exports accessor functions
   (`only_in_a_count`, `only_in_a_name`, `diffs_count`, etc.) that run
   inside the differ module where field access works correctly.

3. **NeverError codegen bug**: Importing `std::os` or `std::file::File`
   transitively pulls in `core::runtime::error::NeverError`, a struct
   with a `Never` field that codegen emits as `{ void }` — invalid
   LLVM IR. The CLI works around this by using direct `@extern("c")`
   declarations for `fopen`, `fread`, `fclose`, and `tml_os_args_*`
   instead of the std:: wrappers.

4. **Performance**: The parse pipeline is line-based and uses byte-by-
   byte string scanning. Large IR files (>10K lines) may be slow in
   debug builds. Use `--release` for production use.

5. **Function matching**: Functions are matched by exact name first,
   then by demangled name (stripping trailing `_<TYPE_ARG>` suffixes).
   Functions with structurally similar bodies but different names are
   reported as separate "only in A" / "only in B" entries.

## Status

This tool is part of `phase12d_ir-diff-tool` in the TML self-hosting
effort. It is the primary correctness-verification mechanism for porting
compiler stages from C++ to TML.
