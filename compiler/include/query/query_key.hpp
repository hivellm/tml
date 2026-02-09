//! # Query Key and Result Types
//!
//! Defines all query keys (inputs) and their corresponding result types (outputs).
//! Each compilation stage has a key type that identifies the computation and a
//! result type that holds the output.

#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

// Forward declarations to avoid heavy includes
namespace tml::lexer {
struct Token;
struct LexerError;
} // namespace tml::lexer

namespace tml::parser {
struct Module;
struct ParseError;
} // namespace tml::parser

namespace tml::types {
class TypeEnv;
struct TypeError;
class ModuleRegistry;
} // namespace tml::types

namespace tml::borrow {
struct BorrowError;
} // namespace tml::borrow

namespace tml::hir {
struct HirModule;
} // namespace tml::hir

namespace tml::mir {
struct Module;
} // namespace tml::mir

namespace tml::query {

// ============================================================================
// Query Key Types
// ============================================================================

/// Key for reading and preprocessing a source file.
struct ReadSourceKey {
    std::string file_path;
    bool operator==(const ReadSourceKey&) const = default;
};

/// Key for tokenizing a source file.
struct TokenizeKey {
    std::string file_path;
    bool operator==(const TokenizeKey&) const = default;
};

/// Key for parsing a module from tokens.
struct ParseModuleKey {
    std::string file_path;
    std::string module_name;
    bool operator==(const ParseModuleKey&) const = default;
};

/// Key for type-checking a module.
struct TypecheckModuleKey {
    std::string file_path;
    std::string module_name;
    bool operator==(const TypecheckModuleKey&) const = default;
};

/// Key for borrow-checking a module.
struct BorrowcheckModuleKey {
    std::string file_path;
    std::string module_name;
    bool operator==(const BorrowcheckModuleKey&) const = default;
};

/// Key for lowering AST to HIR.
struct HirLowerKey {
    std::string file_path;
    std::string module_name;
    bool operator==(const HirLowerKey&) const = default;
};

/// Key for building MIR from HIR.
struct MirBuildKey {
    std::string file_path;
    std::string module_name;
    bool operator==(const MirBuildKey&) const = default;
};

/// Key for generating LLVM IR from a compilation unit.
struct CodegenUnitKey {
    std::string file_path;
    std::string module_name;
    int optimization_level = 0;
    bool debug_info = false;
    bool operator==(const CodegenUnitKey&) const = default;
};

/// Union of all query keys.
using QueryKey = std::variant<ReadSourceKey, TokenizeKey, ParseModuleKey, TypecheckModuleKey,
                              BorrowcheckModuleKey, HirLowerKey, MirBuildKey, CodegenUnitKey>;

/// Tag enum for fast query type discrimination.
enum class QueryKind : uint8_t {
    ReadSource,
    Tokenize,
    ParseModule,
    TypecheckModule,
    BorrowcheckModule,
    HirLower,
    MirBuild,
    CodegenUnit,
    COUNT
};

/// Extract the QueryKind from a QueryKey variant.
[[nodiscard]] QueryKind query_kind(const QueryKey& key);

/// Get a human-readable name for a query kind.
[[nodiscard]] const char* query_kind_name(QueryKind kind);

/// Hash functor for QueryKey (for use in unordered_map).
struct QueryKeyHash {
    size_t operator()(const QueryKey& key) const;
};

/// Equality functor for QueryKey.
struct QueryKeyEqual {
    bool operator()(const QueryKey& a, const QueryKey& b) const {
        return a == b;
    }
};

// ============================================================================
// Query Result Types
// ============================================================================

/// Result of reading and preprocessing source.
struct ReadSourceResult {
    std::string source_code;
    std::string preprocessed;
    bool success = false;
    std::string error_message;
};

/// Result of tokenization.
struct TokenizeResult {
    std::shared_ptr<std::vector<lexer::Token>> tokens;
    bool success = false;
    std::vector<std::string> errors;
};

/// Result of parsing.
struct ParseModuleResult {
    std::shared_ptr<parser::Module> module;
    bool success = false;
    std::vector<std::string> errors;
};

/// Result of type checking.
struct TypecheckResult {
    std::shared_ptr<types::TypeEnv> env;
    std::shared_ptr<types::ModuleRegistry> registry;
    bool success = false;
    std::vector<std::string> errors;
};

/// Result of borrow checking.
struct BorrowcheckResult {
    bool success = false;
    std::vector<std::string> errors;
};

/// Result of HIR lowering.
struct HirLowerResult {
    std::shared_ptr<hir::HirModule> hir_module;
    bool success = false;
};

/// Result of MIR building.
struct MirBuildResult {
    std::shared_ptr<mir::Module> mir_module;
    bool success = false;
    std::vector<std::string> errors;
};

/// Result of LLVM IR generation.
struct CodegenUnitResult {
    std::string llvm_ir;
    std::set<std::string> link_libs;
    bool success = false;
    std::string error_message;
};

/// Unique integer ID for a query invocation (for dependency tracking).
using QueryId = uint64_t;

} // namespace tml::query
