# LLVM Source Code Coverage

## Overview

Implement source code coverage using LLVM's built-in instrumentation, similar to how Vitest/Istanbul works for JavaScript. This will show which lines of library code (lib/core, lib/std) are executed during tests.

## Goals

1. **Line-by-line coverage** - Know exactly which lines were executed
2. **Branch coverage** - Track which branches (if/else, when) were taken
3. **Function coverage** - Track which functions were called
4. **Per-file reports** - Coverage percentage for each source file
5. **HTML reports** - Visual annotation of covered/uncovered lines
6. **CI-friendly output** - LCOV format for codecov/coveralls integration

## Technical Approach

### LLVM Coverage Instrumentation

LLVM provides built-in coverage via two compiler flags:

```bash
clang -fprofile-instr-generate -fcoverage-mapping source.c -o binary
```

- `-fprofile-instr-generate`: Instruments code to count executions
- `-fcoverage-mapping`: Embeds source mapping for reports

### Workflow

```
┌─────────────────┐
│ Library Sources │ (lib/core/*.tml, lib/std/*.tml)
└────────┬────────┘
         │ compile with coverage flags
         ▼
┌─────────────────┐
│ Instrumented    │ (.o files with coverage data)
│ Object Files    │
└────────┬────────┘
         │ link
         ▼
┌─────────────────┐
│ Test Binary     │ (with coverage runtime)
└────────┬────────┘
         │ run tests
         ▼
┌─────────────────┐
│ .profraw files  │ (raw profile data)
└────────┬────────┘
         │ llvm-profdata merge
         ▼
┌─────────────────┐
│ .profdata       │ (merged profile)
└────────┬────────┘
         │ llvm-cov report/show
         ▼
┌─────────────────┐
│ Coverage Report │ (HTML, JSON, LCOV)
└─────────────────┘
```

### Key Commands

```bash
# Set output file for profile data
export LLVM_PROFILE_FILE="coverage-%p.profraw"

# Merge multiple profraw files
llvm-profdata merge -sparse *.profraw -o coverage.profdata

# Generate summary report
llvm-cov report ./binary -instr-profile=coverage.profdata

# Generate HTML report
llvm-cov show ./binary -instr-profile=coverage.profdata \
  -format=html -output-dir=coverage-html

# Generate LCOV format
llvm-cov export ./binary -instr-profile=coverage.profdata \
  -format=lcov > coverage.lcov
```

## Implementation Details

### 1. ObjectCompileOptions Extension

```cpp
// compiler/include/cli/builder/object_compiler.hpp
struct ObjectCompileOptions {
    // ... existing fields ...
    bool coverage = false;  // Enable LLVM coverage instrumentation
};
```

### 2. Clang Flag Injection

```cpp
// compiler/src/cli/builder/object_compiler.cpp
if (options.coverage) {
    args.push_back("-fprofile-instr-generate");
    args.push_back("-fcoverage-mapping");
}
```

### 3. Profile Collection

```cpp
// compiler/src/cli/tester/coverage.cpp (new file)
namespace tml::cli::tester {

struct CoverageCollector {
    fs::path profdata_dir;
    std::vector<fs::path> profraw_files;

    void set_profile_env(int test_index);
    void collect_profraw(const fs::path& exe_path);
    bool merge_profiles(const fs::path& output);
    bool generate_report(const fs::path& exe, const fs::path& profdata);
};

}
```

### 4. Report Generation

```cpp
// Generate HTML report
bool generate_html_report(
    const fs::path& binary,
    const fs::path& profdata,
    const fs::path& output_dir,
    const std::vector<fs::path>& source_dirs
);

// Generate summary for console
struct CoverageSummary {
    int total_lines;
    int covered_lines;
    int total_functions;
    int covered_functions;
    int total_branches;
    int covered_branches;

    double line_percent() const;
    double function_percent() const;
    double branch_percent() const;
};
```

### 5. Console Output (Vitest-style)

```
 Coverage Report
 ───────────────────────────────────────────────────────
 File                    │ Lines  │ Branch │ Funcs
 ───────────────────────────────────────────────────────
 lib/core/src/iter.tml   │  85.2% │  72.1% │  91.3%
 lib/core/src/slice.tml  │  92.4% │  88.5% │ 100.0%
 lib/core/src/option.tml │  45.0% │  32.0% │  60.0%
 lib/std/src/sync/*.tml  │  78.9% │  65.4% │  85.7%
 ───────────────────────────────────────────────────────
 Total                   │  72.3% │  58.2% │  81.5%
 ───────────────────────────────────────────────────────

 Uncovered files:
   lib/core/src/arena.tml (0%)
   lib/core/src/borrow.tml (0%)
   lib/core/src/cache.tml (0%)

 HTML report: coverage/index.html
```

## File Changes

### New Files
- `compiler/src/cli/tester/coverage.cpp` - Coverage collection logic
- `compiler/src/cli/tester/coverage.hpp` - Coverage interfaces
- `compiler/src/cli/builder/coverage_compile.cpp` - Coverage compilation

### Modified Files
- `compiler/include/cli/builder/object_compiler.hpp` - Add coverage flag
- `compiler/src/cli/builder/object_compiler.cpp` - Pass coverage flags to clang
- `compiler/src/cli/builder/compiler_setup.cpp` - Detect llvm-profdata, llvm-cov
- `compiler/src/cli/tester/run.cpp` - Integrate coverage collection
- `compiler/src/cli/commands/cmd_test.hpp` - Add --coverage-source option
- `compiler/src/cli/commands/cmd_test.cpp` - Handle coverage option

## Dependencies

- `llvm-profdata` - For merging profile data
- `llvm-cov` - For generating reports
- Both are part of LLVM toolchain (already required for TML compilation)

## Testing

1. Run `tml test --coverage-source` on a small test suite
2. Verify .profraw files are generated
3. Verify profdata merge works
4. Verify HTML report is generated with correct line annotations
5. Verify console summary matches expected coverage
