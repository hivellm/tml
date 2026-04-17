# Proposal: `tml cc` CLI — drive the phase23b C frontend end-to-end

## Why

Phase 23b delivered the full C17 frontend in TML (`compiler-tml/src/cc/*.tml`):
a lexer, parser (declarations / expressions / statements), type checker, and
MIR lowering — 24/24 items, ~3,800 LOC. The entire pipeline type-checks and
its components pass their smoke tests.

What phase 23b intentionally stopped short of is the outermost shell. There
is no `tml cc <file.c>` command today. The MIR the TML-side lowerer produces
is never fed into the existing MIR→LLVM backend, because nothing invokes it.
A user who types `tml cc compiler/runtime/core/essential.c` gets `unknown
command`.

Phase 23b closed with three concrete follow-up items that all boil down to
the same missing piece:

1. **Phase 7 / 7.1** — `tml cc compiler/runtime/core/essential.c` must
   self-compile TML's C runtime essential.
2. **Phase 7 / 7.2** — `tml cc compiler/runtime/memory/mem.c` must
   self-compile; linking `essential.o + mem.o + rest` must produce binaries
   that pass the TML test suite.
3. **Tail 1.3** — full runtime integration is gated on the CLI shim +
   the handful of TML codegen bugs tracked in the bring-up notes.

This phase delivers the shim. After it lands, TML gains the ability to
compile its own C runtime without calling out to Clang or Zig CC, closing
the last step before phase 23c (C++ subset) can extend the same pipeline
to the LLVM/LLD C++ shim files and complete the self-hosting toolchain.

## What Changes

A new CLI subcommand and a small FFI bridge. No new compilation semantics —
every IR decision is already made by phase 23b modules.

### CLI subcommand `tml cc <file.c>`

A new C++ file `compiler/src/cli/commands/cmd_cc.cpp` registering the `cc`
subcommand in the standard CLI dispatch table. Flags mirror the existing
`tml compile` surface: `-o <path>`, `-c`, `-O0/1/2`, `-I <path>`,
`-D <name>[=<val>]`, `-target <triple>`, `-g`, `--emit=<obj|llvm-ir|mir|ast|tokens>`.

### Pipeline

```
tml cc file.c  →  [cmd_cc.cpp]
                   │
                   ├─ open+read file.c
                   ├─ invoke phase23a preprocessor → List[PpToken]
                   ├─ FFI into TML: c_lexer + tokenize  → List[CToken]
                   ├─ FFI into TML: c_parser + cp_parse_translation_unit → CTranslationUnit
                   ├─ FFI into TML: c_lower + lower_translation_unit → MirModule
                   ├─ hand MirModule to existing MIR → LLVM backend
                   └─ write .obj via existing LLD linker integration
```

### FFI bridge (`compiler/include/cc/cc_bridge.hpp`)

Plain-C ABI wrapper exposing the four TML entry points as opaque-pointer
APIs. The implementation in `compiler/src/cc/cc_bridge.cpp` hands the
opaque pointers through to the TML-compiled entry points registered via
the existing `tml_register_extern` plumbing (same approach `tml std`
uses for core library hooks).

### Pre-work: codegen bug fixes

Three TML codegen bugs documented in the phase 23b bring-up notes
(parser bring-up notes 7, 8, 9) cause the TML-compiled parser / lowerer
to crash at runtime on non-trivial inputs. These must be resolved before
the CLI can actually compile real C files. The fixes live in the C++
compiler (`compiler/src/codegen/`):

1. **Bug #7** — Enum variant pattern-binding fails for non-heap fields
   when the variant leads with a `Heap[T]` (e.g. `Func(Heap[CDeclarator], List[CParam], I64)` rejects `Func(_, ps, _)` at runtime).
2. **Bug #8** — Deeply-nested constructor expressions in a single
   statement hang or crash at runtime — duplicate codegen recurses on
   each nested enum/struct payload without a Heap boundary.
3. **Bug #9** — Large enums with by-value struct payloads crash on the
   duplicate codegen path.

Once fixed, the phase 23b implementation's `Heap`-wrapping workarounds
(applied throughout `parser.tml`, `types.tml`, `lower.tml`) can stay or
be un-applied — either way the pipeline will run.

### Acceptance criteria

- `tml cc compiler/runtime/core/essential.c` produces an `.obj` / `.lib`
  byte-for-byte compatible (for the symbols the TML runtime consumes)
  with the current Clang-produced artifact — symbol names, sizes,
  alignment all match.
- `tml cc compiler/runtime/memory/mem.c` same.
- Linking the TML-compiled objects into the TML test runner binary
  produces a binary that passes the full TML test suite (`tml test`
  with no filter).
- No regression in existing `tml build` / `tml compile` commands.

## Impact

- Affected specs: extends phase23b/STATE.md; unblocks phase23c.
- Affected code: new `compiler/src/cli/commands/cmd_cc.cpp`,
  `compiler/include/cc/cc_bridge.hpp`, `compiler/src/cc/cc_bridge.cpp`;
  fixes in `compiler/src/codegen/` for the three documented codegen bugs;
  optional cleanup of `Heap` workarounds in `compiler-tml/src/cc/*.tml`
  once the bugs are fixed.
- Breaking change: NO (new subcommand + internal bridge only).
- User benefit: TML can compile its own C runtime; removes the last
  external-C-compiler dependency; enables `tml test` to run against a
  runtime produced entirely by TML's toolchain.
