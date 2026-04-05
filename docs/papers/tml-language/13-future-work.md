# 13. Future Work

## 13.1 Self-Hosting

The most ambitious item on TML's roadmap is **self-hosting**: rewriting the TML compiler in TML itself. Currently implemented in C++ (~100K lines), the compiler would be progressively migrated to TML, starting with the standard library (already largely in TML) and extending to the parser, type checker, and codegen.

Self-hosting serves multiple purposes:
1. **Validation**: A language mature enough to implement its own compiler demonstrates real-world capability.
2. **Bootstrapping**: Eliminates the C++ toolchain dependency for building TML.
3. **Dogfooding**: Forces the language to handle complex systems programming — parsing, data structures, code generation, file I/O.
4. **C++ elimination**: Aligns with the project's explicit goal of minimizing C and C++ code.

The migration is planned in phases:
- **Phase 1**: Standard library fully in TML (currently 93.2% complete).
- **Phase 2**: Lexer and parser in TML (requires string processing and data structures).
- **Phase 3**: Type checker in TML (requires complex data structures and algorithms).
- **Phase 4**: IR generation in TML (requires LLVM FFI bindings).
- **Phase 5**: Full self-hosting.

---

## 13.2 Async/Await

TML's async runtime is partially implemented. The language supports `async func` declarations and the `await` keyword, with async lowering as a MIR pass that transforms async function bodies into state machines. Remaining work includes:

- **Async executor**: A work-stealing thread pool for scheduling async tasks.
- **IO integration**: Async file I/O and network I/O through the existing IOCP (Windows) and epoll (Linux) infrastructure.
- **Structured concurrency**: Task groups, cancellation, and timeout propagation.
- **Async iterators**: `AsyncIterator` behavior with `async for` loops.

The design draws from Rust's async model (zero-cost futures, poll-based) while aiming for simpler ergonomics (no `Pin` complexity, automatic pinning where safe).

---

## 13.3 WebAssembly Target

TML's LLVM backend can target WebAssembly through LLVM's wasm32/wasm64 backends. However, practical WASM support requires:

- **WASI bindings**: Mapping TML's standard library I/O to WASI system calls.
- **Memory management**: Adapting the allocator for WASM's linear memory model.
- **Binary size optimization**: Aggressive dead code elimination to produce small WASM binaries.
- **JavaScript interop**: FFI bindings for calling JavaScript from TML and vice versa.

WebAssembly support would enable TML for browser-based applications, serverless functions (Cloudflare Workers, Fastly Compute), and plugin systems.

---

## 13.4 Package Manager

TML currently does not have a package manager. The standard library is monolithic and ships with the compiler. A package ecosystem would require:

- **Package format**: Likely based on the existing `.rlib` format with metadata.
- **Registry**: A central repository for published packages.
- **Dependency resolution**: Version solving with semantic versioning.
- **Build integration**: Integration with TML's existing build system.

The design will likely draw from Cargo (Rust) for ergonomics and Go modules for simplicity, with particular attention to LLM-friendly package discovery (natural language search, structured API documentation).

---

## 13.5 Formal Verification of LLM-First Design Claims

The central thesis of this paper — that keyword-based, LL(1) syntax improves LLM code generation accuracy — is supported by design reasoning and informal testing but lacks formal experimental validation. Future work should include:

1. **Controlled experiments**: Present equivalent programming tasks to LLMs in TML and Rust, measuring syntax error rates, semantic correctness, and time-to-correct-generation.
2. **Token efficiency measurement**: Precise comparison of BPE token counts for equivalent programs across languages.
3. **Error taxonomy**: Systematic classification of LLM generation errors by language, identifying which errors are syntax-induced versus semantic.
4. **Longitudinal study**: As LLM training data includes more TML code, does generation accuracy improve faster than for other languages?

The ongoing LLM IR debugging research project (`docs/papers/llm-ir-debugging/`) provides infrastructure for some of these experiments, with tool usage logging and structured data collection already in place.

---

## 13.6 Cross-Platform Maturity

TML currently targets Windows (primary development platform) with Linux and macOS as secondary targets. Future work includes:

- **Linux CI/CD**: Automated testing on Linux.
- **macOS support**: Apple Silicon (ARM64) target with Mach-O binary format.
- **Cross-compilation**: Building for one platform from another, leveraging Zig CC's cross-compilation capabilities.
- **ARM64 optimization**: Target-specific optimizations for ARM processors.

---

## 13.7 IDE Integration

TML has preliminary IDE support through its MCP server, but full IDE integration requires:

- **Language Server Protocol (LSP)**: Go-to-definition, find references, rename, code actions.
- **Semantic highlighting**: Token classification for syntax coloring based on type information.
- **Inline diagnostics**: Real-time type checking as the user types.
- **Debugger integration**: DAP (Debug Adapter Protocol) for step-through debugging with variable inspection.

The query-based compilation architecture is well-suited to LSP implementation, as individual queries can be re-evaluated incrementally when the source changes.

---

## 13.8 Advanced Type System Features

Several type system extensions are under consideration:

- **Higher-kinded types**: Behavior parameters that are type constructors (`Functor[F[_]]`).
- **GADTs** (Generalized Algebraic Data Types): Type-indexed variants for type-safe embedded DSLs.
- **Effect system**: Tracking side effects in the type system for purity analysis.
- **Dependent types** (limited): Compile-time expressions in type positions beyond const generics.

Each extension would be evaluated against the LLM-first design principle: does it improve or degrade LLM code generation accuracy? Type system features that are powerful but syntactically complex (Rust's higher-ranked trait bounds, C++ SFINAE) would be rejected in favor of simpler alternatives that achieve 80% of the expressiveness with 20% of the complexity.

---

## 13.9 Conclusion

TML represents an early exploration of a new design space: programming languages that treat LLM code generation as a first-class design constraint. The language demonstrates that it is possible to combine the safety guarantees of Rust's ownership model with a syntax that is systematically optimized for both human readability and machine generation.

The compiler's five-layer IR pipeline, 52 MIR optimization passes, and embedded LLVM backend provide performance competitive with existing systems languages. The comprehensive standard library (500+ types, 5,000+ functions) and integrated MCP tooling create an ecosystem designed from the ground up for AI-assisted development.

As LLMs become increasingly central to software development, we anticipate that the principles explored in TML — unique token meanings, LL(1) grammars, self-documenting names, structured tool interfaces — will influence the design of future programming languages, whether as new languages or as evolution of existing ones.

The field of "LLM-aware language design" is nascent. TML is one data point in what will likely become a rich area of research at the intersection of programming language theory, compiler engineering, and artificial intelligence.
