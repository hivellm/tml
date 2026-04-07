# Tasks: AST & TypeEnv Binary Serializers

**Status**: Planned (0/22)
**Depends on**: phase12a (MIR consolidation — serializer needs stable IR)
**Blocks**: phase12f (hybrid pipeline needs serializers to pass data between C++ and TML stages)
**Duration**: 4–6 weeks
**Risk**: Medium
**Reference**: Existing MIR serializers in `compiler/src/mir/serializer/` for format patterns

---

## Phase 1: Serialization Format Design (3 items)

- [ ] 1.1 Study existing MIR serializer in `compiler/src/mir/serializer/binary_writer.cpp` — document format patterns (magic u32 `0x4D495220`, version u16×2, length-prefixed strings, TypeTag bytes)
- [ ] 1.2 Design AST binary format: magic bytes, version header, type tags for each Decl/Expr/Stmt/Type variant, varint encoding for counts, length-prefixed strings
- [ ] 1.3 Design schema: define serialization order for each AST node type (Module → declarations list → each Decl variant via `ast_decls.hpp`, `ast_exprs.hpp`, `ast_stmts.hpp`, `ast_types.hpp`)

## Phase 2: AST Serializer — TML Side (5 items)

- [ ] 2.1 Create `lib/std/src/serial/mod.tml` — binary serialization primitives (`write_u8`, `write_u32`, `write_varint`, `write_str` using `Buffer`)
- [ ] 2.2 Create `lib/std/src/serial/writer.tml` — `BinaryWriter` type wrapping `Buffer` with write methods and flush to `Outcome[Unit, IOError]`
- [ ] 2.3 Create `lib/std/src/serial/reader.tml` — `BinaryReader` type with `read_u8`, `read_u32`, `read_varint`, `read_str` returning `Outcome[T, SerialError]`
- [ ] 2.4 Implement AST node serialization: `Module` → declarations list → each decl with type tag matching `ast_decls.hpp` variants (`FuncDecl`, `StructDecl`, `TraitDecl`, `ImplDecl`, etc.)
- [ ] 2.5 Implement expression and statement serialization: recursive tree walk with depth tracking, serialize `SourceSpan` for each node

## Phase 3: AST Deserializer — C++ Side (4 items)

- [ ] 3.1 Create `compiler/src/serial/ast_reader.cpp` — read binary AST back to C++ `Module` struct (as defined in `compiler/include/parser/ast.hpp`)
- [ ] 3.2 Create `compiler/include/serial/ast_reader.hpp` — public API: `Module read_ast(const std::vector<uint8_t>& data)`
- [ ] 3.3 Handle version compatibility: reject unknown versions with clear error, support forward-compatible unknown fields via skip-ahead using stored lengths
- [ ] 3.4 Integrate into query pipeline: add `ReadSerializedAST` query key to `QueryContext` alongside existing `ParseModuleKey` — select path based on whether `.ast.bin` cache file is present

## Phase 4: TypeEnv Serializer (4 items)

- [ ] 4.1 Design TypeEnv serialization schema: types map, functions map, behaviors map, impls list — all cross-references encoded as indices into a flattened type table
- [ ] 4.2 Implement TypeEnv serialization (TML side) — serialize all maps and cross-reference indices in a stable order matching `compiler/include/types/env.hpp` fields
- [ ] 4.3 Implement TypeEnv deserialization (C++ side) — reconstruct `TypeEnv` from binary, rebuild pointer/reference fields from index table
- [ ] 4.4 Handle type references: serialize as u32 indices into type table; on deserialization, build the table first, then fix up all cross-reference pointers in a second pass

## Phase 5: Round-Trip Tests (4 items)

- [ ] 5.1 Test: serialize simple `Module` (1 function, 1 struct) → deserialize → compare all fields including `SourceSpan`
- [ ] 5.2 Test: serialize complex `Module` (generics, `impl` blocks, closures, `lowlevel` blocks) → deserialize → compare
- [ ] 5.3 Test: serialize `TypeEnv` with 100+ types (stdlib subset) → deserialize → verify all symbol lookups return identical results
- [ ] 5.4 Compile test: parse `.tml` with C++ pipeline, serialize AST, deserialize, type-check deserialized AST → IR-diff output vs direct parse path — must be identical

## Phase 6: Performance (2 items)

- [ ] 6.1 Benchmark: serialize/deserialize the full stdlib AST (`lib/core/` + `lib/std/`) — measure time and peak memory against parsing the source directly
- [ ] 6.2 Verify total serialization/deserialization overhead is less than 5% of compilation time on the benchmark suite from `mcp__tml__project_slow-tests`

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
