# Tasks: Bootstrap Parser

## Progress: 0% (0/32 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/parser/` directory structure
- [ ] 1.2 Set up CMake configuration for parser module
- [ ] 1.3 Create base header files with include guards

## 2. AST Node Types Phase
- [ ] 2.1 Implement base `AstNode` class with span tracking
- [ ] 2.2 Implement expression node hierarchy
  - [ ] 2.2.1 Literal expressions (int, float, string, char, bool)
  - [ ] 2.2.2 Identifier expression
  - [ ] 2.2.3 Binary expression
  - [ ] 2.2.4 Unary expression (prefix and postfix)
  - [ ] 2.2.5 Call expression
  - [ ] 2.2.6 Index expression
  - [ ] 2.2.7 Field access expression
  - [ ] 2.2.8 Closure expression (do)
  - [ ] 2.2.9 If expression
  - [ ] 2.2.10 When expression (pattern matching)
  - [ ] 2.2.11 Block expression
- [ ] 2.3 Implement statement node hierarchy
  - [ ] 2.3.1 Let/Var/Const statement
  - [ ] 2.3.2 Return statement
  - [ ] 2.3.3 Break/Continue statement
  - [ ] 2.3.4 Expression statement
  - [ ] 2.3.5 Loop statement (all forms)
  - [ ] 2.3.6 If statement
  - [ ] 2.3.7 Catch block
- [ ] 2.4 Implement declaration node hierarchy
  - [ ] 2.4.1 Module declaration
  - [ ] 2.4.2 Import declaration
  - [ ] 2.4.3 Function declaration
  - [ ] 2.4.4 Type declaration (struct/enum)
  - [ ] 2.4.5 Trait declaration
  - [ ] 2.4.6 Implement declaration
  - [ ] 2.4.7 Extend declaration
- [ ] 2.5 Implement pattern node hierarchy
- [ ] 2.6 Implement type node hierarchy

## 3. Parser Core Phase
- [ ] 3.1 Implement token stream wrapper with lookahead
- [ ] 3.2 Implement `expect()` and `consume()` methods
- [ ] 3.3 Implement error reporting with context
- [ ] 3.4 Implement error recovery with synchronization

## 4. Expression Parsing Phase
- [ ] 4.1 Implement Pratt parser infrastructure
- [ ] 4.2 Define operator precedence table
- [ ] 4.3 Implement prefix expression parsing
- [ ] 4.4 Implement infix expression parsing
- [ ] 4.5 Implement postfix expression parsing (!, ?)
- [ ] 4.6 Implement call and index expression parsing
- [ ] 4.7 Implement closure parsing (do expressions)

## 5. Statement Parsing Phase
- [ ] 5.1 Implement variable declaration parsing
- [ ] 5.2 Implement control flow parsing (if, when, loop)
- [ ] 5.3 Implement return/break/continue parsing
- [ ] 5.4 Implement catch block parsing

## 6. Declaration Parsing Phase
- [ ] 6.1 Implement module/import parsing
- [ ] 6.2 Implement function declaration parsing
- [ ] 6.3 Implement type declaration parsing
- [ ] 6.4 Implement trait/implement/extend parsing
- [ ] 6.5 Implement generic parameter parsing
- [ ] 6.6 Implement where clause parsing

## 7. Pattern Parsing Phase
- [ ] 7.1 Implement literal pattern parsing
- [ ] 7.2 Implement binding pattern parsing
- [ ] 7.3 Implement struct/tuple pattern parsing
- [ ] 7.4 Implement enum variant pattern parsing
- [ ] 7.5 Implement guard expression parsing

## 8. Testing Phase
- [ ] 8.1 Write unit tests for each AST node type
- [ ] 8.2 Write unit tests for expression parsing
- [ ] 8.3 Write unit tests for statement parsing
- [ ] 8.4 Write unit tests for declaration parsing
- [ ] 8.5 Write integration tests with complete TML programs
- [ ] 8.6 Verify test coverage ≥95%

## 9. Documentation Phase
- [ ] 9.1 Document public API in header files
- [ ] 9.2 Update CHANGELOG.md with parser implementation
