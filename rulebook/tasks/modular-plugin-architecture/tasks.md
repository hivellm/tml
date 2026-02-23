# Tasks: Modular Plugin Architecture

**Status**: Pending (0%)

## 1. Phase 0: Infrastructure

- [ ] 1.1 Create `compiler/include/tml_module.hpp` with `TML_MODULE(name)` macro
- [ ] 1.2 Create `compiler/include/tml_plugin.h` with Plugin ABI (pure C)
- [ ] 1.3 Embed `zstddeclib.c` (decoder-only) in `compiler/third_party/zstd/`
- [ ] 1.4 Create `compiler/include/plugin_loader.hpp` (interface)
- [ ] 1.5 Create `compiler/src/plugin_loader.cpp` (LoadLibrary/dlopen + zstd decompress + cache)
- [ ] 1.6 Create `compiler/cmake/collect_modules.cmake` (scans TML_MODULE in .cpp files)
- [ ] 1.7 Add `TML_BUILD_MODULAR` option to CMakeLists.txt (default OFF)
- [ ] 1.8 Test plugin_loader standalone with a test DLL

## 2. Phase 1: Annotate Sources

- [ ] 2.1 Add `TML_MODULE("compiler")` to frontend sources (lexer, parser, preprocessor)
- [ ] 2.2 Add `TML_MODULE("compiler")` to analysis sources (types, borrow, traits)
- [ ] 2.3 Add `TML_MODULE("compiler")` to IR sources (hir, thir, mir, ir)
- [ ] 2.4 Add `TML_MODULE("compiler")` to infra sources (query, common, log, profiler, json, cli/builder, cli/commands)
- [ ] 2.5 Add `TML_MODULE("codegen_x86")` to LLVM codegen sources (codegen/llvm/**, backend/llvm_backend, backend/lld_linker)
- [ ] 2.6 Add `TML_MODULE("codegen_cranelift")` to Cranelift sources
- [ ] 2.7 Add `TML_MODULE("tools")` to tool sources (format, doc, search, cli/linter)
- [ ] 2.8 Add `TML_MODULE("test")` to test runner sources (cli/tester)
- [ ] 2.9 Add `TML_MODULE("mcp")` to MCP sources
- [ ] 2.10 Verify collect_modules.cmake groups sources correctly

## 3. Phase 2: Extract tml_codegen_x86.dll

- [ ] 3.1 Create SHARED target `tml_codegen_x86` in CMake with LLVM/LLD linked
- [ ] 3.2 Create `compiler/src/llvm_dll/llvm_plugin.cpp` with plugin C entry points
- [ ] 3.3 Modify `create_backend()` to use plugin loader when `TML_BUILD_MODULAR`
- [ ] 3.4 Modify `object_compiler.cpp` to route via plugin C API
- [ ] 3.5 Test `tml build` via modular mode produces identical output to monolithic
- [ ] 3.6 Verify `tml check` works WITHOUT loading tml_codegen_x86.dll

## 4. Phase 3: Thin Launcher

- [ ] 4.1 Create `compiler/src/launcher/main_launcher.cpp` (thin entry point)
- [ ] 4.2 Implement command → plugin mapping in launcher
- [ ] 4.3 Create SHARED target `tml_compiler` in CMake with full frontend
- [ ] 4.4 Export compiler C API (compile_to_ir, check, lex, parse)
- [ ] 4.5 Test `tml --help` without loading any plugin
- [ ] 4.6 Test `tml build`/`tml check`/`tml run` via modular launcher

## 5. Phase 4: Auxiliary Modules

- [ ] 5.1 Create SHARED target `tml_tools` with formatter + linter + doc + search
- [ ] 5.2 Export tools C API (format_file, lint_file, doc_generate)
- [ ] 5.3 Test `tml fmt` and `tml lint` via tml_tools.dll
- [ ] 5.4 Create SHARED target `tml_test` with test runner + coverage
- [ ] 5.5 Export test C API (run_tests, run_coverage)
- [ ] 5.6 Test `tml test` via tml_test.dll
- [ ] 5.7 Create SHARED target `tml_mcp` with MCP server
- [ ] 5.8 Test `tml mcp` via tml_mcp.dll

## 6. Phase 5: TML Tooling

- [ ] 6.1 Write `tools/plugin_pack.tml` (compress DLLs + generate manifest.json)
- [ ] 6.2 Test plugin_pack.tml compiles and runs
- [ ] 6.3 Write `tools/plugin_verify.tml` (verify integrity via SHA256)
- [ ] 6.4 Test plugin_verify.tml compiles and runs
- [ ] 6.5 Integrate `--pack` flag into `scripts/build.bat`
- [ ] 6.6 Test `scripts\build.bat --modular --pack` end-to-end

## 7. Phase 6: Polish

- [ ] 7.1 Implement smart cache with hash check (avoid repeated decompression)
- [ ] 7.2 Clear error messages when plugin not found
- [ ] 7.3 ABI versioning (reject plugins with incompatible ABI)
- [ ] 7.4 Add `--modular` flag to `scripts/build.bat`
- [ ] 7.5 Run full test suite in modular mode
- [ ] 7.6 Verify monolithic build remains identical
- [ ] 7.7 Update CLAUDE.md with plugin system instructions
- [ ] 7.8 Update docs/09-CLI.md with new `--modular` flag
