//! # Codegen Unit Partitioner Implementation
//!
//! Splits a MIR module into N CGUs using deterministic hash-based assignment.
//! Each function is assigned to a CGU based on `hash(func_name) % num_cgus`.

#include "codegen/codegen_partitioner.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace tml::codegen {

CodegenPartitioner::CodegenPartitioner(PartitionOptions options) : options_(std::move(options)) {}

auto CodegenPartitioner::assign_cgu(const std::string& func_name, int num_cgus) -> int {
    size_t h = std::hash<std::string>{}(func_name);
    return static_cast<int>(h % static_cast<size_t>(num_cgus));
}

auto CodegenPartitioner::partition(const mir::Module& module) -> PartitionResult {
    PartitionResult result;

    if (module.functions.empty()) {
        result.success = true;
        return result;
    }

    // Cap CGU count at function count (no empty CGUs)
    int effective_cgus = std::min(options_.num_cgus, static_cast<int>(module.functions.size()));
    if (effective_cgus < 1) {
        effective_cgus = 1;
    }

    // Single CGU: use monolithic path (no overhead)
    if (effective_cgus == 1) {
        CGUResult cgu;
        cgu.cgu_index = 0;

        MirCodegen codegen(options_.codegen_opts);
        cgu.llvm_ir = codegen.generate(module);

        for (const auto& func : module.functions) {
            cgu.function_names.push_back(func.name);
        }

        // Compute fingerprint (simple hash of IR content)
        std::hash<std::string> hasher;
        size_t h = hasher(cgu.llvm_ir);
        std::ostringstream fp;
        fp << std::hex << h;
        cgu.fingerprint = fp.str();

        result.cgus.push_back(std::move(cgu));
        result.success = true;
        return result;
    }

    // Assign functions to CGUs
    std::unordered_map<int, std::vector<size_t>> cgu_functions;
    for (size_t i = 0; i < module.functions.size(); ++i) {
        int cgu_idx = assign_cgu(module.functions[i].name, effective_cgus);
        cgu_functions[cgu_idx].push_back(i);
    }

    // Generate IR for each non-empty CGU
    for (int cgu_idx = 0; cgu_idx < effective_cgus; ++cgu_idx) {
        auto it = cgu_functions.find(cgu_idx);
        if (it == cgu_functions.end()) {
            continue; // Skip empty CGUs (hash distribution gap)
        }

        const auto& func_indices = it->second;

        CGUResult cgu;
        cgu.cgu_index = cgu_idx;

        MirCodegen codegen(options_.codegen_opts);
        cgu.llvm_ir = codegen.generate_cgu(module, func_indices);

        for (size_t idx : func_indices) {
            cgu.function_names.push_back(module.functions[idx].name);
        }

        // Compute fingerprint
        std::hash<std::string> hasher;
        size_t h = hasher(cgu.llvm_ir);
        std::ostringstream fp;
        fp << std::hex << h;
        cgu.fingerprint = fp.str();

        result.cgus.push_back(std::move(cgu));
    }

    TML_LOG_DEBUG("codegen", "CGU partitioning: " << module.functions.size() << " functions -> "
                                                  << result.cgus.size() << " CGUs (requested "
                                                  << effective_cgus << ")");

    result.success = true;
    return result;
}

} // namespace tml::codegen
