# Proposal: AST & TypeEnv Binary Serializers

**Task**: phase12e_ast-serializers
**Status**: Planned
**Priority**: P0
**Estimated effort**: 4–6 weeks
**Risk**: Medium

## Why

The hybrid pipeline (phase12f) must pass structured AST and TypeEnv data between TML-written compiler stages and the C++ pipeline across a process boundary. No serialization mechanism exists; this task defines a stable binary format, writer (TML), and reader (C++) with round-trip verification.

## Problem

The hybrid pipeline (phase12f) must pass structured data — ASTs and type environments — between
a TML-written compiler stage running as a subprocess and the C++ pipeline that continues
processing the output. There is currently no mechanism to serialize a `Module` AST or a `TypeEnv`
to bytes and reconstruct them in a different process.

This is not a problem that can be avoided with shared memory or function calls: the hybrid
pipeline deliberately runs TML stages as separate processes to isolate them from the C++ runtime
and to enable stage caching via executable fingerprints. Process isolation requires that all
inter-stage data cross a serialization boundary.

The problem is also not trivially solved by using text formats like JSON or s-expressions. The
`Module` AST has deep recursive structure with source spans attached to every node, and the
`TypeEnv` has a dense web of cross-references between types (a struct field's type points to
another type entry, which may be generic, with type arguments that are themselves type entries).
A naive text serialization of `TypeEnv` would either serialize duplicates (wasting space) or
require a two-pass algorithm with an ID table. The binary format must be designed upfront to
handle this correctly.

The existing MIR serializer (`compiler/src/mir/serializer/binary_writer.cpp`) demonstrates that
the project already has a working binary format pattern: magic `u32 0x4D495220`, version `u16×2`,
length-prefixed strings, `TypeTag` bytes per variant. This task extends that pattern to cover
AST and TypeEnv.

## Proposed Solution

Three-layer implementation: a generic binary I/O layer in TML, AST-specific serialization logic
split between TML (writer) and C++ (reader), and TypeEnv serialization with a two-pass
cross-reference scheme.

**Layer 1 — Binary I/O primitives** (`lib/std/src/serial/`): A `BinaryWriter` type wrapping
`Buffer` with `write_u8`, `write_u32_le`, `write_u64_le`, `write_varint` (LEB128), and
`write_str` (varint length prefix + UTF-8 bytes). A `BinaryReader` type with corresponding read
methods returning `Outcome[T, SerialError]`. These primitives are used by all higher-level
serializers and are independently testable.

**Layer 2 — AST serialization**: The TML side (`lib/std/src/serial/ast_writer.tml`) walks the
`Module` AST produced by the TML-written parser (phase 1 of self-hosting) and writes a binary
stream. Each AST node variant gets a single-byte tag matching the variant discriminant in
`compiler/include/parser/ast_decls.hpp`, `ast_exprs.hpp`, `ast_stmts.hpp`, `ast_types.hpp`.
Recursive nodes write their children inline; list fields are prefixed by a varint count.
`SourceSpan` (file ID, start offset, end offset) is written as three varints per node.

The C++ side (`compiler/src/serial/ast_reader.cpp`) reads the binary stream and reconstructs
a `Module` using the same node tags. This is integrated into `QueryContext` as a new
`ReadSerializedAST` query key: when a `.ast.bin` file is present alongside the source, the
query reads and deserializes it instead of parsing the source. This path is taken only when the
hybrid pipeline provides a pre-parsed AST from a TML stage.

**Layer 3 — TypeEnv serialization**: `TypeEnv` contains cyclic cross-references (two types can
reference each other through their fields). Serialization uses a flattened type table: first
pass assigns a `u32` index to every `Type*` in the environment, second pass writes each type's
fields using indices instead of pointers wherever a cross-reference occurs. Deserialization
reads the full type table first, then fixes up all cross-reference fields in a second pass.
This is a well-known technique (used by `rustc` for its crate metadata format) and avoids the
complexity of a general graph serializer.

**Format header**: All serialized files begin with:
- Magic: `u32` (AST = `0x544D4C41` "TMLA", TypeEnv = `0x544D4C45` "TMLE")
- Version: `u16` major, `u16` minor (current: 1.0)
- Format flag: unknown fields must be skip-able; each optional section is preceded by a `u32`
  byte length so readers can skip ahead to the next section

## Key Decisions

**TML writes, C++ reads — not the reverse.** The TML-written compiler stages produce ASTs and
TypeEnvs that the C++ backend consumes. The serialization asymmetry (TML writer, C++ reader)
matches the data flow direction. Writing a C++ serializer for ASTs would also be needed to
cache C++-parsed ASTs, but that is a future optimization, not part of this task.

**Type table indices, not pointers.** TypeEnv cross-references are dense and cyclic. Encoding
them as indices into a flat table is the only approach that survives a process boundary. The
two-pass scheme (assign indices, then write; read table, then fix up) is explicitly modeled on
`rustc`'s crate metadata serialization.

**InternedStr from phase12b for symbol names.** Every identifier in the AST appears many times
(a function called in 50 places has its name in 50 `CallExpr` nodes). Using `InternedStr`
(a `u32` index) for all symbol names in the binary format reduces serialized size and avoids
redundant string copying. This is why phase12b is listed as a dependency.

**Version rejection on unknown major, skip-ahead on unknown minor.** A serialized AST produced
by a newer TML stage should not silently corrupt a C++ reader built against an older schema.
Unknown major version → hard error. Unknown minor version with a stored byte-length → skip the
unknown section and continue reading known fields.

**Round-trip test as the acceptance gate.** The task is not complete until a full-pipeline
round-trip test passes: parse `.tml` with C++ → serialize AST → deserialize AST → type-check
→ IR-diff output vs direct parse path. This is more rigorous than unit tests on individual
node types.

## Files to Create/Modify

**Created (TML)**:
- `lib/std/src/serial/mod.tml` — module declaration
- `lib/std/src/serial/writer.tml` — `BinaryWriter` type
- `lib/std/src/serial/reader.tml` — `BinaryReader` type
- `lib/std/src/serial/ast_writer.tml` — AST tree walk + binary serialization
- `lib/std/src/serial/typeenv_writer.tml` — TypeEnv two-pass serialization
- `lib/std/tests/serial/basic.test.tml` — round-trip tests for primitives
- `lib/std/tests/serial/ast_roundtrip.test.tml` — AST serialize/deserialize/compare
- `lib/std/tests/serial/typeenv_roundtrip.test.tml` — TypeEnv serialize/deserialize/lookup

**Created (C++)**:
- `compiler/src/serial/ast_reader.cpp`
- `compiler/include/serial/ast_reader.hpp`
- `compiler/src/serial/typeenv_reader.cpp`
- `compiler/include/serial/typeenv_reader.hpp`

**Modified (C++)**:
- `compiler/src/query/query_context.cpp` — add `ReadSerializedAST` query key
- `compiler/include/query/query_key.hpp` — add `ReadSerializedASTKey` variant
- `compiler/CMakeLists.txt` — add new serial source files

**Modified (TML)**:
- `lib/std/src/mod.tml` — add `pub mod serial`

## Success Criteria

- Round-trip test passes: parse `.tml` with C++ pipeline, serialize AST, deserialize, type-check,
  IR-diff output vs direct parse — must be identical (exit 0 from `tml ir-diff`)
- Round-trip test passes for complex AST: generics, `impl` blocks, closures, `lowlevel` blocks
- TypeEnv round-trip: 100+ types serialized and deserialized, all symbol lookups return identical
  results before and after
- Serialization + deserialization of the full `lib/core/` AST completes in < 5% of the time
  needed to parse all source files from scratch
- Format version mismatch (wrong major) produces a clear error, not a crash

## Dependencies

**Blocks**: phase12f (hybrid pipeline needs serializers to transfer data across the subprocess
boundary at each stage transition).

**Depends on**: phase12a (MIR consolidation — the serializer's AST format must match a stable
single MIR pipeline; if both MIR paths exist, the format could diverge), phase12b (string
interning — `InternedStr` is used for compact symbol name encoding in the binary format).
