# Tasks: Modular Plugin Architecture

**Status**: In Progress (95%)

## 1. Phase 0: Infrastructure

- [x] 1.1 Create `compiler/include/plugin/module.hpp` with `TML_MODULE(name)` macro
- [x] 1.2 Create `compiler/include/plugin/abi.h` with Plugin ABI (pure C)
- [x] 1.3 Embed `zstddeclib.c` (decoder-only) in `compiler/third_party/zstd/`
- [x] 1.4 Create `compiler/include/plugin/loader.hpp` (interface)
- [x] 1.5 Create `compiler/src/plugin/loader.cpp` (LoadLibrary/dlopen + zstd decompress + cache)
- [x] 1.6 Create `compiler/cmake/collect_modules.cmake` (scans TML_MODULE in .cpp files)
- [x] 1.7 Add `TML_BUILD_MODULAR` option to CMakeLists.txt (default OFF)
- [x] 1.8 Test plugin_loader builds successfully (both modular and monolithic modes)

## 2. Phase 1: Annotate Sources

- [x] 2.1 Add `TML_MODULE("compiler")` to frontend sources (lexer, parser, preprocessor)
- [x] 2.2 Add `TML_MODULE("compiler")` to analysis sources (types, borrow, traits)
- [x] 2.3 Add `TML_MODULE("compiler")` to IR sources (hir, thir, mir, ir)
- [x] 2.4 Add `TML_MODULE("compiler")` to infra sources (query, common, log, profiler, json, cli/builder, cli/commands)
- [x] 2.5 Add `TML_MODULE("codegen_x86")` to LLVM codegen sources
- [x] 2.6 Add `TML_MODULE("codegen_cranelift")` to Cranelift sources
- [x] 2.7 Add `TML_MODULE("tools")` to tool sources (format, doc, search, cli/linter)
- [x] 2.8 Add `TML_MODULE("test")` to test runner sources (cli/tester)
- [x] 2.9 Add `TML_MODULE("mcp")` to MCP sources
- [x] 2.10 Add force-include of `plugin/module.hpp` in CMakeLists.txt
- [x] 2.11 Verify collect_modules.cmake groups sources correctly

## 3. Phase 2: Extract tml_codegen_x86.dll

- [x] 3.1 Create SHARED target `tml_codegen_x86` in CMake with LLVM/LLD linked
- [x] 3.2 Create `compiler/src/plugin/llvm_plugin.cpp` with plugin C entry points
- [x] 3.3 Create `compiler/include/plugin/codegen_api.h` with C API typedefs
- [x] 3.4 Test `tml_codegen_x86.dll` builds successfully (~75MB debug)
- [ ] 3.5 Route `create_backend()` via plugin loader (deferred to Phase 7)
- [ ] 3.6 Route `object_compiler.cpp` via plugin C API (deferred to Phase 7)

## 4. Phase 3: Thin Launcher

- [x] 4.1 Create `compiler/src/launcher/main_launcher.cpp` (367KB)
- [x] 4.2 Implement command → plugin mapping
- [x] 4.3 Create `compiler/src/plugin/compiler_plugin.cpp` with `compiler_main()`
- [x] 4.4 Create SHARED target `tml_compiler` in CMake
- [x] 4.5 Add `get_symbol()` to Loader for dynamic function lookup
- [x] 4.6 Test `tml_modular --help` without loading any plugin
- [x] 4.7 Test `tml_modular run` via compiler plugin

## 5. Phase 4: Auxiliary Modules

- [x] 5.1 Create SHARED target `tml_tools` with formatter + linter + doc + search
- [x] 5.2 Create `compiler/src/plugin/tools_plugin.cpp` with plugin ABI exports
- [x] 5.3 Create SHARED target `tml_test` with test runner + coverage
- [x] 5.4 Create `compiler/src/plugin/test_plugin.cpp` with plugin ABI exports
- [x] 5.5 Create SHARED target `tml_mcp` with MCP server
- [x] 5.6 Create `compiler/src/plugin/mcp_plugin.cpp` with plugin ABI exports
- [x] 5.7 Add `add_tml_plugin()` CMake macro for DRY plugin targets
- [x] 5.8 Test all auxiliary plugins build successfully

## 6. Phase 5: TML Tooling

- [x] 6.1 Write `tools/plugin_pack.tml` (compress DLLs + generate manifest.json)
- [x] 6.2 Test plugin_pack.tml compiles and runs
- [x] 6.3 Write `tools/plugin_verify.tml` (verify integrity via SHA256)
- [x] 6.4 Test plugin_verify.tml compiles and runs
- [x] 6.5 Integrate `--pack` flag into `scripts/build.bat`
- [x] 6.6 Test `scripts\build.bat --modular --pack` flag logic

## 7. Phase 6: Polish

- [x] 7.1 Implement smart cache with CRC32 hash check (avoid repeated decompression)
- [x] 7.2 Clear error messages when plugin not found (search paths + fix hints)
- [x] 7.3 ABI versioning (reject plugins with incompatible ABI, clear error message)
- [x] 7.4 Add `--modular` flag to `scripts/build.bat`
- [x] 7.5 Run full test suite — core: 4955 passed, std: all suites green
- [x] 7.6 Verify monolithic build remains identical
- [x] 7.7 Update CLAUDE.md with plugin system instructions
- [x] 7.8 N/A — docs/09-CLI.md does not exist

## 8. Phase 7: Dynamic Backend Routing (deferred)

- [ ] 8.1 Route `create_backend()` via plugin loader in modular mode
- [ ] 8.2 Route `object_compiler.cpp` via plugin C API in modular mode
