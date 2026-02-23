TML_MODULE("compiler")

//! # Log Initialization from CLI
//!
//! Parses logging-related command-line arguments and the TML_LOG
//! environment variable to produce a LogConfig.

#include "log/log.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace tml::log {

LogConfig parse_log_options(int argc, char* argv[]) {
    LogConfig config;
    config.level = LogLevel::Warn; // Default: only warnings and above

    bool has_cli_level = false;
    bool has_cli_filter = false;
    int v_count = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // --log-level=<level>
        if (arg.starts_with("--log-level=")) {
            config.level = parse_level(arg.substr(12));
            has_cli_level = true;
        }
        // --log-filter=<spec>
        else if (arg.starts_with("--log-filter=")) {
            config.filter_spec = arg.substr(13);
            has_cli_filter = true;
        }
        // --log-file=<path>
        else if (arg.starts_with("--log-file=")) {
            config.log_file = arg.substr(11);
        }
        // --log-format=json|text
        else if (arg.starts_with("--log-format=")) {
            std::string fmt = arg.substr(13);
            if (fmt == "json" || fmt == "JSON") {
                config.format = LogFormat::JSON;
            } else {
                config.format = LogFormat::Text;
            }
        }
        // -q / --quiet
        else if (arg == "-q" || arg == "--quiet") {
            config.level = LogLevel::Error;
            has_cli_level = true;
        }
        // Count -v flags: -v = Info, -vv = Debug, -vvv = Trace
        // Also handle --verbose as alias for -v
        else if (arg == "--verbose") {
            if (v_count == 0)
                v_count = 1;
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            // Check for -v, -vv, -vvv
            bool all_v = true;
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] != 'v') {
                    all_v = false;
                    break;
                }
            }
            if (all_v && arg.size() > 1) {
                int count = static_cast<int>(arg.size() - 1);
                if (count > v_count) {
                    v_count = count;
                }
            }
        }
    }

    // Map -v/-vv/-vvv to levels (only if no explicit --log-level)
    if (!has_cli_level && v_count > 0) {
        if (v_count >= 3) {
            config.level = LogLevel::Trace;
        } else if (v_count == 2) {
            config.level = LogLevel::Debug;
        } else {
            config.level = LogLevel::Info;
        }
        has_cli_level = true;
    }

    // Check TML_LOG environment variable as fallback
    if (!has_cli_level && !has_cli_filter) {
        std::string env_str;
#ifdef _WIN32
        char* env_buf = nullptr;
        size_t env_len = 0;
        if (_dupenv_s(&env_buf, &env_len, "TML_LOG") == 0 && env_buf) {
            env_str = env_buf;
            free(env_buf);
        }
#else
        const char* env_log = std::getenv("TML_LOG");
        if (env_log) {
            env_str = env_log;
        }
#endif
        if (!env_str.empty()) {

            // If it contains '=' it's a filter spec, otherwise it's a level
            if (env_str.find('=') != std::string::npos) {
                config.filter_spec = env_str;
            } else if (env_str.find(',') != std::string::npos) {
                // Comma-separated module names without levels
                config.filter_spec = env_str;
            } else {
                // Single level name
                config.level = parse_level(env_str);
            }
        }
    }

    return config;
}

} // namespace tml::log
