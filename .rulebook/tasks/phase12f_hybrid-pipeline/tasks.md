# Tasks: Hybrid Pipeline Framework — C++/TML Stage Swapping

**Status**: Planned (0/18)
**Depends on**: phase12e (AST & TypeEnv serializers), phase12d (IR-diff tool)
**Blocks**: Era 1 Phase 1 (lexer/parser porting — requires hybrid pipeline to integrate TML stages)
**Duration**: 2–3 weeks
**Risk**: Medium
**Purpose**: Allow individual compiler stages to be swapped from C++ to TML implementations at runtime via CLI flag, enabling incremental self-hosting without breaking the existing pipeline

---

## Phase 1: CLI Interface (3 items)

- [ ] 1.1 `compiler/src/cli/dispatcher.cpp` — Add `--stage=<name>:tml` flag parsing alongside existing `--emit-pipeline` and `--no-thir` flags (e.g., `--stage=lexer:tml` overrides the lexer stage)
- [ ] 1.2 Define canonical stage names matching `QueryContext` query kinds: `lexer` (`TokenizeKey`), `parser` (`ParseModuleKey`), `typechecker` (`TypecheckModuleKey`), `hir` (`HirLowerKey`), `mir` (`MirBuildKey`), `codegen` (`CodegenUnitKey`)
- [ ] 1.3 Store stage overrides as `std::map<std::string, StageImpl>` in build `Options` struct — pass through to `QueryContext` at construction time

## Phase 2: Stage Interface Contract (4 items)

- [ ] 2.1 Define input/output binary format for each stage boundary: lexer takes UTF-8 source bytes, outputs serialized token list; all subsequent stages use AST binary format from phase12e
- [ ] 2.2 Lexer stage contract: input = source text on stdin (or path arg), output = serialized `TokenizeResult` on stdout using MIR serializer format pattern (magic + version + length-prefixed token list)
- [ ] 2.3 Parser stage contract: input = serialized token list on stdin, output = serialized `Module` AST (phase12e format) on stdout
- [ ] 2.4 Type checker stage contract: input = serialized `Module` AST on stdin, output = serialized `TypeEnv` (phase12e format) on stdout

## Phase 3: TML Stage Launcher (4 items)

- [ ] 3.1 Create `compiler/src/pipeline/tml_stage.cpp` — `TmlStage` class that compiles a TML stage source file (via internal `QueryContext`) and caches the resulting executable path
- [ ] 3.2 Implement compilation caching: check `.incr-cache/stages/<stage_name>.exe` fingerprint against stage source file — recompile only when source changes
- [ ] 3.3 Implement execution: launch cached TML stage executable as subprocess, write serialized input to stdin, read serialized output from stdout, capture stderr for diagnostics
- [ ] 3.4 Implement deserialization: after subprocess exits, deserialize stdout bytes back to the appropriate C++ type (`TokenizeResult`, `Module`, `TypeEnv`) using phase12e readers

## Phase 4: Query Pipeline Integration (3 items)

- [ ] 4.1 `compiler/src/query/query_context.cpp` — check stage override map before executing each `force<>()` call; if override is set, delegate to `TmlStage::run()` instead of built-in implementation
- [ ] 4.2 For overridden stage: serialize C++ input using phase12e writers → call `TmlStage::run()` → deserialize output → store in query cache as if computed normally → continue C++ pipeline
- [ ] 4.3 Cache compiled TML stage binaries in `.incr-cache/stages/` — invalidate via `mcp__tml__cache_invalidate` when stage source changes; do not delete on normal `--no-cache` runs (stage cache is separate from test cache)

## Phase 5: Integration Tests (4 items)

- [ ] 5.1 Write minimal TML lexer stage at `tools/stages/lexer_stage.tml` — reads source file path from argv, tokenizes using `std::lex` module, serializes token list to stdout in stage contract format
- [ ] 5.2 Test: `tml build --stage=lexer:tml .sandbox/hello.tml` compiles successfully, executable produces correct output matching pure C++ build
- [ ] 5.3 IR-diff (using phase12d tool): compare LLVM IR from pure C++ pipeline vs hybrid pipeline with TML lexer stage — output must be bitwise identical
- [ ] 5.4 Performance: measure per-file overhead of stage subprocess launch + serialization round-trip — must be less than 5% of total compile time for files over 100 lines

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
