//! # Compiler Driver Interface
//!
//! This header defines the main entry point for the TML compiler.
//!
//! ## Entry Point
//!
//! `tml_main()` dispatches to the appropriate command handler based on argv[1].

#pragma once

// Main compiler driver entry point
// Dispatches to appropriate command handlers
int tml_main(int argc, char* argv[]);
