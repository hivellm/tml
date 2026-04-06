# Tasks: C17 Frontend — Parser, Type Checker, C→MIR Lowering

**Status**: Planned (0/24)
**Depends on**: phase23a (C preprocessor), phase15c (MIR builder as target)
**Blocks**: phase23c (C++ subset frontend extends the C frontend)
**Duration**: 12–16 weeks
**Risk**: High — C type system has subtle promotion/conversion rules and pointer arithmetic semantics
**Output**: ~16,200 LOC TML

---

## Phase 1: C Lexer (3 items)

- [ ] 1.1 Create `compiler-tml/src/cc/lexer.tml` — C token types: all 44 C17 keywords (auto, break, case, char, const, continue, default, do, double, else, enum, extern, float, for, goto, if, inline, int, long, register, restrict, return, short, signed, sizeof, static, struct, switch, typedef, union, unsigned, void, volatile, while, _Alignas, _Alignof, _Atomic, _Bool, _Complex, _Generic, _Imaginary, _Noreturn, _Static_assert, _Thread_local); implement `CLexer` type with `next_token() -> CToken` method
- [ ] 1.2 Implement C literal scanning: integer literals (decimal, hex `0x`, octal `0`, binary `0b` as GCC extension) with suffixes (u, l, ul, ll, ull); float literals (decimal, hex) with suffixes (f, l); character literals ('a', '\n', '\x41', '\u0041') including wide chars (L'a'); string literals ("hello") including wide (L""), UTF-8 (u8""), UTF-16 (u""), UTF-32 (U"") prefixes
- [ ] 1.3 Implement C operator scanning: all C17 operators including `->`, `.`, `++`, `--`, `<<`, `>>`, `<<=`, `>>=`, `&=`, `|=`, `^=`, `+=`, `-=`, `*=`, `/=`, `%=`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `...` (ellipsis for variadics); implement trigraph removal and line splicing (backslash-newline) as the first two translation phases

## Phase 2: C Parser — Declarations (5 items)

- [ ] 2.1 Create `compiler-tml/src/cc/parser.tml` — C parser infrastructure: `CParser` type with `parse_translation_unit() -> CTranslationUnit`; `CTranslationUnit` holds `List[CDecl]` (functions and global variables); implement declaration specifier parsing: storage class (auto, extern, register, static, typedef), type specifiers (void, char, short, int, long, float, double, _Bool, _Complex, signed, unsigned), type qualifiers (const, volatile, restrict, _Atomic), function specifiers (inline, _Noreturn)
- [ ] 2.2 Implement variable declarations: `int x = 5;`, `const char *s = "hello";`, `int arr[10] = {1,2,3};`, `static int count = 0;`; handle multiple declarators in one declaration (`int a, b, *c;`); implement initializer expressions (scalar, array `{1,2,3}`, struct `{.field = val}` designated initializers per C99)
- [ ] 2.3 Implement function declarations: `int foo(int a, int b) { ... }` (ANSI style); `int foo(a, b) int a; int b; { ... }` (K&R style, for compatibility with legacy system headers); function prototypes `int foo(int, int);`; variadic functions `int printf(const char *fmt, ...)`;  inline functions; `extern` and `static` linkage
- [ ] 2.4 Implement struct/union/enum declarations: `struct Foo { int x; char y[8]; };` with field declarations, bit fields (`int flags : 4`), anonymous struct/union members (C11); `union { int i; float f; } u;`; `enum Color { RED = 0, GREEN, BLUE };` with explicit values; forward declarations `struct Foo;`; typedef'd structs `typedef struct { ... } Foo;`
- [ ] 2.5 Implement typedef: `typedef unsigned long long uint64_t;`, `typedef struct Node { int val; struct Node *next; } Node;`, `typedef int (*FuncPtr)(int, int);` (function pointer typedefs); typedef'd types must be recognized as type names during subsequent parsing (the typedef-name ambiguity with identifiers in C parsing)

## Phase 3: C Parser — Expressions (4 items)

- [ ] 3.1 Create `compiler-tml/src/cc/parse_expr.tml` — C expression parser using precedence climbing; `CExpr` enum with variants for all C expression forms; implement the 15 C operator precedence levels correctly (postfix > prefix > multiplicative > additive > shift > relational > equality > bitwise-AND > bitwise-XOR > bitwise-OR > logical-AND > logical-OR > conditional > assignment > comma)
- [ ] 3.2 Implement arithmetic, comparison, logical, and bitwise operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `~`, `&`, `|`, `^`, `<<`, `>>`; assignment operators: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`; prefix/postfix `++` and `--`
- [ ] 3.3 Implement pointer and memory operators: dereference `*expr`, address-of `&expr`, arrow `expr->field`, array subscript `expr[idx]` (equivalent to `*(expr + idx)`); `sizeof(expr)` and `sizeof(type)` (evaluated at compile time); cast expressions `(type)expr`; compound literals `(struct Foo){.x = 1}` (C99)
- [ ] 3.4 Implement: ternary `a ? b : c` (right-associative), comma operator `a, b` (sequence point, result is b), function call `f(args)`, struct member access `expr.field`; string literal concatenation (`"hello" " world"` → `"hello world"` during translation phase 6); `_Generic` selection expression (C11 type-generic macros)

## Phase 4: C Parser — Statements (3 items)

- [ ] 4.1 Create `compiler-tml/src/cc/parse_stmt.tml` — C statement parser; `CStmt` enum with variants for all C statement forms; implement expression statement `expr;` and empty statement `;`; implement compound statement (block) `{ decls... stmts... }` with mixed declarations and statements (C99 rule: declarations may appear anywhere in a block, not just at the top)
- [ ] 4.2 Implement control flow: `if (cond) stmt` and `if (cond) stmt else stmt`; `for (init; cond; update) stmt` with C99 for-init declarations (`for (int i = 0; ...)`); `while (cond) stmt`; `do stmt while (cond);`; `switch (expr) { case val: ...; default: ...; }` with fall-through; `break` and `continue`; `return expr;` and `return;`
- [ ] 4.3 Implement: `goto label;` and label declarations `label: stmt`; declaration statements inside blocks (C99 mixed declarations); `__attribute__((noreturn))` and `__attribute__((unused))` (GCC extensions, parsed and ignored for compatibility); `_Static_assert(expr, msg)` (C11); `_Alignas(N)` alignment specifier on declarations

## Phase 5: C Type Checker (4 items)

- [ ] 5.1 Create `compiler-tml/src/cc/types.tml` — C type representation: `CType` enum with variants `Void`, `Bool`, `Char(Signed)`, `Short(Signed)`, `Int(Signed)`, `Long(Signed)`, `LongLong(Signed)`, `Float`, `Double`, `LongDouble`, `Ptr(CType)`, `Array(CType, Maybe[I64])`, `Struct(Str, List[CField])`, `Union(Str, List[CField])`, `Enum(Str, List[CEnumVariant])`, `Func(CType, List[CType], Bool)` (return, params, is_variadic), `Qualified(CType, Qualifiers)`; implement `sizeof` and `alignof` for each type per the platform ABI (System V AMD64 / Windows x64)
- [ ] 5.2 Implement integer promotion rules (C11 §6.3.1.1): `char` and `short` are promoted to `int` in arithmetic expressions; `unsigned char`/`unsigned short` promote to `int` if int can represent all values, otherwise `unsigned int`; `_Bool` promotes to `int`; usual arithmetic conversions (§6.3.1.8): when operands have different integer types, the smaller type is converted to the larger; signed vs unsigned mixing follows specific rules
- [ ] 5.3 Implement implicit conversions: integer-to-float (`int` → `double` in mixed arithmetic); lvalue-to-rvalue (reading a variable produces its value); array-to-pointer decay (`int arr[10]` → `int *` when used as an expression, except with `sizeof` or `&`); function-to-pointer decay (function name → pointer to function); null pointer constant (integer 0 or `(void*)0`) to any pointer type
- [ ] 5.4 Implement pointer arithmetic (C11 §6.5.6): `ptr + n` advances by `n * sizeof(*ptr)` bytes; `ptr - n` similarly; `ptr1 - ptr2` (same array) produces `ptrdiff_t` in elements; pointer comparison valid only for pointers into the same array; `void *` arithmetic is a GCC extension (treat as `char *` arithmetic, sizeof = 1); implement tentative definition resolution for globals

## Phase 6: C → MIR Lowering (3 items)

- [ ] 6.1 Create `compiler-tml/src/cc/lower.tml` — C AST → MIR translation; `CLower` type that produces a `mir::Module`; implement basic value lowering: C integer types → `i8`/`i16`/`i32`/`i64`, float types → `f32`/`f64`, pointer types → `ptr`; implement integer promotions and implicit conversions as explicit MIR cast instructions; lower global variable declarations to MIR globals with correct section (`.data` for initialized, `.bss` for zero-initialized)
- [ ] 6.2 Implement struct/union/array layout: compute field offsets and padding following the System V AMD64 ABI (Linux/macOS) and Windows x64 ABI (Windows); union fields all start at offset 0, union size is the maximum field size rounded up to alignment; arrays are contiguous with element stride = sizeof(element); implement GEP (GetElementPtr) for struct field access (`expr->field` → `ptr + offset`) and array subscript (`ptr[i]` → `ptr + i * stride`); bit fields: pack into the containing integer type, extract/insert with shifts and masks
- [ ] 6.3 Implement control flow lowering: `goto` and labels → unconditional MIR branches; `switch` → MIR `switch_int` instruction with arms for each case; C99 for-init declarations → variable declared at top of loop's enclosing block in MIR; `break`/`continue` → jump to loop exit/continue label; C99 designated initializers and compound literals → MIR alloca + store sequence; `__builtin_expect` (GCC extension) → pass through as branch weight hint

## Phase 7: Testing (2 items)

- [ ] 7.1 Test: `tml cc compiler/runtime/core/essential.c` compiles TML's own C runtime successfully — the compiled object must pass all runtime behavior tests (I/O, panic, assert, test harness entry point); this is the primary correctness gate for the C frontend
- [ ] 7.2 Test: `tml cc compiler/runtime/memory/mem.c` compiles successfully; link with `essential.o`; run the TML test suite against binaries linked with the TML-compiled runtime objects instead of Clang-compiled objects — all tests must pass, proving the C frontend produces ABI-compatible code
