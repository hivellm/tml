#include "cmd_explain.hpp"

#include "cli/diagnostic.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::cli {

// ============================================================================
// Error explanation database
// ============================================================================

static const std::unordered_map<std::string, std::string>& get_explanations() {
    static const std::unordered_map<std::string, std::string> explanations = {

        // ====================================================================
        // Lexer errors (L)
        // ====================================================================

        {"L001", R"(
Invalid character [L001]

The lexer encountered a character that is not valid in TML source code.
TML source files must contain only valid UTF-8 text, and certain characters
like `@` (outside of decorators), `$`, or control characters are not
permitted in identifiers or expressions.

Example of erroneous code:

    let x@ = 42     // '@' is not valid here

How to fix:

    let x = 42      // remove the invalid character

If you intended to use a decorator, it must appear before a declaration:

    @inline
    func compute() -> I32 { return 42 }
)"},

        {"L002", R"(
Unterminated string literal [L002]

A string literal was opened with a double quote `"` but never closed.
Every opening quote must have a matching closing quote on the same line
(or use a multi-line raw string).

Example of erroneous code:

    let s = "hello

How to fix:

    let s = "hello"          // add closing quote

For multi-line strings, use raw string syntax:

    let s = r#"
        This spans
        multiple lines
    "#
)"},

        {"L003", R"(
Invalid number literal [L003]

A numeric literal has an invalid format. This can happen with invalid
type suffixes, malformed exponents, or digits outside the valid range
for the number base.

Example of erroneous code:

    let x = 42i99          // 'i99' is not a valid integer suffix
    let y = 3.14f16        // 'f16' is not a valid float suffix
    let z = 1.0e            // missing exponent digits

Valid integer suffixes: i8, i16, i32, i64, u8, u16, u32, u64
Valid float suffixes: f32, f64

How to fix:

    let x = 42i32          // valid integer suffix
    let y = 3.14f64        // valid float suffix
    let z = 1.0e10         // valid exponent
)"},

        {"L004", R"(
Invalid escape sequence [L004]

A string or template literal contains a backslash followed by a character
that is not a recognized escape sequence. TML supports these escapes:

    \\    backslash
    \"    double quote
    \n    newline
    \r    carriage return
    \t    tab
    \0    null
    \xNN  hex byte (e.g., \x41 = 'A')
    \u{NNNN}  Unicode code point (e.g., \u{03B1} = greek alpha)

Example of erroneous code:

    let s = "hello\q world"     // \q is not a valid escape

How to fix:

    let s = "hello\\q world"    // use \\ for literal backslash
    let s = "helloq world"      // or remove the backslash
)"},

        {"L005", R"(
Unterminated character literal [L005]

A character literal was opened with a single quote `'` but never closed.
Character literals must contain exactly one character (or escape sequence)
and end with a closing single quote.

Example of erroneous code:

    let c = 'a             // missing closing quote

How to fix:

    let c = 'a'            // add closing quote
)"},

        {"L006", R"(
Empty character literal [L006]

A character literal contains no characters. Character literals must
contain exactly one character or escape sequence.

Example of erroneous code:

    let c = ''              // empty character literal

How to fix:

    let c = ' '             // space character
    let c = '\0'            // null character
)"},

        {"L008", R"(
Invalid hexadecimal literal [L008]

A hexadecimal literal (prefixed with `0x`) contains invalid digits or
has no digits after the prefix. Hex digits are 0-9, a-f, A-F.

Example of erroneous code:

    let x = 0xGH           // 'G' and 'H' are not valid hex digits
    let y = 0x              // no digits after prefix

How to fix:

    let x = 0xFF           // valid hexadecimal
    let y = 0x1A           // valid hexadecimal
)"},

        {"L009", R"(
Invalid binary literal [L009]

A binary literal (prefixed with `0b`) contains digits other than 0 or 1,
or has no digits after the prefix.

Example of erroneous code:

    let x = 0b123          // '2' and '3' are not binary digits
    let y = 0b              // no digits after prefix

How to fix:

    let x = 0b101          // valid binary
    let y = 0b1100_1010    // underscores allowed for readability
)"},

        {"L010", R"(
Invalid octal literal [L010]

An octal literal (prefixed with `0o`) contains digits 8 or 9, which
are not valid in octal, or has no digits after the prefix.

Example of erroneous code:

    let x = 0o89           // '8' and '9' are not valid octal digits
    let y = 0o              // no digits after prefix

How to fix:

    let x = 0o77           // valid octal (decimal 63)
    let y = 0o755          // valid octal (decimal 493)
)"},

        {"L012", R"(
Unterminated block comment [L012]

A block comment was opened with `/*` but the closing `*/` was not found.
Block comments can be nested in TML.

Example of erroneous code:

    /* This comment
       never ends...

How to fix:

    /* This comment
       is properly closed */

For nested comments, ensure all levels are closed:

    /* outer /* inner */ still outer */
)"},

        {"L013", R"(
Unterminated raw string literal [L013]

A raw string literal was opened but never closed. Raw strings use the
syntax `r#"..."#` where the number of `#` symbols must match.

Example of erroneous code:

    let s = r#"hello
        world

How to fix:

    let s = r#"hello
        world"#
)"},

        {"L015", R"(
Unterminated template literal [L015]

A template (interpolated) string literal was opened but never closed.
Template literals use backtick syntax and support `${expr}` interpolation.

Example of erroneous code:

    let s = `hello ${name}

How to fix:

    let s = `hello ${name}`
)"},

        // ====================================================================
        // Parser errors (P)
        // ====================================================================

        {"P001", R"(
Unexpected token [P001]

The parser encountered a token that does not fit the expected syntax at
that position. This usually indicates a missing comma, parenthesis,
brace, or other punctuation.

Example of erroneous code:

    func add(a: I32 b: I32) -> I32 {
        return a + b
    }

The parser expects a comma between parameters.

How to fix:

    func add(a: I32, b: I32) -> I32 {
        return a + b
    }
)"},

        {"P002", R"(
Missing semicolon [P002]

TML does not use semicolons as statement terminators. However, this error
can appear when the parser cannot determine where one statement ends and
the next begins, usually due to a missing operator or misplaced keyword.

Example of erroneous code:

    let x = 1 let y = 2    // two statements on one line

How to fix:

    let x = 1
    let y = 2               // put each statement on its own line
)"},

        {"P003", R"(
Missing brace [P003]

A block was opened with `{` but the matching closing `}` was not found,
or a `}` appeared without a matching `{`.

Example of erroneous code:

    func compute() -> I32 {
        if x > 0 {
            return x
        // missing closing brace for if

How to fix:

    func compute() -> I32 {
        if x > 0 {
            return x
        }
    }
)"},

        {"P004", R"(
Invalid expression [P004]

The parser expected an expression but found something that cannot be
parsed as one. Expressions include literals, identifiers, function calls,
binary operations, and more.

Example of erroneous code:

    let x =            // expression expected after `=`
    let y = + 3        // `+` is binary, needs left operand

How to fix:

    let x = 0
    let y = 3          // or: let y = -3 for negation
)"},

        {"P005", R"(
Expected type [P005]

A type annotation is required but was not provided. This occurs in
function parameters, return types, and explicit type annotations.

Example of erroneous code:

    func add(a, b) -> {        // parameters need types
        return a + b
    }

How to fix:

    func add(a: I32, b: I32) -> I32 {
        return a + b
    }
)"},

        // ====================================================================
        // Type errors (T)
        // ====================================================================

        {"T001", R"(
Type mismatch [T001]

The compiler found a value of one type where a different type was expected.
The two types are incompatible and no implicit conversion exists.

Example of erroneous code:

    let x: Str = 42          // expected Str, found I32

The variable `x` is annotated as `Str`, but `42` is an integer literal.

How to fix:

1. Change the annotation to match the value:
       let x: I32 = 42

2. Convert the value:
       let x: Str = 42.to_string()

3. Remove the annotation and let inference decide:
       let x = 42

Related: T016 (return type mismatch), T015 (branch type mismatch)
)"},

        {"T002", R"(
Unknown type [T002]

A type name was used that the compiler cannot find in the current scope.
This usually means the type was misspelled or not imported.

Example of erroneous code:

    let x: Strng = "hello"      // 'Strng' is not defined

How to fix:

    let x: Str = "hello"        // correct the spelling

If the type is in another module:

    use std::collections::HashMap
    let m: HashMap[Str, I32] = HashMap.new()
)"},

        {"T003", R"(
Unknown function [T003]

A function was called that does not exist in the current scope. The
function may be misspelled, not imported, or defined in a different module.

Example of erroneous code:

    let result = computeValue(42)   // function not found

How to fix:

    let result = compute_value(42)  // correct the name (TML uses snake_case)

If the function is in another module:

    use math::compute_value
    let result = compute_value(42)

Related: T006 (unknown method)
)"},

        {"T004", R"(
Argument count mismatch [T004]

A function was called with the wrong number of arguments. The number of
arguments passed must match the number of parameters declared.

Example of erroneous code:

    func add(a: I32, b: I32) -> I32 { return a + b }

    let result = add(1, 2, 3)   // expected 2 args, got 3

How to fix:

    let result = add(1, 2)      // pass the correct number of arguments
)"},

        {"T005", R"(
Unknown field [T005]

An attempt was made to access a field that does not exist on the given
struct or type.

Example of erroneous code:

    type Point { x: F64, y: F64 }
    let p = Point { x: 1.0, y: 2.0 }
    let z = p.z                  // Point has no field 'z'

How to fix:

    let x = p.x                  // use an existing field

If you need a `z` field, add it to the struct:

    type Point3D { x: F64, y: F64, z: F64 }
)"},

        {"T006", R"(
Unknown method [T006]

A method was called on a type that does not have that method. The method
may be misspelled, not implemented for this type, or requires a behavior
implementation.

Example of erroneous code:

    let s: Str = "hello"
    let n = s.to_integer()      // Str has no method 'to_integer'

How to fix:

    let n = s.parse_i32()       // use the correct method name

If the method should come from a behavior:

    impl Display for MyType {
        func to_string(self) -> Str { ... }
    }

Related: T003 (unknown function), T025 (unknown behavior)
)"},

        {"T007", R"(
Cannot infer type [T007]

The compiler cannot determine the type of an expression. This happens
when there is not enough context to infer the type, such as with empty
collections or generic function calls.

Example of erroneous code:

    let items = []               // what type of list?

How to fix:

    let items: List[I32] = []    // provide a type annotation
    let items = [1, 2, 3]       // or provide initial values
)"},

        {"T008", R"(
Duplicate definition [T008]

A name was defined more than once in the same scope. Each name (variable,
function, type, etc.) must be unique within its scope.

Example of erroneous code:

    func compute() -> I32 { return 1 }
    func compute() -> I32 { return 2 }   // duplicate

How to fix:

    func compute_a() -> I32 { return 1 }
    func compute_b() -> I32 { return 2 }
)"},

        {"T009", R"(
Undeclared variable [T009]

A variable was used that has not been declared in the current scope or
any enclosing scope.

Example of erroneous code:

    func main() {
        print(x)                // 'x' is not declared
    }

How to fix:

    func main() {
        let x = 42
        print(x)                // now 'x' is in scope
    }

Note: Variables must be declared before use. TML does not support
forward references to variables.
)"},

        {"T010", R"(
Not callable [T010]

An attempt was made to call something that is not a function or closure.
Only functions, closures, and types with a call operator can be invoked.

Example of erroneous code:

    let x: I32 = 42
    let result = x(1, 2)       // I32 is not callable

How to fix:

    func compute(a: I32, b: I32) -> I32 { return a + b }
    let result = compute(1, 2)
)"},

        {"T013", R"(
Immutable assignment [T013]

An attempt was made to assign a new value to an immutable variable.
In TML, variables declared with `let` are immutable by default.

Example of erroneous code:

    let x = 1
    x = 2                       // cannot assign to immutable 'x'

How to fix:

    var x = 1                   // use 'var' for mutable variables
    x = 2                       // now assignment works

Related: B003 (assign not mutable)
)"},

        {"T014", R"(
Condition not Bool [T014]

An `if` or `loop while` condition must evaluate to `Bool`. A value of
a different type was used as a condition.

Example of erroneous code:

    let x = 42
    if x {                      // I32 is not Bool
        print("yes")
    }

How to fix:

    if x > 0 {                  // use a comparison expression
        print("yes")
    }

    if x != 0 {                 // or explicit comparison
        print("yes")
    }
)"},

        {"T016", R"(
Return type mismatch [T016]

The value returned from a function does not match the declared return type.

Example of erroneous code:

    func get_name() -> Str {
        return 42               // expected Str, found I32
    }

How to fix:

    func get_name() -> Str {
        return "Alice"          // return the correct type
    }

    // Or change the return type:
    func get_value() -> I32 {
        return 42
    }

Related: T001 (type mismatch), T015 (branch type mismatch)
)"},

        // ====================================================================
        // Borrow errors (B)
        // ====================================================================

        {"B001", R"(
Use after move [B001]

A value was used after it was moved to another binding. In TML, when a
non-copyable value is assigned to a new variable or passed to a function,
ownership is transferred (moved). The original variable becomes invalid.

Example of erroneous code:

    let s = "hello"
    let t = s                   // 's' is moved to 't'
    print(s)                    // error: 's' was already moved

How to fix:

1. Use the new owner instead:
       print(t)

2. Duplicate the value if you need both:
       let t = s.duplicate()
       print(s)                 // 's' is still valid

3. Use a reference instead of moving:
       let t = ref s
       print(s)

Related: B005 (borrow after move), B011 (partial move)
)"},

        {"B002", R"(
Move while borrowed [B002]

A value was moved while there is still an active borrow (reference) to it.
Moving a value would invalidate any existing references, which could lead
to dangling pointers.

Example of erroneous code:

    let data = [1, 2, 3]
    let r = ref data            // immutable borrow
    let moved = data            // error: cannot move while borrowed
    print(r)                    // borrow still in use here

How to fix:

1. Drop the borrow before moving:
       let data = [1, 2, 3]
       {
           let r = ref data
           print(r)
       }                        // borrow ends here
       let moved = data         // now safe to move

2. Duplicate instead of moving:
       let data = [1, 2, 3]
       let r = ref data
       let copy = data.duplicate()
)"},

        {"B003", R"(
Assign to non-mutable variable [B003]

An attempt was made to modify a variable through a reference that is not
declared as mutable, or to assign to a `let` binding.

Example of erroneous code:

    let x = 42
    x = 10                      // 'x' is not mutable

How to fix:

    var x = 42                  // use 'var' for mutable binding
    x = 10                      // now assignment works

For references:

    var data = [1, 2, 3]
    let r = mut ref data        // mutable reference
    r.push(4)                   // mutation through mutable ref

Related: T013 (immutable assignment)
)"},

        {"B004", R"(
Assign while borrowed [B004]

A variable was assigned to while there is still an active borrow to it.
Assigning would change the value that the borrow points to, potentially
invalidating the reference.

Example of erroneous code:

    var x = 42
    let r = ref x               // borrow 'x'
    x = 10                      // error: 'x' is borrowed
    print(r)                    // borrow used here

How to fix:

1. Use the borrow before reassigning:
       var x = 42
       let r = ref x
       print(r)                 // use borrow here
       x = 10                   // now safe to reassign

2. Limit the borrow scope:
       var x = 42
       { let r = ref x; print(r) }
       x = 10
)"},

        {"B005", R"(
Borrow after move [B005]

An attempt was made to borrow a value that has already been moved. Once
a value is moved, the original binding is invalid and cannot be borrowed.

Example of erroneous code:

    let data = [1, 2, 3]
    let moved = data            // value moved here
    let r = ref data            // error: 'data' was moved

How to fix:

    let data = [1, 2, 3]
    let r = ref data            // borrow before moving
    // ... use r ...
    let moved = data            // move after borrow is done

Related: B001 (use after move)
)"},

        {"B006", R"(
Mutable borrow of non-mutable variable [B006]

A mutable reference (`mut ref`) was taken to a variable that was not
declared as mutable (`var`). Only `var` bindings can be mutably borrowed.

Example of erroneous code:

    let data = [1, 2, 3]
    let r = mut ref data        // error: 'data' is not mutable

How to fix:

    var data = [1, 2, 3]        // use 'var' to allow mutation
    let r = mut ref data        // now mutable borrow works
    r.push(4)
)"},

        {"B007", R"(
Mutable borrow while immutably borrowed [B007]

A mutable borrow was taken while an immutable borrow is still active.
TML enforces that you cannot have a mutable reference while any other
reference (mutable or immutable) exists to the same value.

Example of erroneous code:

    var data = [1, 2, 3]
    let r1 = ref data           // immutable borrow
    let r2 = mut ref data       // error: already borrowed immutably
    print(r1)                   // immutable borrow used here

How to fix:

1. Use the immutable borrow first:
       var data = [1, 2, 3]
       let r1 = ref data
       print(r1)                // done with immutable borrow
       let r2 = mut ref data    // now safe to borrow mutably

2. Use only one kind of borrow:
       var data = [1, 2, 3]
       let r = mut ref data
       // use r for both reading and writing

Related: B008 (double mutable borrow), B009 (immutable while mutable)
)"},

        {"B008", R"(
Double mutable borrow [B008]

Two mutable borrows were taken from the same value at the same time.
TML allows at most ONE mutable reference to a value at any given time.
This prevents data races and aliasing bugs.

Example of erroneous code:

    var data = [1, 2, 3]
    let r1 = mut ref data       // first mutable borrow
    let r2 = mut ref data       // error: already mutably borrowed
    r1.push(4)

How to fix:

1. Use one borrow at a time:
       var data = [1, 2, 3]
       {
           let r1 = mut ref data
           r1.push(4)
       }                        // first borrow ends
       let r2 = mut ref data    // now safe

2. Use a single reference for all mutations:
       var data = [1, 2, 3]
       let r = mut ref data
       r.push(4)
       r.push(5)

Related: B007 (mutable + immutable borrow conflict)
)"},

        {"B010", R"(
Return local reference [B010]

A function returns a reference to a local variable. When the function
returns, the local variable is dropped, making the reference dangling.

Example of erroneous code:

    func bad() -> ref Str {
        let s = "hello"
        return ref s            // error: 's' will be dropped
    }

How to fix:

    func good() -> Str {
        let s = "hello"
        return s                // return owned value instead
    }

If you need to return a reference, it must refer to data that outlives
the function (e.g., a parameter or static data):

    func first(items: ref List[I32]) -> ref I32 {
        return ref items[0]     // borrows from the input
    }
)"},

        // ====================================================================
        // Codegen errors (C)
        // ====================================================================

        {"C001", R"(
Codegen error [C001]

A general error occurred during code generation. This usually indicates
that a language feature used in the source code is not yet supported by
the code generator.

This is often an internal compiler issue. If you encounter this error
with valid TML code, please report it as a bug.
)"},

        {"C002", R"(
Unsupported feature in codegen [C002]

The code generator does not yet support a particular language feature.
This can happen with newer or experimental features.

If you encounter this error, try simplifying the code or using an
alternative approach. If the feature should be supported, please report
it as a bug.
)"},

        {"C009", R"(
LLVM backend error [C009]

The LLVM backend encountered an internal error while compiling the
generated IR. This is typically an internal compiler issue.

If you encounter this error, try:

1. Building with `--verbose` to see the full LLVM error
2. Using `--emit-ir` to inspect the generated IR
3. Reporting the issue with the IR output

    tml build file.tml --emit-ir --verbose
)"},

        // ====================================================================
        // Preprocessor errors (PP)
        // ====================================================================

        {"PP001", R"(
Preprocessor error [PP001]

An error occurred during preprocessing of conditional compilation directives
(`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#define`).

This usually means a directive is malformed or has a syntax error.

Example of erroneous code:

    #if                          // missing condition
    func platform_code() { }
    #endif

    #ifdef                       // missing symbol name
    func debug_code() { }
    #endif

How to fix:

    #if WINDOWS
    func platform_code() { }
    #endif

    #ifdef DEBUG
    func debug_code() { }
    #endif

Common causes:
- Missing condition after `#if`
- Missing symbol name after `#ifdef` or `#ifndef`
- Mismatched `#if`/`#endif` pairs
- Unknown preprocessor directive
)"},

        {"PP002", R"(
Preprocessor warning [PP002]

A non-fatal issue was detected during preprocessing. The code will still
compile, but the warning indicates a potential problem.

Common causes:
- Redefining an already-defined symbol with `#define`
- Using `#elif` or `#else` after an `#else` block
- Empty `#if` blocks that could be simplified

These warnings can usually be resolved by cleaning up the conditional
compilation structure.
)"},

        // ====================================================================
        // General errors (E)
        // ====================================================================

        {"E001", R"(
File not found [E001]

The specified source file does not exist or cannot be read.

Example:

    tml build nonexistent.tml   // file does not exist

How to fix:

1. Check the file path for typos
2. Verify the file exists: the path is relative to the current directory
3. Check file permissions
)"},

        {"E002", R"(
I/O error [E002]

An error occurred while reading or writing a file. This can happen due
to permission issues, disk full, or corrupted files.

How to fix:

1. Check file permissions
2. Verify disk space is available
3. Check that the output directory exists
)"},

        {"E003", R"(
Internal compiler error [E003]

An unexpected internal error occurred in the compiler. This is a bug
in the TML compiler.

Please report this issue with:
1. The TML source file that triggered the error
2. The compiler version (`tml --version`)
3. The full error output
)"},

    };
    return explanations;
}

// ============================================================================
// run_explain implementation
// ============================================================================

int run_explain(const std::string& code, bool /*verbose*/) {
    // Normalize: uppercase, strip whitespace
    std::string normalized;
    normalized.reserve(code.size());
    for (char c : code) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            normalized += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }

    if (normalized.empty()) {
        std::cerr << "Usage: tml explain <error-code>\n";
        std::cerr << "Example: tml explain T001\n";
        return 1;
    }

    const auto& explanations = get_explanations();
    auto it = explanations.find(normalized);

    if (it != explanations.end()) {
        // Print with colored header
        bool colors = terminal_supports_colors();
        if (colors) {
            std::cout << Colors::Bold << Colors::BrightCyan;
        }
        std::cout << "Explanation for " << normalized;
        if (colors) {
            std::cout << Colors::Reset;
        }
        std::cout << "\n";

        // Print the explanation text
        std::cout << it->second;

        // Ensure trailing newline
        if (!it->second.empty() && it->second.back() != '\n') {
            std::cout << "\n";
        }
        return 0;
    }

    // Not found — suggest similar codes
    std::cerr << "No explanation available for error code `" << normalized << "`.\n\n";

    // Collect all known codes for suggestion
    std::vector<std::string> known_codes;
    known_codes.reserve(explanations.size());
    for (const auto& [key, _] : explanations) {
        known_codes.push_back(key);
    }

    auto suggestions = find_similar_candidates(normalized, known_codes, 3, 2);
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:\n";
        for (const auto& suggestion : suggestions) {
            std::cerr << "  tml explain " << suggestion << "\n";
        }
        std::cerr << "\n";
    }

    // Show available categories
    std::cerr << "Available error code categories:\n";
    std::cerr << "  L001-L015   Lexer errors (tokenization)\n";
    std::cerr << "  P001-P065   Parser errors (syntax)\n";
    std::cerr << "  PP001-PP002 Preprocessor errors (conditional compilation)\n";
    std::cerr << "  T001-T054   Type errors (type checking)\n";
    std::cerr << "  B001-B017   Borrow errors (ownership/lifetimes)\n";
    std::cerr << "  C001-C014   Codegen errors (code generation)\n";
    std::cerr << "  E001-E006   General errors\n";

    return 1;
}

} // namespace tml::cli
