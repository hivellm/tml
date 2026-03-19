#!/bin/bash
# Hook: Re-inject critical architectural context after context compaction
# This runs when SessionStart fires with "compact" matcher
# Output is injected into Claude's context as a system message

cat <<'CONTEXT'
## Re-injected Context (post-compaction)

### Compiler Pipeline (quick reference)
Source → Lexer → Parser(AST) → TypeChecker(TypeEnv) → BorrowChecker → HIR(HirModule) → THIR(ThirModule) → MIR(mir::Module) → MIR Passes → MIR Codegen(LLVM IR text) → LLVM Backend(.obj) → LLD(.exe)

### ⚠️ Critical Reminders
- MIR has TWO paths: HIR→MIR (legacy) and THIR→MIR (new) — changes often need BOTH
- Method calls in MIR path use CallInst (NOT MethodCallInst) — fix in emit_call_inst
- tml_compiler.dll = all compiler code; tml_codegen_x86.dll = only IR→obj+LLD
- NEVER use cmake directly — use scripts/build.bat
- NEVER delete caches or manipulate git without authorization
- Use MCP tools (mcp__tml__test, etc.) NOT bash for TML operations
- Check docs/readme.md before writing TML code (500+ types, 5000+ functions exist)
- Use Teams for 2+ parallel agents

### Key Files for Codegen
- instructions.cpp → emit_call_inst (ALL calls including methods)
- mir_codegen.cpp → MirCodegen::generate()
- mir_types.cpp → MIR type → LLVM type string mapping
- hir_builder.cpp → monomorphization + desugaring
- thir_lower.cpp → coercion insertion + method resolution

### Library Quick Reference
- Text (string building), Buffer (bytes), List[T], HashMap[K,V]
- Maybe[T] (Option), Outcome[T,E] (Result), Heap[T], Shared[T], Sync[T]
- Mutex[T], Arc[T], Iterator, Display, Debug
CONTEXT
