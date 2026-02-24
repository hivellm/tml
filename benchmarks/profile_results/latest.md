# TML vs C++ Profile Benchmark Report

**Date:** 2026-02-24 01:56:53
**Platform:** Windows 10 (AMD64)
**C++ Compiler:** MSVC /O2 (Visual Studio 2022)
**TML Compiler:** tml.exe --release (LLVM -O3)
**Total runtime:** 136.5s

---

## Math

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Integer Addition | 0 | 0 | ~tied |
| Integer Multiplication | 2 | 0 | **TML 2.0x faster** |
| Integer Division | 1 | 0 | ~tied |
| Integer Modulo | 0 | 0 | ~tied |
| Bitwise Operations | 0 | 0 | ~tied |
| Float Addition | 0 | 0 | ~tied |
| Float Multiplication | 0 | 0 | ~tied |
| Square Root | 1 | - | C++ only |
| Fibonacci Recursive (n=20) | 19 | 0 | **TML 19.0x faster** |
| Fibonacci Iterative (n=50) | 10 | 0 | **TML 10.0x faster** |
| Empty Loop | 0 | 0 | ~tied |

## String

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Concat Small (3 strings) | 8 | 131 | C++ 16x |
| Concat Loop (with reserve) | 4 | - | C++ only |
| Concat Loop (naive) | 5 | - | C++ only |
| String Length | 0 | 0 | ~tied |
| String Compare (equal) | 0 | 0 | ~tied |
| String Compare (different) | 0 | 0 | ~tied |
| Int to String | 9 | 35 | C++ 4x |
| String Copy | 3 | - | C++ only |
| String Repeat (50 chars) | 156 | - | C++ only |
| Sprintf Formatting | 73 | - | C++ only |
| Log Building | 13 | - | C++ only |
| Concat Loop (Text - O(n)) | - | 4 | TML only |
| Concat Loop (Str - O(n^2)) | - | 3028 | TML only |
| Log Building (Text - O(n)) | - | 58 | TML only |
| Log Building (Str - O(n^2)) | - | 4152 | TML only |

## Collections

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Vec Push (grow) | 4 | - | C++ only |
| Vec Push (reserved) | 1 | - | C++ only |
| Vec Random Access | 0 | - | C++ only |
| Vec Iteration | 0 | - | C++ only |
| Vec Pop | 1 | - | C++ only |
| Vec Set | 0 | - | C++ only |
| HashMap Insert | 88 | - | C++ only |
| HashMap Insert (reserved) | 59 | - | C++ only |
| HashMap Lookup | 5 | - | C++ only |
| HashMap Contains | 4 | - | C++ only |
| HashMap Remove | 85 | - | C++ only |
| HashMap String Key | 143 | - | C++ only |
| Array Sequential Read | - | 0 | TML only |
| Array Random Access | - | 0 | TML only |
| Array Write | - | 0 | TML only |
| Array Initialization | - | 0 | TML only |
| Linear Search | - | 0 | TML only |
| Accumulate Sum | - | 0 | TML only |

## Hashmap

> C++ benchmark failed: hashmap_bench.exe not found

| Operation | TML (ns/op) | TML Ops/sec |
|---|---|---|
| HashMap Insert | 39 | 25,512,807 |
| HashMap Lookup | 11 | 90,235,605 |
| HashMap Contains | 8 | 122,080,744 |
| HashMap Remove | 41 | 24,211,902 |

## List

> C++ benchmark failed: list_bench.exe not found

| Operation | TML (ns/op) | TML Ops/sec |
|---|---|---|
| List Push (grow) | 2 | 361,010,830 |
| List Random Access | 0 | 166,666,666,666 |
| List Iteration | 0 | 3,874,467,260 |
| List Pop | 1 | 856,311,012 |
| List Set | 0 | 3,051,571,559 |

## Control Flow

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| If-Else Chain (4 branches) | 0 | 0 | ~tied |
| Nested If (4 levels) | 0 | 0 | ~tied |
| Switch Dense (10 cases) | 1 | - | C++ only |
| Switch Sparse (10 cases) | 1 | - | C++ only |
| For Loop | 0 | - | C++ only |
| While + Break | 0 | - | C++ only |
| Nested Loops (1000x1000) | 0 | 0 | ~tied |
| Loop + Continue | 0 | 0 | ~tied |
| Ternary Chain | 0 | 0 | ~tied |
| Short-Circuit AND | 0 | 0 | ~tied |
| Short-Circuit OR | 0 | 0 | ~tied |
| When Dense (10 cases) | - | 0 | TML only |
| When Sparse (10 cases) | - | 0 | TML only |
| Loop | - | 0 | TML only |

## Closure

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Lambda No Capture | 0 | - | C++ only |
| Lambda Value Capture | 0 | - | C++ only |
| Lambda Ref Capture | 0 | - | C++ only |
| Lambda Multi Capture | 0 | - | C++ only |
| std::function Wrapper | 0 | - | C++ only |
| Higher Order Function | 0 | 0 | ~tied |
| Closure Factory | 0 | - | C++ only |
| Manual Loop (index) | 0 | - | C++ only |
| Iterator Loop | 0 | - | C++ only |
| Range-based For | 0 | - | C++ only |
| std::for_each | 0 | - | C++ only |
| std::accumulate | 0 | - | C++ only |
| std::transform | 0 | - | C++ only |
| Filter Pattern | 0 | - | C++ only |
| Chain Operations | 0 | 0 | ~tied |
| Function Pointer | - | 0 | TML only |
| Function Pointer Switch | - | 2 | TML only |
| Function Composition | - | 0 | TML only |
| Manual Loop (array) | - | 0 | TML only |
| Map Simulation | - | 0 | TML only |
| Filter Simulation | - | 0 | TML only |
| Fold/Reduce Simulation | - | 0 | TML only |

## Function

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Inline Call | 0 | 0 | ~tied |
| Direct Call (noinline) | 0 | 0 | ~tied |
| Many Parameters (6 args) | 0 | 0 | ~tied |
| Fibonacci Recursive (n=20) | 21 | 12 | **TML 1.8x faster** |
| Fibonacci Tail (n=50) | 0 | 0 | ~tied |
| Mutual Recursion (n=100) | 0 | 0 | ~tied |
| Function Pointer | 1 | - | C++ only |
| std::function | 2 | - | C++ only |
| Virtual Call | 0 | - | C++ only |
| Devirtualized Call | 0 | - | C++ only |

## Memory

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| malloc/free (64 bytes) | 151 | - | C++ only |
| new/delete Small (16 bytes) | 23 | - | C++ only |
| new/delete Medium (64 bytes) | 154 | - | C++ only |
| new/delete Large (192 bytes) | 28 | - | C++ only |
| Stack Struct Creation | 0 | - | C++ only |
| unique_ptr RAII | 22 | - | C++ only |
| Struct Copy (64 bytes) | 0 | - | C++ only |
| memcpy (1KB) | 0 | - | C++ only |
| Array Alloc (1000 structs) | 29 | - | C++ only |
| Sequential Access | 0 | 0 | ~tied |
| Random Access | 0 | 0 | ~tied |
| Pointer Indirection | 0 | - | C++ only |
| Stack Struct Small (16 bytes) | - | 0 | TML only |
| Stack Struct Medium (64 bytes) | - | 0 | TML only |
| Struct Field Access | - | 0 | TML only |
| Nested Struct Access | - | 0 | TML only |
| Point Creation | - | 0 | TML only |
| Array Copy (1000 elements) | - | 0 | TML only |
| Array Fill (1000 elements) | - | 0 | TML only |

## Text

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| stringstream Append | 185 | - | C++ only |
| string Reserve+Append | 20 | - | C++ only |
| string Naive Append | 4 | - | C++ only |
| Build JSON | 362 | - | C++ only |
| Build HTML | 12 | - | C++ only |
| Build CSV | 34 | - | C++ only |
| Small Appends (1 char) | 0 | - | C++ only |
| Number Formatting | 746 | 87 | **TML 8.6x faster** |
| Log Messages | 53 | 50 | ~tied |
| Path Building | 24 | 35 | C++ 1.5x |
| Text Append (O(1) amortized) | - | 47 | TML only |
| Str Naive Append (O(n^2)) | - | 3080 | TML only |
| Build JSON (10K items) | - | 102 | TML only |
| Build HTML (10K items) | - | 18 | TML only |
| Build CSV (10K rows) | - | 28 | TML only |
| Small Appends push() | - | 0 | TML only |
| Small Appends push_str() | - | 4 | TML only |
| Small Appends fill_char() batch | - | 0 | TML only |
| Small Appends raw ptr | - | 0 | TML only |

## Oop

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Object Creation (stack) | 0 | 0 | ~tied |
| Method Call (non-virtual) | 0 | 0 | ~tied |
| Method Chaining | 0 | 0 | ~tied |
| Virtual Dispatch (3 types) | 2 | - | C++ only |
| Virtual Dispatch (single type) | 0 | - | C++ only |
| Deep Inheritance (4 levels) | 0 | - | C++ only |
| Multiple Inheritance | 0 | - | C++ only |
| Stack Allocation | 0 | 0 | ~tied |
| Heap Allocation (unique_ptr) | 22 | - | C++ only |
| Shared Pointer (shared_ptr) | 25 | - | C++ only |
| Circle Method Calls | - | 1 | TML only |
| Rectangle Method Calls | - | 1 | TML only |
| Deep Composition (4 levels) | - | 0 | TML only |

## Type

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Int Widen (i32->i64) | 0 | 0 | ~tied |
| Int Narrow (i64->i32) | 0 | 0 | ~tied |
| Unsigned to Signed | 0 | 0 | ~tied |
| Signed to Unsigned | 0 | 0 | ~tied |
| Int to Float (i64->f64) | 0 | 0 | ~tied |
| Float to Int (f64->i64) | 0 | 0 | ~tied |
| Float Widen (f32->f64) | 0 | 0 | ~tied |
| Float Narrow (f64->f32) | 0 | 0 | ~tied |
| Byte Chain (i8->i64) | 0 | 0 | ~tied |
| Mixed Type Arithmetic | 2 | 2 | ~tied |
| Pointer to Int | 0 | - | C++ only |
| Int to Pointer | 0 | - | C++ only |
| Bit Reinterpret | 0 | - | C++ only |

## Encoding

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| Base64 Encode (13 bytes) | 36 | 52 | C++ 1.4x |
| Base64 Encode (95 bytes) | 95 | 123 | C++ 1.3x |
| Base64 Decode (20 chars) | 70 | 57 | ~tied |
| Hex Encode (13 bytes) | 34 | 36 | ~tied |
| Hex Decode (26 chars) | 60 | 50 | ~tied |
| Base32 Encode (13 bytes) | 39 | 47 | C++ 1.2x |

## Crypto

| Operation | C++ (ns/op) | TML (ns/op) | Verdict |
|---|---:|---:|---|
| SHA256 (13 bytes) | 306 | 430 | C++ 1.4x |
| SHA256 (95 bytes) | 324 | 447 | C++ 1.4x |
| SHA256 Streaming (3 updates) | 318 | 481 | C++ 1.5x |
| SHA512 (13 bytes) | 416 | 551 | C++ 1.3x |
| MD5 (13 bytes) | 320 | 444 | C++ 1.4x |
| SHA256 + to_hex (13 bytes) | 354 | 532 | C++ 1.5x |

## Json

> C++ benchmark failed: json_bench.exe not found

| Operation | TML (ns/op) | TML Ops/sec |
|---|---|---|
| Parse Tiny (27 bytes) | 277 | 3,598,002 |
| Parse Small (200 bytes) | 1049 | 952,903 |
| Parse Medium (500 bytes) | 2832 | 353,016 |
| Parse Standard (non-SIMD) | 2023 | 494,308 |
| Field Access | 1076 | 929,069 |
| Array Iteration | 1177 | 849,160 |
| Nested Object Access | 1533 | 652,291 |
| Parse + Validate | 1117 | 894,584 |
| Object Traversal | 1170 | 854,022 |

---

## Summary

| | Count | % |
|---|---:|---:|
| **TML faster** | 5 | 8% |
| **Tied** (< 1.2x) | 44 | 72% |
| **C++ faster** | 12 | 20% |
| **Total comparisons** | 61 | 100% |

## Errors

- **C++/hashmap**: hashmap_bench.exe not found
- **C++/list**: list_bench.exe not found
- **C++/json**: json_bench.exe not found
