//! # Codegen Unit Partitioner
//!
//! Splits a MIR module into N independent codegen units (CGUs).
//! Each CGU contains a deterministic subset of functions and produces
//! independent LLVM IR that can be compiled to a separate object file.
//!
//! ## CGU Benefits
//!
//! - **Incremental caching**: Only CGUs with changed functions need recompilation
//! - **Parallel compilation**: Each CGU can be compiled independently
//! - **Smaller LLVM workload**: Smaller IR modules compile faster

#pragma once

#include "codegen/mir_codegen.hpp"
#include "mir/mir.hpp"

#include <string>
#include <vector>

namespace tml::codegen {

/// Result for a single codegen unit.
struct CGUResult {
    int cgu_index = 0;
    std::string llvm_ir;
    std::vector<std::string> function_names;
    std::string fingerprint; ///< Content hash of llvm_ir for caching.
};

/// Result of partitioning a module into CGUs.
struct PartitionResult {
    std::vector<CGUResult> cgus;
    bool success = false;
    std::string error_message;
};

/// Options for CGU partitioning.
struct PartitionOptions {
    int num_cgus = 16;              ///< Maximum number of CGUs (capped at function count).
    MirCodegenOptions codegen_opts; ///< Options passed through to MirCodegen.
};

/// Partitions a MIR module into independent codegen units.
class CodegenPartitioner {
public:
    explicit CodegenPartitioner(PartitionOptions options);

    /// Partition a MIR module into N CGUs.
    /// Each CGU produces independent LLVM IR with `define` for its functions
    /// and `declare` stubs for cross-CGU references.
    auto partition(const mir::Module& module) -> PartitionResult;

    /// Deterministic function-to-CGU assignment.
    static auto assign_cgu(const std::string& func_name, int num_cgus) -> int;

private:
    PartitionOptions options_;
};

} // namespace tml::codegen
