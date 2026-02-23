TML_MODULE("compiler")

//! # Lexer Error Explanations
//!
//! Error codes L001-L020 for tokenization errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_lexer_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"L001", R"EX(
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
)EX"},

        {"L002", R"EX(
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
)EX"},

        {"L003", R"EX(
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
)EX"},

        {"L004", R"EX(
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
)EX"},

        {"L005", R"EX(
Unterminated character literal [L005]

A character literal was opened with a single quote `'` but never closed.
Character literals must contain exactly one character (or escape sequence)
and end with a closing single quote.

Example of erroneous code:

    let c = 'a             // missing closing quote

How to fix:

    let c = 'a'            // add closing quote
)EX"},

        {"L006", R"EX(
Empty character literal [L006]

A character literal contains no characters. Character literals must
contain exactly one character or escape sequence.

Example of erroneous code:

    let c = ''              // empty character literal

How to fix:

    let c = ' '             // space character
    let c = '\0'            // null character
)EX"},

        {"L008", R"EX(
Invalid hexadecimal literal [L008]

A hexadecimal literal (prefixed with `0x`) contains invalid digits or
has no digits after the prefix. Hex digits are 0-9, a-f, A-F.

Example of erroneous code:

    let x = 0xGH           // 'G' and 'H' are not valid hex digits
    let y = 0x              // no digits after prefix

How to fix:

    let x = 0xFF           // valid hexadecimal
    let y = 0x1A           // valid hexadecimal
)EX"},

        {"L009", R"EX(
Invalid binary literal [L009]

A binary literal (prefixed with `0b`) contains digits other than 0 or 1,
or has no digits after the prefix.

Example of erroneous code:

    let x = 0b123          // '2' and '3' are not binary digits
    let y = 0b              // no digits after prefix

How to fix:

    let x = 0b101          // valid binary
    let y = 0b1100_1010    // underscores allowed for readability
)EX"},

        {"L010", R"EX(
Invalid octal literal [L010]

An octal literal (prefixed with `0o`) contains digits 8 or 9, which
are not valid in octal, or has no digits after the prefix.

Example of erroneous code:

    let x = 0o89           // '8' and '9' are not valid octal digits
    let y = 0o              // no digits after prefix

How to fix:

    let x = 0o77           // valid octal (decimal 63)
    let y = 0o755          // valid octal (decimal 493)
)EX"},

        {"L012", R"EX(
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
)EX"},

        {"L013", R"EX(
Unterminated raw string literal [L013]

A raw string literal was opened but never closed. Raw strings use the
syntax `r#"..."#` where the number of `#` symbols must match.

Example of erroneous code:

    let s = r#"hello
        world

How to fix:

    let s = r#"hello
        world"#
)EX"},

        {"L015", R"EX(
Unterminated template literal [L015]

A template (interpolated) string literal was opened but never closed.
Template literals use backtick syntax and support `${expr}` interpolation.

Example of erroneous code:

    let s = `hello ${name}

How to fix:

    let s = `hello ${name}`
)EX"},

        {"L016", R"EX(
Invalid integer type suffix [L016]

A numeric literal has an integer type suffix that is not recognized.

Example of erroneous code:

    let x = 42i99              // 'i99' is not a valid suffix
    let y = 0xFF_u3            // 'u3' is not a valid suffix
    let z = 0b1010i7           // 'i7' is not a valid suffix

Valid integer suffixes:
    i8, i16, i32, i64, i128    — signed integer types
    u8, u16, u32, u64, u128    — unsigned integer types

How to fix:

    let x = 42i32              // correct
    let y = 0xFF_u8            // correct
    let z = 0b1010i32          // correct

Related: L003 (general invalid number)
)EX"},

        {"L017", R"EX(
Invalid float type suffix [L017]

A numeric literal has a float type suffix that is not recognized.

Example of erroneous code:

    let x = 3.14f16            // 'f16' is not a valid suffix
    let y = 42f128             // 'f128' is not a valid suffix

Valid float suffixes:
    f32    — 32-bit floating point
    f64    — 64-bit floating point

How to fix:

    let x = 3.14f32            // correct
    let y = 42f64              // correct

Related: L003 (general invalid number)
)EX"},

        {"L018", R"EX(
Missing exponent digits [L018]

A floating-point literal has an exponent indicator (e or E) but no
digits follow it.

Example of erroneous code:

    let x = 1.0e               // missing exponent digits
    let y = 2.5E+              // sign present but no digits
    let z = 3e-                // sign present but no digits

How to fix:

    let x = 1.0e10             // correct
    let y = 2.5E+3             // correct
    let z = 3e-2               // correct

Related: L003 (general invalid number)
)EX"},

        {"L019", R"EX(
Invalid floating-point number [L019]

A floating-point literal could not be parsed as a valid number.
This can happen when the number is out of range for 64-bit floats
or has an otherwise malformed format.

Example of erroneous code:

    let x = 1.0e99999          // overflow: out of F64 range

How to fix: use a value within the valid floating-point range:

    let x = 1.0e308            // near max F64

Related: L003 (general invalid number), L011 (number overflow)
)EX"},

        {"L020", R"EX(
Invalid integer number [L020]

A decimal integer literal could not be parsed as a valid number.
This typically happens when the number exceeds the maximum value
for a 64-bit unsigned integer.

Example of erroneous code:

    let x = 99999999999999999999  // exceeds U64 max

How to fix: use a value within the valid integer range:

    let x = 9_223_372_036_854_775_807   // I64 max

Related: L003 (general invalid number), L011 (number overflow)
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
