TML_MODULE("compiler")

//! # Backend Error Explanations
//!
//! Error codes K001-K011 for LLVM backend errors.
//! Error codes N001-N008 for linker (LLD) errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_backend_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        // ====================================================================
        // K001-K011: LLVM backend errors
        // ====================================================================

        {"K001", R"EX(
Failed to parse LLVM IR [K001]

The LLVM backend could not parse the IR (Intermediate Representation) that
the TML compiler generated. This is always an internal compiler bug — the
compiler produced invalid LLVM IR text.

Common causes:

1. A codegen pass emitted a malformed instruction or type reference
2. A function signature mismatch between call site and definition
3. A type that was not declared before use in the IR

How to diagnose:

    tml build myfile.tml --emit-ir    # Inspect the generated IR
    tml explain K001                  # This explanation

If you see this error with valid TML code, please report it as a compiler bug,
including the generated IR from `--emit-ir`.
)EX"},

        {"K002", R"EX(
Module verification failed [K002]

The LLVM module verifier rejected the generated IR. The IR parsed successfully
(so it is not a K001), but it violates LLVM's structural rules — compiling or
executing it would be undefined behavior. This is always an internal TML
compiler bug.

The verifier checks structural correctness of the IR — things like:
- All basic blocks must end with a terminator instruction
- All values must be defined before use (dominance)
- Type consistency across instructions
- Well-formed PHI nodes and attribute combinations

Since v0.3.54 this is a HARD ERROR on every emission path (object file,
in-process buffer, and JIT), both before and after the optimization pipeline.
The error message names the phase ("pre-optimization" / "post-optimization" /
"jit").

How to diagnose:

    tml build myfile.tml --emit-ir    # Inspect the generated IR
    tml explain K002                  # This explanation

Escape hatch (bisection ONLY — the produced binary may have undefined
behavior):

    TML_NO_VERIFY_IR=1 tml build myfile.tml

Report this as a compiler bug with the `--emit-ir` output.
)EX"},

        {"K003", R"EX(
Failed to create LLVM target machine [K003]

The LLVM backend could not create a target machine for the requested target
architecture. This prevents object code generation.

Common causes:

1. The requested target triple is not supported by this build of LLVM
2. The CPU or feature flags specified are not valid for the target
3. LLVM was built without support for the target architecture

How to fix:

- Ensure you are targeting a supported architecture (x86_64, aarch64)
- Check that LLVM was built with support for the target

This is typically an installation or configuration issue.
)EX"},

        {"K004", R"EX(
Failed to emit object file [K004]

The LLVM backend failed to write the compiled object file to disk. This can
happen due to disk I/O errors or permission issues.

Common causes:

1. The output directory does not exist or is not writable
2. The disk is full
3. A file lock is held on the output file by another process

How to fix:

- Check that the build output directory exists and is writable
- Ensure there is sufficient disk space
- Close any programs that may be locking the output file
)EX"},

        {"K005", R"EX(
Failed to get LLVM target [K005]

The LLVM backend could not look up the specified target. This usually means
the target triple is unknown to the installed version of LLVM.

A target triple has the form: `<arch>-<vendor>-<os>-<env>`
For example: `x86_64-pc-windows-msvc` or `x86_64-unknown-linux-gnu`

How to fix:

- Verify the target triple is correct for your platform
- Ensure LLVM was built with support for the desired target architecture
- The default target (native) should always work on a supported platform
)EX"},

        {"K006", R"EX(
Failed to create memory buffer for IR [K006]

The LLVM backend could not allocate a memory buffer to hold the generated IR.
This is typically an out-of-memory condition.

How to fix:

- Check available system memory
- Try compiling a smaller input file
- Close other memory-intensive applications

If this error occurs consistently on small files, it may indicate a compiler
bug where IR is being generated unnecessarily large.
)EX"},

        {"K007", R"EX(
Failed to open IR file [K007]

The LLVM backend could not open the specified IR file for reading. This error
occurs when compiling from a pre-existing `.ll` file.

Common causes:

1. The file path is incorrect
2. The file does not have read permissions
3. The file is locked by another process

How to fix:

- Verify the file path is correct
- Check file permissions
- Ensure no other process has an exclusive lock on the file
)EX"},

        {"K008", R"EX(
IR file not found [K008]

The specified LLVM IR file does not exist at the given path. This error
occurs when compiling from a pre-existing `.ll` file.

How to fix:

- Verify the file path is correct
- Ensure the file was generated before attempting to compile it
- Check for typos in the file path
)EX"},

        {"K009", R"EX(
LLVM backend not initialized [K009]

The LLVM backend was called before it was properly initialized. This is an
internal compiler error that should not occur in normal usage.

This error indicates a bug in the compiler's initialization sequence. Please
report it as a compiler bug with the full error output and the source file
that triggered it.
)EX"},

        {"K010", R"EX(
Failed to create LLVM context [K010]

The LLVM backend could not create an LLVM context, which is required for all
compilation operations. This is typically an out-of-memory condition or an
LLVM installation issue.

How to fix:

- Check available system memory
- Verify that LLVM libraries are correctly installed
- Try restarting the compiler

If this error occurs consistently, it may indicate a corrupted LLVM installation.
)EX"},

        {"K011", R"EX(
Failed to emit object to memory [K011]

The LLVM backend failed to emit the compiled object code to an in-memory
buffer. This is used for in-process compilation where the object code is
passed directly to the linker without writing to disk.

Common causes:

1. Out-of-memory condition
2. A code generation error that the LLVM backend cannot recover from
3. An unsupported instruction or target feature

How to fix:

- Check available system memory
- Try rebuilding with `--release` for optimized compilation
- If the error is reproducible, report it as a compiler bug with `--emit-ir` output
)EX"},

        // ====================================================================
        // N001-N008: Linker (LLD) errors
        // ====================================================================

        {"N001", R"EX(
Linking failed [N001]

The linker (LLD) exited with a non-zero exit code, indicating that linking
failed. This is a general linker failure that encompasses many possible causes.

Common causes:

1. Undefined symbol — a function or variable is referenced but not defined
2. Multiple definition — the same symbol is defined in more than one object file
3. Incompatible object files — objects compiled for different targets
4. Missing library — a required library was not found

How to diagnose:

Look at the linker output above this error for the specific failure reason.
Common messages:
- "undefined reference to `symbol`" → a function is missing
- "multiple definition of `symbol`" → duplicate symbol in two object files

How to fix:

- Ensure all required source files are compiled and linked
- Check that library dependencies are installed
- Verify that all symbols have implementations
)EX"},

        {"N002", R"EX(
In-process LLD linking failed [N002]

The in-process LLD linker (running inside the compiler process) failed. This
is different from the subprocess linker failure (N001) — the linker ran but
reported a link error.

Common causes:

1. Undefined symbol in the TML runtime or user code
2. Object file ABI mismatch (wrong calling convention, struct layout)
3. Missing runtime library symbols

How to diagnose:

The error message includes the LLD exit code. Look at the output above for
specific linker diagnostics.

How to fix:

- Check for unimplemented functions or missing `@extern("c")` declarations
- Verify runtime library is correctly built (`scripts/build.bat`)
- Ensure all imported modules are available
)EX"},

        {"N003", R"EX(
LLD in-process unavailable [N003]

The in-process LLD linker cannot be used because a previous linking call
corrupted the global LLD state. LLD has global state that cannot be reset
between calls in the same process.

This is a known limitation of the in-process LLD approach. When it occurs,
the compiler falls back to a subprocess linker.

This error is typically handled automatically by the compiler. If you see it
in normal compilation, it indicates the compiler's fallback mechanism failed.

How to fix:

- This is usually handled automatically — try recompiling
- If persistent, use `--no-cache` to force a clean compilation
- Report as a bug if fallback to subprocess linker also fails
)EX"},

        {"N004", R"EX(
LLD linker not initialized [N004]

The linker was called before it was initialized. This is an internal compiler
error that indicates a bug in the compiler's build pipeline.

Please report this as a compiler bug with:
- The TML source file that triggered it
- The full error output
- The compiler version (`tml --version`)
)EX"},

        {"N005", R"EX(
No object files provided for linking [N005]

The linker was invoked but no object files were provided as input. At least
one compiled object file is required to produce an executable or library.

Common causes:

1. The compilation step failed silently, producing no object files
2. A bug in the compiler's build pipeline skipped code generation
3. An empty source file or module with no code to compile

How to fix:

- Check that source files contain compilable code (not just comments)
- Look for earlier compilation errors that may have prevented object file generation
- Use `tml build --verbose` for more detailed output
)EX"},

        {"N006", R"EX(
Object file not found [N006]

The linker expected an object file at a specific path but the file does not
exist. This indicates the compilation step did not produce the expected output.

Common causes:

1. A compilation error occurred but was not reported correctly
2. The build directory was cleaned between compilation and linking
3. File system permissions prevented the object file from being written

How to fix:

- Check for compilation errors in the output above
- Ensure the build directory is writable
- Try a clean build: `tml build --no-cache`
)EX"},

        {"N007", R"EX(
Output file was not created [N007]

The linker completed without error but the expected output file (executable
or library) was not found at the expected path.

Common causes:

1. The linker wrote the output to a different path
2. A post-link operation moved or deleted the output
3. A file system issue prevented the output from being written

How to fix:

- Check the specified output path for write permissions
- Look for the output file in the current working directory
- Try specifying an explicit output path with `--output`
- Ensure the build directory is writable
)EX"},

        {"N008", R"EX(
Static library creation failed [N008]

The linker failed to create a static library (`.lib` or `.a` file) from the
provided object files.

Common causes:

1. The `llvm-ar` or equivalent archiver tool is not available
2. The output path for the static library is not writable
3. One of the input object files is corrupted or incompatible

How to fix:

- Ensure the LLVM toolchain is correctly installed
- Check that the output directory exists and is writable
- Verify that all object files were compiled successfully
- Try a clean build: `tml build --no-cache`
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
