# 2. Syntax Design: Keywords Over Symbols

## 2.1 Design Philosophy

TML's syntax is guided by a single overarching principle: **every token should have exactly one meaning, and that meaning should be self-evident to both humans and large language models.** This principle manifests as a systematic preference for English keywords over symbolic operators, and for explicit structure over contextual inference.

This is not merely an aesthetic choice. It is a response to a measurable problem: LLMs trained on multi-language corpora frequently confuse context-dependent symbols. The character `<` serves as a comparison operator, a generic delimiter, an HTML tag opener, a shell redirect, and a bitwise shift operand — depending on context. An LLM generating code must disambiguate these meanings through surrounding context, a process that introduces errors. TML eliminates this class of error entirely by ensuring that each token has one and only one syntactic role.

The grammar is designed to be **LL(1)** — a single token of lookahead is sufficient to determine which production rule to apply. This property is unusual among modern systems programming languages (Rust requires unbounded lookahead for certain constructs; C++ is context-sensitive). LL(1) parsing is significant because it mirrors the autoregressive generation model of LLMs: each token is produced given only the preceding context, with no ability to backtrack.

---

## 2.2 Systematic Syntax Decisions

### 2.2.1 Generics: [T] vs <T>

| Language | Syntax | Ambiguity |
|----------|--------|-----------|
| TML | `List[T]` | None — `[` is always generic/index |
| Rust | `Vec<T>` | `<` vs comparison requires parser backtracking |
| C++ | `vector<T>` | `>>` closing vs right-shift, `<` vs less-than |
| Go | `List[T]` (since 1.18) | Same bracket choice as TML |
| Python | `list[T]` (since 3.9) | Same bracket choice as TML |

TML uses square brackets for generic type parameters: `HashMap[K, V]`, `Maybe[T]`, `func identity[T](x: T) -> T`. The rationale is both syntactic and pragmatic:

1. **No parser ambiguity.** The expression `a < b` is always a comparison. There is no context in which `<` introduces a type parameter list.
2. **No multi-token confusion.** The Rust expression `a<b, c>` requires the parser to determine whether this is two comparisons or a generic instantiation — a decision that depends on the type environment, not syntax.
3. **Industry convergence.** Go (1.18+), Python (3.9+), and Scala 3 have independently adopted square brackets for generics, suggesting that the language design community is converging away from angle brackets.
4. **LLM generation accuracy.** In our informal testing, LLMs generate syntactically correct generic instantiations at a higher rate with bracket syntax, because `[` is unambiguous in virtually all programming contexts.

### 2.2.2 Closures: do(x) vs |x|

| Language | Syntax | Ambiguity |
|----------|--------|-----------|
| TML | `do(x) x * 2` | None — `do` is a keyword |
| Rust | `\|x\| x * 2` | `\|` vs bitwise OR, markdown table separator |
| C++ | `[](auto x) { return x * 2; }` | Complex capture syntax |
| Go | `func(x int) int { return x * 2 }` | Verbose but clear |
| Python | `lambda x: x * 2` | `lambda` keyword (clear) |

TML's closure syntax uses the `do` keyword followed by parenthesized parameters:

```
items.filter(do(x) x > 0).map(do(x) x * 2)
```

The pipe character `|` in Rust serves triple duty: bitwise OR operator, closure parameter delimiter, and (in markdown) table column separator. LLMs generating Rust code inside markdown documentation frequently produce malformed closures because the pipe character is interpreted as a table boundary. TML eliminates this class of error entirely.

The `do` keyword also provides a clear visual signal of closure boundaries. In nested expressions, Rust's pipe-delimited closures can be visually confusing:

```rust
// Rust: Where does one closure end and another begin?
items.iter().filter(|x| x.map(|y| y > 0).unwrap_or(false))
```

```
// TML: Closure boundaries are unambiguous
items.iter().filter(do(x) x.map(do(y) y > 0).unwrap_or(false))
```

### 2.2.3 Pattern Matching: when vs match

| Language | Keyword | Rationale |
|----------|---------|-----------|
| TML | `when` | Reads as natural English: "when x is..." |
| Rust | `match` | Technical term from ML tradition |
| Python | `match` (3.10+) | Followed Rust/ML convention |
| Kotlin | `when` | Same choice as TML — readability |

TML uses `when` for pattern matching, a choice shared with Kotlin. The keyword reads naturally in English context:

```
when status {
    Ok(value) -> process(value),
    Err(e) -> log_error(e),
}
```

This reads as "when status is Ok(value), process value; when status is Err(e), log the error." The `match` keyword, by contrast, requires the reader to know that "match" means "pattern match" — a term from the ML type theory tradition that is not self-evident to programmers without functional programming background.

### 2.2.4 Behaviors: behavior vs trait

| Language | Keyword | Rationale |
|----------|---------|-----------|
| TML | `behavior` | Describes what it defines — a set of behaviors |
| Rust | `trait` | Genetic metaphor — less intuitive |
| Go | `interface` | Method set — clear but limited |
| Swift | `protocol` | Communication metaphor |
| C++ | `concept` (C++20) | Mathematical — abstract |

The choice of `behavior` over `trait` is semantic rather than syntactic. A behavior defines what a type *can do* — its behaviors. The word "trait" comes from genetics and personality psychology; its use in programming is a term of art that must be learned. "Behavior" is immediately comprehensible:

```
behavior Printable {
    func to_text(this) -> Str
}

impl Printable for Point {
    func to_text(this) -> Str {
        return `({this.x}, {this.y})`
    }
}
```

Reading `behavior Printable` communicates: "this defines a printable behavior that types can implement." Reading `trait Printable` requires knowing that "trait" means "interface" in Rust's vocabulary.

### 2.2.5 Boolean Operators: Keywords vs Symbols

| Operation | TML | Rust/C++/Go | Python |
|-----------|-----|-------------|--------|
| Logical AND | `and` | `&&` | `and` |
| Logical OR | `or` | `\|\|` | `or` |
| Logical NOT | `not` | `!` | `not` |

TML follows Python's choice of keyword boolean operators. The rationale is threefold:

1. **Single meaning.** In Rust, `!` is both logical NOT and the macro invocation sigil (`println!`). In C, `!` is logical NOT while `~` is bitwise NOT. In TML, `not` is always logical NOT, and there is no macro system.
2. **LLM token clarity.** The string `&&` is typically tokenized as two or three tokens by LLM tokenizers (`&` + `&` or `&&`), while `and` is one token. This affects both generation accuracy and context window efficiency.
3. **Readability.** `if x > 0 and x < 100` reads more naturally than `if x > 0 && x < 100`, especially for programmers coming from Python, SQL, or natural language backgrounds.

### 2.2.6 References: ref T vs &T

| Language | Immutable Ref | Mutable Ref |
|----------|--------------|-------------|
| TML | `ref T` | `mut ref T` |
| Rust | `&T` | `&mut T` |
| C++ | `const T&` | `T&` |

TML replaces the ampersand with the keyword `ref`:

```
func length(s: ref Str) -> I64 { ... }
func append(s: mut ref Str, suffix: Str) { ... }
```

The `&` character is overloaded across languages: address-of (C), reference (C++/Rust), bitwise AND (everywhere), and string concatenation (some languages). Using the word `ref` eliminates all ambiguity and reads naturally: "a reference to Str" versus "an ampersand Str."

### 2.2.7 Error Types: Maybe/Outcome vs Option/Result

| TML | Rust | Rationale |
|-----|------|-----------|
| `Maybe[T]` | `Option<T>` | "Maybe there's a value" — self-documenting |
| `Just(x)` | `Some(x)` | "Just this value" — affirmative |
| `Nothing` | `None` | "Nothing here" — descriptive |
| `Outcome[T, E]` | `Result<T, E>` | "The outcome of an operation" — process-oriented |
| `Ok(x)` | `Ok(x)` | Identical — already clear |
| `Err(e)` | `Err(e)` | Identical — already clear |

The names `Maybe` and `Outcome` describe their *purpose* rather than their *structure*. A `Maybe[User]` value communicates "there might be a user" more directly than `Option<User>`. An `Outcome[File, IoError]` communicates "the outcome of a file operation" more clearly than `Result<File, IoError>`.

### 2.2.8 Smart Pointers: Purpose-Named

| TML | Rust | TML Name Rationale |
|-----|------|--------------------|
| `Heap[T]` | `Box<T>` | Describes *where* the value lives |
| `Shared[T]` | `Rc<T>` | Describes *how* ownership works |
| `Sync[T]` | `Arc<T>` | Describes *thread-safety* property |

`Box` is a metaphor. `Rc` is an abbreviation (Reference Counted). `Arc` is an abbreviation (Atomically Reference Counted). These names require domain knowledge to decode. TML's names — `Heap`, `Shared`, `Sync` — describe the semantic property directly.

### 2.2.9 Unsafe: lowlevel vs unsafe

TML uses `lowlevel` instead of `unsafe` for blocks that bypass the borrow checker and type safety guarantees:

```
lowlevel {
    let raw = mem_alloc(size)
    ptr_write(raw, value)
}
```

The word "unsafe" implies danger and irresponsibility. This framing discourages developers from using it even when it is the correct tool — for example, when implementing data structures that require pointer arithmetic. The word "lowlevel" is descriptively accurate without moral judgment: the code operates at a lower level of abstraction. This is consistent with how experienced systems programmers think about such code — not as "dangerous" but as "close to the machine."

### 2.2.10 Other Notable Decisions

**Unified loop construct:**
```
loop (condition) { body }          // while loop
for item in collection { body }    // iterator loop
for i in 0 to 10 { body }         // range loop (exclusive)
for i in 1 through 10 { body }    // range loop (inclusive)
```

The `to` and `through` keywords replace Rust's `..` and `..=` operators. "0 to 10" and "1 through 10" are immediately understandable; "0..10" and "0..=10" require knowledge of Rust's range syntax.

**Template literals:**
```
let greeting = `Hello, {name}! You have {count} messages.`
```

Template literals use backtick delimiters with `{expr}` interpolation, following the JavaScript/TypeScript convention. This is simpler than Rust's `format!("{}", name)` macro and avoids the format string mini-language.

**Let-else guards:**
```
let Just(user) = find_user(id) else { return Nothing }
let Just(email) = user.email else { return Nothing }
```

This pattern replaces deeply nested pattern matching with flat, sequential guard expressions — dramatically improving readability for error handling paths.

**Optional chaining:**
```
let name = parse(json)?.get_string("user")?.get_string("name")
```

The `?.` operator propagates `Nothing` through method chains, avoiding nested `when` expressions. This is borrowed from JavaScript/TypeScript and Kotlin, languages that have proven the ergonomic value of optional chaining.

---

## 2.3 Token Efficiency Analysis

An important practical consideration for LLM usage is token efficiency — how many tokens a given construct consumes in the LLM's context window. We compare token counts for equivalent constructs (using a BPE tokenizer approximation):

| Construct | TML | Rust | Savings |
|-----------|-----|------|---------|
| Generic function signature | `func max[T: Ord](a: T, b: T) -> T` (12 tokens) | `fn max<T: Ord>(a: T, b: T) -> T` (13 tokens) | ~8% |
| Closure in chain | `.filter(do(x) x > 0)` (8 tokens) | `.filter(\|x\| x > 0)` (9 tokens) | ~11% |
| Error propagation | `let v = try_parse()!` (6 tokens) | `let v = try_parse()?;` (7 tokens) | ~14% |
| Boolean expression | `if a and b or not c` (7 tokens) | `if a && b \|\| !c` (8-10 tokens) | ~20% |
| Reference parameter | `ref List[I32]` (4 tokens) | `&Vec<i32>` (5 tokens) | ~20% |
| Pattern match | `when x { ... }` (4 tokens) | `match x { ... }` (4 tokens) | 0% |

While individual savings are modest (8-20%), they compound across a typical function body. A 500-line module may contain hundreds of these constructs, resulting in 10-15% fewer tokens for equivalent TML code compared to Rust. Within the constrained context windows of LLMs, this efficiency translates directly to more code fitting within a single generation context.

---

## 2.4 Cognitive Load Analysis

Cognitive load in programming language syntax can be decomposed into three components:

1. **Intrinsic load**: The inherent complexity of the concept being expressed.
2. **Extraneous load**: Complexity introduced by the notation system itself.
3. **Germane load**: Effort spent building useful mental models.

TML's syntax decisions systematically reduce extraneous load:

- **Symbol overloading** increases extraneous load because the reader must determine which meaning applies. TML eliminates this by assigning each symbol a single meaning.
- **Abbreviations** increase extraneous load because they must be memorized. TML uses full words (`behavior`, `func`, `when`) instead of abbreviations (`trait`, `fn`, `match`).
- **Nested syntax** increases extraneous load because the reader must track multiple levels of structure. TML provides `let-else` and `?.` to flatten deeply nested expressions.

The trade-off is verbosity: TML code is marginally longer than equivalent Rust code. However, research in cognitive psychology suggests that readability and comprehensibility are more important than brevity for error avoidance — a finding that applies equally to human and machine code generators.

---

## 2.5 Error Recovery

TML's keyword-based syntax provides superior error messages compared to symbol-heavy syntaxes. Consider a missing closing bracket:

```
// TML error: Expected ']' to close generic parameter list starting at line 5
func sort[T: Ord(items: List[T]) -> List[T]
                 ^--- expected ']' here

// Rust equivalent: This is harder to diagnose because '<' could be comparison
fn sort<T: Ord(items: Vec<T>) -> Vec<T>
              ^--- is this a function call or a generic parameter?
```

Because `[` always opens a generic parameter list in TML (there is no other meaning), the compiler can immediately identify the error and provide a precise diagnostic. In Rust, the `<` character's dual role (comparison vs generic) means the parser must explore multiple interpretations before identifying the error, often producing less precise messages.

This property is particularly valuable for LLM-generated code: when the compiler provides clear, unambiguous error messages, the LLM can correct its output more reliably in subsequent iterations.
