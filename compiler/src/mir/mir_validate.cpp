TML_MODULE("compiler")

//! # MIR Validation Pass Implementation
//!
//! Three sub-checks are run for every function in the module:
//!
//! 1. **validate_types** — iterates every `InstructionData` and reports any
//!    entry whose `.type` pointer is null (excluding `StoreInst`, `FenceInst`,
//!    and `AtomicStoreInst` which are legitimately void).
//!
//! 2. **validate_terminators** — verifies that every `BasicBlock` has a
//!    populated `terminator` field.
//!
//! 3. **validate_blocks** — checks that the function has at least one block
//!    (entry block) and that no block is entirely empty (no instructions AND
//!    no terminator).

#include "mir/mir_validate.hpp"

#include "log/log.hpp"

#include <sstream>
#include <string>

namespace tml::mir {

// ============================================================================
// Helper: instruction kind name (for diagnostic messages)
// ============================================================================

namespace {

/// Returns a short, human-readable name for an instruction variant.
auto instruction_kind_name(const Instruction& inst) -> std::string {
    return std::visit(
        [](const auto& i) -> std::string {
            using T = std::decay_t<decltype(i)>;
            if constexpr (std::is_same_v<T, BinaryInst>)
                return "BinaryInst";
            else if constexpr (std::is_same_v<T, UnaryInst>)
                return "UnaryInst";
            else if constexpr (std::is_same_v<T, LoadInst>)
                return "LoadInst";
            else if constexpr (std::is_same_v<T, StoreInst>)
                return "StoreInst";
            else if constexpr (std::is_same_v<T, AllocaInst>)
                return "AllocaInst";
            else if constexpr (std::is_same_v<T, GetElementPtrInst>)
                return "GetElementPtrInst";
            else if constexpr (std::is_same_v<T, ExtractValueInst>)
                return "ExtractValueInst";
            else if constexpr (std::is_same_v<T, InsertValueInst>)
                return "InsertValueInst";
            else if constexpr (std::is_same_v<T, CallInst>)
                return "CallInst";
            else if constexpr (std::is_same_v<T, MethodCallInst>)
                return "MethodCallInst";
            else if constexpr (std::is_same_v<T, CastInst>)
                return "CastInst";
            else if constexpr (std::is_same_v<T, PhiInst>)
                return "PhiInst";
            else if constexpr (std::is_same_v<T, ConstantInst>)
                return "ConstantInst";
            else if constexpr (std::is_same_v<T, SelectInst>)
                return "SelectInst";
            else if constexpr (std::is_same_v<T, StructInitInst>)
                return "StructInitInst";
            else if constexpr (std::is_same_v<T, EnumInitInst>)
                return "EnumInitInst";
            else if constexpr (std::is_same_v<T, TupleInitInst>)
                return "TupleInitInst";
            else if constexpr (std::is_same_v<T, ArrayInitInst>)
                return "ArrayInitInst";
            else if constexpr (std::is_same_v<T, AwaitInst>)
                return "AwaitInst";
            else if constexpr (std::is_same_v<T, ClosureInitInst>)
                return "ClosureInitInst";
            else if constexpr (std::is_same_v<T, AtomicLoadInst>)
                return "AtomicLoadInst";
            else if constexpr (std::is_same_v<T, AtomicStoreInst>)
                return "AtomicStoreInst";
            else if constexpr (std::is_same_v<T, AtomicRMWInst>)
                return "AtomicRMWInst";
            else if constexpr (std::is_same_v<T, AtomicCmpXchgInst>)
                return "AtomicCmpXchgInst";
            else if constexpr (std::is_same_v<T, FenceInst>)
                return "FenceInst";
            else if constexpr (std::is_same_v<T, VectorLoadInst>)
                return "VectorLoadInst";
            else if constexpr (std::is_same_v<T, VectorStoreInst>)
                return "VectorStoreInst";
            else if constexpr (std::is_same_v<T, VectorBinaryInst>)
                return "VectorBinaryInst";
            else if constexpr (std::is_same_v<T, VectorReductionInst>)
                return "VectorReductionInst";
            else if constexpr (std::is_same_v<T, VectorSplatInst>)
                return "VectorSplatInst";
            else if constexpr (std::is_same_v<T, VectorExtractInst>)
                return "VectorExtractInst";
            else if constexpr (std::is_same_v<T, VectorInsertInst>)
                return "VectorInsertInst";
            else if constexpr (std::is_same_v<T, MakeDynObjectInst>)
                return "MakeDynObjectInst";
            else
                return "UnknownInst";
        },
        inst);
}

/// Returns true for instruction kinds that are legitimately void (no result
/// type) and should not trigger a null-type warning.
auto is_void_instruction(const Instruction& inst) -> bool {
    // StoreInst, AtomicStoreInst, FenceInst, and VectorStoreInst produce no
    // SSA value; their InstructionData.type is expected to be null.
    return std::holds_alternative<StoreInst>(inst) ||
           std::holds_alternative<AtomicStoreInst>(inst) ||
           std::holds_alternative<FenceInst>(inst) || std::holds_alternative<VectorStoreInst>(inst);
}

// ============================================================================
// Sub-check: null type fields
// ============================================================================

/// Check every instruction in every block of `func` for a null `.type`.
/// Appends one warning entry per violation into `result`.
void validate_types(const Function& func, ValidationResult& result) {
    for (const auto& block : func.blocks) {
        for (size_t idx = 0; idx < block.instructions.size(); ++idx) {
            const auto& idata = block.instructions[idx];

            // Void instructions legitimately have no result type.
            if (is_void_instruction(idata.inst)) {
                continue;
            }

            // result == INVALID_VALUE means this instruction intentionally
            // produces no value (e.g. some side-effect-only calls that the
            // builder chose not to assign). Allow that case.
            if (idata.result == INVALID_VALUE) {
                continue;
            }

            if (!idata.type) {
                ++result.null_type_count;
                std::ostringstream msg;
                msg << "[MV001] null type on instruction " << instruction_kind_name(idata.inst)
                    << " in function '" << func.name << "' block " << block.id << " index " << idx
                    << " (value_id=" << idata.result << ")";
                result.warnings.push_back(msg.str());
                TML_LOG_WARN("mir", msg.str());
            }
        }
    }
}

// ============================================================================
// Sub-check: missing terminators
// ============================================================================

/// Check that every block in `func` has a populated terminator.
void validate_terminators(const Function& func, ValidationResult& result) {
    for (const auto& block : func.blocks) {
        if (!block.terminator.has_value()) {
            ++result.missing_terminator_count;
            std::ostringstream msg;
            msg << "[MV002] block " << block.id << " ('" << block.name << "') in function '"
                << func.name << "' has no terminator";
            result.warnings.push_back(msg.str());
            TML_LOG_WARN("mir", msg.str());
        }
    }
}

// ============================================================================
// Sub-check: empty/missing blocks
// ============================================================================

/// Check that `func` has at least one block and that no block is completely
/// empty (no instructions and no terminator).
void validate_blocks(const Function& func, ValidationResult& result) {
    if (func.blocks.empty()) {
        ++result.empty_block_count;
        std::ostringstream msg;
        msg << "[MV003] function '" << func.name << "' has no basic blocks (missing entry block)";
        result.warnings.push_back(msg.str());
        TML_LOG_WARN("mir", msg.str());
        return;
    }

    for (const auto& block : func.blocks) {
        if (block.instructions.empty() && !block.terminator.has_value()) {
            ++result.empty_block_count;
            std::ostringstream msg;
            msg << "[MV003] empty block " << block.id << " ('" << block.name << "') in function '"
                << func.name << "' has no instructions and no terminator";
            result.warnings.push_back(msg.str());
            TML_LOG_WARN("mir", msg.str());
        }
    }
}

} // anonymous namespace

// ============================================================================
// Public entry point
// ============================================================================

auto validate_module(const Module& module) -> ValidationResult {
    ValidationResult result;

    for (const auto& func : module.functions) {
        validate_types(func, result);
        validate_terminators(func, result);
        validate_blocks(func, result);
    }

    if (!result.is_clean()) {
        TML_LOG_WARN("mir", "[MV000] MIR validation found "
                                << result.null_type_count << " null type(s), "
                                << result.missing_terminator_count << " missing terminator(s), "
                                << result.empty_block_count << " empty block(s) across "
                                << module.functions.size() << " function(s)");
    }

    return result;
}

} // namespace tml::mir
