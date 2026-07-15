# Proposal: C++ Subset Frontend — Classes, Templates, RAII

## Why

TML's backend uses two C++ shim files to interface with LLVM and LLD:
`compiler/src/backend/llvm_backend.cpp` (~550 LOC) and `compiler/src/backend/lld_linker.cpp`
(~670 LOC). These files are currently compiled by Clang or GCC. As long as TML cannot compile them,
TML requires an external C++ compiler — the final dependency preventing full toolchain independence.

Phase 23c eliminates that dependency. By implementing a C++ SUBSET frontend — not full C++20, but
exactly the subset needed by the shim files and their headers — TML gains the ability to compile its
own C++ glue code, completing the self-hosting toolchain.

This is the final task in ERA 4 and in the entire TML roadmap. After phase 23c, TML can build
itself from scratch using only TML tools.

## What Changes

A C++ subset frontend is added by extending the C17 frontend from phase23b. The C++ parser, type
checker, and lowering pass inherit from their C counterparts and add C++-specific constructs.
The MIR output is identical in structure to what the C and TML frontends produce — no new backend
changes are needed.

### Targeted Subset

The subset is chosen by analyzing what the LLVM/LLD shim files actually use:

| Feature | Used by shim | In subset |
|---------|-------------|-----------|
| Namespaces | Yes (`llvm::`, `lld::`) | Yes |
| Classes with methods | Yes | Yes |
| Constructors / destructors | Yes | Yes |
| Single inheritance | Yes (LLVM class hierarchy) | Yes |
| Virtual methods | Yes (LLVM IR builders) | Yes |
| Function templates | Yes (`make_unique`, etc.) | Yes |
| Class templates | Yes (`SmallVector<T>`, etc.) | Yes |
| Operator overloading | Yes (`<<` for streams) | Yes |
| RAII | Yes (all LLVM objects) | Yes |
| Move semantics | Yes (`std::move`) | Yes |
| `std::string`, `std::vector` | Yes | Yes |
| `std::unique_ptr` | Yes | Yes |
| Exceptions | No | No |
| Multiple inheritance | No (LLVM uses single) | No |
| `std::shared_ptr` | No | No |
| Coroutines, modules, ranges | No | No |
| RTTI / `dynamic_cast` | No (LLVM has its own) | No |

### Namespaces and ADL (~800 LOC)

Full namespace declaration, using directives, qualified lookup, and argument-dependent lookup (ADL).
ADL is required for `operator<<` to resolve correctly across namespace boundaries.

### Classes and Inheritance (~4,000 LOC)

Class declarations with access specifiers, member functions (including `const`, `static`, `explicit`,
`mutable`), constructors with member initializer lists, destructors, copy/move constructors and
assignment operators (Rule of Five), single inheritance, virtual dispatch via vtable pointer.

### Templates (~8,000 LOC)

Function templates with argument deduction, class templates with lazy instantiation, full/partial
specialization, non-type template parameters, template template parameters. Templates are the largest
component because of SFINAE, instantiation caching, and name mangling.

### Operator Overloading and RAII (~3,000 LOC)

All overloadable C++ operators, RAII destructor insertion in MIR lowering (reverse-order cleanup at
scope exit), move semantics (`T&&` rvalue references, `std::move`), and function overloading with
full overload resolution (viability check, ranking, tie-breaking).

### Standard Library Subset (~6,000 LOC)

`std::string` with SSO, `std::ostringstream`, `std::string_view`, `std::vector<T>`,
`std::unique_ptr<T>` with `std::make_unique`, and the C compatibility headers (`<cstdio>`,
`<cstdlib>`, `<cstring>`, `<cstdint>`, `<cstddef>`). This is the minimum needed to compile
the LLVM/LLD headers.

### C++ Preprocessor Extension (~500 LOC delta)

Phase 23a's preprocessor handles C macros. C++ adds `__cplusplus` = 201703L, `__cpp_*` feature
test macros, and `#pragma` (ignored for most, `#pragma once` already handled). These are small
additions to the existing preprocessor.

## Key Decisions

**Subset by shim analysis, not by spec**: Instead of implementing C++ features in specification
order, the subset is determined by what `llvm_backend.cpp` and `lld_linker.cpp` actually require.
Every feature in the subset can be traced to a concrete use in those files.

**No exceptions**: The shim files use LLVM's `Error` type and `Expected<T>` for error handling, not
C++ exceptions. Implementing exceptions (stack unwinding, EH tables, personality functions) would
add enormous complexity with zero benefit for the target files. Exception specifications (`noexcept`)
are parsed and used for optimization hints but no unwind tables are emitted.

**No RTTI**: LLVM has its own RTTI system (`llvm::isa<>`, `llvm::cast<>`, `llvm::dyn_cast<>`)
that does not use C++ RTTI. `dynamic_cast` and `typeid` are not needed and are not implemented.

**Itanium ABI name mangling on Linux/macOS, MSVC mangling on Windows**: The shim objects must be
linkable against the LLVM and LLD static libraries. Those libraries use the platform's native
name mangling. TML's C++ frontend must produce identically-mangled names for compatibility.

**Vtable layout follows the Itanium ABI**: vtable pointer is the first field; vtable entries are in
declaration order; virtual destructors occupy two consecutive slots (deleting destructor and
complete-object destructor). MSVC uses a different vtable layout on Windows — both must be supported.

**Templates are the hardest part**: SFINAE (Substitution Failure Is Not An Error) requires the type
checker to speculatively instantiate templates and recover from failure. Template specialization
ordering requires a partial-order relation on templates. Instantiation caching is critical for
compile performance. This is why templates are estimated at 8,000 LOC — nearly a quarter of the
total implementation.

## Risk

**Very High**: C++ is the most complex mainstream programming language. Even a careful subset
implementation risks encountering unexpected feature interactions in the LLVM headers. Specific
risks:

- LLVM headers use `#ifdef __cplusplus` extensively — the C++ preprocessor must handle this correctly
- LLVM headers use complex template patterns (`CRTP`, `mixin-base`) that stress template instantiation
- Name mangling must be bit-for-bit correct for the shim objects to link against LLVM/LLD static libs
- `std::string` SSO is a common source of bugs; getting the size/capacity accounting right requires
  careful implementation

**Mitigation**: Implement incrementally — compile one header at a time, starting with the simplest
(`<cstddef>`) and working up to the most complex (`<llvm/IR/IRBuilder.h>`). Each successful header
compilation is a checkpoint.

## Success Criteria

1. `tml c++ compiler/src/backend/llvm_backend.cpp` compiles successfully
2. `tml c++ compiler/src/backend/lld_linker.cpp` compiles successfully
3. The compiled objects link with LLVM/LLD static libraries via TML's own linker
4. The resulting `tml.exe` passes the full TML test suite
5. **Full Toolchain Independence**: TML built by TML, linked by TML, tested by TML

## Dependencies

- **Requires**: phase23b (C17 frontend — the C++ frontend extends all C parsing infrastructure);
  phase23a (C preprocessor — C++ adds `__cplusplus` and `__cpp_*` macros as a small delta)
- **Blocks**: Nothing — this is ERA 4's final task; full C++20 (modules, coroutines, concepts,
  ranges, `std::format`) is future/community work beyond TML's roadmap

## Estimated Size

~35,000 LOC TML — the longest and hardest task in the entire TML plan:
- `compiler-tml/src/cxx/parser.tml` (~4,000 LOC — namespace, class, template declaration parsing)
- `compiler-tml/src/cxx/templates.tml` (~8,000 LOC — instantiation, deduction, specialization, SFINAE)
- `compiler-tml/src/cxx/overload.tml` (~3,000 LOC — overload resolution, ADL, operator overloading)
- `compiler-tml/src/cxx/lower.tml` (~4,000 LOC — RAII insertion, vtable layout, name mangling)
- `compiler-tml/src/cxx/stdlib/string.tml` (~2,000 LOC — std::string with SSO, ostringstream)
- `compiler-tml/src/cxx/stdlib/vector.tml` (~1,500 LOC — std::vector, std::unique_ptr)
- `compiler-tml/src/cxx/stdlib/compat.tml` (~1,500 LOC — C compatibility headers in namespace std)
- `compiler-tml/src/cxx/types.tml` (~3,000 LOC — C++ type system: references, rvalue refs, cv-quals)
- `compiler-tml/src/cxx/mangle.tml` (~2,500 LOC — Itanium ABI + MSVC name mangling)
- `compiler-tml/src/cxx/ast.tml` (~5,500 LOC — C++ AST nodes extending C AST)
