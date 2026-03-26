---
name: MCP iostream crash fix
description: std::ifstream/ofstream crashes in MCP DLL context on Windows with Zig CC — replaced with C FILE* I/O
type: project
---

## MCP docs/search crash: iostream ABI incompatibility across DLL boundaries

**Root cause**: `std::ifstream` and `std::ofstream` operations crash silently (segfault) when called from code loaded as a DLL (`tml_compiler.dll`) by the MCP server (`tml_mcp.exe`) on Windows. The Zig CC toolchain produces binaries where C++ iostream objects don't work correctly across DLL boundaries.

**Symptoms**: Process dies with no exception, no error message. try/catch blocks don't catch it. `fprintf(stderr)` logging showed the crash occurs inside `buf << file.rdbuf()` (ifstream) or `out.write(...)` (ofstream).

**Fix**: Replaced ALL `std::ifstream`/`std::ofstream` usage in `mcp_tools_docs.cpp` with C `FILE*` I/O (`fopen`/`fread`/`fwrite`/`fclose`). For line-by-line reading (hpp doc extraction), read entire file into `std::string` then use `std::istringstream` for getline.

**Files changed**: `compiler/src/mcp/mcp_tools_docs.cpp`
- `parse_file_for_docs()`: ifstream -> FILE* fopen/fread
- `extract_hpp_docs()`: ifstream -> FILE* fopen/fread + istringstream for getline
- `save_cached_indices()`: ofstream -> `write_binary_file()` helper using FILE* fwrite
- `read_binary_file()`: ifstream -> FILE* fopen/fread
- `load_cached_indices()`: ifstream -> read_binary_file() (already C-based)

**Why**: There are other `std::ifstream` usages in `mcp_tools.cpp` (line 184) and `mcp_tools_project.cpp` (lines 297, 320, 424, 1465) that may also crash. These should be converted to C FILE* I/O if they are triggered and crash.

**How to apply**: Any new file I/O in MCP server code (`compiler/src/mcp/`) must use C `FILE*` functions, not C++ iostream. This applies to the plugin DLL context specifically.
