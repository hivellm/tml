//! # SIMD Vectorization Pass
//!
//! This module provides auto-vectorization for loops and SLP (Superword-Level
//! Parallelism) for straight-line code. It generates LLVM vector instructions
//! for improved performance on SIMD-capable hardware.
//!
//! ## Loop Vectorization
//!
//! Transforms scalar loops into vector operations:
//!
//! ```tml
//! // Before: scalar loop
//! loop i in 0 to 1024 {
//!     arr[i] = arr[i] * 2.0
//! }
//!
//! // After: vectorized (conceptual, 4-wide)
//! loop i in 0 to 1024 by 4 {
//!     vec = load <4 x f32> arr[i:i+4]
//!     vec = fmul <4 x f32> vec, <2.0, 2.0, 2.0, 2.0>
//!     store <4 x f32> vec, arr[i:i+4]
//! }
//! ```
//!
//! ## SLP Vectorization
//!
//! Combines adjacent scalar operations into vector operations:
//!
//! ```tml
//! // Before: separate operations
//! x = a + b
//! y = c + d
//! z = e + f
//! w = g + h
//!
//! // After: single vector operation
//! <x, y, z, w> = <a, c, e, g> + <b, d, f, h>
//! ```
//!
//! ## Reductions
//!
//! Handles reduction patterns (sum, product, min, max):
//!
//! ```tml
//! // Before: scalar reduction
//! let sum = 0
//! loop i in 0 to N {
//!     sum = sum + arr[i]
//! }
//!
//! // After: vectorized with horizontal reduction
//! // accumulate in vector, then reduce at end
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/loop_opts.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

// ============================================================================
// Vector Type Support
// ============================================================================

/// Supported vector element types
enum class VectorElementType {
    I8,  ///< 8-bit integer
    I16, ///< 16-bit integer
    I32, ///< 32-bit integer
    I64, ///< 64-bit integer
    F32, ///< 32-bit float
    F64, ///< 64-bit float
};

/// Vector width (number of lanes)
enum class VectorWidth {
    V2 = 2,   ///< 2 lanes (128-bit for f64)
    V4 = 4,   ///< 4 lanes (128-bit for f32, 256-bit for f64)
    V8 = 8,   ///< 8 lanes (256-bit for f32)
    V16 = 16, ///< 16 lanes (512-bit for f32)
};

/// Target vector register width
enum class TargetVectorWidth {
    SSE = 128,   ///< SSE: 128-bit vectors
    AVX = 256,   ///< AVX: 256-bit vectors
    AVX512 = 512 ///< AVX-512: 512-bit vectors
};

// ============================================================================
// Vectorization Statistics
// ============================================================================

/// Statistics collected during vectorization
struct VectorizationStats {
    size_t loops_analyzed = 0;         ///< Loops examined for vectorization
    size_t loops_vectorized = 0;       ///< Loops successfully vectorized
    size_t loops_not_vectorizable = 0; ///< Loops that couldn't be vectorized
    size_t reductions_vectorized = 0;  ///< Reduction patterns vectorized
    size_t slp_groups_found = 0;       ///< SLP opportunities found
    size_t slp_groups_vectorized = 0;  ///< SLP groups vectorized
    size_t vector_instructions = 0;    ///< Vector instructions generated

    // Reasons for not vectorizing
    size_t failed_memory_dep = 0;    ///< Memory dependence prevented
    size_t failed_control_flow = 0;  ///< Complex control flow prevented
    size_t failed_unknown_trip = 0;  ///< Unknown trip count
    size_t failed_alignment = 0;     ///< Alignment issues
    size_t failed_type_mismatch = 0; ///< Non-vectorizable types

    void reset() {
        *this = VectorizationStats{};
    }
};

// ============================================================================
// Memory Dependence Analysis
// ============================================================================

/// Memory access descriptor
struct MemoryAccess {
    ValueId ptr;       ///< Base pointer
    ValueId index;     ///< Index (for array access)
    bool is_read;      ///< True for load, false for store
    size_t inst_index; ///< Index in block
    uint32_t block_id; ///< Block containing access
};

/// Dependence type between two memory accesses
enum class DependenceType {
    None,    ///< No dependence
    True,    ///< RAW (Read After Write)
    Anti,    ///< WAR (Write After Read)
    Output,  ///< WAW (Write After Write)
    Unknown, ///< Unknown (conservative)
};

/// Distance in iterations between dependent accesses
struct DependenceDistance {
    DependenceType type = DependenceType::None;
    std::optional<int64_t> distance; ///< Distance in loop iterations (nullopt = unknown)
    bool loop_carried = false;       ///< True if crosses loop iteration boundary
};

/// Memory dependence analyzer for vectorization
class MemoryDependenceAnalysis {
public:
    /// Analyze memory dependences in a loop
    void analyze_loop(const Function& func, const LoopInfo& loop);

    /// Check dependence between two accesses
    [[nodiscard]] auto get_dependence(const MemoryAccess& a, const MemoryAccess& b) const
        -> DependenceDistance;

    /// Check if loop can be vectorized (no preventing dependences)
    [[nodiscard]] auto can_vectorize(size_t vector_width) const -> bool;

    /// Get all memory accesses in the loop
    [[nodiscard]] auto get_accesses() const -> const std::vector<MemoryAccess>& {
        return accesses_;
    }

    /// Clear analysis results
    void clear() {
        accesses_.clear();
        dependences_.clear();
        alloca_bases_.clear();
        gep_bases_.clear();
    }

private:
    std::vector<MemoryAccess> accesses_;
    std::vector<std::pair<size_t, DependenceDistance>> dependences_; // (access_idx, dep)

    /// Map from pointer ValueId to its alloca base (if from local variable)
    std::unordered_map<ValueId, ValueId> alloca_bases_;

    /// Map from GEP result to its base pointer
    std::unordered_map<ValueId, ValueId> gep_bases_;

    /// Check if two pointers may alias
    [[nodiscard]] auto may_alias(ValueId ptr1, ValueId ptr2) const -> bool;

    /// Compute distance for array accesses
    [[nodiscard]] auto compute_distance(const MemoryAccess& a, const MemoryAccess& b,
                                        const LoopInfo& loop) const -> std::optional<int64_t>;

    /// Get the ultimate base pointer for a value (following GEPs)
    [[nodiscard]] auto get_base_pointer(ValueId ptr) const -> ValueId;
};

// ============================================================================
// Reduction Detection
// ============================================================================

// ReductionOp is defined in mir/mir.hpp

/// Reduction pattern descriptor
struct ReductionInfo {
    ReductionOp op;          ///< Reduction operation
    ValueId accumulator;     ///< Accumulator variable (PHI node)
    ValueId init_value;      ///< Initial value
    ValueId update_value;    ///< Value being accumulated
    MirTypePtr element_type; ///< Type of elements being reduced
    bool is_ordered = false; ///< True if reduction must preserve order (FP strict)
};

// ============================================================================
// Loop Vectorization Pass
// ============================================================================

/// Configuration for loop vectorization
struct VectorizationConfig {
    TargetVectorWidth target_width = TargetVectorWidth::SSE; ///< Target vector width
    size_t vectorization_factor = 4;                         ///< Default vectorization factor
    size_t min_trip_count = 8;                               ///< Minimum trip count to vectorize
    bool vectorize_reductions = true;                        ///< Vectorize reduction patterns
    bool allow_unaligned = true;  ///< Allow unaligned vector loads/stores
    bool use_masked_ops = false;  ///< Use masked operations for tail
    bool force_vectorize = false; ///< Vectorize even if cost model says no
};

/// Loop vectorization pass
class LoopVectorizationPass : public FunctionPass {
public:
    explicit LoopVectorizationPass(VectorizationConfig config = {}) : config_(config) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "LoopVectorization";
    }

    [[nodiscard]] auto stats() const -> const VectorizationStats& {
        return stats_;
    }

    void set_config(const VectorizationConfig& config) {
        config_ = config;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    VectorizationConfig config_;
    VectorizationStats stats_;
    MemoryDependenceAnalysis mem_dep_;
    std::vector<LoopInfo> loops_;

    /// Analyze loops in function
    void analyze_loops(const Function& func);

    /// Check if a loop can be vectorized
    [[nodiscard]] auto can_vectorize_loop(const Function& func, const LoopInfo& loop) -> bool;

    /// Determine optimal vectorization factor
    [[nodiscard]] auto compute_vf(const Function& func, const LoopInfo& loop) const -> size_t;

    /// Vectorize a single loop
    auto vectorize_loop(Function& func, LoopInfo& loop, size_t vf) -> bool;

    /// Detect reduction patterns in loop
    [[nodiscard]] auto detect_reductions(const Function& func, const LoopInfo& loop)
        -> std::vector<ReductionInfo>;

    /// Vectorize a reduction
    auto vectorize_reduction(Function& func, LoopInfo& loop, const ReductionInfo& red, size_t vf)
        -> bool;

    /// Generate vector load instruction
    auto gen_vector_load(Function& func, BasicBlock& block, ValueId ptr, size_t vf,
                         MirTypePtr elem_type) -> ValueId;

    /// Generate vector store instruction
    void gen_vector_store(Function& func, BasicBlock& block, ValueId ptr, ValueId vec_val,
                          size_t vf, MirTypePtr elem_type);

    /// Generate vector binary operation
    auto gen_vector_binop(Function& func, BasicBlock& block, BinOp op, ValueId lhs, ValueId rhs,
                          size_t vf, MirTypePtr elem_type) -> ValueId;

    /// Generate horizontal reduction (vector -> scalar)
    auto gen_horizontal_reduce(Function& func, BasicBlock& block, ReductionOp op, ValueId vec,
                               size_t vf, MirTypePtr elem_type) -> ValueId;

    /// Check if instruction can be vectorized
    [[nodiscard]] auto is_vectorizable_inst(const InstructionData& inst) const -> bool;

    /// Check if type can be vectorized
    [[nodiscard]] auto is_vectorizable_type(MirTypePtr type) const -> bool;

    /// Get loop trip count (if known)
    [[nodiscard]] auto get_trip_count(const LoopInfo& loop) const -> std::optional<int64_t>;
};

// ============================================================================
// SLP (Superword-Level Parallelism) Vectorization Pass
// ============================================================================

/// A group of scalar instructions that can be combined into a vector op
struct SLPGroup {
    std::vector<const InstructionData*> insts; ///< Instructions in group
    size_t vector_width;                       ///< Width of resulting vector
    MirTypePtr element_type;                   ///< Element type
    bool is_load = false;                      ///< True if this is a load group
    bool is_store = false;                     ///< True if this is a store group
};

/// SLP vectorization pass
class SLPVectorizationPass : public FunctionPass {
public:
    explicit SLPVectorizationPass(VectorizationConfig config = {}) : config_(config) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "SLPVectorization";
    }

    [[nodiscard]] auto stats() const -> const VectorizationStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    VectorizationConfig config_;
    VectorizationStats stats_;

    /// Find groups of instructions that can be vectorized together
    [[nodiscard]] auto find_slp_groups(const Function& func, const BasicBlock& block)
        -> std::vector<SLPGroup>;

    /// Check if two instructions are isomorphic (same operation, different data)
    [[nodiscard]] auto are_isomorphic(const InstructionData& a, const InstructionData& b) const
        -> bool;

    /// Check if instructions access consecutive memory locations
    [[nodiscard]] auto
    are_consecutive_accesses(const Function& func,
                             const std::vector<const InstructionData*>& loads) const -> bool;

    /// Vectorize an SLP group
    auto vectorize_group(Function& func, BasicBlock& block, const SLPGroup& group) -> bool;
};

// ============================================================================
// Combined Vectorization Pass
// ============================================================================

/// Combined pass that runs loop vectorization and SLP
class VectorizationPass : public MirPass {
public:
    explicit VectorizationPass(VectorizationConfig config = {}) : config_(config) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "Vectorization";
    }

    auto run(Module& module) -> bool override;

    [[nodiscard]] auto stats() const -> const VectorizationStats& {
        return stats_;
    }

    void set_config(const VectorizationConfig& config) {
        config_ = config;
    }

private:
    VectorizationConfig config_;
    VectorizationStats stats_;
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Get the LLVM vector type string for a given element type and width
auto get_llvm_vector_type(VectorElementType elem, size_t width) -> std::string;

/// Get vector element type from MIR type
auto mir_type_to_vector_element(MirTypePtr type) -> std::optional<VectorElementType>;

/// Get the byte size of a vector element type
auto vector_element_size(VectorElementType elem) -> size_t;

/// Check if an operation can be vectorized
auto is_vectorizable_binop(BinOp op) -> bool;

/// Get the reduction operation from a binary operation
auto binop_to_reduction(BinOp op) -> std::optional<ReductionOp>;

} // namespace tml::mir
