//! # THIR — Typed High-level IR
//!
//! Aggregator header for the THIR subsystem. Includes all THIR types,
//! statements, and module definitions.
//!
//! THIR sits between HIR and MIR in the compilation pipeline:
//!
//! ```
//! HIR (fully typed, monomorphized)
//!  |
//!  v
//! ThirLower (uses TraitSolver)
//!  |- materialize coercions (numeric, deref, ref)
//!  |- resolve method calls via trait solver
//!  |- desugar operator overloading to method calls
//!  |- check pattern exhaustiveness
//!  '- normalize associated types
//!  |
//!  v
//! THIR (explicit coercions, resolved methods, exhaustive patterns)
//!  |
//!  v
//! ThirMirBuilder
//!  |
//!  v
//! MIR (SSA, basic blocks)
//! ```
//!
//! ## Opt-in
//!
//! THIR is enabled via `--use-thir`. The existing HIR->MIR path remains default.

#pragma once

#include "thir/thir_expr.hpp"
#include "thir/thir_module.hpp"
#include "thir/thir_stmt.hpp"
