#include "query/query_provider.hpp"

#include "query/query_core.hpp"

namespace tml::query {

void QueryProviderRegistry::register_provider(QueryKind kind, ProviderFn provider) {
    auto idx = static_cast<size_t>(kind);
    if (idx < providers_.size()) {
        providers_[idx] = std::move(provider);
    }
}

const ProviderFn* QueryProviderRegistry::get_provider(QueryKind kind) const {
    auto idx = static_cast<size_t>(kind);
    if (idx < providers_.size() && providers_[idx]) {
        return &providers_[idx];
    }
    return nullptr;
}

void QueryProviderRegistry::register_core_providers() {
    register_provider(QueryKind::ReadSource, providers::provide_read_source);
    register_provider(QueryKind::Tokenize, providers::provide_tokenize);
    register_provider(QueryKind::ParseModule, providers::provide_parse_module);
    register_provider(QueryKind::TypecheckModule, providers::provide_typecheck_module);
    register_provider(QueryKind::BorrowcheckModule, providers::provide_borrowcheck_module);
    register_provider(QueryKind::HirLower, providers::provide_hir_lower);
    register_provider(QueryKind::ThirLower, providers::provide_thir_lower);
    register_provider(QueryKind::MirBuild, providers::provide_mir_build);
    register_provider(QueryKind::CodegenUnit, providers::provide_codegen_unit);
}

} // namespace tml::query
