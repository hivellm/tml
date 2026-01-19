//! # Function Inlining Pass
//!
//! This pass replaces function calls with the callee's body.
//!
//! ## Inlining Decisions
//!
//! | Attribute       | Behavior                           |
//! |-----------------|------------------------------------|
//! | @inline         | Always inline (up to recursion)    |
//! | @always_inline  | Always inline                      |
//! | @noinline       | Never inline                       |
//! | @never_inline   | Never inline                       |
//! | (none)          | Heuristic-based decision           |
//!
//! ## Optimization Levels
//!
//! | Level | Threshold Multiplier |
//! |-------|---------------------|
//! | -O0   | No inlining         |
//! | -O1   | 1x (conservative)   |
//! | -O2   | 2x (moderate)       |
//! | -O3   | 4x (aggressive)     |
//!
//! ## Inlining Process
//!
//! 1. Build call graph and function map
//! 2. Analyze each call site for cost/benefit
//! 3. Clone callee body with value remapping
//! 4. Replace call with inlined code
//! 5. Iterate until no more inlining possible

#include "mir/passes/inlining.hpp"

#include "mir/passes/pgo.hpp"

#include <algorithm>
#include <unordered_set>

namespace tml::mir {

// ============================================================================
// Helper Functions
// ============================================================================

// Check if function has @inline or @always_inline attribute
static auto has_inline_attr(const Function& func) -> bool {
    for (const auto& attr : func.attributes) {
        if (attr == "inline" || attr == "always_inline") {
            return true;
        }
    }
    return false;
}

// Check if function has @noinline or @never_inline attribute
static auto has_noinline_attr(const Function& func) -> bool {
    for (const auto& attr : func.attributes) {
        if (attr == "noinline" || attr == "never_inline") {
            return true;
        }
    }
    return false;
}

// Rebuild predecessors and successors from terminators
// This MUST be called after any transformation that modifies block structure
static void rebuild_cfg(Function& func) {
    // Build block ID to index map
    std::unordered_map<uint32_t, size_t> block_id_to_idx;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_id_to_idx[func.blocks[i].id] = i;
    }

    // Clear all predecessors and successors
    for (auto& block : func.blocks) {
        block.predecessors.clear();
        block.successors.clear();
    }

    // Rebuild from terminators
    for (auto& block : func.blocks) {
        if (!block.terminator)
            continue;

        std::vector<uint32_t> succs;
        std::visit(
            [&succs](const auto& t) {
                using T = std::decay_t<decltype(t)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    succs.push_back(t.target);
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    succs.push_back(t.true_block);
                    succs.push_back(t.false_block);
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    succs.push_back(t.default_block);
                    for (const auto& [_, target] : t.cases) {
                        succs.push_back(target);
                    }
                }
                // ReturnTerm and UnreachableTerm have no successors
            },
            *block.terminator);

        for (uint32_t succ_id : succs) {
            block.successors.push_back(succ_id);

            // Add this block as predecessor of successor
            auto it = block_id_to_idx.find(succ_id);
            if (it != block_id_to_idx.end()) {
                func.blocks[it->second].predecessors.push_back(block.id);
            }
        }
    }
}

// Reorder blocks in reverse post-order (RPO) which approximates dominance order.
// This ensures that for acyclic CFG edges, the definition block comes before use blocks.
// For loops, back-edges will point to blocks with lower indices.
// This is CRITICAL for passes like LICM and ConstantHoist that use block indices
// as a proxy for dominance.
static void reorder_blocks_rpo(Function& func) {
    if (func.blocks.empty())
        return;

    // Build block ID to index map
    std::unordered_map<uint32_t, size_t> block_id_to_idx;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_id_to_idx[func.blocks[i].id] = i;
    }

    // DFS to compute post-order
    std::vector<uint32_t> post_order;
    std::unordered_set<uint32_t> visited;
    std::vector<std::pair<uint32_t, size_t>> stack; // (block_id, next_successor_index)

    // Start from entry block (first block)
    uint32_t entry_id = func.blocks[0].id;
    stack.push_back({entry_id, 0});
    visited.insert(entry_id);

    while (!stack.empty()) {
        auto& [block_id, succ_idx] = stack.back();

        auto idx_it = block_id_to_idx.find(block_id);
        if (idx_it == block_id_to_idx.end()) {
            stack.pop_back();
            continue;
        }

        const auto& block = func.blocks[idx_it->second];
        const auto& succs = block.successors;

        // Find next unvisited successor
        while (succ_idx < succs.size() && visited.count(succs[succ_idx])) {
            succ_idx++;
        }

        if (succ_idx < succs.size()) {
            // Visit this successor
            uint32_t succ_id = succs[succ_idx];
            succ_idx++; // Move to next successor for when we return
            visited.insert(succ_id);
            stack.push_back({succ_id, 0});
        } else {
            // All successors visited, add to post-order
            post_order.push_back(block_id);
            stack.pop_back();
        }
    }

    // Reverse post-order is the dominance order
    std::reverse(post_order.begin(), post_order.end());

    // Add any unreachable blocks at the end (shouldn't happen normally)
    for (const auto& block : func.blocks) {
        if (!visited.count(block.id)) {
            post_order.push_back(block.id);
        }
    }

    // Reorder blocks according to RPO
    std::vector<BasicBlock> new_blocks;
    new_blocks.reserve(func.blocks.size());

    for (uint32_t block_id : post_order) {
        auto idx_it = block_id_to_idx.find(block_id);
        if (idx_it != block_id_to_idx.end()) {
            new_blocks.push_back(std::move(func.blocks[idx_it->second]));
        }
    }

    func.blocks = std::move(new_blocks);
}

// Check if function is directly recursive (calls itself)
static auto is_directly_recursive(const Function& func) -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                if (call->func_name == func.name) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Check if a function name is a constructor (ends with _new or _new_*)
static auto is_constructor_name(const std::string& name) -> bool {
    // Pattern: ClassName_new or ClassName_new_variant
    auto pos = name.rfind("_new");
    if (pos == std::string::npos) {
        return false;
    }
    // Must be at end or followed by underscore (for variants like _new_default)
    return pos + 4 == name.length() || name[pos + 4] == '_';
}

// Check if a call is to a base class constructor (for constructor chaining)
static auto is_base_constructor_call(const CallInst& call, const Function& caller) -> bool {
    // If caller is a constructor and callee is also a constructor
    // This indicates base constructor call in inheritance chain
    if (!is_constructor_name(caller.name)) {
        return false;
    }
    return is_constructor_name(call.func_name);
}

// Check if a function is a single-expression method (getter/setter pattern)
// These methods are trivial and should always be inlined for OOP performance
static auto is_single_expression_method(const Function& func, int max_size) -> bool {
    if (func.blocks.empty()) {
        return false;
    }

    // Count total instructions (excluding terminators)
    int total_instructions = 0;
    for (const auto& block : func.blocks) {
        total_instructions += static_cast<int>(block.instructions.size());
    }

    // Single-expression methods have few instructions
    if (total_instructions > max_size) {
        return false;
    }

    // Should have exactly one block with a return terminator
    if (func.blocks.size() == 1) {
        const auto& block = func.blocks[0];
        if (block.terminator && std::holds_alternative<ReturnTerm>(*block.terminator)) {
            // This is a single-block function that just returns
            // Patterns: getter (load + return), setter (store + return), trivial (return constant)
            return true;
        }
    }

    return false;
}

// ============================================================================
// InliningPass Implementation
// ============================================================================

auto InliningPass::run(Module& module) -> bool {
    bool changed = false;
    stats_ = InliningStats{};
    inline_counter_ = 0; // Reset inline counter for this run

    // Build call graph and function map
    build_call_graph(module);

    // Adjust thresholds based on optimization level
    int threshold_multiplier = 1;
    switch (options_.optimization_level) {
    case 0:
        threshold_multiplier = 0; // No inlining at -O0
        break;
    case 1:
        threshold_multiplier = 1;
        break;
    case 2:
        threshold_multiplier = 2;
        break;
    case 3:
        threshold_multiplier = 4;
        break;
    default:
        threshold_multiplier = 2;
    }

    if (threshold_multiplier == 0) {
        return false; // No inlining at -O0
    }

    // Iterate until no more inlining possible
    bool made_progress = true;
    int iteration = 0;
    const int max_iterations = 5;

    while (made_progress && iteration < max_iterations) {
        made_progress = false;
        iteration++;

        for (auto& func : module.functions) {
            // Process each block by index (blocks may be added during inlining)
            // Only process original blocks in this iteration
            size_t original_block_count = func.blocks.size();
            for (size_t block_idx = 0; block_idx < original_block_count; ++block_idx) {
                // Re-fetch block reference since vector may have been modified
                auto& block = func.blocks[block_idx];

                for (size_t i = 0; i < block.instructions.size(); ++i) {
                    auto& inst = block.instructions[i];

                    // Check if this is a call instruction
                    auto* call = std::get_if<CallInst>(&inst.inst);
                    if (!call)
                        continue;

                    stats_.calls_analyzed++;

                    // Track devirtualized calls
                    bool is_devirt = call->is_devirtualized();
                    if (is_devirt) {
                        stats_.devirt_calls_analyzed++;
                    }

                    // Track constructor calls
                    bool is_ctor = is_constructor_name(call->func_name);
                    bool is_base_ctor = false;
                    if (is_ctor) {
                        stats_.constructor_calls_analyzed++;
                        is_base_ctor = is_base_constructor_call(*call, func);
                    }

                    // Find the callee
                    auto callee_it = function_map_.find(call->func_name);
                    if (callee_it == function_map_.end()) {
                        stats_.no_definition++;
                        continue;
                    }

                    const Function* callee = callee_it->second;

                    // Check for @noinline attribute
                    if (has_noinline_attr(*callee)) {
                        stats_.never_inline++;
                        continue;
                    }

                    // Skip directly recursive functions (causes code explosion)
                    // For devirtualized calls, also check the original method name
                    // to prevent recursive virtual method inlining
                    if (is_directly_recursive(*callee)) {
                        stats_.recursive_limit_hit++;
                        continue;
                    }

                    // Check for @inline attribute or single-expression methods
                    // Single-expression methods (getters/setters) are always inlined for OOP
                    // performance
                    bool force_inline = has_inline_attr(*callee);
                    if (!force_inline && options_.always_inline_single_expr) {
                        force_inline =
                            is_single_expression_method(*callee, options_.single_expr_max_size);
                    }
                    if (force_inline) {
                        // Check recursive limit even for @inline
                        std::string call_key = func.name + "->" + callee->name;
                        if (inline_depth_[call_key] >= options_.recursive_limit) {
                            stats_.recursive_limit_hit++;
                            continue;
                        }

                        inline_depth_[call_key]++;
                        int callee_size = count_instructions(*callee);

                        if (inline_call(func, func.blocks[block_idx], i, *callee)) {
                            changed = true;
                            made_progress = true;
                            stats_.calls_inlined++;
                            stats_.always_inline++;
                            stats_.total_instructions_inlined += static_cast<size_t>(callee_size);

                            // Track devirtualized call inlining statistics
                            if (is_devirt) {
                                stats_.devirt_calls_inlined++;
                                const auto& devirt = *call->devirt_info;
                                if (devirt.from_sealed_class) {
                                    stats_.devirt_sealed_inlined++;
                                } else if (devirt.from_exact_type) {
                                    stats_.devirt_exact_inlined++;
                                } else if (devirt.from_single_impl) {
                                    stats_.devirt_single_inlined++;
                                }
                            }

                            // Track constructor call inlining statistics
                            if (is_ctor) {
                                stats_.constructor_calls_inlined++;
                                if (is_base_ctor) {
                                    stats_.base_constructor_inlined++;
                                }
                            }

                            // Rebuild CFG after inlining to ensure predecessors/successors are
                            // correct This is critical for passes like LICM that depend on CFG
                            // structure
                            rebuild_cfg(func);

                            // Restart block processing
                            i = static_cast<size_t>(-1);
                        }
                        continue;
                    }

                    // Check recursive limit
                    std::string call_key = func.name + "->" + callee->name;
                    if (inline_depth_[call_key] >= options_.recursive_limit) {
                        stats_.recursive_limit_hit++;
                        continue;
                    }

                    // Check callee size
                    int callee_size = count_instructions(*callee);
                    if (callee_size > options_.max_callee_size) {
                        stats_.too_large++;
                        continue;
                    }

                    // Analyze cost
                    InlineCost cost = analyze_cost(*call, *callee);
                    cost.threshold = calculate_threshold(func, *call) * threshold_multiplier;

                    if (cost.should_inline()) {
                        inline_depth_[call_key]++;

                        if (inline_call(func, func.blocks[block_idx], i, *callee)) {
                            changed = true;
                            made_progress = true;
                            stats_.calls_inlined++;
                            stats_.total_instructions_inlined += static_cast<size_t>(callee_size);

                            // Track devirtualized call inlining statistics
                            if (is_devirt) {
                                stats_.devirt_calls_inlined++;
                                const auto& devirt = *call->devirt_info;
                                if (devirt.from_sealed_class) {
                                    stats_.devirt_sealed_inlined++;
                                } else if (devirt.from_exact_type) {
                                    stats_.devirt_exact_inlined++;
                                } else if (devirt.from_single_impl) {
                                    stats_.devirt_single_inlined++;
                                }
                            }

                            // Track constructor call inlining statistics
                            if (is_ctor) {
                                stats_.constructor_calls_inlined++;
                                if (is_base_ctor) {
                                    stats_.base_constructor_inlined++;
                                }
                            }

                            // Rebuild CFG after inlining to ensure predecessors/successors are
                            // correct This is critical for passes like LICM that depend on CFG
                            // structure
                            rebuild_cfg(func);

                            // Restart block processing
                            i = static_cast<size_t>(-1);
                        }
                    } else {
                        stats_.calls_not_inlined++;
                    }
                }
            }
        }
    }

    // After all inlining is complete, reorder blocks in each modified function
    // to maintain the dominance order invariant. This is critical for passes like
    // LICM and ConstantHoist that use block indices as a proxy for dominance.
    if (changed) {
        for (auto& func : module.functions) {
            rebuild_cfg(func);
            reorder_blocks_rpo(func);
        }
    }

    return changed;
}

void InliningPass::build_call_graph(Module& module) {
    function_map_.clear();
    call_graph_.clear();
    inline_depth_.clear();

    // Build function map
    for (const auto& func : module.functions) {
        function_map_[func.name] = &func;
    }

    // Build call graph
    for (const auto& func : module.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    call_graph_[func.name].insert(call->func_name);
                }
            }
        }
    }
}

auto InliningPass::calculate_threshold([[maybe_unused]] const Function& caller,
                                       const CallInst& call) const -> int {
    int threshold = options_.base_threshold;

    // Bonus for small callees
    auto callee_it = function_map_.find(call.func_name);
    if (callee_it != function_map_.end()) {
        int size = count_instructions(*callee_it->second);
        if (size < 10) {
            threshold += 100; // Small functions get bonus
        } else if (size < 50) {
            threshold += 50;
        }
    }

    // Bonus for devirtualized calls - inlining eliminates vtable overhead
    if (call.is_devirtualized() && options_.prioritize_devirt) {
        const auto& devirt = *call.devirt_info;
        threshold += options_.devirt_bonus;

        // Extra bonus based on devirtualization reason
        if (devirt.from_exact_type) {
            threshold += options_.devirt_exact_bonus;
        } else if (devirt.from_sealed_class) {
            threshold += options_.devirt_sealed_bonus;
        }
    }

    // Bonus for constructor calls - inlining constructors:
    // - Enables field initialization optimization (SROA can break up struct)
    // - Allows vtable pointer store elimination for stack-allocated objects
    // - Enables constant propagation of initial values
    if (options_.prioritize_constructors && is_constructor_name(call.func_name)) {
        threshold += options_.constructor_bonus;

        // Extra bonus for base constructor calls (inheritance chains)
        // Inlining entire chain exposes optimization opportunities
        if (is_base_constructor_call(call, caller)) {
            threshold += options_.base_constructor_bonus;
        }
    }

    // PGO bonus for hot call sites
    if (profile_data_ && options_.inline_hot) {
        // Look up call site in profile data
        for (const auto& cs : profile_data_->call_sites) {
            if (cs.caller == caller.name && cs.callee == call.func_name) {
                if (cs.call_count >= options_.pgo_hot_threshold) {
                    // Hot call site: significantly increase threshold
                    threshold += options_.pgo_hot_bonus;
                } else if (options_.pgo_skip_cold &&
                           cs.call_count < options_.pgo_hot_threshold / 10) {
                    // Cold call site: significantly reduce threshold
                    threshold -= options_.base_threshold / 2;
                }
                break;
            }
        }
    }

    return threshold;
}

auto InliningPass::get_call_site_profile(const std::string& caller, const std::string& callee,
                                         uint32_t block_id, size_t inst_index) const
    -> const CallSiteProfile* {
    if (!profile_data_) {
        return nullptr;
    }

    for (const auto& cs : profile_data_->call_sites) {
        if (cs.caller == caller && cs.callee == callee && cs.block_id == block_id &&
            cs.inst_index == inst_index) {
            return &cs;
        }
    }
    return nullptr;
}

auto InliningPass::count_instructions(const Function& func) const -> int {
    int count = 0;
    for (const auto& block : func.blocks) {
        count += static_cast<int>(block.instructions.size());
        if (block.terminator) {
            count++; // Count terminator
        }
    }
    return count;
}

auto InliningPass::analyze_cost(const CallInst& call, const Function& callee) const -> InlineCost {
    InlineCost cost;

    // Base cost is instruction count
    cost.instruction_cost = count_instructions(callee);

    // Call overhead saved
    cost.call_overhead_saved = options_.call_penalty;

    // Additional savings for devirtualized calls (vtable lookup overhead)
    // Inlining devirtualized calls eliminates:
    // - Load vtable pointer from object
    // - Load function pointer from vtable
    // - Indirect call (harder to predict)
    if (call.is_devirtualized()) {
        constexpr int vtable_load_cost = 5;    // Loading vtable pointer
        constexpr int func_ptr_load_cost = 5;  // Loading function from vtable
        constexpr int indirect_call_cost = 10; // Branch prediction penalty
        cost.call_overhead_saved += vtable_load_cost + func_ptr_load_cost + indirect_call_cost;
    }

    // Additional savings for constructor inlining
    // Inlining constructors exposes field initialization to optimization:
    // - SROA can decompose struct into scalars
    // - Constant propagation can optimize initial values
    // - Dead store elimination can remove unused fields
    if (is_constructor_name(call.func_name)) {
        constexpr int field_init_exposure = 15; // Benefit from exposing field inits
        constexpr int alloca_exposure = 10;     // Potential stack allocation optimization
        cost.call_overhead_saved += field_init_exposure + alloca_exposure;
    }

    // Size increase estimate
    cost.size_increase = cost.instruction_cost;

    return cost;
}

auto InliningPass::inline_call(Function& caller, BasicBlock& block, size_t call_index,
                               const Function& callee) -> bool {
    // Get the call instruction
    if (call_index >= block.instructions.size())
        return false;

    auto& inst = block.instructions[call_index];
    auto* call = std::get_if<CallInst>(&inst.inst);
    if (!call)
        return false;

    // Don't inline empty functions or extern functions
    if (callee.blocks.empty())
        return false;

    // CRITICAL: Capture original block ID BEFORE any modifications to caller.blocks
    // because push_back may cause reallocation and invalidate the block reference.
    uint32_t original_block_id = block.id;

    // Clone callee's body with unique inline ID
    int inline_id = inline_counter_++;
    auto inlined_blocks = clone_function_body(callee, caller.next_value_id, call->args, inline_id);

    if (inlined_blocks.empty())
        return false;

    // Update caller's next_value_id based on cloned instructions
    ValueId max_value_id = caller.next_value_id;
    for (const auto& cloned_block : inlined_blocks) {
        for (const auto& cloned_inst : cloned_block.instructions) {
            if (cloned_inst.result != INVALID_VALUE && cloned_inst.result >= max_value_id) {
                max_value_id = cloned_inst.result + 1;
            }
        }
    }
    caller.next_value_id = max_value_id;

    // Find the block index in caller
    size_t block_index = 0;
    for (size_t i = 0; i < caller.blocks.size(); ++i) {
        if (&caller.blocks[i] == &block) {
            block_index = i;
            break;
        }
    }

    // Create a continuation block for instructions after the call
    BasicBlock continuation_block;
    continuation_block.id = caller.next_block_id++;
    continuation_block.name = block.name + "_cont";

    // Move instructions after the call to the continuation block
    for (size_t i = call_index + 1; i < block.instructions.size(); ++i) {
        continuation_block.instructions.push_back(std::move(block.instructions[i]));
    }
    block.instructions.resize(call_index); // Remove call and everything after

    // Move the original terminator to the continuation block
    continuation_block.terminator = block.terminator;
    block.terminator = std::nullopt;

    // Assign IDs to inlined blocks and build block ID remapping
    std::unordered_map<uint32_t, uint32_t> block_id_map;
    uint32_t inline_entry_id = caller.next_block_id;
    for (size_t i = 0; i < inlined_blocks.size(); ++i) {
        uint32_t old_id = inlined_blocks[i].id;
        uint32_t new_id = caller.next_block_id++;
        block_id_map[old_id] = new_id;
        inlined_blocks[i].id = new_id;
    }

    // Remap block IDs in phi nodes (critical for loop back-edges)
    for (auto& inlined_block : inlined_blocks) {
        for (auto& phi_inst : inlined_block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&phi_inst.inst)) {
                for (auto& [val, from_block] : phi->incoming) {
                    auto it = block_id_map.find(from_block);
                    if (it != block_id_map.end()) {
                        from_block = it->second;
                    }
                }
            }
        }
    }

    // Remap block references in terminators of inlined blocks
    // and convert returns to branches to continuation
    ValueId call_result_id = inst.result;
    MirTypePtr return_type = callee.return_type;
    std::vector<std::pair<Value, uint32_t>> phi_inputs; // For return value collection

    for (auto& inlined_block : inlined_blocks) {
        if (!inlined_block.terminator)
            continue;

        std::visit(
            [&](auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    auto it = block_id_map.find(term.target);
                    if (it != block_id_map.end()) {
                        term.target = it->second;
                    } else {
                        // Target block not found in callee - branch to continuation
                        term.target = continuation_block.id;
                    }
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    auto it_true = block_id_map.find(term.true_block);
                    if (it_true != block_id_map.end()) {
                        term.true_block = it_true->second;
                    } else {
                        // True target not found - branch to continuation
                        term.true_block = continuation_block.id;
                    }
                    auto it_false = block_id_map.find(term.false_block);
                    if (it_false != block_id_map.end()) {
                        term.false_block = it_false->second;
                    } else {
                        // False target not found - branch to continuation
                        term.false_block = continuation_block.id;
                    }
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    auto it_default = block_id_map.find(term.default_block);
                    if (it_default != block_id_map.end()) {
                        term.default_block = it_default->second;
                    } else {
                        term.default_block = continuation_block.id;
                    }
                    for (auto& case_block : term.cases) {
                        auto it_case = block_id_map.find(case_block.second);
                        if (it_case != block_id_map.end()) {
                            case_block.second = it_case->second;
                        } else {
                            case_block.second = continuation_block.id;
                        }
                    }
                } else if constexpr (std::is_same_v<T, ReturnTerm>) {
                    // Convert return to branch to continuation
                    // Collect return value for phi node
                    if (term.value && call_result_id != INVALID_VALUE) {
                        phi_inputs.push_back({*term.value, inlined_block.id});
                    }
                    // Replace return with branch to continuation
                    inlined_block.terminator = BranchTerm{continuation_block.id};
                }
            },
            *inlined_block.terminator);
    }

    // Connect the original block to the inlined entry block
    block.terminator = BranchTerm{inline_entry_id};

    // If the call had a result, create a phi node at the start of continuation
    if (call_result_id != INVALID_VALUE && !phi_inputs.empty()) {
        PhiInst phi;
        phi.incoming = phi_inputs;
        phi.result_type = return_type;

        InstructionData phi_inst;
        phi_inst.result = call_result_id;
        phi_inst.type = return_type;
        phi_inst.inst = phi;

        // Insert phi at the beginning of continuation block
        continuation_block.instructions.insert(continuation_block.instructions.begin(), phi_inst);
    }

    // Add all inlined blocks to the function
    for (auto& inlined_block : inlined_blocks) {
        caller.blocks.push_back(std::move(inlined_block));
    }

    // Add the continuation block
    // Note: original_block_id was captured at the start of the function to avoid
    // reading from potentially invalidated reference after push_back
    uint32_t continuation_block_id = continuation_block.id;
    caller.blocks.push_back(std::move(continuation_block));

    // CRITICAL: Update PHIs in other blocks that reference the original block
    // Since we moved the terminator to the continuation block, any PHIs that
    // expected edges from the original block now need to reference the continuation
    int phi_updates = 0;
    for (auto& caller_block : caller.blocks) {
        // Skip the original block and continuation block themselves
        if (caller_block.id == original_block_id || caller_block.id == continuation_block_id) {
            continue;
        }

        for (auto& phi_inst : caller_block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&phi_inst.inst)) {
                for (auto& [val, from_block] : phi->incoming) {
                    // If the PHI references the original block, update to continuation
                    if (from_block == original_block_id) {
                        from_block = continuation_block_id;
                        phi_updates++;
                    }
                }
            }
        }
    }
    // Debug: print if we made any updates (disabled)
    (void)phi_updates; // Suppress unused warning

    return true;
}

auto InliningPass::clone_function_body(const Function& callee, ValueId first_new_id,
                                       const std::vector<Value>& args, int inline_id)
    -> std::vector<BasicBlock> {
    std::vector<BasicBlock> cloned;

    // Build value remapping from callee parameters to call arguments
    std::unordered_map<ValueId, ValueId> value_map;
    // Also build type map for parameter -> argument type propagation
    // This is critical for ExtractValueInst to use the correct aggregate type
    std::unordered_map<ValueId, MirTypePtr> type_map;
    for (size_t i = 0; i < callee.params.size() && i < args.size(); ++i) {
        value_map[callee.params[i].value_id] = args[i].id;
        if (args[i].type) {
            type_map[callee.params[i].value_id] = args[i].type;
        }
    }

    // Generate new value IDs for all instructions
    ValueId next_id = first_new_id;
    for (const auto& block : callee.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                value_map[inst.result] = next_id++;
            }
        }
    }

    // Clone each block with unique names using inline_id
    for (const auto& block : callee.blocks) {
        BasicBlock new_block;
        new_block.id = block.id; // Will be remapped by caller
        new_block.name = "inline" + std::to_string(inline_id) + "_" + block.name;

        // Clone instructions
        for (const auto& inst : block.instructions) {
            InstructionData new_inst = inst;

            // Remap result
            if (inst.result != INVALID_VALUE) {
                auto it = value_map.find(inst.result);
                if (it != value_map.end()) {
                    new_inst.result = it->second;
                }
            }

            // Remap operands
            std::visit(
                [&value_map, &type_map](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        auto it = value_map.find(i.left.id);
                        if (it != value_map.end())
                            i.left.id = it->second;
                        it = value_map.find(i.right.id);
                        if (it != value_map.end())
                            i.right.id = it->second;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        auto it = value_map.find(i.operand.id);
                        if (it != value_map.end())
                            i.operand.id = it->second;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        auto it = value_map.find(i.ptr.id);
                        if (it != value_map.end())
                            i.ptr.id = it->second;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        auto it = value_map.find(i.ptr.id);
                        if (it != value_map.end())
                            i.ptr.id = it->second;
                        it = value_map.find(i.value.id);
                        if (it != value_map.end())
                            i.value.id = it->second;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            auto it = value_map.find(arg.id);
                            if (it != value_map.end())
                                arg.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        auto it = value_map.find(i.condition.id);
                        if (it != value_map.end())
                            i.condition.id = it->second;
                        it = value_map.find(i.true_val.id);
                        if (it != value_map.end())
                            i.true_val.id = it->second;
                        it = value_map.find(i.false_val.id);
                        if (it != value_map.end())
                            i.false_val.id = it->second;
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        // Remap receiver
                        auto it = value_map.find(i.receiver.id);
                        if (it != value_map.end())
                            i.receiver.id = it->second;
                        // Remap arguments
                        for (auto& arg : i.args) {
                            it = value_map.find(arg.id);
                            if (it != value_map.end())
                                arg.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        auto it = value_map.find(i.base.id);
                        if (it != value_map.end())
                            i.base.id = it->second;
                        for (auto& idx : i.indices) {
                            it = value_map.find(idx.id);
                            if (it != value_map.end())
                                idx.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        // Save original ID for type lookup before remapping
                        ValueId orig_id = i.aggregate.id;
                        auto it = value_map.find(orig_id);
                        if (it != value_map.end())
                            i.aggregate.id = it->second;
                        // Propagate type from argument to aggregate
                        // This is critical for correct LLVM IR generation when
                        // inlining methods - the aggregate type must match the
                        // actual receiver type, not the parameter type
                        auto type_it = type_map.find(orig_id);
                        if (type_it != type_map.end()) {
                            i.aggregate.type = type_it->second;
                            i.aggregate_type = type_it->second;
                        }
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        ValueId orig_agg_id = i.aggregate.id;
                        auto it = value_map.find(orig_agg_id);
                        if (it != value_map.end())
                            i.aggregate.id = it->second;
                        it = value_map.find(i.value.id);
                        if (it != value_map.end())
                            i.value.id = it->second;
                        // Propagate type for aggregate
                        auto type_it = type_map.find(orig_agg_id);
                        if (type_it != type_map.end()) {
                            i.aggregate.type = type_it->second;
                            i.aggregate_type = type_it->second;
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        auto it = value_map.find(i.operand.id);
                        if (it != value_map.end())
                            i.operand.id = it->second;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, block_id] : i.incoming) {
                            auto it = value_map.find(val.id);
                            if (it != value_map.end())
                                val.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (auto& field : i.fields) {
                            auto it = value_map.find(field.id);
                            if (it != value_map.end())
                                field.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (auto& p : i.payload) {
                            auto it = value_map.find(p.id);
                            if (it != value_map.end())
                                p.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (auto& elem : i.elements) {
                            auto it = value_map.find(elem.id);
                            if (it != value_map.end())
                                elem.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (auto& elem : i.elements) {
                            auto it = value_map.find(elem.id);
                            if (it != value_map.end())
                                elem.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        auto it = value_map.find(i.poll_value.id);
                        if (it != value_map.end())
                            i.poll_value.id = it->second;
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (auto& [name, val] : i.captures) {
                            auto it = value_map.find(val.id);
                            if (it != value_map.end())
                                val.id = it->second;
                        }
                    }
                    // AllocaInst and ConstantInst don't have Value references
                },
                new_inst.inst);

            new_block.instructions.push_back(new_inst);
        }

        // Clone terminator
        if (block.terminator) {
            new_block.terminator = *block.terminator;

            // Remap values in terminator
            std::visit(
                [&value_map](auto& term) {
                    using T = std::decay_t<decltype(term)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (term.value) {
                            auto it = value_map.find(term.value->id);
                            if (it != value_map.end()) {
                                term.value->id = it->second;
                            }
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        auto it = value_map.find(term.condition.id);
                        if (it != value_map.end()) {
                            term.condition.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        auto it = value_map.find(term.discriminant.id);
                        if (it != value_map.end()) {
                            term.discriminant.id = it->second;
                        }
                    }
                },
                *new_block.terminator);
        }

        cloned.push_back(std::move(new_block));
    }

    return cloned;
}

void InliningPass::remap_values(
    [[maybe_unused]] std::vector<BasicBlock>& blocks,
    [[maybe_unused]] const std::unordered_map<ValueId, ValueId>& value_map) {
    // Already done in clone_function_body
}

auto InliningPass::get_decision(const std::string& caller, const std::string& callee) const
    -> InlineDecision {
    auto it = function_map_.find(callee);
    if (it == function_map_.end()) {
        return InlineDecision::NoDefinition;
    }

    const Function* func = it->second;

    // Check for @noinline attribute
    if (has_noinline_attr(*func)) {
        return InlineDecision::NeverInline;
    }

    // Check for @inline attribute
    if (has_inline_attr(*func)) {
        return InlineDecision::AlwaysInline;
    }

    std::string key = caller + "->" + callee;
    auto depth_it = inline_depth_.find(key);
    if (depth_it != inline_depth_.end() && depth_it->second >= options_.recursive_limit) {
        return InlineDecision::RecursiveLimit;
    }

    if (count_instructions(*func) > options_.max_callee_size) {
        return InlineDecision::TooLarge;
    }

    return InlineDecision::Inline;
}

// ============================================================================
// AlwaysInlinePass Implementation
// ============================================================================

auto AlwaysInlinePass::run([[maybe_unused]] Module& module) -> bool {
    // This pass identifies @inline functions
    // Actual inlining is handled by InliningPass
    // For now, just return false (no changes)
    stats_ = InliningStats{};
    return false;
}

} // namespace tml::mir
