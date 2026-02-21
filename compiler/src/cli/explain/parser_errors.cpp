//! # Parser Error Explanations
//!
//! Error codes P001-P065 for syntax/parsing errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_parser_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"P001", R"EX(
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
)EX"},

        {"P002", R"EX(
Missing semicolon [P002]

TML does not use semicolons as statement terminators. However, this error
can appear when the parser cannot determine where one statement ends and
the next begins, usually due to a missing operator or misplaced keyword.

Example of erroneous code:

    let x = 1 let y = 2    // two statements on one line

How to fix:

    let x = 1
    let y = 2               // put each statement on its own line
)EX"},

        {"P003", R"EX(
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
)EX"},

        {"P004", R"EX(
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
)EX"},

        {"P005", R"EX(
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
)EX"},

        {"P007", R"EX(
Expected pattern [P007]

The parser expected a pattern (used in `when` arms, `let` bindings, and
destructuring) but found something else.

Example of erroneous code:

    when value {
        => println("no pattern")    // missing pattern before =>
    }

How to fix:

    when value {
        42 => println("forty-two"),
        x => println("other: {x}")
    }

Patterns include: literals, identifiers, enum variants, wildcards (_),
tuples, and struct patterns.
)EX"},

        {"P008", R"EX(
Expected colon [P008]

The parser expected a colon `:` but found a different token. This
commonly occurs in type annotations, struct fields, and `when` arms.

Example of erroneous code:

    let x I32 = 42             // missing colon before type
    type Point { x F64 }      // missing colon in field

How to fix:

    let x: I32 = 42            // add colon before type
    type Point { x: F64 }     // add colon in field
)EX"},

        {"P009", R"EX(
Expected comma [P009]

The parser expected a comma `,` separating items in a list, but found
something else. This occurs in function parameters, arguments, struct
fields, tuple elements, and type arguments.

Example of erroneous code:

    func add(a: I32 b: I32) -> I32 { ... }
    let t = (1 2 3)

How to fix:

    func add(a: I32, b: I32) -> I32 { ... }
    let t = (1, 2, 3)
)EX"},

        {"P010", R"EX(
Expected parenthesis [P010]

The parser expected an opening `(` or closing `)` parenthesis but
found a different token.

Example of erroneous code:

    func add a: I32, b: I32) -> I32 { ... }    // missing (
    let result = compute(1, 2                   // missing )

How to fix:

    func add(a: I32, b: I32) -> I32 { ... }
    let result = compute(1, 2)
)EX"},

        {"P014", R"EX(
Invalid literal in pattern [P014]

A pattern position contains a literal that is not valid in that context.
Patterns support integer, float, string, boolean, and character literals.

Example of erroneous code:

    when value {
        [1, 2] => println("array")    // array is not a pattern literal
    }

How to fix:

    when value {
        1 => println("one"),
        2 => println("two"),
        _ => println("other")
    }
)EX"},

        {"P017", R"EX(
Unclosed parenthesis [P017]

An opening parenthesis `(` was found but the matching closing `)` is
missing. This often happens with nested expressions or multi-line
function calls.

Example of erroneous code:

    let result = compute(
        a + b,
        c * d
    // missing closing )

How to fix:

    let result = compute(
        a + b,
        c * d
    )
)EX"},

        {"P019", R"EX(
Invalid operator [P019]

The parser found a token that is not a valid operator in the current
context. TML uses word operators (`and`, `or`, `not`) instead of
symbol operators (`&&`, `||`, `!`).

Example of erroneous code:

    if x && y { ... }          // use 'and' instead
    if !done { ... }           // use 'not' instead

How to fix:

    if x and y { ... }
    if not done { ... }

Valid operators: +, -, *, /, %, ==, !=, <, >, <=, >=, and, or, not
)EX"},

        {"P022", R"EX(
Expected type name [P022]

The parser expected a type name (like `I32`, `Str`, `List[T]`) but
found a different token.

Example of erroneous code:

    type = { x: I32 }         // missing type name
    let x: = 42               // missing type after colon

How to fix:

    type Point = { x: I32 }
    let x: I32 = 42
)EX"},

        {"P026", R"EX(
Expected module name [P026]

The parser expected a module name in an `impl` block or `use` statement
but found a different token.

Example of erroneous code:

    impl for MyType { ... }    // missing behavior name
    use                        // missing module path

How to fix:

    impl Display for MyType { ... }
    use std::collections::List
)EX"},

        {"P037", R"EX(
Expected expression in interpolation [P037]

A string interpolation `${...}` or `{...}` is missing the expression
inside the braces.

Example of erroneous code:

    let s = `hello ${}`        // empty interpolation

How to fix:

    let s = `hello ${name}`
    let s = `value: ${x + 1}`
)EX"},

        {"P041", R"EX(
Invalid struct pattern [P041]

A struct destructuring pattern has invalid syntax. Struct patterns must
use `TypeName { field1, field2 }` syntax.

Example of erroneous code:

    when value {
        Point { } => ...       // might be valid but check syntax
    }

How to fix:

    when value {
        Point { x, y } => println("({x}, {y})"),
        _ => println("other")
    }
)EX"},

        {"P043", R"EX(
Invalid tuple pattern [P043]

A tuple pattern has invalid syntax or an unexpected token inside
the parentheses.

Example of erroneous code:

    let (a b) = get_pair()     // missing comma

How to fix:

    let (a, b) = get_pair()
)EX"},

        {"P045", R"EX(
Expected field type [P045]

A struct or enum variant field declaration is missing its type annotation.

Example of erroneous code:

    type Point {
        x,                     // missing type
        y: F64
    }

How to fix:

    type Point {
        x: F64,
        y: F64
    }
)EX"},

        {"P047", R"EX(
Invalid when arm [P047]

A `when` (match) expression has an invalid arm. Each arm must have
a pattern followed by `=>` and a body expression.

Example of erroneous code:

    when value {
        1 -> println("one")   // use => not ->
    }

How to fix:

    when value {
        1 => println("one"),
        _ => println("other")
    }
)EX"},

        {"P048", R"EX(
Expected arrow in when arm [P048]

A `when` arm is missing the `=>` fat arrow between the pattern and
the body expression.

Example of erroneous code:

    when value {
        1 println("one")      // missing =>
    }

How to fix:

    when value {
        1 => println("one"),
        _ => println("other")
    }
)EX"},

        {"P050", R"EX(
Expected get or set [P050]

A property declaration expected `get` or `set` accessor but found
something else.

Example of erroneous code:

    type Rect {
        prop area {
            return width * height  // missing 'get'
        }
    }

How to fix:

    type Rect {
        prop area {
            get { return this.width * this.height }
        }
    }
)EX"},

        {"P064", R"EX(
Invalid closure expression [P064]

A closure (anonymous function) has invalid syntax. Closures use the
`do(params) expr` syntax.

Example of erroneous code:

    let f = do { x + 1 }      // missing parameter list

How to fix:

    let f = do(x) { x + 1 }
    let add = do(a, b) { a + b }
)EX"},

        {"P065", R"EX(
Invalid closure parameter [P065]

A closure parameter has invalid syntax. Closure parameters are
comma-separated identifiers inside parentheses.

Example of erroneous code:

    let f = do(x:) { x + 1 }  // incomplete type annotation

How to fix:

    let f = do(x) { x + 1 }
    let f = do(x: I32) { x + 1 }
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
