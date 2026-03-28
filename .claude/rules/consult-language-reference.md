# MANDATORY: Consult Language Reference Before Implementing

Before writing ANY new TML code, you MUST use the MCP documentation tools AND read `docs/readme.md`.

## Why

The TML standard library has 500+ types and 5000+ functions already implemented. Past implementations ignored existing APIs, used wrong syntax, and created bugs that cost hours to debug. The MCP docs tools provide instant access to correct syntax, existing APIs, and known limitations.

## NEVER Read Source Files to Understand APIs

**DO NOT use `Read` on `lib/core/src/*.tml` or `lib/std/src/*.tml` to discover what a type provides.** Source files contain lowlevel implementation details (pointer arithmetic, memory layouts) that waste tokens and obscure the public API.

**INSTEAD use:**
- `mcp__tml__docs_list(module="std::collections::List", kind="method")` — see all public methods
- `mcp__tml__docs_search(query="sort list")` — find relevant functions
- `mcp__tml__docs_get(id="std::collections::List::sort")` — full docs for one item
- `mcp__tml__docs_resolve(name="HashMap")` ��� find full qualified path

Only read source files when you need to **modify** the implementation, not when you need to **use** it.

## Step 1: Use MCP Docs Tools (PREFERRED — instant, searchable)

**Before writing ANY TML code, call one of these:**

- `mcp__tml__docs_search` — Search for types, functions, behaviors by keyword
  - Example: `mcp__tml__docs_search(query="Mutex lock")` → finds Mutex API
  - Example: `mcp__tml__docs_search(query="impl behavior")` → finds impl syntax
  - Example: `mcp__tml__docs_search(query="loop while", kind="function")` → finds loop syntax

- `mcp__tml__docs_list` — List ALL items in a module
  - Example: `mcp__tml__docs_list(module="std::sync")` → all sync types and functions
  - Example: `mcp__tml__docs_list(module="core::iter")` → all iterator adapters

- `mcp__tml__docs_get` — Get FULL documentation for a specific item
  - Example: `mcp__tml__docs_get(id="std::collections::HashMap")` → full API with examples

## Step 2: Check Static Docs (FALLBACK — if MCP is down)

1. **Check `docs/readme.md`** for existing types before using `lowlevel { ptr_read/ptr_write/mem_alloc }`
2. **Check `docs/packages/`** for detailed API docs of any module
3. **Check `docs/user/`** for tutorial-style guides on language features
4. **Check `docs/specs/`** for formal language specification

## Common Syntax Pitfalls (check docs to avoid these)

| Wrong | Correct | How to check |
|-------|---------|-------------|
| `impl Type with Behavior` | `impl Behavior for Type` | `docs_search("impl behavior")` |
| `loop { }` | `loop (condition) { }` | `docs_search("loop syntax")` |
| `Start(name: I64)` | `Start(I64)` | `docs_search("enum variant")` |
| `type Alias = I32` | May not work cross-module | `docs_search("type alias")` |
| `lowlevel { str_from_raw }` | Has codegen bug, use FFI | `docs_search("str_from_raw")` |

## Rules

1. **ALWAYS use MCP docs** before writing new TML modules — search for existing types first
2. **Use `Text`** for string building — not manual `copy_nonoverlapping` chains
3. **Use `Buffer`** for byte manipulation — not raw `ptr_read[U8]`/`ptr_write[U8]`
4. **Use `HashMap`/`List`** for collections — not manual array+offset layouts
5. **Use `Outcome[T,E]` with `!`** — not raw I64 error codes
6. **Use template literals** — `` `Hello, {name}!` `` works today (returns `Text`)
7. **Use `Mutex[T]`/`Sync[T]`** for shared state — not manual memory layouts

## The ONLY acceptable uses of `lowlevel` are:

- FFI calls to C runtime (`@extern("c")` wrappers)
- Performance-critical inner loops where profiling proves abstraction overhead matters
- Implementing core library primitives themselves
