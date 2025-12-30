// MIR Function Implementation

#include "mir/mir.hpp"

namespace tml::mir {

auto Function::create_block(const std::string& label) -> uint32_t {
    uint32_t id = next_block_id++;
    BasicBlock block;
    block.id = id;
    block.name = label.empty() ? "bb" + std::to_string(id) : label;
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
