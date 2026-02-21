//! # Codegen Error Explanations
//!
//! Error codes C001-C014 for code generation errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_codegen_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"C001", R"EX(
Codegen error [C001]

A general error occurred during code generation. This usually indicates
that a language feature used in the source code is not yet supported by
the code generator.

This is often an internal compiler issue. If you encounter this error
with valid TML code, please report it as a bug.
)EX"},

        {"C002", R"EX(
Unsupported feature in codegen [C002]

The code generator does not yet support a particular language feature.
This can happen with newer or experimental features.

If you encounter this error, try simplifying the code or using an
alternative approach. If the feature should be supported, please report
it as a bug.
)EX"},

        {"C003", R"EX(
Type error in codegen [C003]

A type-related error occurred during code generation. This happens when
the code generator encounters a type mismatch, invalid type conversion,
or unsupported type operation that was not caught by the type checker.

Common causes:

1. Invalid cast or conversion between incompatible types
2. Closure or function type requirement not met
3. Invalid operation on a type (e.g., arithmetic on non-numeric types)

Example of erroneous code:

    // A function expecting a specific closure signature
    func apply(f: do(I32) -> I32, x: I32) -> I32 {
        return f(x)
    }

    // Passing a closure with wrong return type
    let result = apply(do(x) { "hello" }, 42)

If this error appears on code that should be valid, it may indicate
a compiler bug — please report it.
)EX"},

        {"C004", R"EX(
Function not found in codegen [C004]

The code generator could not find a function or variable that was
referenced in the source code. This typically happens when a function
was declared but not defined, or when name resolution fails during
code generation.

Common causes:

1. Calling a function that was forward-declared but never implemented
2. Referencing a variable that was optimized away or not emitted
3. Missing library function linkage

If this error appears on valid code, it may indicate a compiler bug.
Try building with `--verbose` for more details.
)EX"},

        {"C005", R"EX(
Struct or class not found in codegen [C005]

The code generator could not find the definition of a struct or class
that is referenced in the code. This can happen when:

1. A struct is used in a virtual method table but its type info is missing
2. A class inheritance chain references an undefined parent class
3. A type was expected to be generated but was not emitted

Common scenarios:

    // Virtual dispatch requires class metadata
    class Animal { virtual func speak(this) -> Str }
    class Dog extends Animal {
        func speak(this) -> Str { return "woof" }
    }

If the base class type info is not found, this error is emitted.

This is usually an internal compiler issue. Please report it with
the source file and `--verbose` output.
)EX"},

        {"C006", R"EX(
Method not found in codegen [C006]

The code generator could not find a method or field on a type during
code generation. This happens when method resolution succeeds in the
type checker but the corresponding implementation is not available
during codegen.

Common causes:

1. A method defined in an `impl` block was not properly registered
2. A field access on a type that has no corresponding LLVM struct member
3. A behavior method implementation is missing

This is usually an internal compiler issue. Try building with
`--verbose` to see which method could not be found.
)EX"},

        {"C007", R"EX(
Invalid generic instantiation [C007]

A generic type or function was instantiated with type parameters that
are invalid or unsupported during code generation.

Example of erroneous code:

    func identity[T](x: T) -> T { return x }

    // If T is instantiated with a type that codegen can't handle
    let result = identity[SomeUnsupportedType](value)

How to fix:

Ensure the type arguments are concrete, supported types. Generic
instantiation requires all type parameters to be resolved to known types.
)EX"},

        {"C008", R"EX(
Missing implementation in codegen [C008]

The code generator expected an implementation (method body, required
argument, or runtime support) that was not provided. This is the most
common codegen error.

Common causes:

1. A method call is missing required arguments:

    let s: Str = "hello"
    s.split()              // split() requires a delimiter argument

2. A behavior method was not implemented for a type
3. A runtime function that codegen depends on is not linked

How to fix:

Check the method signature and provide all required arguments.
Use `tml doc <type>` to see method signatures and requirements.
)EX"},

        {"C009", R"EX(
LLVM backend error [C009]

The LLVM backend encountered an internal error while compiling the
generated IR. This is typically an internal compiler issue.

If you encounter this error, try:

1. Building with `--verbose` to see the full LLVM error
2. Using `--emit-ir` to inspect the generated IR
3. Reporting the issue with the IR output

    tml build file.tml --emit-ir --verbose
)EX"},

        {"C010", R"EX(
Linker error [C010]

An error occurred during the linking phase. The compiled object files
could not be linked into the final executable or library.

Common causes:

1. Missing library dependencies (e.g., runtime library not found)
2. Undefined symbols (functions declared but not defined)
3. Duplicate symbol definitions
4. Incompatible object file formats

How to fix:

1. Ensure all required libraries are available
2. Check that all `@extern` functions have corresponding libraries linked
3. Build with `--verbose` to see the full linker output
)EX"},

        {"C011", R"EX(
ABI compatibility error [C011]

An ABI (Application Binary Interface) compatibility error occurred.
This happens when the calling convention or data layout does not match
between the TML code and external code.

Common causes:

1. Mismatched `@extern` calling convention (e.g., "c" vs "stdcall")
2. Struct layout differences between TML and C code
3. Function pointer ABI mismatch

How to fix:

Ensure `@extern` declarations match the actual C/C++ signatures exactly,
including calling convention and parameter types.
)EX"},

        {"C012", R"EX(
Runtime error in codegen [C012]

A runtime-related error was detected during code generation. This
happens when the code generator detects a situation that would cause
a runtime failure.

Common causes:

1. Stack overflow detected during recursive type expansion
2. Infinite type instantiation loop
3. Runtime support function not available

This is usually an internal compiler issue. Please report it.
)EX"},

        {"C013", R"EX(
FFI error in codegen [C013]

An error occurred while generating code for FFI (Foreign Function Interface)
bindings. This happens when `@extern` function declarations have
issues during code generation.

Common causes:

1. Unsupported parameter type in extern function
2. Variadic extern function with invalid arguments
3. Incompatible struct layout for FFI interop

Example:

    @extern("c")
    func printf(fmt: Ptr[U8], ...) -> I32    // variadic FFI

Ensure parameter types are FFI-compatible: integer types, float types,
`Ptr[T]`, `Str` (as `Ptr[U8]`).
)EX"},

        {"C014", R"EX(
Intrinsic error in codegen [C014]

An error occurred while generating code for a compiler intrinsic or
built-in operation.

Common causes:

1. Invalid arguments to a memory intrinsic (ptr_read, ptr_write, etc.)
2. Unsupported intrinsic for the target platform
3. Type mismatch in intrinsic arguments

How to fix:

Check the intrinsic documentation for the correct argument types.
Memory intrinsics require proper pointer types and sizes.

    // Correct usage:
    let value = ptr_read[I32](ptr)           // read I32 from pointer
    ptr_write[I32](ptr, 42)                   // write I32 to pointer
    copy_nonoverlapping[I32](src, dst, count)  // copy N elements
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
