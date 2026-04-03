# Proposal: Fix Codegen Module Resolution — Transitive Imports + pub use

## Why
The HTTP server, Response builder, DI module, and any code using deep module paths (4+ segments)
CANNOT compile because the codegen fails to emit `declare`/`define` for transitively imported
functions. The type checker resolves them fine but the codegen is blind to them. This blocks:
- HTTP server sample (`samples/11-http-server/server.tml`)
- Response builder using middleware modules (cors, security, etag, cache_control)
- Any library code that imports types from reorganized subdirectories
- The entire NestJS-style framework (phase0_nestjs-http-decorators)

## What Changes
1. Recursive private_imports loading in env_module_load.cpp
2. pub use re-export resolution in codegen (emit declares for re-exported symbols)
3. AST codegen struct param ABI alignment with MIR path fix

## Impact
- Affected code: compiler/src/types/env_module_load*.cpp, compiler/src/codegen/llvm/core/
- Breaking change: NO
- User benefit: Deep module imports work correctly — any reorganized library compiles
