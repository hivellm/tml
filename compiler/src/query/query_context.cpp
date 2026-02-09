#include "query/query_context.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace tml::query {

// ============================================================================
// QueryKey utilities
// ============================================================================

QueryKind query_kind(const QueryKey& key) {
    return static_cast<QueryKind>(key.index());
}

const char* query_kind_name(QueryKind kind) {
    switch (kind) {
    case QueryKind::ReadSource:
        return "read_source";
    case QueryKind::Tokenize:
        return "tokenize";
    case QueryKind::ParseModule:
        return "parse_module";
    case QueryKind::TypecheckModule:
        return "typecheck_module";
    case QueryKind::BorrowcheckModule:
        return "borrowcheck_module";
    case QueryKind::HirLower:
        return "hir_lower";
    case QueryKind::MirBuild:
        return "mir_build";
    case QueryKind::CodegenUnit:
        return "codegen_unit";
    default:
        return "unknown";
    }
}

size_t QueryKeyHash::operator()(const QueryKey& key) const {
    size_t h = std::hash<size_t>{}(key.index());

    auto hash_combine = [](size_t seed, size_t val) -> size_t {
        return seed ^ (val + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    };

    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, ReadSourceKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
            } else if constexpr (std::is_same_v<T, TokenizeKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
            } else if constexpr (std::is_same_v<T, ParseModuleKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
            } else if constexpr (std::is_same_v<T, TypecheckModuleKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
            } else if constexpr (std::is_same_v<T, BorrowcheckModuleKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
            } else if constexpr (std::is_same_v<T, HirLowerKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
            } else if constexpr (std::is_same_v<T, MirBuildKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
            } else if constexpr (std::is_same_v<T, CodegenUnitKey>) {
                h = hash_combine(h, std::hash<std::string>{}(k.file_path));
                h = hash_combine(h, std::hash<std::string>{}(k.module_name));
                h = hash_combine(h, std::hash<int>{}(k.optimization_level));
                h = hash_combine(h, std::hash<bool>{}(k.debug_info));
            }
        },
        key);

    return h;
}

// ============================================================================
// QueryContext
// ============================================================================

QueryContext::QueryContext(const QueryOptions& options) : options_(options) {
    providers_.register_core_providers();
}

ReadSourceResult QueryContext::read_source(const std::string& file_path) {
    return force<ReadSourceResult>(ReadSourceKey{file_path});
}

TokenizeResult QueryContext::tokenize(const std::string& file_path) {
    return force<TokenizeResult>(TokenizeKey{file_path});
}

ParseModuleResult QueryContext::parse_module(const std::string& file_path,
                                             const std::string& module_name) {
    return force<ParseModuleResult>(ParseModuleKey{file_path, module_name});
}

TypecheckResult QueryContext::typecheck_module(const std::string& file_path,
                                               const std::string& module_name) {
    return force<TypecheckResult>(TypecheckModuleKey{file_path, module_name});
}

BorrowcheckResult QueryContext::borrowcheck_module(const std::string& file_path,
                                                   const std::string& module_name) {
    return force<BorrowcheckResult>(BorrowcheckModuleKey{file_path, module_name});
}

HirLowerResult QueryContext::hir_lower(const std::string& file_path,
                                       const std::string& module_name) {
    return force<HirLowerResult>(HirLowerKey{file_path, module_name});
}

MirBuildResult QueryContext::mir_build(const std::string& file_path,
                                       const std::string& module_name) {
    return force<MirBuildResult>(MirBuildKey{file_path, module_name});
}

CodegenUnitResult QueryContext::codegen_unit(const std::string& file_path,
                                             const std::string& module_name) {
    return force<CodegenUnitResult>(
        CodegenUnitKey{file_path, module_name, options_.optimization_level, options_.debug_info});
}

void QueryContext::invalidate_file(const std::string& file_path) {
    // Invalidate all query types for this file
    cache_.invalidate_dependents(ReadSourceKey{file_path});
}

} // namespace tml::query
