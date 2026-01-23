//! JSON Memory Allocator Implementation

#include "json/json_allocator.hpp"

#include "common.hpp"

#include "json/json_fast_parser.hpp"
#include "json/json_value.hpp"

namespace tml::json {

// ============================================================================
// JsonDocument Implementation
// ============================================================================

auto JsonDocument::parse(std::string_view input) -> std::optional<JsonDocument> {
    // Estimate arena size based on input size
    // JSON typically expands 2-3x when parsed into structures
    size_t estimated_size = std::max(input.size() * 2, JsonArena::DEFAULT_BLOCK_SIZE);
    return parse(input, estimated_size);
}

auto JsonDocument::parse(std::string_view input, size_t arena_size) -> std::optional<JsonDocument> {
    JsonDocument doc(arena_size);

    // Use the fast parser
    auto result = fast::parse_json_fast(input);
    if (!is_ok(result)) {
        return std::nullopt;
    }

    doc.set_root(std::move(unwrap(result)));
    return doc;
}

} // namespace tml::json
