//! # C Compiler Command Interface
//!
//! `tml cc <file.c>` — compile a C source file to a native `.obj` using
//! the TML-authored C17 frontend shipped under `compiler-tml/src/cc/`.
//!
//! ## Usage
//!
//! ```
//! tml cc foo.c                          # produce foo.obj
//! tml cc foo.c -o bar.obj
//! tml cc foo.c -O2 -I include -D NDEBUG
//! tml cc foo.c -target x86_64-pc-windows-msvc
//! tml cc foo.c --emit=llvm-ir           # stop after LLVM IR
//! tml cc foo.c --emit=mir               # stop after MIR
//! tml cc foo.c --emit=ast               # stop after parsing
//! tml cc foo.c --emit=tokens            # stop after preprocessing
//! ```
//!
//! The pipeline is:
//!
//! ```text
//!   source bytes  →  cc_bridge_preproc   →  token stream
//!                 →  cc_bridge_parse     →  translation unit
//!                 →  cc_bridge_lower     →  MIR module
//!                 →  existing LLVM backend
//!                 →  .obj via LLD
//! ```
//!
//! Each `--emit=<stage>` exits after the matching stage without
//! invoking downstream work.

#pragma once

namespace tml::cli {

/// Run the `tml cc` command.
///
/// @param argc  argv count including `tml` and the `cc` literal.
/// @param argv  argv vector; argv[0] is the executable path, argv[1]
///              is `"cc"`, argv[2..] are the file path and flags.
/// @param verbose  Forwarded from the global `-v` / `--verbose` flag.
/// @return Process exit code: 0 on success, non-zero on any failure
///         (parse error, lowering error, linker error, stub returned
///         from `cc_bridge`).
int run_cc(int argc, char* argv[], bool verbose);

} // namespace tml::cli
