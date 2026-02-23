TML_MODULE("compiler")

//! # Profile-Guided Optimization (PGO) - Implementation

#include "mir/passes/pgo.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace tml::mir {

// ============================================================================
// ProfileData Implementation
// ============================================================================

auto ProfileData::load(const std::string& path) -> std::optional<ProfileData> {
    return ProfileIO::read(path);
}

auto ProfileData::save(const std::string& path) const -> bool {
    return ProfileIO::write(path, *this);
}

void ProfileData::merge(const ProfileData& other) {
    total_samples += other.total_samples;

    // Merge function profiles
    for (const auto& other_func : other.functions) {
        bool found = false;
        for (auto& func : functions) {
            if (func.name == other_func.name) {
                func.call_count += other_func.call_count;
                func.total_cycles += other_func.total_cycles;
                // Merge block counts
                for (const auto& other_block : other_func.blocks) {
                    bool block_found = false;
                    for (auto& block : func.blocks) {
                        if (block.block_id == other_block.block_id) {
                            block.execution_count += other_block.execution_count;
                            block_found = true;
                            break;
                        }
                    }
                    if (!block_found) {
                        func.blocks.push_back(other_block);
                    }
                }
                // Merge edge counts
                for (const auto& other_edge : other_func.edges) {
                    bool edge_found = false;
                    for (auto& edge : func.edges) {
                        if (edge.from_block == other_edge.from_block &&
                            edge.to_block == other_edge.to_block) {
                            edge.count += other_edge.count;
                            edge_found = true;
                            break;
                        }
                    }
                    if (!edge_found) {
                        func.edges.push_back(other_edge);
                    }
                }
                found = true;
                break;
            }
        }
        if (!found) {
            functions.push_back(other_func);
        }
    }

    // Merge call site profiles
    for (const auto& other_cs : other.call_sites) {
        bool found = false;
        for (auto& cs : call_sites) {
            if (cs.caller == other_cs.caller && cs.callee == other_cs.callee &&
                cs.block_id == other_cs.block_id && cs.inst_index == other_cs.inst_index) {
                cs.call_count += other_cs.call_count;
                found = true;
                break;
            }
        }
        if (!found) {
            call_sites.push_back(other_cs);
        }
    }
}

auto ProfileData::get_function(const std::string& name) const -> const FunctionProfile* {
    for (const auto& func : functions) {
        if (func.name == name) {
            return &func;
        }
    }
    return nullptr;
}

auto ProfileData::get_hot_functions(uint64_t threshold) const
    -> std::vector<const FunctionProfile*> {
    std::vector<const FunctionProfile*> hot;
    for (const auto& func : functions) {
        if (func.is_hot(threshold)) {
            hot.push_back(&func);
        }
    }
    std::sort(hot.begin(), hot.end(), [](const FunctionProfile* a, const FunctionProfile* b) {
        return a->call_count > b->call_count;
    });
    return hot;
}

auto ProfileData::get_hot_call_sites(uint64_t threshold) const
    -> std::vector<const CallSiteProfile*> {
    std::vector<const CallSiteProfile*> hot;
    for (const auto& cs : call_sites) {
        if (cs.is_hot(threshold)) {
            hot.push_back(&cs);
        }
    }
    std::sort(hot.begin(), hot.end(), [](const CallSiteProfile* a, const CallSiteProfile* b) {
        return a->call_count > b->call_count;
    });
    return hot;
}

// ============================================================================
// ProfileIO Implementation
// ============================================================================

auto ProfileIO::read(const std::string& path) -> std::optional<ProfileData> {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    ProfileData data;
    std::string line;

    // Simple text format: VERSION <version>
    //                     MODULE <name>
    //                     FUNCTION <name> <call_count> <cycles>
    //                     BLOCK <id> <count>
    //                     EDGE <from> <to> <count>
    //                     CALLSITE <caller> <callee> <block> <inst> <count>

    FunctionProfile* current_func = nullptr;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "VERSION") {
            iss >> data.version;
        } else if (token == "MODULE") {
            iss >> data.module_name;
        } else if (token == "SAMPLES") {
            iss >> data.total_samples;
        } else if (token == "FUNCTION") {
            FunctionProfile func;
            iss >> func.name >> func.call_count >> func.total_cycles;
            data.functions.push_back(func);
            current_func = &data.functions.back();
        } else if (token == "BLOCK" && current_func) {
            BlockProfile block;
            iss >> block.block_id >> block.execution_count;
            current_func->blocks.push_back(block);
        } else if (token == "EDGE" && current_func) {
            EdgeProfile edge;
            iss >> edge.from_block >> edge.to_block >> edge.count;
            current_func->edges.push_back(edge);
        } else if (token == "CALLSITE") {
            CallSiteProfile cs;
            iss >> cs.caller >> cs.callee >> cs.block_id >> cs.inst_index >> cs.call_count;
            data.call_sites.push_back(cs);
        }
    }

    return data;
}

auto ProfileIO::write(const std::string& path, const ProfileData& data) -> bool {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "VERSION " << data.version << "\n";
    file << "MODULE " << data.module_name << "\n";
    file << "SAMPLES " << data.total_samples << "\n";

    for (const auto& func : data.functions) {
        file << "FUNCTION " << func.name << " " << func.call_count << " " << func.total_cycles
             << "\n";
        for (const auto& block : func.blocks) {
            file << "BLOCK " << block.block_id << " " << block.execution_count << "\n";
        }
        for (const auto& edge : func.edges) {
            file << "EDGE " << edge.from_block << " " << edge.to_block << " " << edge.count << "\n";
        }
    }

    for (const auto& cs : data.call_sites) {
        file << "CALLSITE " << cs.caller << " " << cs.callee << " " << cs.block_id << " "
             << cs.inst_index << " " << cs.call_count << "\n";
    }

    return true;
}

auto ProfileIO::merge(const std::vector<std::string>& paths) -> std::optional<ProfileData> {
    if (paths.empty()) {
        return std::nullopt;
    }

    auto result = read(paths[0]);
    if (!result) {
        return std::nullopt;
    }

    for (size_t i = 1; i < paths.size(); i++) {
        auto other = read(paths[i]);
        if (other) {
            result->merge(*other);
        }
    }

    return result;
}

auto ProfileIO::validate(const ProfileData& data, const Module& module) -> bool {
    // Check that profiled functions exist in the module
    for (const auto& func_profile : data.functions) {
        bool found = false;
        for (const auto& func : module.functions) {
            if (func.name == func_profile.name) {
                found = true;
                // Validate block IDs
                for (const auto& block_profile : func_profile.blocks) {
                    if (block_profile.block_id >= func.blocks.size()) {
                        return false; // Invalid block ID
                    }
                }
                break;
            }
        }
        if (!found) {
            // Function no longer exists - this is a warning, not an error
        }
    }
    return true;
}

// ============================================================================
// ProfileInstrumentationPass Implementation
// ============================================================================

auto ProfileInstrumentationPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    for (auto& func : module.functions) {
        if (instrument_function(func)) {
            stats_.functions_profiled++;
            changed = true;
        }
    }

    return changed;
}

auto ProfileInstrumentationPass::instrument_function(Function& func) -> bool {
    // Skip empty functions
    if (func.blocks.empty()) {
        return false;
    }

    // Add counters (placeholder - actual implementation would insert counter instructions)
    add_block_counters(func);
    add_edge_counters(func);
    add_call_counters(func);

    return true;
}

void ProfileInstrumentationPass::add_block_counters(Function& func) {
    // Would add a counter increment at the beginning of each block
    // For now, just track stats
    for (size_t i = 0; i < func.blocks.size(); i++) {
        // Would insert: call @__pgo_block_counter(func_id, block_id)
        (void)i;
    }
}

void ProfileInstrumentationPass::add_edge_counters(Function& func) {
    // Would add edge counters for branches
    for (auto& block : func.blocks) {
        if (block.terminator) {
            if (std::holds_alternative<CondBranchTerm>(*block.terminator)) {
                // Would insert edge counter before the branch
                stats_.branch_hints_applied++;
            }
        }
    }
}

void ProfileInstrumentationPass::add_call_counters(Function& func) {
    // Would add call site counters
    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); i++) {
            if (std::holds_alternative<CallInst>(block.instructions[i].inst)) {
                // Would insert: call @__pgo_call_counter(caller, callee, site_id)
            }
        }
    }
}

// ============================================================================
// PgoInliningPass Implementation
// ============================================================================

auto PgoInliningPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    // Get hot call sites
    auto hot_sites = profile_.get_hot_call_sites(hot_threshold_);

    for (const auto* cs : hot_sites) {
        if (should_inline(*cs)) {
            // Mark for inlining (actual inlining done by InliningPass)
            // For now, just track stats
            stats_.inlining_decisions++;
            changed = true;
        }
    }

    (void)module; // Will be used for actual inlining

    return changed;
}

auto PgoInliningPass::should_inline(const CallSiteProfile& cs) const -> bool {
    // Inline if:
    // 1. Call count exceeds threshold
    // 2. Callee is not too large
    // 3. Callee exists in profile (indicates it's meaningful)

    if (!cs.is_hot(hot_threshold_)) {
        return false;
    }

    // Check inline priority
    return get_inline_priority(cs.callee) > 0;
}

auto PgoInliningPass::get_inline_priority(const std::string& callee) const -> int {
    const auto* func = profile_.get_function(callee);
    if (!func) {
        return 0;
    }

    // Higher priority for:
    // - Hot functions
    // - Small functions (few blocks)
    // - Functions with few cycles

    int priority = 0;

    if (func->is_hot()) {
        priority += 10;
    }

    if (func->blocks.size() < 5) {
        priority += 5;
    }

    if (func->total_cycles < 1000) {
        priority += 3;
    }

    return priority;
}

// ============================================================================
// BranchProbabilityPass Implementation
// ============================================================================

auto BranchProbabilityPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    for (auto& func : module.functions) {
        const auto* fp = profile_.get_function(func.name);
        if (fp && apply_branch_hints(func, *fp)) {
            changed = true;
        }
    }

    return changed;
}

auto BranchProbabilityPass::apply_branch_hints(Function& func, const FunctionProfile& fp) -> bool {
    bool changed = false;

    for (size_t block_idx = 0; block_idx < func.blocks.size(); block_idx++) {
        auto& block = func.blocks[block_idx];

        if (block.terminator) {
            if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
                // Get edge counts
                uint64_t true_count =
                    fp.get_edge_count(static_cast<uint32_t>(block_idx), cond->true_block);
                uint64_t false_count =
                    fp.get_edge_count(static_cast<uint32_t>(block_idx), cond->false_block);

                if (true_count > 0 || false_count > 0) {
                    float prob = calculate_probability(true_count, false_count);

                    // Apply hint if branch is strongly biased
                    if (prob > 0.9f || prob < 0.1f) {
                        // Would set branch weight metadata
                        stats_.branch_hints_applied++;
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

auto BranchProbabilityPass::calculate_probability(uint64_t taken, uint64_t not_taken) const
    -> float {
    uint64_t total = taken + not_taken;
    if (total == 0) {
        return 0.5f; // No data, assume 50/50
    }
    return static_cast<float>(taken) / static_cast<float>(total);
}

// ============================================================================
// BlockLayoutPass Implementation
// ============================================================================

auto BlockLayoutPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    for (auto& func : module.functions) {
        const auto* fp = profile_.get_function(func.name);
        if (fp && reorder_blocks(func, *fp)) {
            stats_.blocks_reordered++;
            changed = true;
        }
    }

    return changed;
}

auto BlockLayoutPass::reorder_blocks(Function& func, const FunctionProfile& fp) -> bool {
    if (func.blocks.size() < 2) {
        return false;
    }

    // Place hot successors after their predecessors
    place_hot_successors(func, fp);

    // Group cold blocks at end
    group_cold_blocks(func, fp);

    return true;
}

void BlockLayoutPass::place_hot_successors(Function& func, const FunctionProfile& fp) {
    // For each conditional branch, try to place the likely successor next
    for (size_t i = 0; i < func.blocks.size(); i++) {
        auto& block = func.blocks[i];
        if (block.terminator) {
            if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
                uint64_t true_count = fp.get_edge_count(static_cast<uint32_t>(i), cond->true_block);
                uint64_t false_count =
                    fp.get_edge_count(static_cast<uint32_t>(i), cond->false_block);

                // If true branch is taken more often and not already next
                if (true_count > false_count && cond->true_block != i + 1) {
                    // Would reorder blocks to place true_block immediately after
                    // This is a placeholder - actual reordering requires CFG updates
                }
            }
        }
    }
}

void BlockLayoutPass::group_cold_blocks(Function& func, const FunctionProfile& fp) {
    // Identify cold blocks (execution count below threshold)
    constexpr uint64_t cold_threshold = 10;

    std::vector<uint32_t> cold_blocks;
    for (size_t i = 0; i < func.blocks.size(); i++) {
        if (fp.get_block_count(static_cast<uint32_t>(i)) < cold_threshold) {
            cold_blocks.push_back(static_cast<uint32_t>(i));
        }
    }

    // Would move cold blocks to end of function
    // This is a placeholder - actual reordering requires CFG updates
    (void)cold_blocks;
}

// ============================================================================
// PgoPass Implementation (Combined Pass)
// ============================================================================

auto PgoPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    // Apply branch hints first
    if (enable_branch_hints_) {
        BranchProbabilityPass bp_pass(profile_);
        if (bp_pass.run(module)) {
            stats_.branch_hints_applied += bp_pass.get_stats().branch_hints_applied;
            changed = true;
        }
    }

    // Apply block layout
    if (enable_block_layout_) {
        BlockLayoutPass bl_pass(profile_);
        if (bl_pass.run(module)) {
            stats_.blocks_reordered += bl_pass.get_stats().blocks_reordered;
            changed = true;
        }
    }

    // Apply inlining hints (actual inlining done separately)
    if (enable_inlining_) {
        PgoInliningPass inline_pass(profile_);
        if (inline_pass.run(module)) {
            stats_.inlining_decisions += inline_pass.get_stats().inlining_decisions;
            changed = true;
        }
    }

    // Count hot/cold functions
    for (const auto& func : module.functions) {
        const auto* fp = profile_.get_function(func.name);
        if (fp) {
            stats_.functions_profiled++;
            if (fp->is_hot()) {
                stats_.hot_functions++;
            } else {
                stats_.cold_functions++;
            }
        }
    }

    return changed;
}

} // namespace tml::mir
