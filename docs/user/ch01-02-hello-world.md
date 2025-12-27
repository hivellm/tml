# Hello, World!

Now that you have TML installed, let's write your first TML program.
It's traditional to write a program that prints "Hello, World!" to the
screen, so we'll do exactly that.

## Creating a Project File

Create a new file called `hello.tml` with the following content:

```tml
func main() {
    println("Hello, World!")
}
```

Save the file and run it:

```bash
tml run hello.tml
```

You should see:

```
Hello, World!
```

Congratulations! You've officially written a TML program.

## Anatomy of a TML Program

Let's look at each part of the program:

```tml
func main() {
    println("Hello, World!")
}
```

### The `func` Keyword

In TML, functions are defined using the `func` keyword (not `fn` like Rust).
This is more explicit and readable.

### The `main` Function

The `main` function is special: it's the entry point of every TML program.
When you run a TML program, execution starts at `main`.

### The Function Body

The function body is enclosed in curly braces `{ }`. Inside, we have a
single statement that calls `println`.

### The `println` Function

`println` is a builtin function that prints text to the console, followed
by a newline. TML also provides `print` which doesn't add a newline.

## Compiling and Running

TML provides several ways to run your code:

### Run Directly

```bash
tml run hello.tml
```

This compiles and runs the program in one step.

### Check Syntax

```bash
tml check hello.tml
```

This checks for syntax and type errors without running the program.

### Parse Only

```bash
tml parse hello.tml
```

This parses the file and shows the AST (useful for debugging).

## What's Next?

Now that you have a working TML installation and know how to run programs,
let's learn about variables and data types in the next chapter.
