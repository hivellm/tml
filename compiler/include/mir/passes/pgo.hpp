//! # Profile-Guided Optimization (PGO) Pass
//!
//! Implements profile-guided optimizations using runtime execution data:
//! - Hot call site inlining
//! - Branch probability optimization
//! - Block layout optimization
//! - Profile data reading, writing, and merging

#ifndef TML_MIR_PASSES_PGO_HPP
#define TML_MIR_PASSES_PGO_HPP

#include "mir/mir_pass.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::mir {

// ============================================================================
// Profile Data Structures
// ============================================================================

/// Edge frequency data (from -> to).
struct EdgeProfile {
    uint32_t from_block;
    uint32_t to_block;
    uint64_t count;
};

/// Basic block execution count.
struct BlockProfile {
    uint32_t block_id;
    uint64_t execution_count;
};

/// Function profile data.
struct FunctionProfile {
    std::string name;
    uint64_t call_count = 0;
    uint64_t total_cycles = 0;
    std::vector<BlockProfile> blocks;
    std::vector<EdgeProfile> edges;

    /// Get block execution count.
    [[nodiscard]] auto get_block_count(uint32_t block_id) const -> uint64_t {
        for (const auto& block : blocks) {
            if (block.block_id == block_id) {
                return block.execution_count;
            }
        }
        return 0;
    }

    /// Get edge count.
    [[nodiscard]] auto get_edge_count(uint32_t from, uint32_t to) const -> uint64_t {
        for (const auto& edge : edges) {
            if (edge.from_block == from && edge.to_block == to) {
                return edge.count;
            }
        }
        return 0;
    }

    /// Check if function is "hot" (frequently called).
    [[nodiscard]] auto is_hot(uint64_t threshold = 1000) const -> bool {
        return call_count >= threshold;
    }
};

/// Call site profile data.
struct CallSiteProfile {
    std::string caller;
    std::string callee;
    uint32_t block_id;
    size_t inst_index;
    uint64_t call_count;

    /// Check if call site is hot.
    [[nodiscard]] auto is_hot(uint64_t threshold = 100) const -> bool {
        return call_count >= threshold;
    }
};

/// Complete profile data file.
struct ProfileData {
    std::string version = "1.0";
    std::string module_name;
    uint64_t total_samples = 0;
    std::vector<FunctionProfile> functions;
    std::vector<CallSiteProfile> call_sites;

    /// Loads profile data from a file.
    static auto load(const std::string& path) -> std::optional<ProfileData>;

    /// Saves profile data to a file.
    auto save(const std::string& path) const -> bool;

    /// Merges another profile into this one.
    void merge(const ProfileData& other);

    /// Gets function profile by name.
    [[nodiscard]] auto get_function(const std::string& name) const -> const FunctionProfile*;

    /// Gets hot functions (sorted by call count descending).
    [[nodiscard]] auto get_hot_functions(uint64_t threshold = 1000) const
        -> std::vector<const FunctionProfile*>;

    /// Gets hot call sites (sorted by call count descending).
    [[nodiscard]] auto get_hot_call_sites(uint64_t threshold = 100) const
        -> std::vector<const CallSiteProfile*>;
};

// ============================================================================
// PGO Statistics
// ============================================================================

/// Statistics for PGO pass.
struct PgoStats {
    size_t functions_profiled = 0;
    size_t hot_functions = 0;
    size_t cold_functions = 0;
    size_t inlining_decisions = 0;
    size_t branch_hints_applied = 0;
    size_t blocks_reordered = 0;

    void reset() {
        *this = PgoStats{};
    }
};

// ============================================================================
// Profile Instrumentation Pass
// ============================================================================

/// Inserts profiling counters into the code.
class ProfileInstrumentationPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ProfileInstrumentation";
    }

    auto run(Module& module) -> bool override;

    /// Gets stats.
    [[nodiscard]] auto get_stats() const -> PgoStats {
        return stats_;
    }

private:
    PgoStats stats_;

    /// Instruments a function with profiling counters.
    auto instrument_function(Function& func) -> bool;

    /// Adds block entry counters.
    void add_block_counters(Function& func);

    /// Adds edge counters for branches.
    void add_edge_counters(Function& func);

    /// Adds call site counters.
    void add_call_counters(Function& func);
};

// ============================================================================
// Profile Reader/Writer
// ============================================================================

/// Reads and writes profile data files.
class ProfileIO {
public:
    /// Reads profile data from a file.
    static auto read(const std::string& path) -> std::optional<ProfileData>;

    /// Writes profile data to a file.
    static auto write(const std::string& path, const ProfileData& data) -> bool;

    /// Merges multiple profile files.
    static auto merge(const std::vector<std::string>& paths) -> std::optional<ProfileData>;

    /// Validates profile data against module.
    static auto validate(const ProfileData& data, const Module& module) -> bool;
};

// ============================================================================
// Hot Call Site Inlining Pass
// ============================================================================

/// Uses profile data to guide inlining decisions.
class PgoInliningPass : public MirPass {
public:
    explicit PgoInliningPass(const ProfileData& profile) : profile_(profile) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "PgoInlining";
    }

    auto run(Module& module) -> bool override;

    /// Gets stats.
    [[nodiscard]] auto get_stats() const -> PgoStats {
        return stats_;
    }

    /// Sets hot call threshold.
    void set_hot_threshold(uint64_t threshold) {
        hot_threshold_ = threshold;
    }

private:
    const ProfileData& profile_;
    PgoStats stats_;
    uint64_t hot_threshold_ = 100;

    /// Checks if a call site should be inlined based on profile.
    [[nodiscard]] auto should_inline(const CallSiteProfile& cs) const -> bool;

    /// Gets inlining priority based on profile data.
    [[nodiscard]] auto get_inline_priority(const std::string& callee) const -> int;
};

// ============================================================================
// Branch Probability Pass
// ============================================================================

/// Applies branch probability hints based on profile data.
class BranchProbabilityPass : public MirPass {
public:
    explicit BranchProbabilityPass(const ProfileData& profile) : profile_(profile) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "BranchProbability";
    }

    auto run(Module& module) -> bool override;

    /// Gets stats.
    [[nodiscard]] auto get_stats() const -> PgoStats {
        return stats_;
    }

private:
    const ProfileData& profile_;
    PgoStats stats_;

    /// Applies branch hints to a function.
    auto apply_branch_hints(Function& func, const FunctionProfile& fp) -> bool;

    /// Calculates branch probability from edge counts.
    [[nodiscard]] auto calculate_probability(uint64_t taken, uint64_t not_taken) const -> float;
};

// ============================================================================
// Block Layout Pass
// ============================================================================

/// Reorders basic blocks for better branch prediction.
class BlockLayoutPass : public MirPass {
public:
    explicit BlockLayoutPass(const ProfileData& profile) : profile_(profile) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "BlockLayout";
    }

    auto run(Module& module) -> bool override;

    /// Gets stats.
    [[nodiscard]] auto get_stats() const -> PgoStats {
        return stats_;
    }

private:
    const ProfileData& profile_;
    PgoStats stats_;

    /// Reorders blocks in a function based on execution frequency.
    auto reorder_blocks(Function& func, const FunctionProfile& fp) -> bool;

    /// Places hot successors immediately after their predecessors.
    void place_hot_successors(Function& func, const FunctionProfile& fp);

    /// Groups cold blocks at end of function.
    void group_cold_blocks(Function& func, const FunctionProfile& fp);
};

// ============================================================================
// Combined PGO Pass
// ============================================================================

/// Combined profile-guided optimization pass.
class PgoPass : public MirPass {
public:
    explicit PgoPass(const ProfileData& profile) : profile_(profile) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "PGO";
    }

    auto run(Module& module) -> bool override;

    /// Gets combined stats.
    [[nodiscard]] auto get_stats() const -> PgoStats {
        return stats_;
    }

    /// Enables/disables specific PGO optimizations.
    void enable_inlining(bool enable) {
        enable_inlining_ = enable;
    }
    void enable_branch_hints(bool enable) {
        enable_branch_hints_ = enable;
    }
    void enable_block_layout(bool enable) {
        enable_block_layout_ = enable;
    }

private:
    const ProfileData& profile_;
    PgoStats stats_;
    bool enable_inlining_ = true;
    bool enable_branch_hints_ = true;
    bool enable_block_layout_ = true;
};

} // namespace tml::mir

#endif // TML_MIR_PASSES_PGO_HPP
