//! # Module Declaration
//!
//! Provides the TML_MODULE("name") macro that source files use to declare
//! which plugin module they belong to. This is used by the build system
//! (CMake) to assign source files to SHARED library targets in modular builds.
//!
//! ## Usage
//!
//! Place at the top of each .cpp file, before any #include:
//!
//! ```cpp
//! TML_MODULE("compiler")      // → compiler.dll
//! TML_MODULE("codegen_x86")   // → codegen_x86.dll
//! TML_MODULE("tools")         // → tools.dll
//! TML_MODULE("test")          // → test.dll
//! TML_MODULE("mcp")           // → mcp.dll
//! ```
//!
//! ## How It Works
//!
//! The macro itself is a **no-op** at compile time — zero runtime cost.
//! The CMake script `cmake/collect_modules.cmake` scans source files for
//! `TML_MODULE("xxx")` patterns and generates per-module source lists.
//! Files without TML_MODULE are assigned to "compiler" by default.

#pragma once

// TML_MODULE declares which plugin module this source file belongs to.
// This is metadata for the build system only — no runtime effect.
#define TML_MODULE(name) /* plugin module: name */
