# Tasks: Fix Codegen Module Resolution — Transitive Imports + pub use

**Status**: Planning. 0% (0/18).
**Blocker for**: HTTP server, Response builder, NestJS framework, any deep module import
**Reproducer**: `tml run samples/11-http-server/server.tml` → crashes with missing declares

## Bug 1: Private Imports Not Loaded Recursively (8 items)

When module A imports module B that imports module C, C's functions don't get
`declare` statements in the IR. The type checker finds them but the codegen doesn't.

**Root cause**: `env_module_load.cpp` loads private_imports for directly imported modules
but NOT for their transitive dependencies. The binary cache path and GlobalModuleCache
path DO load private imports, but the file-loading path only goes 1 level deep.

- [ ] 1.1 `env_module_load.cpp` — make private_imports loading recursive (BFS or DFS with visited set)
- [ ] 1.2 Add visited set to prevent circular import loops (A→B→A)
- [ ] 1.3 Limit recursion depth to 10 (safety — no real app has deeper chains)
- [ ] 1.4 Test: A imports B imports C → C's functions get `declare` in A's IR
- [ ] 1.5 Test: A imports B imports C imports D → D's functions available
- [ ] 1.6 Test: circular A→B→A doesn't infinite loop
- [ ] 1.7 Verify: `samples/11-http-server/server.tml` compiles (Response uses cache_control, security, etag)
- [ ] 1.8 Verify: all `std/http` tests still pass (161/161)

## Bug 2: pub use Re-Exports Invisible to Codegen (5 items)

`pub use std::http::router::router::{node_new}` in `router/mod.tml` makes `node_new`
visible to the type checker at `std::http::router::node_new`, but the codegen doesn't
emit a `declare`/`define` for it. The call site references `@tml_node_new` which LLVM
can't find, so it infers `i32` return type.

**Root cause**: The codegen scans `env_.module_registry()->get_all_modules()` and emits
declares for functions in each module's `functions` map. But `pub use` re-exports add
symbols to the PARENT module's namespace without adding them to the module's `functions` map.

- [ ] 2.1 `runtime_modules_tml.cpp` — when emitting declares, also resolve `pub use` re-exports
- [ ] 2.2 Module registry: store re-exported function signatures under the re-exporting module
- [ ] 2.3 OR: during codegen first pass, collect ALL referenced function names and emit declares for each
- [ ] 2.4 Test: `pub use` re-exported function gets correct `declare` with correct return type
- [ ] 2.5 Test: `router/mod.tml` re-exports `node_new` → call site gets `call i64` not `call i32`

## Bug 3: AST Path Struct Param ABI (3 items)

The MIR codegen path was fixed (struct params → ptr), but the AST codegen path
in `func.cpp` may still generate by-value struct params for some cases.

- [ ] 3.1 `compiler/src/codegen/llvm/decl/func.cpp` — audit ALL struct param generation
- [ ] 3.2 Ensure ALL `%struct.*` params become `ptr` in function signatures (AST path)
- [ ] 3.3 Test: free function with struct param compiles via AST path without segfault

## Bug 4: Response Builder Restore (2 items)

After fixing bugs 1-3, restore the Response builder to use middleware modules properly.

- [ ] 4.1 Revert `response_builder.tml` to version that imports cors, security, etag, cache_control
- [ ] 4.2 Verify: `res.json(data)` internally calls `cors_headers()`, `security_headers()`, etc.

## Success Criteria

```bash
# All must pass:
tml run samples/11-http-server/server.tml
# → prints "TML HTTP Server on http://localhost:3000"
# → curl http://localhost:3000/api/users returns JSON with CORS + Security + ETag headers

tml test --suite std/http --no-cache
# → 161/161 pass

tml test --suite core/str --no-cache
# → 25/25 pass
```

## Key Files

| File | What to Change |
|------|---------------|
| `compiler/src/types/env_module_load.cpp` | Recursive private_imports loading |
| `compiler/src/types/env_module_loading.cpp` | Native module loading (may also need recursion) |
| `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp` | Emit declares for pub use re-exports |
| `compiler/src/codegen/llvm/core/generate.cpp` | First pass — collect all referenced symbols |
| `compiler/src/codegen/llvm/decl/func.cpp` | AST path struct param → ptr |
| `lib/std/src/http/framework/response_builder.tml` | Restore middleware imports after fix |
