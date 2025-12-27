#include "build_config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace tml::cli {

// ============================================================================
// Validation Functions
// ============================================================================

bool is_valid_semver(const std::string& version) {
    // Simple semver validation: MAJOR.MINOR.PATCH
    std::regex semver_regex(R"(^\d+\.\d+\.\d+(-[0-9A-Za-z-]+)?(\+[0-9A-Za-z-]+)?$)");
    return std::regex_match(version, semver_regex);
}

bool is_valid_package_name(const std::string& name) {
    if (name.empty())
        return false;

    // Must start with lowercase letter
    if (!std::islower(name[0]))
        return false;

    // Can contain lowercase, digits, hyphens, underscores
    for (char c : name) {
        if (!std::islower(c) && !std::isdigit(c) && c != '-' && c != '_') {
            return false;
        }
    }

    return true;
}

// ============================================================================
// PackageInfo
// ============================================================================

bool PackageInfo::validate() const {
    if (name.empty())
        return false;
    if (!is_valid_package_name(name))
        return false;
    if (version.empty())
        return false;
    if (!is_valid_semver(version))
        return false;
    if (edition != "2024")
        return false; // Only 2024 supported currently
    return true;
}

// ============================================================================
// LibConfig
// ============================================================================

bool LibConfig::validate() const {
    if (path.empty())
        return false;

    // Validate crate types
    for (const auto& type : crate_types) {
        if (type != "rlib" && type != "lib" && type != "dylib") {
            return false;
        }
    }

    return true;
}

// ============================================================================
// BinConfig
// ============================================================================

bool BinConfig::validate() const {
    return !name.empty() && !path.empty();
}

// ============================================================================
// Dependency
// ============================================================================

bool Dependency::validate() const {
    if (name.empty())
        return false;

    // Must be exactly one of: version, path, or git
    int count = 0;
    if (!version.empty())
        count++;
    if (!path.empty())
        count++;
    if (!git.empty())
        count++;

    if (count != 1)
        return false;

    // If version dependency, validate semver constraint
    if (!version.empty()) {
        // Allow semver constraints: ^1.2.0, ~1.2.3, >=1.0.0, <2.0.0, 1.2.3
        // For now, just check it's not empty (full validation complex)
        return !version.empty();
    }

    return true;
}

// ============================================================================
// BuildSettings
// ============================================================================

bool BuildSettings::validate() const {
    // 0-3: O0-O3, 4: Os, 5: Oz
    return optimization_level >= 0 && optimization_level <= 5;
}

// ============================================================================
// ProfileConfig
// ============================================================================

bool ProfileConfig::validate() const {
    if (name != "debug" && name != "release")
        return false;
    return settings.validate();
}

// ============================================================================
// Manifest
// ============================================================================

bool Manifest::validate() const {
    if (!package.validate())
        return false;

    if (lib && !lib->validate())
        return false;

    for (const auto& bin : bins) {
        if (!bin.validate())
            return false;
    }

    for (const auto& [name, dep] : dependencies) {
        if (!dep.validate())
            return false;
    }

    if (!build.validate())
        return false;

    for (const auto& [name, profile] : profiles) {
        if (!profile.validate())
            return false;
    }

    return true;
}

BuildSettings Manifest::get_build_settings(const std::string& profile_name) const {
    auto it = profiles.find(profile_name);
    if (it != profiles.end()) {
        return it->second.settings;
    }
    return build;
}

std::optional<Manifest> Manifest::load(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    SimpleTomlParser parser(content);
    return parser.parse();
}

std::optional<Manifest> Manifest::load_from_current_dir() {
    fs::path manifest_path = fs::current_path() / "tml.toml";
    return load(manifest_path);
}

// ============================================================================
// SimpleTomlParser
// ============================================================================

SimpleTomlParser::SimpleTomlParser(const std::string& content)
    : content_(content), pos_(0), line_(1) {}

void SimpleTomlParser::skip_whitespace() {
    while (!is_eof() && std::isspace(peek())) {
        if (peek() == '\n')
            line_++;
        advance();
    }
}

void SimpleTomlParser::skip_comment() {
    if (peek() == '#') {
        while (!is_eof() && peek() != '\n') {
            advance();
        }
    }
}

char SimpleTomlParser::advance() {
    if (is_eof())
        return '\0';
    return content_[pos_++];
}

std::string SimpleTomlParser::parse_identifier() {
    std::string result;
    while (!is_eof() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
        result += advance();
    }
    return result;
}

std::string SimpleTomlParser::parse_string() {
    if (peek() != '"') {
        set_error("Expected string");
        return "";
    }
    advance(); // Skip opening quote

    std::string result;
    while (!is_eof() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (is_eof())
                break;
            char escaped = advance();
            switch (escaped) {
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
                result += escaped;
                break;
            }
        } else {
            result += advance();
        }
    }

    if (peek() != '"') {
        set_error("Unterminated string");
        return "";
    }
    advance(); // Skip closing quote

    return result;
}

int SimpleTomlParser::parse_number() {
    std::string num_str;
    while (!is_eof() && std::isdigit(peek())) {
        num_str += advance();
    }
    return std::stoi(num_str);
}

bool SimpleTomlParser::parse_boolean() {
    std::string value = parse_identifier();
    return value == "true";
}

std::vector<std::string> SimpleTomlParser::parse_string_array() {
    std::vector<std::string> result;

    if (peek() != '[') {
        set_error("Expected array");
        return result;
    }
    advance(); // Skip '['

    skip_whitespace();

    while (!is_eof() && peek() != ']') {
        skip_whitespace();

        if (peek() == '"') {
            result.push_back(parse_string());
        }

        skip_whitespace();

        if (peek() == ',') {
            advance();
            skip_whitespace();
        }
    }

    if (peek() != ']') {
        set_error("Expected closing bracket");
        return result;
    }
    advance(); // Skip ']'

    return result;
}

void SimpleTomlParser::set_error(const std::string& message) {
    error_message_ = "Line " + std::to_string(line_) + ": " + message;
}

std::optional<PackageInfo> SimpleTomlParser::parse_package_section() {
    PackageInfo info;

    skip_whitespace();
    skip_comment();

    while (!is_eof() && peek() != '[') {
        skip_whitespace();
        skip_comment();

        if (peek() == '[' || is_eof())
            break;

        std::string key = parse_identifier();
        skip_whitespace();

        if (peek() != '=') {
            set_error("Expected '=' after key");
            return std::nullopt;
        }
        advance();
        skip_whitespace();

        if (key == "name") {
            info.name = parse_string();
        } else if (key == "version") {
            info.version = parse_string();
        } else if (key == "edition") {
            info.edition = parse_string();
        } else if (key == "description") {
            info.description = parse_string();
        } else if (key == "license") {
            info.license = parse_string();
        } else if (key == "repository") {
            info.repository = parse_string();
        } else if (key == "authors") {
            info.authors = parse_string_array();
        }

        skip_whitespace();
        skip_comment();
    }

    return info;
}

std::optional<LibConfig> SimpleTomlParser::parse_lib_section() {
    LibConfig config;

    skip_whitespace();
    skip_comment();

    while (!is_eof() && peek() != '[') {
        skip_whitespace();
        skip_comment();

        if (peek() == '[' || is_eof())
            break;

        std::string key = parse_identifier();
        skip_whitespace();

        if (peek() != '=') {
            set_error("Expected '=' after key");
            return std::nullopt;
        }
        advance();
        skip_whitespace();

        if (key == "path") {
            config.path = parse_string();
        } else if (key == "name") {
            config.name = parse_string();
        } else if (key == "crate-type") {
            config.crate_types = parse_string_array();
        } else if (key == "emit-header") {
            config.emit_header = parse_boolean();
        }

        skip_whitespace();
        skip_comment();
    }

    return config;
}

std::optional<BinConfig> SimpleTomlParser::parse_bin_section() {
    BinConfig config;

    skip_whitespace();
    skip_comment();

    while (!is_eof() && peek() != '[') {
        skip_whitespace();
        skip_comment();

        if (peek() == '[' || is_eof())
            break;

        std::string key = parse_identifier();
        skip_whitespace();

        if (peek() != '=') {
            set_error("Expected '=' after key");
            return std::nullopt;
        }
        advance();
        skip_whitespace();

        if (key == "name") {
            config.name = parse_string();
        } else if (key == "path") {
            config.path = parse_string();
        }

        skip_whitespace();
        skip_comment();
    }

    return config;
}

std::map<std::string, Dependency> SimpleTomlParser::parse_dependencies_section() {
    std::map<std::string, Dependency> deps;

    skip_whitespace();
    skip_comment();

    while (!is_eof() && peek() != '[') {
        skip_whitespace();
        skip_comment();

        if (peek() == '[' || is_eof())
            break;

        std::string name = parse_identifier();
        skip_whitespace();

        if (peek() != '=') {
            set_error("Expected '=' after dependency name");
            return deps;
        }
        advance();
        skip_whitespace();

        Dependency dep;
        dep.name = name;

        if (peek() == '"') {
            // Simple version string
            dep.version = parse_string();
        } else if (peek() == '{') {
            // Inline table: { path = "...", version = "..." }
            advance(); // Skip '{'
            skip_whitespace();

            while (!is_eof() && peek() != '}') {
                std::string key = parse_identifier();
                skip_whitespace();

                if (peek() != '=')
                    break;
                advance();
                skip_whitespace();

                std::string value = parse_string();

                if (key == "version") {
                    dep.version = value;
                } else if (key == "path") {
                    dep.path = value;
                } else if (key == "git") {
                    dep.git = value;
                } else if (key == "tag") {
                    dep.tag = value;
                }

                skip_whitespace();
                if (peek() == ',') {
                    advance();
                    skip_whitespace();
                }
            }

            if (peek() == '}')
                advance();
        }

        deps[name] = dep;

        skip_whitespace();
        skip_comment();
    }

    return deps;
}

std::optional<BuildSettings> SimpleTomlParser::parse_build_section() {
    BuildSettings settings;

    skip_whitespace();
    skip_comment();

    while (!is_eof() && peek() != '[') {
        skip_whitespace();
        skip_comment();

        if (peek() == '[' || is_eof())
            break;

        std::string key = parse_identifier();
        skip_whitespace();

        if (peek() != '=') {
            set_error("Expected '=' after key");
            return std::nullopt;
        }
        advance();
        skip_whitespace();

        if (key == "optimization-level") {
            settings.optimization_level = parse_number();
        } else if (key == "emit-ir") {
            settings.emit_ir = parse_boolean();
        } else if (key == "emit-header") {
            settings.emit_header = parse_boolean();
        } else if (key == "verbose") {
            settings.verbose = parse_boolean();
        } else if (key == "cache") {
            settings.cache = parse_boolean();
        } else if (key == "parallel") {
            settings.parallel = parse_boolean();
        }

        skip_whitespace();
        skip_comment();
    }

    return settings;
}

std::optional<ProfileConfig>
SimpleTomlParser::parse_profile_section(const std::string& profile_name) {
    ProfileConfig profile;
    profile.name = profile_name;

    auto settings = parse_build_section();
    if (!settings)
        return std::nullopt;

    profile.settings = *settings;
    return profile;
}

std::optional<Manifest> SimpleTomlParser::parse() {
    Manifest manifest;

    while (!is_eof()) {
        skip_whitespace();
        skip_comment();

        if (is_eof())
            break;

        if (peek() == '[') {
            advance(); // Skip '['

            // Check for array section [[bin]]
            bool is_array = false;
            if (peek() == '[') {
                is_array = true;
                advance();
            }

            std::string section = parse_identifier();

            // Handle profile.debug or profile.release
            std::string subsection;
            if (peek() == '.') {
                advance();
                subsection = parse_identifier();
            }

            if (is_array && peek() == ']') {
                advance(); // Skip second ']'
            }

            if (peek() != ']') {
                set_error("Expected ']' after section name");
                return std::nullopt;
            }
            advance(); // Skip ']'

            skip_whitespace();
            skip_comment();

            // Parse section content
            if (section == "package") {
                auto pkg = parse_package_section();
                if (!pkg)
                    return std::nullopt;
                manifest.package = *pkg;
            } else if (section == "lib") {
                auto lib = parse_lib_section();
                if (!lib)
                    return std::nullopt;
                manifest.lib = *lib;
            } else if (section == "bin" && is_array) {
                auto bin = parse_bin_section();
                if (!bin)
                    return std::nullopt;
                manifest.bins.push_back(*bin);
            } else if (section == "dependencies") {
                manifest.dependencies = parse_dependencies_section();
            } else if (section == "build") {
                auto build = parse_build_section();
                if (!build)
                    return std::nullopt;
                manifest.build = *build;
            } else if (section == "profile" && !subsection.empty()) {
                auto profile = parse_profile_section(subsection);
                if (!profile)
                    return std::nullopt;
                manifest.profiles[subsection] = *profile;
            }
        } else {
            // Skip unknown content
            advance();
        }
    }

    if (!error_message_.empty()) {
        return std::nullopt;
    }

    return manifest;
}

} // namespace tml::cli
