# MANDATORY: Consult Language Reference Before Implementing

Before writing ANY new TML code, you MUST read `docs/readme.md` to check what types, functions, and modules already exist in the language.

## Why

The TML standard library has 500+ types and 5000+ functions already implemented. Past implementations ignored existing APIs and used raw `lowlevel` blocks everywhere, producing massively verbose and unsafe code.

## Rules

1. **ALWAYS check `docs/readme.md`** for existing types before using `lowlevel { ptr_read/ptr_write/mem_alloc }`
2. **Use `Text`** for string building — not manual `copy_nonoverlapping` chains
3. **Use `Buffer`** for byte manipulation — not raw `ptr_read[U8]`/`ptr_write[U8]`
4. **Use `HashMap`/`List`** for collections — not manual array+offset layouts
5. **Use `Outcome[T,E]` with `!`** — not raw I64 error codes
6. **Use template literals** — `` `Hello, {name}!` `` works today (returns `Text`)
7. **Use `Mutex[T]`/`Sync[T]`** for shared state — not manual memory layouts
8. **Check `docs/packages/`** for detailed API docs of any module
9. **Check `docs/user/`** for tutorial-style guides on language features
10. **Check `docs/specs/`** for formal language specification

## The ONLY acceptable uses of `lowlevel` are:

- FFI calls to C runtime (`@extern("c")` wrappers)
- Performance-critical inner loops where profiling proves abstraction overhead matters
- Implementing core library primitives themselves
