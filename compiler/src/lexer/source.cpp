#include "lexer/source.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace tml::lexer {

Source::Source(std::string filename, std::string content)
    : filename_(std::move(filename)), content_(std::move(content)) {
    build_line_index();
}

void Source::build_line_index() {
    line_offsets_.clear();
    line_offsets_.push_back(0); // First line starts at offset 0

    for (size_t i = 0; i < content_.size(); ++i) {
        if (content_[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
}

auto Source::at(size_t offset) const -> char {
    if (offset >= content_.size()) {
        return '\0';
    }
    return content_[offset];
}

auto Source::slice(size_t start, size_t end) const -> std::string_view {
    if (start >= content_.size()) {
        return {};
    }
    end = std::min(end, content_.size());
    return std::string_view(content_).substr(start, end - start);
}

auto Source::location(size_t offset) const -> SourceLocation {
    // Binary search for the line
    auto it = std::upper_bound(line_offsets_.begin(), line_offsets_.end(), offset);
    if (it != line_offsets_.begin()) {
        --it;
    }

    auto line_index = static_cast<uint32_t>(std::distance(line_offsets_.begin(), it));
    auto line_start = *it;
    auto column = static_cast<uint32_t>(offset - line_start);

    return SourceLocation{.file = filename_,
                          .line = line_index + 1, // 1-indexed
                          .column = column + 1,   // 1-indexed
                          .offset = static_cast<uint32_t>(offset),
                          .length = 1};
}

auto Source::line(uint32_t line_num) const -> std::string_view {
    if (line_num == 0 || line_num > line_offsets_.size()) {
        return {};
    }

    auto line_index = line_num - 1;
    auto start = line_offsets_[line_index];
    size_t end;

    if (line_index + 1 < line_offsets_.size()) {
        end = line_offsets_[line_index + 1];
        // Remove trailing newline if present
        if (end > start && content_[end - 1] == '\n') {
            --end;
        }
        // Remove trailing carriage return if present (Windows line endings)
        if (end > start && content_[end - 1] == '\r') {
            --end;
        }
    } else {
        end = content_.size();
    }

    return std::string_view(content_).substr(start, end - start);
}

auto Source::line_count() const -> uint32_t {
    return static_cast<uint32_t>(line_offsets_.size());
}

auto Source::from_file(const std::string& path) -> Result<Source, std::string> {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "Failed to open file: " + path;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.fail() && !file.eof()) {
        return "Failed to read file: " + path;
    }

    return Source(path, buffer.str());
}

auto Source::from_string(std::string content, std::string name) -> Source {
    return Source(std::move(name), std::move(content));
}

} // namespace tml::lexer
