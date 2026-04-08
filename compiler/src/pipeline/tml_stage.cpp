TML_MODULE("compiler")

#include "pipeline/tml_stage.hpp"

#include "log/log.hpp"
#include "testing/testing_process.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::pipeline {

namespace {

/// Locate the running tml executable. Used to (re)compile stage sources.
fs::path get_self_exe_path() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(std::string(buf, len));
    }
#else
    std::error_code ec;
    auto exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe;
    }
#endif
    return {};
}

/// Walk up from CWD looking for the workspace root (a directory containing
/// `tml.toml` with a `[workspace]` section). Falls back to CWD.
fs::path find_workspace_root() {
    auto p = fs::current_path();
    while (!p.empty() && p.has_parent_path()) {
        auto manifest = p / "tml.toml";
        std::error_code ec;
        if (fs::exists(manifest, ec)) {
            std::ifstream in(manifest);
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("[workspace]") != std::string::npos) {
                    return p;
                }
            }
        }
        if (p == p.parent_path()) {
            break;
        }
        p = p.parent_path();
    }
    return fs::current_path();
}

} // namespace

TmlStage::TmlStage(std::string stage_name) : stage_name_(std::move(stage_name)) {
    auto root = find_workspace_root();
    source_path_ = root / "tools" / "stages" / (stage_name_ + "_stage.tml");

    auto cache_dir = root / ".incr-cache" / "stages";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);

#ifdef _WIN32
    cached_exe_path_ = cache_dir / (stage_name_ + ".exe");
#else
    cached_exe_path_ = cache_dir / stage_name_;
#endif
    fingerprint_path_ = cache_dir / (stage_name_ + ".fp");
}

std::string TmlStage::current_fingerprint() const {
    std::error_code ec;
    if (!fs::exists(source_path_, ec)) {
        return {};
    }
    auto sz = fs::file_size(source_path_, ec);
    if (ec) {
        return {};
    }
    auto mtime = fs::last_write_time(source_path_, ec);
    if (ec) {
        return {};
    }
    auto mt = mtime.time_since_epoch().count();
    std::ostringstream out;
    out << sz << ":" << mt;
    return out.str();
}

std::string TmlStage::read_cached_fingerprint() const {
    std::error_code ec;
    if (!fs::exists(fingerprint_path_, ec)) {
        return {};
    }
    std::ifstream in(fingerprint_path_);
    std::string line;
    std::getline(in, line);
    return line;
}

void TmlStage::write_cached_fingerprint(const std::string& fp) const {
    std::ofstream out(fingerprint_path_, std::ios::trunc);
    out << fp;
}

bool TmlStage::ensure_compiled() {
    std::error_code ec;
    if (!fs::exists(source_path_, ec)) {
        TML_LOG_ERROR("stage", "Stage source not found: " << source_path_.string());
        return false;
    }

    auto fp_now = current_fingerprint();
    auto fp_cached = read_cached_fingerprint();
    if (!fp_now.empty() && fp_now == fp_cached && fs::exists(cached_exe_path_, ec)) {
        return true; // Cache hit.
    }

    auto self = get_self_exe_path();
    if (self.empty()) {
        TML_LOG_ERROR("stage", "Cannot locate tml.exe to compile stage '" << stage_name_ << "'");
        return false;
    }

    // tml build <source> -o <cached_exe_dir> --output-name <cached_exe_stem>
    // The build CLI writes to <output_dir>/<basename>.exe by default; we
    // post-rename to ensure the canonical cached path.
    auto build_dir = cached_exe_path_.parent_path();
    tml::testing::ProcessOptions opts;
    opts.exe_path = self.string();
    opts.args = {"build", source_path_.string(), "--output-dir", build_dir.string()};
    opts.timeout = std::chrono::milliseconds(300'000);

    auto proc = tml::testing::Process::launch(opts);
    if (!proc) {
        TML_LOG_ERROR("stage", "Failed to launch build for stage '" << stage_name_ << "'");
        return false;
    }
    auto result = proc->wait();
    if (result.exit_code != 0) {
        TML_LOG_ERROR("stage", "Compilation of stage '" << stage_name_ << "' failed (exit "
                                                        << result.exit_code << ")\n"
                                                        << result.stderr_output);
        return false;
    }

    // The build pipeline emits an executable named after the source stem.
    auto produced =
#ifdef _WIN32
        build_dir / (source_path_.stem().string() + ".exe");
#else
        build_dir / source_path_.stem();
#endif
    if (!fs::exists(produced, ec)) {
        TML_LOG_ERROR("stage",
                      "Stage build succeeded but produced exe missing: " << produced.string());
        return false;
    }
    if (produced != cached_exe_path_) {
        fs::rename(produced, cached_exe_path_, ec);
        if (ec) {
            TML_LOG_ERROR("stage", "Failed to rename produced exe: " << ec.message());
            return false;
        }
    }
    write_cached_fingerprint(fp_now);
    return true;
}

StageRunResult TmlStage::run(const std::vector<uint8_t>& input_bytes) {
    StageRunResult out;
    if (!ensure_compiled()) {
        out.error = "compile failed";
        return out;
    }

    // Write input to a temp file; pass its path as argv[1].
    std::error_code ec;
    auto tmp_dir = fs::temp_directory_path(ec);
    if (ec) {
        out.error = "no temp dir";
        return out;
    }
    auto tmp_in = tmp_dir / ("tml_stage_" + stage_name_ + "_" +
                             std::to_string(reinterpret_cast<uintptr_t>(&out)) + ".bin");
    {
        std::ofstream f(tmp_in, std::ios::binary | std::ios::trunc);
        if (!f) {
            out.error = "cannot write input temp file";
            return out;
        }
        f.write(reinterpret_cast<const char*>(input_bytes.data()),
                static_cast<std::streamsize>(input_bytes.size()));
    }

    tml::testing::ProcessOptions opts;
    opts.exe_path = cached_exe_path_.string();
    opts.args = {tmp_in.string()};
    opts.timeout = std::chrono::milliseconds(300'000);

    auto proc = tml::testing::Process::launch(opts);
    if (!proc) {
        out.error = "subprocess launch failed";
        fs::remove(tmp_in, ec);
        return out;
    }
    auto result = proc->wait();
    fs::remove(tmp_in, ec);

    out.exit_code = result.exit_code;
    out.stderr_text = std::move(result.stderr_output);
    if (result.exit_code != 0) {
        out.error = "stage exited non-zero";
        return out;
    }
    out.stdout_bytes.assign(result.stdout_output.begin(), result.stdout_output.end());
    out.success = true;
    return out;
}

} // namespace tml::pipeline
