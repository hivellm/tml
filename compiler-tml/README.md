# TML Compiler (Self-Hosting)

The self-hosting TML compiler, written in TML itself. This is the second-stage compiler being built to eventually replace the C++ bootstrap compiler. It implements the full compilation pipeline — from lexing through native code emission — in pure TML.

## Status

**Active development** on branch `feat/self-hosting-compiler`. The TML parser is the default frontend since v0.2.15; other stages are progressively being ported from C++.

| Stage | Status | Notes |
|-------|--------|-------|
| Lexer | Complete | `src/lexer/` |
| Parser | **Default frontend** | `--stage=parser:tml` (default since v0.2.15) |
| AST | Complete | Full AST types + serialization |
| Type System | Complete | Types, environments, builtins, inference, coercion |
| Type Checker | Complete | Expressions, statements, calls, patterns, exhaustiveness |
| HIR | Complete | Builder, lowering, monomorphization |
| THIR | Complete | Typed HIR with exhaustiveness checking |
| MIR | Complete | Instructions, blocks, modules, builder |
| MIR Passes | Complete | 16 optimization passes |
| Codegen (LLVM) | Complete | Full LLVM IR emission |
| Native Backend | Complete | x86-64 + AArch64, register allocation, object emission |
| Linker | Complete | PE/COFF, ELF, Mach-O + incremental linking |
| Query System | Complete | Demand-driven, incremental, cached |
| CLI | Complete | Builder, dispatcher, diagnostics |
| Formatter | In progress | `src/format/` |
| C Preprocessor | Complete | Macros, conditionals, predefined symbols |

## Architecture

```
compiler-tml/
├── src/
│   ├── main.tml                # Compiler entry point
│   ├── main_frontend.tml       # Frontend-only binary (parser stage)
│   ├── source.tml              # Source file management
│   ├── token.tml               # Token types and utilities
│   │
│   ├── lexer/                  # Tokenizer
│   │   └── lexer.tml
│   │
│   ├── parser/                 # LL(1) recursive descent parser
│   │   ├── common.tml          # Shared parser types
│   │   ├── parse_module.tml    # Top-level module parsing
│   │   ├── parse_decl.tml      # Declarations (func, type, enum, impl, etc.)
│   │   ├── parse_expr.tml      # Expressions
│   │   ├── parse_stmt.tml      # Statements
│   │   ├── parse_type.tml      # Type annotations
│   │   └── parse_pattern.tml   # Pattern matching patterns
│   │
│   ├── ast/                    # Abstract syntax tree (12 files)
│   │   ├── nodes.tml           # Core AST node types
│   │   ├── decls.tml           # Declaration nodes
│   │   ├── exprs.tml           # Expression nodes
│   │   ├── stmts.tml           # Statement nodes
│   │   ├── types.tml           # Type expression nodes
│   │   ├── patterns.tml        # Pattern nodes
│   │   ├── oop.tml             # OOP nodes (class, interface)
│   │   ├── module.tml          # Module node
│   │   ├── serial.tml          # AST binary serialization
│   │   └── ast_writer.tml      # AST pretty printer
│   │
│   ├── types/                  # Type system (21 files)
│   │   ├── ty.tml              # Type enum (16 variants)
│   │   ├── env.tml             # Type environment (scoped symbol tables)
│   │   ├── builtins.tml        # Primitive types, core enums, behaviors
│   │   ├── register.tml        # AST declaration → type registration
│   │   ├── imports.tml         # Module import resolution
│   │   ├── coercion.tml        # Type coercion rules
│   │   ├── pipeline.tml        # Type checking pipeline
│   │   ├── module.tml          # Module-level type info
│   │   ├── module_binary.tml   # Binary .tml.meta type cache
│   │   ├── module_loader.tml   # Module loading
│   │   ├── checker/            # Expression/statement/call/pattern checking
│   │   ├── behaviors/          # Behavior (trait) dispatch and solving
│   │   └── infer/              # Type inference and unification
│   │
│   ├── hir/                    # High-level IR (9 files)
│   │   ├── builder.tml         # HIR construction from AST
│   │   ├── lower_expr.tml      # Expression lowering
│   │   ├── monomorph.tml       # Generic monomorphization
│   │   ├── pattern.tml         # Pattern compilation
│   │   └── printer.tml         # HIR pretty printer
│   │
│   ├── thir/                   # Typed HIR (7 files)
│   │   ├── lower.tml           # THIR lowering from HIR
│   │   ├── exhaustiveness.tml  # Pattern exhaustiveness checking
│   │   ├── expr.tml            # Typed expressions
│   │   ├── pattern.tml         # Typed patterns
│   │   └── stmt.tml            # Typed statements
│   │
│   ├── mir/                    # Mid-level IR (SSA form)
│   │   ├── inst.tml            # MIR instructions
│   │   ├── block.tml           # Basic blocks
│   │   ├── module.tml          # MIR module
│   │   ├── builder/core.tml    # MIR builder
│   │   ├── printer.tml         # MIR pretty printer
│   │   ├── types.tml           # MIR type system
│   │   └── passes/             # 16 optimization passes
│   │       ├── mem2reg.tml         # Alloca → SSA promotion
│   │       ├── const_fold.tml      # Constant folding
│   │       ├── const_prop.tml      # Constant propagation
│   │       ├── copy_prop.tml       # Copy propagation
│   │       ├── dce.tml             # Dead code elimination
│   │       ├── dfe.tml             # Dead function elimination
│   │       ├── sroa.tml            # Scalar replacement of aggregates
│   │       ├── inlining.tml        # Function inlining
│   │       ├── devirtualization.tml # Virtual → direct call conversion
│   │       ├── escape_analysis.tml  # Heap → stack promotion
│   │       ├── licm.tml            # Loop-invariant code motion
│   │       ├── tail_call.tml       # Tail call optimization
│   │       ├── rvo.tml             # Return value optimization
│   │       ├── block_merge.tml     # Basic block merging
│   │       ├── simplify_cfg.tml    # Control flow simplification
│   │       ├── uce.tml             # Unreachable code elimination
│   │       └── inst_simplify.tml   # Instruction simplification
│   │
│   ├── codegen/                # LLVM IR emission (16 files)
│   │   ├── emit_module.tml     # Module-level emission
│   │   ├── emit_func.tml       # Function emission
│   │   ├── emit_inst.tml       # Instruction emission
│   │   ├── emit_call.tml       # Call emission
│   │   ├── emit_method.tml     # Method dispatch emission
│   │   ├── emit_type.tml       # Type emission
│   │   ├── emit_generic.tml    # Generic instantiation
│   │   ├── emit_derive.tml     # @derive/@auto code generation
│   │   ├── emit_drop.tml       # Destructor/drop glue
│   │   ├── emit_let.tml        # Let binding emission
│   │   ├── emit_intrinsic.tml  # Compiler intrinsics
│   │   ├── layout.tml          # Type layout (size, alignment)
│   │   ├── runtime_decls.tml   # Runtime function declarations
│   │   └── types.tml           # LLVM type mapping
│   │
│   ├── native/                 # Native code backend (27 files)
│   │   ├── machir.tml          # Machine IR
│   │   ├── mir_lower.tml       # MIR → Machine IR lowering
│   │   ├── linear_scan.tml     # Register allocation
│   │   ├── liveness.tml        # Liveness analysis
│   │   ├── stack_alloc.tml     # Stack frame layout
│   │   ├── name_mangle.tml     # Symbol name mangling
│   │   ├── obj_writer.tml      # Object file writer
│   │   ├── pipeline.tml        # Native compilation pipeline
│   │   ├── x86/               # x86-64 backend
│   │   │   ├── encode.tml      # Instruction encoding
│   │   │   ├── emit.tml        # Code emission
│   │   │   ├── sse.tml         # SSE/SIMD
│   │   │   ├── peephole.tml    # Peephole optimizations
│   │   │   ├── frame.tml       # Stack frames
│   │   │   ├── debug_info.tml  # DWARF generation
│   │   │   └── calling_conv.tml # Calling conventions
│   │   └── aarch64/           # AArch64 backend
│   │       ├── encode.tml      # Instruction encoding
│   │       ├── elf_emit.tml    # ELF emission
│   │       ├── macho_emit.tml  # Mach-O emission
│   │       ├── regs.tml        # Register definitions
│   │       └── calling_conv.tml # Calling conventions
│   │
│   ├── link/                   # Linker (16 files)
│   │   ├── pe/                 # PE/COFF (Windows)
│   │   ├── elf/                # ELF (Linux)
│   │   ├── macho/              # Mach-O (macOS, with code signing)
│   │   └── incr/               # Incremental linking
│   │
│   ├── query/                  # Query system (5 files)
│   │   ├── context.tml         # Query context
│   │   ├── cache.tml           # Query result caching
│   │   ├── key.tml             # Query keys
│   │   └── incremental.tml     # Red-green incremental
│   │
│   ├── serial/                 # Binary serialization (5 files)
│   │   ├── reader.tml          # Binary reader
│   │   ├── writer.tml          # Binary writer
│   │   ├── ast.tml             # AST serialization
│   │   └── typeenv.tml         # Type env serialization
│   │
│   ├── cli/                    # CLI frontend
│   │   ├── dispatcher.tml      # Command dispatch
│   │   ├── builder.tml         # Build orchestration
│   │   └── diagnostic.tml      # Error formatting
│   │
│   ├── cc/preproc/             # C preprocessor (5 files)
│   ├── format/                 # Code formatter
│   └── testing/                # Test framework integration
│
└── tests/                      # Test suite (65 test files)
    ├── ast/                    # AST construction and roundtrip
    ├── bootstrap/              # Bootstrap verification
    ├── cli/                    # CLI integration
    ├── codegen/                # IR emission tests
    ├── hir/                    # HIR type tests
    ├── lexer/                  # Tokenizer tests
    ├── mir/                    # MIR optimization tests
    ├── native/                 # Native backend tests (x86, AArch64, COFF, ELF, Mach-O)
    ├── parser/                 # Parser tests
    ├── query/                  # Query system tests
    ├── serial/                 # Serialization roundtrip tests
    ├── source/                 # Source management tests
    ├── thir/                   # THIR type tests
    ├── token/                  # Token utility tests
    └── types/                  # Type system tests (unification, inference, dispatch)
```

**170 source files, 65 test files** across all stages.

## Self-Hosting Strategy

The self-hosting compiler is being built incrementally, stage by stage:

1. **Parser** (complete, default since v0.2.15) — TML parses itself, C++ fallback via `--stage=parser:cpp`
2. **Type checker** (complete) — full type inference, unification, exhaustiveness checking
3. **MIR pipeline** (complete) — 16 optimization passes in pure TML
4. **Codegen** (complete) — LLVM IR emission from MIR
5. **Native backend** (complete) — x86-64 + AArch64, register allocation, object emission
6. **Linker** (complete) — PE/COFF, ELF, Mach-O with incremental linking support
7. **Full bootstrap** — compile the compiler with itself (in progress)

Each stage is verified against the C++ bootstrap compiler using differential testing and the Rust-as-Reference IR methodology (ADR-005).

## Running Tests

```bash
# Run all compiler-tml tests
tml test --suite=compiler-tml

# Run a specific test file
tml test compiler-tml/tests/parser/parse_path_only.test.tml

# Run with debug layers (IR + MIR output on failure)
tml test compiler-tml/tests/codegen/layout.test.tml --debug-layers
```

## Key Design Decisions

- **No C/C++ in the self-hosting compiler** — pure TML with `@extern("c")` FFI only for LLVM API calls
- **Index-based storage** — `List[T]` + `HashMap[Str, I64]` pattern avoids codegen bugs with complex generic values (K001)
- **Named result structs** — all parser functions return `ParsedX` structs, never tuples (avoids K001 tuple codegen bug)
- **`common.tml` as module root** — `mod` is a reserved keyword in TML, so module root files are named `common.tml`
- **Type named `ty.tml`** — `type` is a reserved keyword in TML

## License

Apache-2.0
