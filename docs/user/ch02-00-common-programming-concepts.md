# Common Programming Concepts

This chapter covers the constructs that appear in nearly every TML program. If you have
programmed before, most of these ideas will be familiar — what is new is how TML expresses
them.

The sections are:

- **Variables and mutability** — TML distinguishes `let` (immutable) from `var` (mutable) at
  the declaration site. Constants use `const`.
- **Data types** — TML is statically typed. Every value has a type the compiler knows at
  compile time. This chapter covers the built-in scalar and compound types.
- **Functions** — Defined with `func`, called by name. The last expression in a function body
  is its return value.
- **Comments** — Line comments with `//`, block comments with `/* */`, and documentation
  comments with `///`.
- **Control flow** — `if` expressions, the unified `loop` keyword, and the `when` pattern
  matching expression.

Read this chapter sequentially. Each section builds on the ones before it, and the examples in
later chapters assume you are comfortable with all of these concepts.
