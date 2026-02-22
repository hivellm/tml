//! # Codegen Error Explanations
//!
//! Error codes C001-C035 for code generation errors.

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

        {"C015", R"EX(
Missing method argument [C015]

A method was called without a required argument. Most methods on primitive
types, collections, and wrapper types require at least one argument.

Example of erroneous code:

    let x: I32 = 42
    let y = x.add()              // error: add() requires an argument

    let arr = [1, 2, 3]
    let idx = arr.get()          // error: get() requires an index argument

How to fix:

    let y = x.add(10)            // provide the required argument
    let idx = arr.get(0)         // provide the index

This error applies to arithmetic methods (add, sub, mul, div, rem),
comparison methods (cmp, eq, ne), bitwise methods (bitand, bitor, bitxor),
shift methods (shift_left, shift_right), and collection accessors.
)EX"},

        {"C016", R"EX(
Missing closure argument [C016]

A method that requires a closure was called without one. Methods like `map`,
`filter`, `and_then`, `or_else`, and `unwrap_or_else` on Maybe and Array
types require a closure argument.

Example of erroneous code:

    let m: Maybe[I32] = Just(42)
    let result = m.map()         // error: map requires a closure argument

    let arr = [1, 2, 3]
    let doubled = arr.map()      // error: map requires a closure argument

How to fix:

    let result = m.map(do(x) { x * 2 })
    let doubled = arr.map(do(x) { x * 2 })

Methods that require closures:
- Maybe: map, filter, and_then, or_else, unwrap_or_else
- Array: map
)EX"},

        {"C017", R"EX(
Missing function or closure argument [C017]

A method that requires a function reference or closure was called without one.
Methods on Outcome types accept either a closure or a named function reference.

Example of erroneous code:

    let r: Outcome[I32, Str] = Ok(42)
    let mapped = r.map()         // error: map requires a function argument

How to fix:

    // Using a closure:
    let mapped = r.map(do(x) { x * 2 })

    // Using a function reference:
    func double(x: I32) -> I32 { return x * 2 }
    let mapped = r.map(double)

Methods that require function/closure arguments:
- Outcome: map, and_then, or_else, unwrap_or_else, is_ok_and, is_err_and
)EX"},

        {"C018", R"EX(
Missing multiple arguments [C018]

A method that requires two or more arguments was called with fewer arguments
than expected.

Example of erroneous code:

    let x: I32 = 42
    x.clamp(10)                  // error: clamp() requires two arguments

    let m: Maybe[I32] = Just(42)
    m.map_or(0)                  // error: map_or requires a default and closure

How to fix:

    x.clamp(10, 100)             // provide both min and max
    m.map_or(0, do(x) { x * 2 })  // provide default value AND closure

Methods that require multiple arguments:
- Numeric: clamp(min, max)
- Maybe/Outcome: map_or(default, closure)
)EX"},

        {"C019", R"EX(
Pointer method missing argument [C019]

A pointer method was called without its required argument. Pointer operations
like `write` and `offset` require explicit arguments for safety.

Example of erroneous code:

    let p: Ptr[I32] = mem_alloc[I32](1)
    p.write()                    // error: Ptr.write() requires a value argument
    p.offset()                   // error: Ptr.offset() requires an offset argument

How to fix:

    p.write(42)                  // write a value through the pointer
    let next = p.offset(1)      // offset by 1 element
)EX"},

        {"C020", R"EX(
Second argument must be a closure [C020]

A method's second argument must be a closure, but a non-closure value was
provided. This applies to methods like `map_or` where the first argument
is a default value and the second is a transformation closure.

Example of erroneous code:

    let m: Maybe[I32] = Just(42)
    m.map_or(0, 99)              // error: second argument must be a closure

How to fix:

    m.map_or(0, do(x) { x * 2 })  // second arg is a closure
)EX"},

        {"C021", R"EX(
Argument is not a closure or function reference [C021]

A method expected a closure or function reference as an argument, but received
a value of a different type. The argument was provided, but it has the wrong
kind — it must be a `do(...) { ... }` closure or a named function.

Example of erroneous code:

    let r: Outcome[I32, Str] = Ok(42)
    r.map(99)                    // error: 99 is not a closure or function ref
    r.is_ok_and("yes")           // error: "yes" is not a predicate

How to fix:

    r.map(do(x) { x * 2 })      // pass a closure
    r.is_ok_and(do(x) { x > 0 })  // pass a predicate closure

Difference from C017: C017 means no argument was provided at all.
C021 means an argument WAS provided but is not callable.
)EX"},

        {"C022", R"EX(
Tuple pattern requires initializer [C022]

A tuple destructuring pattern was used in a `let` or `var` binding without
an initializer expression. Tuple patterns must have a value to destructure.

Example of erroneous code:

    let (a, b): (I32, I32)       // error: no initializer to destructure

How to fix:

    let (a, b) = (1, 2)         // provide a tuple value to destructure
    let (x, y) = get_point()    // or call a function returning a tuple
)EX"},

        {"C023", R"EX(
Compound assignment requires variable [C023]

A compound assignment operator (`+=`, `-=`, `*=`, etc.) was used on an
expression that is not a variable. These operators modify a value in place,
so the left-hand side must be a mutable variable.

Example of erroneous code:

    42 += 1                      // error: cannot compound-assign to a literal
    get_value() += 1             // error: cannot compound-assign to function return

How to fix:

    var x = 42
    x += 1                       // compound-assign to a mutable variable
)EX"},

        {"C024", R"EX(
Cannot call non-function field [C024]

A field access expression was used with call syntax, but the field is not
a function or closure type. You can only call fields that hold callable values.

Example of erroneous code:

    type Config { port: I32 }
    let c = Config { port: 8080 }
    c.port()                     // error: 'port' is I32, not a function

How to fix:

    let p = c.port               // access the field value directly

    // Or if the field should be callable, declare it as a closure type:
    type Handler { on_event: do(Str) -> Bool }
    let h = Handler { on_event: do(s) { true } }
    h.on_event("click")         // this works — field is callable
)EX"},

        {"C025", R"EX(
Invalid type in 'is' expression [C025]

The type used in an `is` type-check expression could not be resolved.
The `is` operator checks whether a value is of a specific type at runtime.

Example of erroneous code:

    let x: Any = 42
    if x is NonexistentType {    // error: type cannot be resolved
        // ...
    }

How to fix:

    if x is I32 {                // use a valid, known type
        // ...
    }
)EX"},

        {"C026", R"EX(
Operation requires a variable [C026]

A reference (`ref`), increment (`++`), or decrement (`--`) operation was
applied to an expression that is not a variable. These operations need a
memory location to reference or modify.

Example of erroneous code:

    let r = ref 42               // error: can only take reference of variables
    42++                         // error: can only increment variables
    get_value()--                // error: can only decrement variables

How to fix:

    var x = 42
    let r = ref x                // take reference of a variable
    x++                          // increment a variable
    x--                          // decrement a variable
)EX"},

        {"C027", R"EX(
Field resolution error [C027]

The code generator could not resolve a field access on a struct or tuple,
either because the object type could not be determined, or because a tuple
index is out of bounds.

Example of erroneous code:

    let t = (1, 2, 3)
    let x = t.5                  // error: tuple index 5 out of bounds (3 elements)

How to fix:

    let x = t.0                  // valid tuple index (0-based)
    let y = t.2                  // last element of a 3-element tuple

If the error is "Cannot resolve field access object", it usually indicates
an internal compiler issue. Try simplifying the expression or report it.
)EX"},

        {"C028", R"EX(
Class not found for virtual dispatch [C028]

During code generation, a class referenced in a virtual method call
could not be found in the vtable layout map.

Example of erroneous code:

    class Animal {
        @virtual func speak() -> Str { return "..." }
    }
    // Internal error: class 'Animal' not registered for virtual dispatch

This typically indicates an internal compiler issue. The type checker
should have caught unknown classes earlier.

Related: C005 (struct/class not found), C006 (method not found)
)EX"},

        {"C029", R"EX(
Cannot determine class for base expression [C029]

During code generation for a `base.member` expression, the compiler
could not determine which class the current method belongs to.

This typically indicates an internal compiler issue where the `self`
parameter is not available or has an unexpected type.

Related: C030 (class has no base), C005 (struct/class not found)
)EX"},

        {"C030", R"EX(
Class has no base class in codegen [C030]

During code generation, a `base` expression was encountered in a class
that does not extend any base class.

Example of erroneous code:

    class Dog {
        func speak() -> Str {
            return base.speak()    // Dog has no base class
        }
    }

This should normally be caught by the type checker (T076). If you
see this error, it may indicate a compiler pipeline issue.

Related: T076 (class has no base class), C029 (no class context)
)EX"},

        {"C031", R"EX(
Base class not found in codegen [C031]

During code generation, the base class referenced by a child class
could not be found in the type environment.

This typically indicates an internal compiler issue. The type checker
should have verified the base class exists earlier.

Related: T046 (base class not found), C030 (class has no base)
)EX"},

        {"C032", R"EX(
Unknown class in new expression [C032]

During code generation for a `new ClassName()` expression, the class
type could not be found in the class type registry.

Example of erroneous code:

    let obj = new UnknownClass()   // class not registered in codegen

This should normally be caught by the type checker (T075). If you
see this error, it may indicate a compiler pipeline issue.

Related: T075 (class not found), C005 (struct/class not found)
)EX"},

        {"C033", R"EX(
Method not found in vtable [C033]

During code generation for a virtual method call, the method name
could not be found in the class's vtable layout.

Example of erroneous code:

    class Animal {
        @virtual func speak() -> Str { return "..." }
    }
    let a: Animal = ...
    a.fly()                        // not in Animal's vtable

This should normally be caught by the type checker. If you see this
error during codegen, it may indicate a vtable registration issue.

Related: C006 (method not found), C028 (class not in vtable)
)EX"},

        {"C034", R"EX(
Field not found in base class during codegen [C034]

During code generation for a `base.field` expression, the field
could not be found in the base class definition.

Example of erroneous code:

    class Animal {
        let name: Str
    }
    class Dog extends Animal {
        func show() -> Str {
            return base.color      // 'color' not in Animal
        }
    }

How to fix: access only fields defined in the base class:

    return base.name               // correct

Related: T067 (base field not found), C006 (method not found)
)EX"},

        {"C035", R"EX(
Unknown static method [C035]

During code generation, a static method call `Type::method()` could
not be resolved. The method does not exist on the type.

Example of erroneous code:

    let result = MyStruct::unknown_method()   // no such static method

How to fix: call static methods that are defined on the type:

    let result = MyStruct::new()              // correct

Related: C006 (method not found), T006 (unknown method)
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
