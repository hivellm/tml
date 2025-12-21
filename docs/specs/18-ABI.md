# TML v1.0 — Application Binary Interface

## 1. Overview

The TML ABI defines how compiled TML code interfaces at the binary level. This specification ensures:
- Compatibility between separately compiled modules
- Interoperability with C/C++ code
- Stable library interfaces across compiler versions

## 2. Symbol Naming

### 2.1 Name Mangling Scheme

```
_TML$<version>$<module>$<item>$<signature>

Components:
- _TML         : TML symbol prefix
- <version>    : ABI version (v1)
- <module>     : Module path with $ separator
- <item>       : Function/type name
- <signature>  : Type signature hash (8 hex chars)
```

### 2.2 Mangling Examples

```tml
// Source
module mylib.utils

public func parse(s: String) -> I32
public func parse(s: String, radix: I32) -> I32
type Parser { ... }
```

```
// Mangled symbols
_TML$v1$mylib$utils$parse$a1b2c3d4     // parse(String) -> I32
_TML$v1$mylib$utils$parse$e5f6g7h8     // parse(String, I32) -> I32
_TML$v1$mylib$utils$Parser$new$i9j0k1l2
_TML$v1$mylib$utils$Parser$drop$m3n4o5p6
```

### 2.3 Special Symbols

| Symbol | Mangled Name |
|--------|--------------|
| Module init | `_TML$v1$<module>$__init` |
| Module cleanup | `_TML$v1$<module>$__fini` |
| Type drop | `_TML$v1$<module>$<Type>$drop$<hash>` |
| Type clone | `_TML$v1$<module>$<Type>$clone$<hash>` |
| Trait vtable | `_TML$v1$<module>$<Type>$<Trait>$vtable` |

### 2.4 Extern "C" (No Mangling)

```tml
// These use C names directly
extern "C" func my_c_function() -> I32
// Symbol: my_c_function

#[export_name = "custom_name"]
public func exported() -> I32
// Symbol: custom_name
```

## 3. Calling Conventions

### 3.1 TML Default Convention

Based on platform ABI (SysV AMD64 or Win64):

**Register Usage (x86_64 SysV):**
| Register | Purpose |
|----------|---------|
| rdi, rsi, rdx, rcx, r8, r9 | Integer/pointer args 1-6 |
| xmm0-xmm7 | Float args 1-8 |
| rax | Return value (integer) |
| xmm0 | Return value (float) |
| rsp | Stack pointer |
| rbp | Frame pointer (optional) |
| rbx, r12-r15 | Callee-saved |

**Register Usage (x86_64 Windows):**
| Register | Purpose |
|----------|---------|
| rcx, rdx, r8, r9 | Args 1-4 (int/ptr) |
| xmm0-xmm3 | Args 1-4 (float) |
| rax | Return value |
| rbx, rbp, rdi, rsi, r12-r15 | Callee-saved |

### 3.2 Argument Passing

```
Classification:
- INTEGER: integers, pointers, references up to 8 bytes
- SSE: floats, doubles
- MEMORY: structs > 16 bytes, arrays

Rules:
1. Arguments ≤ 8 bytes: passed in registers
2. Arguments 8-16 bytes: split across 2 registers
3. Arguments > 16 bytes: passed by hidden pointer
4. Aggregates with only floats: use SSE registers
```

### 3.3 Return Values

```
1. Return ≤ 8 bytes: in rax (integer) or xmm0 (float)
2. Return 8-16 bytes: rax + rdx or xmm0 + xmm1
3. Return > 16 bytes: caller allocates, passes hidden pointer in rdi
```

### 3.4 Stack Frame Layout

```
High addresses
┌─────────────────────────┐
│    Previous frame       │
├─────────────────────────┤
│    Return address       │ ← pushed by call
├─────────────────────────┤
│    Saved rbp            │ ← frame pointer
├─────────────────────────┤
│    Local variables      │
├─────────────────────────┤
│    Spilled registers    │
├─────────────────────────┤
│    Call arguments       │ ← for calls made
│    (stack-passed)       │
├─────────────────────────┤
│    Red zone (128 bytes) │ ← SysV only, leaf functions
└─────────────────────────┘
Low addresses (rsp)
```

## 4. Type Layout

### 4.1 Primitive Types

| Type | Size | Alignment | Representation |
|------|------|-----------|----------------|
| Bool | 1 | 1 | 0x00 = false, 0x01 = true |
| I8/U8 | 1 | 1 | Two's complement / Unsigned |
| I16/U16 | 2 | 2 | Little-endian |
| I32/U32 | 4 | 4 | Little-endian |
| I64/U64 | 8 | 8 | Little-endian |
| I128/U128 | 16 | 16 | Little-endian |
| F32 | 4 | 4 | IEEE 754 single |
| F64 | 8 | 8 | IEEE 754 double |
| Char | 4 | 4 | Unicode scalar value |
| Unit | 0 | 1 | Zero-sized |

### 4.2 Pointer Types

| Type | Size | Alignment |
|------|------|-----------|
| `&T` | 8 | 8 |
| `&mut T` | 8 | 8 |
| `*const T` | 8 | 8 |
| `*mut T` | 8 | 8 |
| `Box[T]` | 8 | 8 |

### 4.3 Fat Pointers

```
Slice &[T]:
┌─────────────────┬─────────────────┐
│   data: *T      │    len: U64     │
│   (8 bytes)     │    (8 bytes)    │
└─────────────────┴─────────────────┘
Size: 16, Align: 8

Trait object &dyn Trait:
┌─────────────────┬─────────────────┐
│   data: *T      │  vtable: *VTable│
│   (8 bytes)     │    (8 bytes)    │
└─────────────────┴─────────────────┘
Size: 16, Align: 8
```

### 4.4 Struct Layout

```tml
type Point { x: F64, y: F64 }

Layout (default - Rust-like):
┌─────────────────┬─────────────────┐
│    x: F64       │    y: F64       │
│   (8 bytes)     │   (8 bytes)     │
└─────────────────┴─────────────────┘
Size: 16, Align: 8

// With repr(C)
#[repr(C)]
type CPoint { x: F64, y: F64 }
// Same layout, but guaranteed C-compatible order

// Padding example
type Padded { a: U8, b: U64, c: U8 }

Default layout (may reorder):
┌────┬────┬─────────┬────┬─────────┐
│ a  │ c  │ padding │  b │ padding │
│ 1  │ 1  │   6     │  8 │    0    │
└────┴────┴─────────┴────┴─────────┘
Size: 16, Align: 8

repr(C) layout (no reorder):
┌────┬─────────┬─────────┬────┬─────────┐
│ a  │ padding │    b    │ c  │ padding │
│ 1  │   7     │    8    │ 1  │    7    │
└────┴─────────┴─────────┴────┴─────────┘
Size: 24, Align: 8
```

### 4.5 Enum Layout

```tml
// Simple enum (no data)
type Color = Red | Green | Blue
// Size: 1, Align: 1 (fits in U8 tag)

// Enum with data
type Option[T] = Some(T) | None

Layout for Option[I32]:
┌─────┬─────────┬──────────┐
│ tag │ padding │   value  │
│  1  │    3    │    4     │
└─────┴─────────┴──────────┘
Size: 8, Align: 4

// Niche optimization for Option[&T]:
// Uses null pointer for None
// Size: 8 (same as &T)

// Complex enum
type Value =
    | Int(I64)
    | Float(F64)
    | String(String)

Layout:
┌─────┬─────────┬────────────────────────────┐
│ tag │ padding │     largest variant        │
│  1  │    7    │   String: 24 bytes         │
└─────┴─────────┴────────────────────────────┘
Size: 32, Align: 8
```

### 4.6 String Layout

```
String:
┌─────────────────┬─────────────────┬─────────────────┐
│   ptr: *U8      │    len: U64     │  capacity: U64  │
│   (8 bytes)     │   (8 bytes)     │   (8 bytes)     │
└─────────────────┴─────────────────┴─────────────────┘
Size: 24, Align: 8
```

### 4.7 Collection Layouts

```
List[T] (Vec):
┌─────────────────┬─────────────────┬─────────────────┐
│   ptr: *T       │    len: U64     │  capacity: U64  │
│   (8 bytes)     │   (8 bytes)     │   (8 bytes)     │
└─────────────────┴─────────────────┴─────────────────┘
Size: 24, Align: 8

Box[T]:
┌─────────────────┐
│    ptr: *T      │
│   (8 bytes)     │
└─────────────────┘
Size: 8, Align: 8

Rc[T]:
┌─────────────────┐
│  ptr: *RcInner  │  → RcInner { strong: U64, weak: U64, value: T }
│   (8 bytes)     │
└─────────────────┘
Size: 8, Align: 8

Arc[T]: Same as Rc, but with atomic counters
```

## 5. Trait Objects (Virtual Tables)

### 5.1 VTable Layout

```
VTable for trait Show:
┌─────────────────┐
│   drop_fn       │  0: destructor
├─────────────────┤
│   size          │  8: size of type
├─────────────────┤
│   align         │ 16: alignment
├─────────────────┤
│   show_fn       │ 24: Show.show method
└─────────────────┘

For trait with multiple methods:
┌─────────────────┐
│   drop_fn       │  0
│   size          │  8
│   align         │ 16
│   method_1      │ 24
│   method_2      │ 32
│   ...           │
└─────────────────┘
```

### 5.2 Trait Object Calls

```tml
func call_show(obj: &dyn Show) {
    obj.show()
}

// Generated:
// 1. Load vtable pointer from obj+8
// 2. Load show_fn from vtable+24
// 3. Call show_fn with data pointer (obj+0)
```

## 6. Exception Handling

### 6.1 Panic Unwinding

TML uses platform unwinding mechanism:
- Linux/macOS: DWARF-based (libunwind)
- Windows: SEH (Structured Exception Handling)

```
Unwinding flow:
1. Panic called
2. Runtime searches for handlers
3. Each frame's cleanup runs (Drop calls)
4. If no catch, process terminates
```

### 6.2 Landing Pad

```llvm
; LLVM IR for function with cleanup
define void @function() personality ptr @__tml_personality {
entry:
    %obj = call ptr @allocate()
    invoke void @may_panic()
        to label %continue unwind label %cleanup
continue:
    call void @use(ptr %obj)
    call void @drop_obj(ptr %obj)
    ret void
cleanup:
    %lp = landingpad { ptr, i32 }
        cleanup
    call void @drop_obj(ptr %obj)
    resume { ptr, i32 } %lp
}
```

### 6.3 Personality Function

```cpp
// TML personality function
extern "C" _Unwind_Reason_Code __tml_personality(
    int version,
    _Unwind_Action actions,
    uint64_t exception_class,
    _Unwind_Exception* exception,
    _Unwind_Context* context
);
```

## 7. Thread-Local Storage

### 7.1 TLS Variables

```tml
#[thread_local]
var COUNTER: I32 = 0

// Access generates TLS lookup
func increment() {
    COUNTER += 1
}
```

### 7.2 TLS Model

| Model | Use Case |
|-------|----------|
| `local-exec` | Main executable, static linking |
| `initial-exec` | Shared lib loaded at startup |
| `local-dynamic` | Multiple TLS vars in same DSO |
| `global-dynamic` | dlopen'd libraries |

## 8. Atomics and Memory Model

### 8.1 Memory Ordering

| TML | LLVM | C++11 |
|-----|------|-------|
| `Relaxed` | `monotonic` | `memory_order_relaxed` |
| `Acquire` | `acquire` | `memory_order_acquire` |
| `Release` | `release` | `memory_order_release` |
| `AcqRel` | `acq_rel` | `memory_order_acq_rel` |
| `SeqCst` | `seq_cst` | `memory_order_seq_cst` |

### 8.2 Atomic Operations

```tml
import std.sync.atomic.AtomicI32

var counter = AtomicI32.new(0)

func increment() {
    counter.fetch_add(1, Ordering.SeqCst)
}

// Generated:
// lock xadd [counter], 1  (x86)
// ldaxr/stlxr loop        (ARM)
```

## 9. Debug Information

### 9.1 DWARF Format

TML generates DWARF v4/v5 debug info:

```
DW_TAG_compile_unit
  DW_AT_name: "src/main.tml"
  DW_AT_language: DW_LANG_TML (custom)
  DW_AT_producer: "tmlc 1.0.0"

DW_TAG_subprogram
  DW_AT_name: "process"
  DW_AT_linkage_name: "_TML$v1$main$process$a1b2c3d4"
  DW_AT_decl_file: 1
  DW_AT_decl_line: 42

DW_TAG_structure_type
  DW_AT_name: "Point"
  DW_TAG_member
    DW_AT_name: "x"
    DW_AT_type: <F64>
    DW_AT_data_member_location: 0
```

### 9.2 Source Maps

```json
{
  "version": 1,
  "file": "main.o",
  "source": "src/main.tml",
  "mappings": [
    {"address": "0x1000", "line": 42, "column": 5, "stable_id": "@a1b2c3d4"}
  ]
}
```

## 10. Version Compatibility

### 10.1 ABI Version

```
Current ABI: TML ABI v1

Version is encoded in:
- Symbol prefix: _TML$v1$...
- Object metadata
- Library metadata

Breaking changes increment version:
- Type layout changes
- Calling convention changes
- Name mangling changes
```

### 10.2 Stability Guarantees

| Category | Stable |
|----------|--------|
| Symbol mangling scheme | Yes |
| Primitive type layout | Yes |
| Calling convention | Yes |
| VTable layout | Yes |
| Collection layouts | No (internal) |
| Optimization internals | No |

### 10.3 Forward Compatibility

```tml
// Library compiled with tmlc 1.0
// Can be used by tmlc 1.x (same ABI version)

// Check at load time:
// 1. Verify ABI version matches
// 2. Verify type signatures match
// 3. Verify capability requirements
```

---

*Previous: [17-FFI.md](./17-FFI.md)*
*Next: [19-RUNTIME.md](./19-RUNTIME.md) — Runtime System*
