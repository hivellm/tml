# 10. LLM-First Language Design

## 10.1 The Thesis

Programming languages have been designed exclusively for human programmers for over seven decades. From FORTRAN (1957) to Rust (2015), every language design decision has optimized for human cognitive processes: reading speed, typing convenience, visual pattern recognition, and cultural familiarity.

The emergence of Large Language Models as code generators — GitHub Copilot, Claude, GPT-4, and their successors — introduces a fundamentally new consumer of programming language syntax. LLMs process code through subword tokenization, generate tokens autoregressively with no backtracking, and lack the contextual reasoning that allows humans to disambiguate overloaded symbols. These differences create a design space for programming languages that are optimized for both human and machine code generation.

TML (To Machine Language) is, to our knowledge, the first programming language explicitly designed with LLM code generation as a primary design constraint. This section describes the specific design decisions that arise from treating LLM optimization as a first-class concern.

---

## 10.2 The Problem: Symbol Ambiguity in LLM Generation

LLMs trained on multi-language corpora encounter persistent ambiguity from context-dependent symbols. Consider the character `<`:

| Context | Meaning | Language |
|---------|---------|----------|
| `a < b` | Less-than comparison | All languages |
| `Vec<T>` | Generic type parameter | Rust, C++, Java, TypeScript |
| `<div>` | HTML tag opener | HTML, JSX |
| `<<` | Left bit shift | C, C++, Rust |
| `<<<` | Heredoc | Bash, PHP |
| `<-` | Channel receive | Go |

An LLM generating code must determine which meaning applies from surrounding context. This is a classification problem that the model solves probabilistically — and probabilistic solutions have nonzero error rates. When the model generates `Vec<HashMap<K, V>>` and the tokenizer splits this into tokens including `>>`, the model must have learned that `>>` in this context is two closing angle brackets, not a right-shift operator.

TML eliminates this entire class of error. The character `<` has exactly one meaning: less-than comparison. Generic parameters use `[T]`, which is unambiguous in all contexts.

### 10.2.1 Ambiguity Inventory

We catalog the symbols that cause the most LLM generation errors, and TML's resolution for each:

| Symbol | Ambiguities (in other languages) | TML Resolution |
|--------|----------------------------------|----------------|
| `<` / `>` | Comparison, generics, HTML, bit shift, heredoc, channel | `<` `>` = comparison only; generics use `[` `]` |
| `\|` | Bitwise OR, closure delimiter, match arm, markdown table | `\|` = bitwise OR only; closures use `do()` |
| `!` | Logical NOT, macro invocation, unwrap operator | `not` = logical NOT; no macros; `!` = error propagation |
| `&` | Reference, bitwise AND, address-of, logical AND | `ref` = reference; `and` = logical AND; `&` = bitwise AND |
| `?` | Error propagation, ternary, optional type, regex | `!` = error propagation; `?.` = optional chaining |
| `::` | Path separator, turbofish, associated type | `::` = path only (no turbofish needed with `[T]`) |
| `..` / `...` | Range, spread, rest params, variadic | `to` / `through` = ranges; no spread operator |
| `#` | Attribute, macro, comment, preprocessor | `@` = decorators; `//` = comment; `#if` = conditional compilation |
| `->` | Return type, closure return, match arm | `->` = return type and match arm (consistent) |

TML reduces the total number of context-dependent symbols from 24+ (in Rust) to approximately 5. Each remaining multi-use symbol (`=`, `.`, `,`, `:`, `;`) has meanings that are structurally unambiguous (determined by position, not by type context).

---

## 10.3 Token Efficiency

LLMs operate within fixed context windows (4K to 1M tokens). Every token consumed by boilerplate syntax is a token unavailable for program logic. TML's keyword-based syntax is designed to be token-efficient despite using longer identifiers.

### 10.3.1 Tokenizer Behavior

Modern LLM tokenizers (BPE, SentencePiece) treat common English words as single tokens but may split symbolic sequences into multiple tokens:

| Expression | Approximate Tokens |
|-----------|-------------------|
| `and` | 1 token |
| `&&` | 1-2 tokens (depends on tokenizer) |
| `behavior` | 1-2 tokens |
| `trait` | 1 token |
| `do(x)` | 3 tokens (`do`, `(`, `x)`) |
| `\|x\|` | 3 tokens (`\|`, `x`, `\|`) |
| `Maybe[I32]` | 4 tokens |
| `Option<i32>` | 4-5 tokens |

The token counts are approximately equal for individual constructs. However, TML gains efficiency through:

1. **No macro syntax**: Rust's `println!("{}", x)` involves macro invocation, format string parsing, and argument matching. TML's template literal approach is simpler.
2. **No lifetime annotations**: `'a`, `'b`, `'static` each consume tokens. TML has none.
3. **No turbofish**: Rust's `collect::<Vec<_>>()` requires the turbofish operator for type disambiguation. TML's `collect[List[I32]]()` or simple `collect()` with inference avoids this.
4. **Simpler attribute syntax**: `@auto(debug, equal)` vs `#[derive(Debug, PartialEq, Eq)]`.

### 10.3.2 Compound Savings

For a typical function implementing a sorted insertion into a collection:

```
// TML (~45 tokens)
func insert_sorted[T: Ordered](list: mut ref List[T], item: T) {
    let pos = list.iter().position(do(x) x.compare(ref item) == Ordering.Greater)
    when pos {
        Just(i) -> list.insert(i, item),
        Nothing -> list.push(item),
    }
}
```

```rust
// Rust (~55 tokens)
fn insert_sorted<T: Ord>(list: &mut Vec<T>, item: T) {
    let pos = list.iter().position(|x| x.cmp(&item) == Ordering::Greater);
    match pos {
        Some(i) => list.insert(i, item),
        None => list.push(item),
    }
}
```

The TML version saves approximately 18% tokens for equivalent semantics. Across a 1000-line module, this compounds to meaningful context window savings.

---

## 10.4 LL(1) Grammar and Autoregressive Generation

TML's grammar is LL(1): a single token of lookahead is sufficient to determine the applicable production rule. This property is architecturally aligned with how LLMs generate code.

LLMs generate tokens autoregressively — each token is chosen based on all preceding tokens, with no ability to backtrack and revise earlier choices. This is structurally identical to LL(1) parsing, where each production is determined by the current token and the parser state.

An LL(1) grammar means that the "correct" next token is always determinable from local context. The LLM does not need to generate speculative sequences that might later prove syntactically invalid (a problem with C++ templates, where `>>` might need to be `> >` depending on nesting depth that was determined many tokens ago).

### 10.4.1 Practical Implications

| Property | Impact on LLM Generation |
|----------|-------------------------|
| No backtracking needed | Each generated token is definitively correct given the context |
| Unique token meanings | No need to resolve ambiguity through distant context |
| Keyword-initiated blocks | `func`, `type`, `when`, `loop` unambiguously start their constructs |
| No macro expansion | Generated code is the actual code — no hidden transformations |
| Mandatory return | No confusion about implicit vs explicit returns |

### 10.4.2 Comparison with Other Grammars

| Language | Grammar Class | LLM-Problematic Constructs |
|----------|--------------|---------------------------|
| TML | LL(1) | None by design |
| Rust | Context-sensitive (turbofish, lifetimes) | `<>` ambiguity, `'a` lifetimes, macro syntax |
| C++ | Context-sensitive (templates, dependent names) | `>>` in templates, `typename`, SFINAE |
| Go | LL(1) with minor exceptions | Mostly clean; implicit semicolons can confuse |
| Python | LL(1) with indent sensitivity | Significant whitespace, implicit line joining |

---

## 10.5 Self-Documenting Identifiers

TML systematically prefers names that describe intent over names that are abbreviated or metaphorical:

| TML | Alternative | Why TML's Name is Better for LLMs |
|-----|------------|-----------------------------------|
| `behavior` | `trait` | LLM training data contains "behavior" in many contexts — the word's meaning is clear |
| `when` | `match` | "when X is Y" reads as natural English — LLMs are trained extensively on English |
| `Maybe[T]` | `Option<T>` | "maybe there's a value" is immediately comprehensible |
| `Outcome[T,E]` | `Result<T,E>` | "the outcome of an operation" is unambiguous |
| `Just(x)` | `Some(x)` | "just this value" is affirmative and clear |
| `Nothing` | `None` | "nothing" is universally understood |
| `Heap[T]` | `Box<T>` | "on the heap" describes memory location |
| `Shared[T]` | `Rc<T>` | "shared ownership" describes the semantic |
| `Sync[T]` | `Arc<T>` | "thread-synchronized" describes the guarantee |
| `Duplicate` | `Clone` | "duplicate this value" describes the action |
| `lowlevel` | `unsafe` | "low-level code" is descriptive, not judgmental |

When an LLM encounters `Shared[T]` in training data or generation context, the word "shared" activates semantic associations related to shared access, multiple owners, and reference semantics — which is exactly what the type provides. When it encounters `Rc<T>`, it must have specifically learned that "Rc" is an abbreviation for "Reference Counted" — a fact that exists only in Rust's documentation.

---

## 10.6 MCP Tooling for LLM-Assisted Development

TML provides a complete Model Context Protocol (MCP) server that exposes compiler operations as structured tool calls. This enables LLM agents to interact with the compiler programmatically rather than parsing text output.

### 10.6.1 Available Tools

| Category | Tools | Purpose |
|----------|-------|---------|
| Compilation | `compile`, `build`, `run`, `check` | Build and execute TML code |
| Testing | `test` (with `structured`, `debug_layers`, `coverage`) | Run tests with machine-readable output |
| Diagnostics | `emit-ir`, `emit-mir`, `explain` | Inspect compiler intermediates |
| Documentation | `docs_search`, `docs_get`, `docs_list`, `docs_resolve` | Query 5000+ documented APIs |
| Quality | `format`, `lint`, `cache_invalidate` | Code quality tools |
| Debug | `debug` (with `check_leaks`, `backtrace`), `profile` | Runtime debugging |

### 10.6.2 Debug Layers: Multi-IR Diagnosis

The `debug_layers` feature is unique to TML. When a test fails, the LLM agent can request all intermediate representations for the failing function:

1. **HIR output**: Shows the type-resolved, desugared code. If the HIR is wrong, the bug is in the type checker or HIR builder.
2. **MIR output**: Shows the SSA-form basic blocks. If the MIR is wrong but the HIR is correct, the bug is in the MIR builder.
3. **LLVM IR output**: Shows the generated machine-level IR. If the LLVM IR is wrong but the MIR is correct, the bug is in the codegen.
4. **Diagnosis hints**: The compiler includes textual hints about likely error sources based on the IR patterns.

This transforms compiler debugging from an opaque "something went wrong" into a structured "the error is at this specific compilation layer" — information that an LLM agent can act on directly.

### 10.6.3 Research Data Collection

Every MCP tool invocation is logged to `mcp-call-log.jsonl` with:
- Tool name and parameters
- Duration in milliseconds
- Session identifier
- Sequence number

This data enables research into how LLMs use compiler tools — which tools are most effective, what patterns lead to successful debugging, and how tool usage correlates with code quality. A companion research project (`docs/papers/llm-ir-debugging/`) analyzes this data to improve both the tools and the LLM's debugging strategies.

---

## 10.7 Implications for Language Design

TML's approach suggests several principles for future programming language design in the LLM era:

### 10.7.1 Principle: Minimize Symbol Overloading

Every context-dependent symbol is a classification problem for the LLM. Languages designed for LLM generation should aim for a bijection between tokens and meanings.

### 10.7.2 Principle: Prefer Keywords to Symbols

Keywords benefit from LLM training data: the word "behavior" appears in millions of English texts with consistent meaning. The symbol `trait` must be learned specifically from programming contexts. Keywords also compress better in BPE tokenizers.

### 10.7.3 Principle: Make Grammar Align with Generation Model

LLMs generate left-to-right with no backtracking. LL(1) grammars align naturally with this generation model. Context-sensitive grammars require the model to maintain long-range dependencies that increase error probability.

### 10.7.4 Principle: Provide Structured Tool Interfaces

Raw text output from compilers is difficult for LLMs to parse reliably. Structured interfaces (MCP, LSP, JSON output) enable LLMs to interact with development tools programmatically, reducing the parsing burden and enabling systematic debugging.

### 10.7.5 Principle: Name Things for Transfer Learning

LLM knowledge transfers across domains. A type named `Maybe` activates associations from English ("maybe it exists, maybe it doesn't") that transfer directly to the programming concept. A type named `Option` activates associations from multiple domains (stock options, configuration options, menu options) that may not transfer.

---

## 10.8 Limitations and Open Questions

TML's LLM-first design is based on hypotheses about LLM behavior that are supported by informal testing but not yet formally validated. Key open questions include:

1. **Quantitative accuracy improvement**: Does keyword-based syntax measurably reduce LLM generation errors? By how much? Controlled experiments comparing LLM performance on equivalent TML and Rust tasks would provide definitive data.

2. **Training data bias**: Current LLMs are trained primarily on existing languages. A language designed for LLMs may paradoxically perform worse simply because LLMs have less training data for it. This bootstrapping problem may resolve as TML codebases grow.

3. **Verbosity trade-off**: TML code is slightly more verbose than Rust. Does the clarity advantage outweigh the additional tokens? This may depend on context window size — as context windows grow, token efficiency becomes less critical.

4. **Human ergonomics**: TML is designed for both humans and LLMs. Some design decisions that help LLMs (no implicit returns, mandatory type annotations in some contexts) add friction for human programmers. The optimal balance is an empirical question.

5. **Generalization**: Do TML's principles transfer to other language designs? Could an existing language (Rust, Go) adopt LLM-friendly syntax without breaking backward compatibility?

These questions motivate ongoing research, including the LLM IR debugging study described in the companion paper (`docs/papers/llm-ir-debugging/`).
