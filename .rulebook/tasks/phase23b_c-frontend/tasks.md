# Tasks: C17 Frontend — Parser, Type Checker, C→MIR Lowering

**Status**: In progress (12/24 — lexer + parser declarations + parser expressions complete)
**Depends on**: phase23a (C preprocessor), phase15c (MIR builder as target)
**Blocks**: phase23c (C++ subset frontend extends the C frontend)
**Duration**: 12–16 weeks
**Risk**: High — C type system has subtle promotion/conversion rules and pointer arithmetic semantics
**Output**: ~16,200 LOC TML

---

## Phase 1: C Lexer (3 items — COMPLETE)

- [x] 1.1 Created `compiler-tml/src/cc/lexer.tml` — C token types and classifier. All 44 C17 keywords in a HashMap-backed table (Auto … _Thread_local). `CLexer` with `c_lexer(input)` constructor and `tokenize(lex) -> List[CToken]` driver that drops Whitespace/Newline (translation phase 7). Commit `1970439f`.
- [x] 1.2 Implemented literal payload decoders in the same file. `decode_int`: hex (`0x`) / octal (`0`) / binary (`0b` GCC ext) / decimal with `u`/`l`/`ll`/`ul`/`ull` suffixes in any case order, into a `U64` with an `is_hex_or_octal` flag for sign promotion. `decode_float`: decimal parser inline (stdlib `parse_f64 -> Maybe[F64]` hit K001) + C99 hex-float path (`0x1.8p3`) with `f`/`l` suffix. `decode_char`: single-char escapes, hex (`\xHH`), octal (`\NNN`), UCN (`\uHHHH`, `\UHHHHHHHH`), encoding prefixes (`L`, `u`, `U`, `u8`) — multi-char constants fold via left-shift. `decode_string`: same escape engine + per-encoding byte layout (Plain/Utf8 = low byte, Utf16 = LE pair, Wide/Utf32 = LE quad). Commit `7ae539b5`.
- [x] 1.3 C17 punctuator scanning — all 47 operators in the punct table in item 1.1: `->`, `.`, `++`, `--`, `<<`, `>>`, `<<=`, `>>=`, `&=`, `|=`, `^=`, `+=`, `-=`, `*=`, `/=`, `%=`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `...`, plus all 6 digraphs (`<:` `:>` `<%` `%>` `%:` `%:%:`) normalized to their primary spelling. Trigraph removal and line splicing live in the preprocessor (phase23a, translation phases 1–2); the C lexer is translation phase 7 and never sees them.

## Phase 2: C Parser — Declarations (5 items — COMPLETE)

- [x] 2.1 Created `compiler-tml/src/cc/ast.tml` (C AST node types) and `compiler-tml/src/cc/parser.tml` (parser infrastructure). `CParser` owns a `List[CToken]` cursor plus a `typedefs: HashMap[Str, I64]` for the typedef-name ambiguity. `CTranslationUnit` holds `List[Heap[CDecl]]`. `cp_parse_specifiers` accepts the 6 C17 specifier categories in any order, folds scalar counts (`unsigned long long` → `ULongLong`, `signed int` → `Int`, etc.), and rejects contradictions (e.g. `signed unsigned`). Aggregate specifiers (`struct Foo`) emit a `CBaseType::StructRef(tag)` reference. Commit `5c44e00a`.
- [x] 2.2 Implemented declarator parsing (`cp_parse_declarator`, `cp_parse_abstract_declarator`) with pointer/array/function layers wrapping outwards from the leaf `Ident`/`Abstract`. `cp_parse_initializer` handles scalar (`= 5`), brace (`= {1,2,3}`), and designated (`= {[0] = 7, .field = val}`) forms with nested designator chains. Multiple declarators per declaration (`int a, *b, c;`) expand to separate `CVarDecl`s sharing the specifier.
- [x] 2.3 Implemented function declarator with full ANSI-style prototype + definition support. `cp_parse_param_list` recognises empty `()`, explicit `(void)`, regular prototypes, and variadic `(..., ...)`. Function bodies parse via `cp_parse_compound_stmt` (a brace-matching consumer for Phase 2; Phase 4 replaces with full statement parsing). K&R-style `int f(a,b) int a; int b; { ... }` parameter declarations hold a `kr_decls: List[CVarDecl]` slot for Phase 4 expansion. `CFuncDecl` carries `is_variadic`, `body: Maybe[Heap[CStmt]]`, and the full declarator spine.
- [x] 2.4 Implemented struct/union/enum bodies via `cp_parse_struct_body` and `cp_parse_enum_body`. Struct/union fields support plain declarators, bit fields (`int flags : 4`), anonymous bit fields (`int : 2`) and anonymous aggregate members (C11 `struct { ... };`). Enum variants support optional `= expr` explicit values. Forward declarations (`struct Foo;`) emit a `CStructDef` with `is_forward = 1` and empty fields.
- [x] 2.5 Implemented typedef via the shared declarator path: when `CStorageClass::Typedef` appears in the specifier list, each declarator's name is added to `parser.typedefs` immediately and a `CDecl::TypedefDef` is emitted. Subsequent declarations resolve the name via `cp_parse_specifiers`'s typedef-name branch, which yields `CBaseType::Typedef(name)`. Function pointer typedefs (`typedef int (*F)(int);`) fall out naturally — the declarator wraps a function layer inside a pointer.

## Phase 3: C Parser — Expressions (4 items — COMPLETE)

- [x] 3.1 Created `compiler-tml/src/cc/parse_expr.tml` as the public entry point for the expression parser. The implementation itself lives in `parser.tml` (expression, declarator, type-name and initializer grammars are mutually recursive in C so TML's non-circular `use` imports force them into a single file — same pattern as `compiler-tml/src/parser/common.tml`). `parse_expr.tml` re-exports `cp_parse_expr`, `cp_parse_assign_expr`, `cp_parse_initializer`, `cp_parse_type_name`, and `cp_at_type_start` as its public surface. Precedence climbing implements all 15 C operator levels (§6.5) with correct left/right associativity: level 15 postfix (call, subscript, `.`, `->`, `++`/`--`), 14 prefix (`++`/`--`, `+`/`-`/`!`/`~`/`*`/`&`, `sizeof`, `_Alignof`), 13 cast, 12–0 binary/ternary/assignment/comma.
- [x] 3.2 Arithmetic, comparison, logical and bitwise operators all wired via `infix_info(op) → (prec<<8 | binop_code)` and `binop_from_code` dispatch: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `~`, `&`, `|`, `^`, `<<`, `>>`. Assignment operators resolved via `assign_op_from_punct` — `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=` — right-associative and folded into `CExpr::Assign(CAssignOp, lhs, rhs)`. Prefix/postfix `++`/`--` map to `CUnaryOp::PreInc`/`CExpr::PostInc` etc.
- [x] 3.3 Pointer and memory operators: dereference (`*e`), address-of (`&e`), arrow (`e->f`), array subscript (`e[i]`) — all emitted as dedicated `CExpr` variants, not desugared to pointer arithmetic (the type checker does that lowering). `sizeof` disambiguates between `sizeof(T)` and `sizeof e` by peeking past `(` for a type-starter keyword / typedef-name. `_Alignof(T)` always takes a type. Casts check for `( type-start )` before falling through to parenthesised primary. Compound literals `(T){...}` detected by peeking for `{` after the type-name's closing `)`.
- [x] 3.4 Ternary `a ? b : c` parsed in `cp_parse_cond` — right-associative so the else-branch recursively allows a nested conditional. Comma operator folded left-associatively in `cp_parse_expr`. Function calls and struct member access handled in `cp_parse_postfix`. String literal concatenation done in `cp_parse_primary`: adjacent `StringLit` tokens are glued into a single `CExpr::StringLit`, preserving the first token's encoding — mismatches are flagged by the type checker (so errors can point at the offending literal). `_Generic(ctrl, T1: e1, default: ed)` parsed by `cp_parse_generic`, producing `CExpr::Generic(ctrl, List[CGenericAssoc])` where each assoc carries `is_default`, `ty: CTypeName`, and the value expression.

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

## Lexer bring-up notes (phase 1 — already banked)

Six pre-existing compiler bugs surfaced during lexer bring-up and were
worked around inline; each is a candidate for a dedicated follow-up
before it bites the parser phases:

1. `base` is a reserved TML keyword (`KwBase`) — local variable had to be renamed `radix`
2. `return StructName { ... }` inside an `if` confuses the parser (reads `{` as trailing block) — bind result to a typed local before `return`
3. `HashMap.get(k)` returns `V` directly (zero-value on miss), NOT `Maybe[V]` as the signature suggests — use `.has(k) + .get(k)` pair
4. `if-expr` with F64 branches emits phi nodes that contain integer `0` (K001 codegen) — decompose into `var` + imperative `if`
5. `for _ in 0 to n { acc = acc * 10.0 }` has the same F64-phi K001 issue — replace with `loop (count > 0)` over a separate counter
6. `Str.parse_f64 -> Maybe[F64]` loops forever at runtime (K001 on `Maybe[F64]`) — inline the decimal parser rather than using stdlib

## Parser bring-up notes (phase 2 — already banked)

Three additional TML codegen bugs surfaced while bringing up the
declaration parser and were worked around inline. Each is a candidate
for a dedicated follow-up before it bites the expression/statement
phases:

7. Enum variants holding a `Heap[T]` followed by other fields fail codegen on pattern-binding the non-heap fields ("Unknown variable: ..." — e.g. `Func(Heap[CDeclarator], List[CParam], I64)` rejects `Func(_, ps, _)`). Workaround: collapse the multi-arg variant into a single named struct (`CFuncDeclPart { inner, params, is_variadic }`) and match `Func(part)` to read `part.params` / `part.is_variadic`.
8. Deeply-nested constructor expressions in a single statement — e.g. `decls.push(Heap[CDecl]::new(CDecl::Var(Heap[CVarDecl]::new(vd))))` — hang or crash at runtime because TML's duplicate codegen recursively walks each nested enum/struct payload. Workaround: introduce named local bindings for every heap allocation step so the constructor tree is flat (`let vd_heap = Heap[CVarDecl]::new(vd); let decl_val = CDecl::Var(vd_heap); let decl_heap = Heap[CDecl]::new(decl_val); decls.push(decl_heap)`).
9. Large enums whose variants carry whole structs by value (e.g. `CDecl::Var(CVarDecl)` where `CVarDecl` has 7 nested fields) produce runtime crashes in the duplicate path. Workaround: wrap each CDecl variant payload in `Heap[T]` so the enum payload is pointer-sized (`Var(Heap[CVarDecl])`, `Func(Heap[CFuncDecl])`, `StructDef(Heap[CStructDef])`, etc.). This mirrors what the TML parser already does for `Decl::Impl(Heap[ImplDecl])`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
