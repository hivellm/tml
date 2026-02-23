TML_MODULE("compiler")

//! # MIR Function Implementation
//!
//! This file implements MIR function-level operations.
//!
//! ## Functions
//!
//! - `create_block()`: Create a new basic block with label
//! - `get_block()`: Get block by ID (const and non-const)
//!
//! ## Basic Block Management
//!
//! Each function maintains a list of basic blocks forming a CFG.
//! Blocks are created with unique IDs and optional labels.

#include "mir/mir.hpp"

namespace tml::mir {

auto Function::create_block(const std::string& label) -> uint32_t {
    uint32_t id = next_block_id++;
    BasicBlock block;
    block.id = id;
    // Always append block ID to ensure unique label names in LLVM IR.
    // Without this, multiple blocks with the same label (e.g., "if.then")
    // would produce invalid IR with duplicate labels.
    block.name = label.empty() ? "bb" + std::to_string(id) : label + std::to_string(id);
    blocks.push_back(std::move(block));
    return id;
}

auto Function::get_block(uint32_t id) -> BasicBlock* {
    for (auto& block : blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

auto Function::get_block(uint32_t id) const -> const BasicBlock* {
    for (const auto& block : blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

} // namespace tml::mir
