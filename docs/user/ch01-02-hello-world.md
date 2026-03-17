# Hello, World!

With TML installed, you are ready to write your first program. By tradition, that program prints
"Hello, World!" to the screen.

## Creating the File

Create a new file called `hello.tml` with the following content:

```tml
func main() {
    println("Hello, World!")
}
```

Save the file, then run it:

```bash
tml run hello.tml
```

Output:

```
Hello, World!
```

That is a complete, working TML program.

## Anatomy of the Program

```tml
func main() {
    println("Hello, World!")
}
```

### `func`

The `func` keyword declares a function. TML uses `func` rather than `fn` (Rust) or `function`
(JavaScript) — explicit and unambiguous.

### `main`

`main` is the entry point of every TML program. When you run a TML program, execution begins at
`main`. The name is required: TML will refuse to link a program that has no `main` function.

### The function body

The body of the function is enclosed in curly braces `{ }`. Most constructs in TML that contain
a block of code use curly braces.

### `println`

`println` is a builtin function that prints its argument to standard output, followed by a
newline. TML also provides `print`, which prints without a trailing newline.

```tml
func main() {
    print("Hello, ")    // no newline
    println("World!")   // adds newline
}
```

Output:

```
Hello, World!
```

## CLI Commands

The `tml` tool provides several commands for working with TML programs.

### `tml run`

Compiles and immediately runs a source file:

```bash
tml run hello.tml
```

This is the fastest way to try out a program. The compiled binary is temporary — it is not
saved to disk by default.

### `tml build`

Compiles a source file and writes the executable to disk:

```bash
tml build hello.tml
```

By default this produces `hello` (or `hello.exe` on Windows) in the current directory. To
specify the output path:

```bash
tml build hello.tml --output dist/hello
```

### `tml check`

Type-checks a source file without producing any output:

```bash
tml check hello.tml
```

Use this when you want to verify that your code is correct without running it. It is faster than
a full build because it skips code generation and linking.

## Common Mistakes

**Forgetting the return type arrow** — For `main`, no return type is needed. For other functions
that return a value, you must write `-> ReturnType`. This is covered in detail in the Functions
section.

**Using semicolons** — TML does not require semicolons at the end of statements. The compiler
will accept them in most positions, but idiomatic TML omits them.

**Mismatched braces** — Every `{` must have a matching `}`. If `tml check` reports an unexpected
token error near the end of a file, look for an unclosed brace.

## What's Next?

You have a working TML installation and know how to run programs. The next chapter covers the
fundamental building blocks of the language: variables, data types, functions, comments, and
control flow.
