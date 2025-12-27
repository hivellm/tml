#ifndef TML_LEXER_SOURCE_HPP
#define TML_LEXER_SOURCE_HPP

#include "common.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tml::lexer {

// Source file representation
class Source {
public:
    Source(std::string filename, std::string content);

    // Get the entire source content
    [[nodiscard]] auto content() const -> std::string_view {
        return content_;
    }

    // Get the filename
    [[nodiscard]] auto filename() const -> std::string_view {
        return filename_;
    }

    // Get length in bytes
    [[nodiscard]] auto length() const -> size_t {
        return content_.size();
    }

    // Get character at offset (UTF-8 byte)
    [[nodiscard]] auto at(size_t offset) const -> char;

    // Get a substring
    [[nodiscard]] auto slice(size_t start, size_t end) const -> std::string_view;

    // Get line/column from byte offset
    [[nodiscard]] auto location(size_t offset) const -> SourceLocation;

    // Get a specific line (1-indexed)
    [[nodiscard]] auto line(uint32_t line_num) const -> std::string_view;

    // Get total number of lines
    [[nodiscard]] auto line_count() const -> uint32_t;

    // Load source from file
    [[nodiscard]] static auto from_file(const std::string& path) -> Result<Source, std::string>;

    // Create source from string (for tests/REPL)
    [[nodiscard]] static auto from_string(std::string content,
                                          std::string name = "<input>") -> Source;

private:
    std::string filename_;
    std::string content_;
    std::vector<size_t> line_offsets_; // Offset of each line start

    void build_line_index();
};

} // namespace tml::lexer

#endif // TML_LEXER_SOURCE_HPP
