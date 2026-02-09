//! # Core Query Providers
//!
//! Provider implementations that wrap each compilation stage.
//! Each provider is a function that takes a QueryContext and QueryKey,
//! executes the stage, and returns the result as std::any.

#pragma once

#include "query/query_key.hpp"

#include <any>

namespace tml::query {

// Forward declaration
class QueryContext;

namespace providers {

/// Provider: read and preprocess a source file.
std::any provide_read_source(QueryContext& ctx, const QueryKey& key);

/// Provider: tokenize preprocessed source.
std::any provide_tokenize(QueryContext& ctx, const QueryKey& key);

/// Provider: parse tokens into AST module.
std::any provide_parse_module(QueryContext& ctx, const QueryKey& key);

/// Provider: type-check a parsed module.
std::any provide_typecheck_module(QueryContext& ctx, const QueryKey& key);

/// Provider: borrow-check a type-checked module.
std::any provide_borrowcheck_module(QueryContext& ctx, const QueryKey& key);

/// Provider: lower AST+TypeEnv to HIR.
std::any provide_hir_lower(QueryContext& ctx, const QueryKey& key);

/// Provider: build MIR from HIR.
std::any provide_mir_build(QueryContext& ctx, const QueryKey& key);

/// Provider: generate LLVM IR from MIR (or AST fallback).
std::any provide_codegen_unit(QueryContext& ctx, const QueryKey& key);

} // namespace providers

} // namespace tml::query
