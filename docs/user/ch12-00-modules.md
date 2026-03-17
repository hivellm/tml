# Modules and Imports

As a TML project grows, keeping all code in a single file becomes impractical. TML's module system lets you split code across files, control what each file exposes, and import the standard library and your own modules with a uniform syntax.

## What Is a Module?

In TML, every source file is a module. A module has a name (derived from its filename or its `mod` declaration), a set of public symbols it exposes to other modules, and a set of private symbols visible only within itself. When you import a module with `use`, you gain access to its public symbols.

The module system has three components:

1. **`mod` declarations** — declare the name and path of a module, typically at the top of a file
2. **`use` declarations** — import symbols from another module into the current scope
3. **`pub` visibility** — mark symbols as accessible from outside their defining module

## Why Modules Matter

Without modules, name collisions are unavoidable, files grow without bound, and there is no boundary between public API and internal implementation detail. With modules:

- Related code lives together and is easy to find
- Internal helpers can remain private, keeping the public API surface small
- Different parts of a program can evolve independently

## Standard Library Structure

The TML standard library is organized into two top-level libraries:

- `core` — low-level primitives: allocation, memory, strings, iterators, error types, numeric operations
- `std` — higher-level modules: collections, file I/O, JSON, cryptography, networking, concurrency

You import from either with `use`:

```tml
use std::collections::List
use std::collections::{HashMap, HashSet}
use core::fmt::Display
```

## What This Chapter Covers

- **Defining Modules** (ch12-01) — how `mod` declarations work, file naming conventions, and how to organize code into a directory structure
- **Use Declarations and Visibility** (ch12-02) — the full syntax of `use`, the `pub` modifier, re-exports, and the module resolution order
