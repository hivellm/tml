//! # TmlStage — hybrid pipeline stage launcher (phase12f)
//!
//! Compiles a TML stage source file (`tools/stages/<name>_stage.tml`) once,
//! caches the resulting executable under `.incr-cache/stages/<name>.exe`, and
//! launches it as a subprocess to override the C++ implementation of one
//! pipeline stage. Input bytes are passed via a temp file path on argv[1];
//! output bytes are read from the subprocess stdout. Stderr is captured for
//! diagnostics.
//!
//! See: docs/specs/33-SERIAL-FORMAT.md "Stage I/O Conventions".

#ifndef TML_PIPELINE_TML_STAGE_HPP
#define TML_PIPELINE_TML_STAGE_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tml::pipeline {

/// Result of running a TML stage subprocess.
struct StageRunResult {
    bool success = false;
    std::vector<uint8_t> stdout_bytes;
    std::string stderr_text;
    int exit_code = -1;
    std::string error; ///< Set when launch/compile failed before exec.
};

/// Hybrid pipeline stage launcher.
///
/// Each `TmlStage` represents one canonical compiler stage (lexer, parser,
/// typechecker, hir, mir, codegen) implemented as a TML program. The first
/// call to `run()` for a given stage compiles the source if the cached
/// executable is missing or its fingerprint is stale; subsequent calls reuse
/// the cached binary.
class TmlStage {
public:
    /// `stage_name` must be one of: lexer, parser, typechecker, hir, mir,
    /// codegen. The stage source is looked up at
    /// `<workspace>/tools/stages/<stage_name>_stage.tml`.
    explicit TmlStage(std::string stage_name);

    /// Returns the canonical stage name passed to the constructor.
    [[nodiscard]] const std::string& name() const noexcept {
        return stage_name_;
    }

    /// Resolved source path; may not exist on disk yet.
    [[nodiscard]] const std::filesystem::path& source_path() const noexcept {
        return source_path_;
    }

    /// Resolved cached-exe path under `.incr-cache/stages/`.
    [[nodiscard]] const std::filesystem::path& cached_exe_path() const noexcept {
        return cached_exe_path_;
    }

    /// Compile the stage source if the cached exe is missing or stale.
    /// Returns true on success or when no recompile was needed.
    bool ensure_compiled();

    /// Run the stage with the given input bytes. Writes input to a temp file
    /// (whose path is passed as argv[1] to the subprocess) and reads
    /// stdout into the result. Calls `ensure_compiled()` first.
    StageRunResult run(const std::vector<uint8_t>& input_bytes);

private:
    std::string stage_name_;
    std::filesystem::path source_path_;
    std::filesystem::path cached_exe_path_;
    std::filesystem::path fingerprint_path_;

    /// Compute a stable fingerprint of the stage source (size + mtime).
    [[nodiscard]] std::string current_fingerprint() const;

    /// Read the cached fingerprint sidecar; empty if missing.
    [[nodiscard]] std::string read_cached_fingerprint() const;

    /// Write the fingerprint sidecar.
    void write_cached_fingerprint(const std::string& fp) const;
};

} // namespace tml::pipeline

#endif
