TML_MODULE("compiler")

//! # HIR Text Reader
//!
//! Parses human-readable HIR text format back into HirModule.
//!
//! ## Overview
//!
//! This is a basic implementation primarily for testing. The text format is
//! not meant to be a complete round-trip format - use binary serialization
//! for that purpose.
//!
//! ## Format
//!
//! The text format is a simplified representation of HIR that includes:
//! - Module header with name and source path
//! - Type definitions (structs and enums)
//! - Function definitions
//!
//! ## Usage
//!
//! ```cpp
//! std::istringstream iss(hir_text);
//! HirTextReader reader(iss);
//! HirModule module = reader.read_module();
//!
//! if (reader.has_error()) {
//!     std::cerr << "Parse error: " << reader.error_message() << "\n";
//! }
//! ```

#include "hir/hir_serialize.hpp"

namespace tml::hir {

HirTextReader::HirTextReader(std::istream& in) : in_(in) {}

auto HirTextReader::read_module() -> HirModule {
    HirModule module;

    // Text format parsing is complex; for now just return an empty module
    // This is primarily for testing the API - use binary format for real work
    set_error("Text format parsing not fully implemented");

    return module;
}

void HirTextReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = msg;
}

auto HirTextReader::next_line() -> bool {
    if (std::getline(in_, current_line_)) {
        ++line_num_;
        pos_ = 0;
        return true;
    }
    return false;
}

void HirTextReader::skip_whitespace() {
    while (pos_ < current_line_.size() && std::isspace(current_line_[pos_])) {
        ++pos_;
    }
}

auto HirTextReader::peek_char() -> char {
    if (pos_ < current_line_.size()) {
        return current_line_[pos_];
    }
    return '\0';
}

auto HirTextReader::read_char() -> char {
    if (pos_ < current_line_.size()) {
        return current_line_[pos_++];
    }
    return '\0';
}

auto HirTextReader::read_identifier() -> std::string {
    skip_whitespace();
    std::string id;
    while (pos_ < current_line_.size()) {
        char c = current_line_[pos_];
        if (std::isalnum(c) || c == '_') {
            id += c;
            ++pos_;
        } else {
            break;
        }
    }
    return id;
}

auto HirTextReader::read_number() -> int64_t {
    skip_whitespace();
    int64_t result = 0;
    bool negative = false;

    if (pos_ < current_line_.size() && current_line_[pos_] == '-') {
        negative = true;
        ++pos_;
    }

    while (pos_ < current_line_.size() && std::isdigit(current_line_[pos_])) {
        result = result * 10 + (current_line_[pos_] - '0');
        ++pos_;
    }

    return negative ? -result : result;
}

auto HirTextReader::read_string_literal() -> std::string {
    skip_whitespace();

    if (pos_ >= current_line_.size() || current_line_[pos_] != '"') {
        return "";
    }
    ++pos_; // Skip opening quote

    std::string result;
    while (pos_ < current_line_.size() && current_line_[pos_] != '"') {
        if (current_line_[pos_] == '\\' && pos_ + 1 < current_line_.size()) {
            ++pos_;
            switch (current_line_[pos_]) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case 'r':
                result += '\r';
                break;
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += current_line_[pos_];
            }
        } else {
            result += current_line_[pos_];
        }
        ++pos_;
    }

    if (pos_ < current_line_.size()) {
        ++pos_; // Skip closing quote
    }

    return result;
}

auto HirTextReader::expect(char c) -> bool {
    skip_whitespace();
    if (pos_ < current_line_.size() && current_line_[pos_] == c) {
        ++pos_;
        return true;
    }
    return false;
}

auto HirTextReader::expect(const std::string& s) -> bool {
    skip_whitespace();
    if (pos_ + s.size() <= current_line_.size() && current_line_.substr(pos_, s.size()) == s) {
        pos_ += s.size();
        return true;
    }
    return false;
}

} // namespace tml::hir
