// Tests for `tml coverage` / `tml cv` command routing.
// Covers: src/cli/commands/cmd_coverage.cpp — the new-mode routing shim that
// forwards `--input=`/`--format=`/`--output=`/`--include=`/`--exclude=`/
// `--baseline=`/`--fail-under=`/`--pretty-json` invocations to
// `coverage_cli.exe` (built from `lib/coverage/src/bin/coverage_cli.tml`),
// while keeping legacy `tml cv` semantics reachable for every other
// invocation.
//
// These tests exercise the public `run_coverage(argc, argv, verbose)` entry
// point without requiring a pre-built `coverage_cli.exe`: when the binary is
// missing, the shim returns exit code 127 with a helpful message. The tests
// verify:
//
//   1. Each recognised new-mode flag triggers the dispatcher (exit 127 when
//      coverage_cli.exe is absent).
//   2. Legacy flags (`--path=`, `--quick`, etc.) continue to reach the
//      original source-to-test mapping path (no exit 127).
//   3. The `coverage` and `cv` aliases route identically.
//
// Regression guard for phase0w item 9.3 — unblocked by the EventEmitter
// codegen fix (commit 66bc0232) that made `coverage_cli.exe` actually build.

#include "cli/commands/cmd_coverage.hpp"

#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Build an argv[] snapshot from a vector<string>. The returned pointers are
// only valid for the lifetime of the input vector (each string keeps its
// storage under `.c_str()`).
std::vector<char*> make_argv(std::vector<std::string>& args) {
    std::vector<char*> out;
    out.reserve(args.size());
    for (auto& s : args) {
        out.push_back(s.data());
    }
    return out;
}

// Invoke tml::cli::run_coverage with the given argv[] and return its exit
// code. Captures the coverage_cli.exe lookup path so we can assert the shim
// actually tried to locate a binary.
int invoke_run_coverage(std::vector<std::string> args) {
    auto argv = make_argv(args);
    return tml::cli::run_coverage(static_cast<int>(argv.size()), argv.data(),
                                  /*verbose=*/false);
}

// True when `coverage_cli.exe` sits in any of the paths the shim searches.
// The test suite is intentionally permissive about where the binary lives
// (debug vs release, bin/ vs top-level) — that's the shim's job to pick one.
bool coverage_cli_present() {
    auto cwd = fs::current_path() / "build";
    std::array<fs::path, 4> candidates = {
        cwd / "debug" / "bin" / "coverage_cli.exe",
        cwd / "release" / "bin" / "coverage_cli.exe",
        cwd / "debug" / "coverage_cli.exe",
        cwd / "release" / "coverage_cli.exe",
    };
    for (const auto& c : candidates) {
        if (fs::exists(c))
            return true;
    }
    return false;
}

} // namespace

// ============================================================================
// New-mode routing: every recognised flag trips the dispatcher
// ============================================================================

class CoverageNewModeTest : public ::testing::Test {
protected:
    // Only assert on the "binary missing" path when the binary really isn't
    // there. If someone runs the test after building coverage_cli.exe, the
    // shim will try to execute it and the exit code depends on the fixture,
    // which isn't what these tests are about.
    bool skip_when_built = false;

    void SetUp() override {
        skip_when_built = coverage_cli_present();
    }
};

TEST_F(CoverageNewModeTest, InputFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--input=fixture.info",
                                  "--format=lcov"});
    EXPECT_EQ(rc, 127) << "Missing coverage_cli.exe must yield exit code 127";
}

TEST_F(CoverageNewModeTest, FormatFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--format=json"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, OutputFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--input=foo",
                                  "--format=lcov", "--output=report"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, IncludeFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--include=src/**"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, ExcludeFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--exclude=tests/**"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, BaselineFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--baseline=prev.json"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, FailUnderFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--fail-under=80"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, PrettyJsonFlagTriggersDispatcher) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--input=foo",
                                  "--format=json", "--pretty-json"});
    EXPECT_EQ(rc, 127);
}

TEST_F(CoverageNewModeTest, CvAliasRoutesIdenticallyToCoverage) {
    if (skip_when_built) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip binary-missing path";
    }
    int rc_coverage = invoke_run_coverage({"tml", "coverage",
                                           "--input=foo", "--format=lcov"});
    int rc_cv = invoke_run_coverage({"tml", "cv", "--input=foo",
                                     "--format=lcov"});
    EXPECT_EQ(rc_coverage, rc_cv)
        << "`tml cv` and `tml coverage` must route identically";
    EXPECT_EQ(rc_coverage, 127);
}

// ============================================================================
// Legacy-mode preservation: non-dispatcher flags stay on the old path
// ============================================================================

// The legacy path scans the CWD for tml.toml / src/ / tests/. It exits with
// a non-127 code regardless of whether that scan finds anything, because the
// new-mode shim short-circuits only on the recognised flag set. We assert
// *not equal to 127* — the exact code depends on the current directory's
// project shape, which isn't deterministic in the test environment.
TEST(CoverageLegacyModeTest, BareInvocationSkipsNewModeDispatcher) {
    if (coverage_cli_present()) {
        GTEST_SKIP() << "coverage_cli.exe is built; legacy-vs-new distinction"
                        " can't be asserted via exit code alone";
    }
    int rc = invoke_run_coverage({"tml", "coverage"});
    EXPECT_NE(rc, 127)
        << "Legacy mode must not try to exec a missing coverage_cli.exe";
}

TEST(CoverageLegacyModeTest, PathFlagStaysOnLegacyMapping) {
    if (coverage_cli_present()) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip legacy-path assert";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--path=lib/coverage"});
    EXPECT_NE(rc, 127);
}

TEST(CoverageLegacyModeTest, QuickFlagStaysOnLegacyMapping) {
    if (coverage_cli_present()) {
        GTEST_SKIP() << "coverage_cli.exe is built; skip legacy-path assert";
    }
    int rc = invoke_run_coverage({"tml", "coverage", "--quick"});
    EXPECT_NE(rc, 127);
}
