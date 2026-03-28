# Proposal: Bootstrap Parser

## Why

The parser transforms the token stream from the lexer into an Abstract Syntax Tree (AST). This is the second critical stage of the compiler pipeline. The parser must implement the complete TML grammar as specified in `docs/specs/03-GRAMMAR.md` using an LL(1) parsing strategy for predictable, efficient parsing. Without a correct parser, the compiler cannot understand the structure of TML programs.

## What Changes

### New Components

1. **AST Node Types** (`src/parser/ast.hpp`)
   - Complete hierarchy of AST node types
   - Expression nodes (literals, binary, unary, call, etc.)
   - Statement nodes (let, var, return, if, when, loop, etc.)
   - Declaration nodes (func, type, trait, implement, etc.)
   - Pattern nodes (for pattern matching)
   - Type nodes (for type annotations)

2. **Parser Core** (`src/parser/parser.hpp`, `src/parser/parser.cpp`)
   - Token stream consumption
   - LL(1) parsing with lookahead
   - Pratt parsing for expressions (precedence climbing)
   - Error recovery with synchronization points
   - Span tracking for all AST nodes

3. **Operator Precedence** (`src/parser/precedence.hpp`)
   - Precedence levels for all operators
   - Associativity rules
   - Infix/prefix/postfix handling

### Grammar Coverage

Based on `docs/specs/03-GRAMMAR.md`:
- **Declarations**: `module`, `import`, `func`, `type`, `trait`, `implement`, `extend`
- **Statements**: `let`, `var`, `const`, `return`, `break`, `continue`, `if`, `when`, `loop`, `catch`
- **Expressions**: Binary, unary, call, index, field access, closures, async/await
- **Patterns**: Literal, identifier, tuple, struct, enum variant, wildcard
- **Types**: Named, generic, function, tuple, reference, pointer

## Impact

- **Affected specs**: 03-GRAMMAR.md, 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/parser/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Enables TML source code to be parsed into AST for analysis
- **Dependencies**: Requires bootstrap-lexer to be complete

## Success Criteria

1. All grammar productions from spec are implemented
2. Operator precedence matches specification
3. Error messages include context (expected vs found)
4. Error recovery allows parsing to continue after errors
5. All AST nodes have accurate span information
6. Test coverage ≥95%
