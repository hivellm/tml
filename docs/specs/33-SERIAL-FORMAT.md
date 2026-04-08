# 33 — Binary Serialization Format

Specification for the AST and TypeEnv binary serialization used by the hybrid
pipeline (phase12f) to transfer data between TML compiler stages.

## Overview

Two binary formats exist:

| Format | Magic (LE) | Purpose |
|--------|-----------|---------|
| AST | `0x41535420` | Serialized `Module` (parser AST) |
| TypeEnv | `0x54454E56` | Serialized type environment |

Both share the same header structure and primitive encoding.

## Header (6 bytes)

```
magic:         u32 (LE)
version_major: u8
version_minor: u8
```

Version compatibility: unknown major = hard error; unknown minor = skip unknown
sections (future-proofing).

## Primitive Encoding

| Type | Format |
|------|--------|
| `u8` | 1 byte |
| `u16` | 2 bytes LE |
| `u32` | 4 bytes LE |
| `u64` | 8 bytes LE |
| `bool` | u8 (0/1) |
| `varint` | Unsigned LEB-128 |
| `string` | varint(len) + UTF-8 bytes |
| `opt_string` | varint(len); len=0 means None |
| `opt<T>` | u8 tag (0=None, 1=Some) + T |
| `list<T>` | varint(count) + count x T |

## AST Format

```
Header
string        module_name
list<string>  doc_comments
SourceSpan    module_span
list<Decl>    declarations
```

Each `Decl` starts with `u8 tag`:

| Tag | Kind |
|-----|------|
| 0 | FuncDecl |
| 1 | StructDecl |
| 2 | UnionDecl |
| 3 | EnumDecl |
| 4 | TraitDecl |
| 5 | ImplDecl |
| 6 | TypeAliasDecl |
| 7 | ConstDecl |
| 8 | UseDecl |
| 9 | ModDecl |

Expressions use `u8 tag` (0-34), statements `u8 tag` (0-4), types `u8 tag`
(0-9), patterns `u8 tag` (0-8). See `design.md` in the task directory for
full field-by-field schema.

### SourceSpan

```
start_line:   u32
start_col:    u32
start_offset: u32
start_length: u32
end_line:     u32
end_col:      u32
end_offset:   u32
end_length:   u32
```

## TypeEnv Format

```
Header
list<StructDef>     structs
list<EnumDef>       enums
list<BehaviorDef>   behaviors
list<FuncGroup>     functions    (grouped: name + overloads)
... additional sections (6 more, currently empty)
```

### Semantic Type Tags (inline type encoding)

| Tag | Type | Payload |
|-----|------|---------|
| 0x00 | Null | none |
| 0x01 | Primitive | u8 kind (0-16) |
| 0x02 | Named | string name + string module_path |
| 0x03 | Ref | bool is_mut + opt_string lifetime + SemType inner |
| 0x04 | Ptr | bool is_mut + SemType inner |
| 0x05 | Array | SemType elem + varint size |
| 0x06 | Slice | SemType elem |
| 0x07 | Tuple | list<SemType> elems |
| 0x08 | Func | list<SemType> params + SemType ret |
| 0x09 | Generic | string name |
| 0x0A | Maybe | SemType inner |
| 0x0B | Outcome | SemType ok + SemType err |
| 0x0C | Trait object | string trait_name |
| 0x0D | Impl trait | string trait_name |
| 0x0E | Self | none |
| 0x0F | Infer | none |
| 0x10 | Associated | string type_name + string assoc_name |

### Primitive Kinds (u8)

| Value | Kind |
|-------|------|
| 0 | I8 |
| 1 | I16 |
| 2 | I32 |
| 3 | I64 |
| 4 | U8 |
| 5 | U16 |
| 6 | U32 |
| 7 | U64 |
| 8 | F32 |
| 9 | F64 |
| 10 | Bool |
| 11 | Char |
| 12 | Str |
| 13 | Unit |
| 14 | RawPtr |
| 15 | Void |
| 16 | Never |

### Function Groups

Functions are stored in groups for overload support:

```
varint   group_count
  string   group_name
  varint   overload_count
    FuncSig  signature (repeated overload_count times)
```

## Query Integration

When `QueryContext::parse_module(path)` is called, it checks for a sibling
`<path>.ast.bin` file. If present, the binary AST is deserialized instead of
lexing + parsing the source. This enables the hybrid pipeline to provide
pre-parsed ASTs from TML-written stages.

See: `compiler/src/query/query_context.cpp` lines 121-161.

## C++ Reader API

```cpp
#include "serial/ast_reader.hpp"
#include "serial/typeenv_reader.hpp"

// AST
parser::Module mod = serial::read_ast(bytes, "source.tml");

// TypeEnv
types::TypeEnv env = serial::read_typeenv(bytes);
auto sd = env.lookup_struct("MyStruct");
```

## TML Writer API

```tml
use std::serial::BinaryWriter
use std::serial::{write_u8, write_varint, write_str}

let buf = Buffer::new(4096)
write_u8(buf, 0x41)     // magic byte
write_varint(buf, 42)   // LEB-128
write_str(buf, "hello") // length-prefixed
```

## Files

| File | Role |
|------|------|
| `lib/std/src/serial/mod.tml` | Primitives + error types |
| `lib/std/src/serial/writer.tml` | BinaryWriter type |
| `lib/std/src/serial/reader.tml` | BinaryReader type |
| `compiler/src/serial/ast_reader.cpp` | C++ AST deserializer |
| `compiler/include/serial/ast_reader.hpp` | C++ AST reader API |
| `compiler/src/serial/typeenv_reader.cpp` | C++ TypeEnv deserializer |
| `compiler/include/serial/typeenv_reader.hpp` | C++ TypeEnv reader API |
| `compiler/tests/serial/serial_reader_test.cpp` | 26 round-trip + perf tests |

## Performance

Measured on 200 structs x 10 fields (233KB AST, 8.7KB TypeEnv):

- AST deserialization: ~2.5ms per iteration
- TypeEnv deserialization: ~0.75ms per iteration

Both are well under 5% of compilation overhead.
