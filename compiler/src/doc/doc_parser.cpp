//! # Documentation Comment Parser Implementation
//!
//! This file implements the parsing of documentation comments to extract
//! structured information like @param, @returns, @example, etc.

#include "doc/doc_parser.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace tml::doc {

namespace {

/// Trims whitespace from both ends of a string.
auto trim(const std::string& s) -> std::string {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

/// Checks if a line starts with a tag.
[[maybe_unused]] auto is_tag_line(const std::string& line) -> bool {
    auto trimmed = trim(line);
    return !trimmed.empty() && trimmed[0] == '@';
}

/// Parses a @param tag: "@param name description"
auto parse_param_tag(const std::string& text) -> std::optional<DocParam> {
    // Skip "@param "
    size_t pos = 6;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }

    // Extract name
    size_t name_start = pos;
    while (pos < text.size() && !std::isspace(text[pos])) {
        ++pos;
    }
    if (pos == name_start) {
        return std::nullopt;
    }
    std::string name = text.substr(name_start, pos - name_start);

    // Skip whitespace before description
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }

    std::string description = pos < text.size() ? text.substr(pos) : "";

    return DocParam{.name = std::move(name), .type = "", .description = trim(description)};
}

/// Parses a @returns tag: "@returns description"
auto parse_returns_tag(const std::string& text) -> DocReturn {
    // Skip "@returns " or "@return "
    size_t pos = text[7] == 's' ? 8 : 7;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }
    return DocReturn{.type = "", .description = trim(text.substr(pos))};
}

/// Parses a @throws tag: "@throws ErrorType description"
auto parse_throws_tag(const std::string& text) -> std::optional<DocThrows> {
    // Skip "@throws "
    size_t pos = 7;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }

    // Extract error type
    size_t type_start = pos;
    while (pos < text.size() && !std::isspace(text[pos])) {
        ++pos;
    }
    if (pos == type_start) {
        return std::nullopt;
    }
    std::string error_type = text.substr(type_start, pos - type_start);

    // Skip whitespace before description
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }

    std::string description = pos < text.size() ? text.substr(pos) : "";

    return DocThrows{.error_type = std::move(error_type), .description = trim(description)};
}

/// Parses a @see tag: "@see symbol"
auto parse_see_tag(const std::string& text) -> std::string {
    // Skip "@see "
    size_t pos = 4;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }
    return trim(text.substr(pos));
}

/// Parses a @since tag: "@since version"
auto parse_since_tag(const std::string& text) -> std::string {
    // Skip "@since "
    size_t pos = 6;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }
    return trim(text.substr(pos));
}

/// Parses a @deprecated tag: "@deprecated message"
auto parse_deprecated_tag(const std::string& text) -> DocDeprecation {
    // Skip "@deprecated "
    size_t pos = 11;
    while (pos < text.size() && std::isspace(text[pos])) {
        ++pos;
    }
    return DocDeprecation{.message = trim(text.substr(pos)), .since = "", .replacement = ""};
}

} // namespace

auto extract_summary(const std::string& doc_text) -> std::string {
    if (doc_text.empty()) {
        return "";
    }

    std::istringstream stream(doc_text);
    std::string line;
    std::string summary;
    bool in_summary = true;

    while (std::getline(stream, line)) {
        auto trimmed = trim(line);

        // Stop at blank line
        if (trimmed.empty() && !summary.empty()) {
            break;
        }

        // Stop at any tag
        if (!trimmed.empty() && trimmed[0] == '@') {
            break;
        }

        // Stop at code block
        if (trimmed.find("```") == 0) {
            break;
        }

        if (in_summary && !trimmed.empty()) {
            if (!summary.empty()) {
                summary += " ";
            }
            summary += trimmed;
        }
    }

    return summary;
}

auto extract_code_blocks(const std::string& doc_text) -> std::vector<DocExample> {
    std::vector<DocExample> examples;
    std::istringstream stream(doc_text);
    std::string line;

    while (std::getline(stream, line)) {
        auto trimmed = trim(line);

        // Look for code block start
        if (trimmed.find("```") == 0) {
            // Extract language (if any)
            std::string language = "tml";
            if (trimmed.size() > 3) {
                language = trim(trimmed.substr(3));
                if (language.empty()) {
                    language = "tml";
                }
            }

            // Collect code until closing ```
            std::string code;
            while (std::getline(stream, line)) {
                trimmed = trim(line);
                if (trimmed.find("```") == 0) {
                    break;
                }
                if (!code.empty()) {
                    code += "\n";
                }
                code += line;
            }

            if (!code.empty()) {
                examples.push_back(
                    DocExample{.code = std::move(code), .description = "", .language = language});
            }
        }
    }

    return examples;
}

auto parse_doc_comment(const std::string& doc_text) -> ParsedDoc {
    ParsedDoc result;

    if (doc_text.empty()) {
        return result;
    }

    // Extract summary first
    result.summary = extract_summary(doc_text);

    // Extract code blocks as examples
    result.examples = extract_code_blocks(doc_text);

    // Parse line by line for tags
    std::istringstream stream(doc_text);
    std::string line;
    std::string body;
    std::string current_tag_content;
    std::string current_tag_type;

    auto flush_tag = [&]() {
        if (current_tag_type.empty()) {
            return;
        }

        auto content = trim(current_tag_content);
        if (current_tag_type == "param") {
            if (auto param = parse_param_tag("@param " + content)) {
                result.params.push_back(*param);
            }
        } else if (current_tag_type == "returns" || current_tag_type == "return") {
            result.returns = parse_returns_tag("@returns " + content);
        } else if (current_tag_type == "throws") {
            if (auto throws = parse_throws_tag("@throws " + content)) {
                result.throws.push_back(*throws);
            }
        } else if (current_tag_type == "see") {
            auto ref = parse_see_tag("@see " + content);
            if (!ref.empty()) {
                result.see_also.push_back(ref);
            }
        } else if (current_tag_type == "since") {
            result.since = parse_since_tag("@since " + content);
        } else if (current_tag_type == "deprecated") {
            result.deprecated = parse_deprecated_tag("@deprecated " + content);
        } else if (current_tag_type == "example") {
            // @example blocks are handled separately via code block extraction
        }

        current_tag_type.clear();
        current_tag_content.clear();
    };

    bool in_code_block = false;

    while (std::getline(stream, line)) {
        auto trimmed = trim(line);

        // Track code blocks
        if (trimmed.find("```") == 0) {
            in_code_block = !in_code_block;
            if (!body.empty()) {
                body += "\n";
            }
            body += line;
            continue;
        }

        // Don't parse tags inside code blocks
        if (in_code_block) {
            if (!body.empty()) {
                body += "\n";
            }
            body += line;
            continue;
        }

        // Check for new tag
        if (!trimmed.empty() && trimmed[0] == '@') {
            // Flush previous tag
            flush_tag();

            // Determine tag type
            size_t space_pos = trimmed.find(' ');
            std::string tag_name = space_pos != std::string::npos ? trimmed.substr(1, space_pos - 1)
                                                                  : trimmed.substr(1);

            // Convert to lowercase for comparison
            std::transform(tag_name.begin(), tag_name.end(), tag_name.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            current_tag_type = tag_name;
            current_tag_content =
                space_pos != std::string::npos ? trimmed.substr(space_pos + 1) : "";
        } else if (!current_tag_type.empty()) {
            // Continuation of current tag
            if (!current_tag_content.empty()) {
                current_tag_content += " ";
            }
            current_tag_content += trimmed;
        } else {
            // Regular body text
            if (!body.empty()) {
                body += "\n";
            }
            body += line;
        }
    }

    // Flush final tag
    flush_tag();

    result.body = trim(body);

    return result;
}

} // namespace tml::doc
