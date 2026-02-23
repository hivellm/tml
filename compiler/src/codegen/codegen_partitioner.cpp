TML_MODULE("compiler")

//! # Codegen Unit Partitioner Implementation
//!
//! Splits a MIR module into N CGUs using deterministic hash-based assignment.
//! Each function is assigned to a CGU based on `hash(func_name) % num_cgus`.
//!
//! ## Function-Level Fingerprinting
//!
//! Each function's MIR content is fingerprinted independently. The CGU
//! fingerprint is composed from the sorted function fingerprints, enabling
//! fine-grained cache invalidation: if no function in a CGU changed, the
//! CGU fingerprint remains stable and the cached object can be reused
//! without regenerating IR.

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

/// Hash combiner using the golden ratio constant for good avalanche properties.
static inline void hash_combine(size_t& h, size_t value) {
    h ^= value + 0x9e3779b9 + (h << 6) + (h >> 2);
}

/// Compute a fingerprint for a single MIR function based on its structure.
/// This hash captures the function's params, blocks, instructions, and terminators
/// so that unchanged functions produce stable fingerprints across builds.
static auto fingerprint_mir_function(const mir::Function& func) -> size_t {
    std::hash<std::string> str_hasher;
    size_t h = str_hasher(func.name);

    // Mix in parameter info
    for (const auto& param : func.params) {
        hash_combine(h, str_hasher(param.name));
        hash_combine(h, static_cast<size_t>(param.value_id));
    }

    // Mix in block count and instruction counts
    hash_combine(h, func.blocks.size());
    for (const auto& block : func.blocks) {
        hash_combine(h, str_hasher(block.name));
        hash_combine(h, block.instructions.size());

        // Mix in instruction variant indices and result IDs
        for (const auto& inst_data : block.instructions) {
            hash_combine(h, inst_data.inst.index());                // Which instruction variant
            hash_combine(h, static_cast<size_t>(inst_data.result)); // Result ValueId
        }

        // Mix in terminator variant index
        if (block.terminator.has_value()) {
            hash_combine(h, block.terminator->index());
        }
    }

    // Mix in attributes
    for (const auto& attr : func.attributes) {
        hash_combine(h, str_hasher(attr));
    }

    // Mix in flags
    hash_combine(h, (func.is_public ? 1u : 0u));
    hash_combine(h, (func.is_async ? 2u : 0u));
    hash_combine(h, (func.uses_sret ? 4u : 0u));

    return h;
}

/// Compose a CGU fingerprint from sorted per-function fingerprints.
static auto compose_cgu_fingerprint(const std::vector<FunctionFingerprint>& func_fps)
    -> std::string {
    size_t combined = 0;
    for (const auto& fp : func_fps) {
        combined ^= fp.mir_hash + 0x9e3779b9 + (combined << 6) + (combined >> 2);
    }
    std::ostringstream oss;
    oss << std::hex << combined;
    return oss.str();
}

auto CodegenPartitioner::partition(const mir::Module& module) -> PartitionResult {
    PartitionResult result;

    if (module.functions.empty()) {
        result.success = true;
        return result;
    }

    // Compute per-function MIR fingerprints upfront
    std::vector<FunctionFingerprint> all_func_fps;
    all_func_fps.reserve(module.functions.size());
    for (const auto& func : module.functions) {
        FunctionFingerprint fp;
        fp.name = func.name;
        fp.mir_hash = fingerprint_mir_function(func);
        all_func_fps.push_back(std::move(fp));
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

        for (size_t i = 0; i < module.functions.size(); ++i) {
            cgu.function_names.push_back(module.functions[i].name);
            cgu.function_fingerprints.push_back(all_func_fps[i]);
        }

        // Compose CGU fingerprint from function MIR fingerprints
        cgu.fingerprint = compose_cgu_fingerprint(cgu.function_fingerprints);

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

        // Collect per-function fingerprints for this CGU
        for (size_t idx : func_indices) {
            cgu.function_names.push_back(module.functions[idx].name);
            cgu.function_fingerprints.push_back(all_func_fps[idx]);
        }

        // Compose CGU fingerprint from function MIR fingerprints.
        // This is stable across builds when functions don't change,
        // enabling cache hits without regenerating IR.
        cgu.fingerprint = compose_cgu_fingerprint(cgu.function_fingerprints);

        // Generate IR for this CGU
        MirCodegen codegen(options_.codegen_opts);
        cgu.llvm_ir = codegen.generate_cgu(module, func_indices);

        result.cgus.push_back(std::move(cgu));
    }

    TML_LOG_DEBUG("codegen", "CGU partitioning: " << module.functions.size() << " functions -> "
                                                  << result.cgus.size() << " CGUs (requested "
                                                  << effective_cgus << ")");

    result.success = true;
    return result;
}

} // namespace tml::codegen
