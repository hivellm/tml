// Lint command - style linting (text-based)

#include "linter_internal.hpp"

namespace tml::cli::linter {

// ============================================================================
// Style Linting (text-based)
// ============================================================================

void lint_style(const fs::path& filepath, const std::string& content, LintResult& result,
                const LintConfig& config, bool fix_mode, std::string& fixed_content) {
    std::istringstream stream(content);
    std::string line;
    int line_number = 0;
    bool file_modified = false;
    std::vector<std::string> lines;

    while (std::getline(stream, line)) {
        line_number++;

        // Check for tabs (TML uses spaces)
        if (config.check_tabs && config.is_rule_enabled(RULE_TAB) &&
            line.find('\t') != std::string::npos) {
            if (fix_mode) {
                std::string fixed;
                for (char c : line) {
                    if (c == '\t') {
                        fixed += "    ";
                    } else {
                        fixed += c;
                    }
                }
                line = fixed;
                file_modified = true;
            } else {
                result.issues.push_back({filepath.string(), line_number, 1, RULE_TAB,
                                         "use spaces instead of tabs", Severity::Error, ""});
                result.errors++;
            }
        }

        // Check for trailing whitespace
        if (config.check_trailing && config.is_rule_enabled(RULE_TRAIL) && !line.empty()) {
            size_t last_non_space = line.find_last_not_of(" \t\r");
            if (last_non_space != std::string::npos && last_non_space < line.length() - 1) {
                if (fix_mode) {
                    line = line.substr(0, last_non_space + 1);
                    file_modified = true;
                } else {
                    result.issues.push_back({filepath.string(), line_number,
                                             static_cast<int>(last_non_space + 2), RULE_TRAIL,
                                             "trailing whitespace", Severity::Error, ""});
                    result.errors++;
                }
            }
        }

        // Check for very long lines
        if (config.check_line_length && config.is_rule_enabled(RULE_LINE_LENGTH) &&
            static_cast<int>(line.length()) > config.max_line_length) {
            result.issues.push_back(
                {filepath.string(), line_number, config.max_line_length + 1, RULE_LINE_LENGTH,
                 "line exceeds " + std::to_string(config.max_line_length) + " characters (" +
                     std::to_string(line.length()) + " chars)",
                 Severity::Warning, ""});
            result.warnings++;
        }

        lines.push_back(line);
    }

    // Build fixed content
    if (fix_mode && file_modified) {
        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); ++i) {
            oss << lines[i];
            if (i < lines.size() - 1) {
                oss << "\n";
            }
        }
        oss << "\n";
        fixed_content = oss.str();
    } else {
        fixed_content = content;
    }
}

} // namespace tml::cli::linter
