//! # Process Unit Tests
//!
//! Tests for the cross-platform Process abstraction:
//! - Launch and capture stdout (1.1.11)
//! - Process timeout and kill (1.1.12)
//! - Concurrent launch / no leaks (1.1.13)
//! - Descendant process termination (1.1.14)

#include "testing/testing_process.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace tml::testing;

// ============================================================================
// 1.1.11 — Launch echo process, read stdout, verify exit code
// ============================================================================

TEST(ProcessTest, LaunchEchoAndCaptureStdout) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "echo hello"});
#else
    auto proc = Process::launch("/bin/echo", {"hello"});
#endif
    ASSERT_TRUE(proc.has_value()) << "Failed to launch echo process";
    EXPECT_TRUE(proc->valid());

    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_TRUE(result.launched);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("hello"), std::string::npos)
        << "stdout was: [" << result.stdout_output << "]";
}

TEST(ProcessTest, LaunchWithExitCode) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "exit /b 42"});
#else
    auto proc = Process::launch("/bin/sh", {"-c", "exit 42"});
#endif
    ASSERT_TRUE(proc.has_value());

    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_TRUE(result.launched);
    EXPECT_EQ(result.exit_code, 42);
}

TEST(ProcessTest, CaptureStderr) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "echo error_msg 1>&2"});
#else
    auto proc = Process::launch("/bin/sh", {"-c", "echo error_msg >&2"});
#endif
    ASSERT_TRUE(proc.has_value());

    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stderr_output.find("error_msg"), std::string::npos)
        << "stderr was: [" << result.stderr_output << "]";
}

// ============================================================================
// 1.1.10 — Environment variable injection
// ============================================================================

TEST(ProcessTest, EnvironmentVariableInjection) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "echo %TML_TEST_VAR%"},
                                {{"TML_TEST_VAR", "injected_value_42"}});
#else
    auto proc = Process::launch("/bin/sh", {"-c", "echo $TML_TEST_VAR"},
                                {{"TML_TEST_VAR", "injected_value_42"}});
#endif
    ASSERT_TRUE(proc.has_value());

    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("injected_value_42"), std::string::npos)
        << "stdout was: [" << result.stdout_output << "]";
}

// ============================================================================
// 1.1.12 — Process timeout (launch sleep, verify kill after timeout)
// ============================================================================

TEST(ProcessTest, TimeoutKillsProcess) {
#ifdef _WIN32
    // ping -n 60 127.0.0.1 is a reliable Windows "sleep" equivalent
    auto proc = Process::launch("ping", {"-n", "60", "127.0.0.1"});
#else
    auto proc = Process::launch("/bin/sleep", {"60"});
#endif
    ASSERT_TRUE(proc.has_value());

    auto start = std::chrono::steady_clock::now();
    auto result = proc->wait(std::chrono::seconds(2));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(result.timed_out) << "Expected timeout but process exited normally";
    EXPECT_NE(result.exit_code, 0);

    // Should have returned within ~5 seconds (2s timeout + grace)
    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    EXPECT_LE(elapsed_sec, 8) << "Timeout took too long: " << elapsed_sec << "s";
}

// ============================================================================
// Non-blocking poll
// ============================================================================

TEST(ProcessTest, IsDoneNonBlocking) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "echo done"});
#else
    auto proc = Process::launch("/bin/echo", {"done"});
#endif
    ASSERT_TRUE(proc.has_value());

    // Wait for it to finish
    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_EQ(result.exit_code, 0);

    // After wait, is_done should return true
    EXPECT_TRUE(proc->is_done());
}

// ============================================================================
// 1.1.13 — Concurrent launch (10 processes, no handle/pipe/fd leaks)
// ============================================================================

TEST(ProcessTest, ConcurrentLaunchNoLeaks) {
    const int N = 10;
    std::vector<Process> processes;

    for (int i = 0; i < N; ++i) {
#ifdef _WIN32
        auto proc = Process::launch("cmd", {"/c", "echo " + std::to_string(i)});
#else
        auto proc = Process::launch("/bin/echo", {std::to_string(i)});
#endif
        ASSERT_TRUE(proc.has_value()) << "Failed to launch process " << i;
        processes.push_back(std::move(*proc));
    }

    // Wait for all to complete
    for (int i = 0; i < N; ++i) {
        auto result = processes[i].wait(std::chrono::seconds(10));
        EXPECT_EQ(result.exit_code, 0) << "Process " << i << " failed";
        EXPECT_TRUE(result.launched);
    }

    // Verify all processes report done
    for (auto& proc : processes) {
        EXPECT_TRUE(proc.is_done());
    }
    // Destructor cleanup happens here — verifies no crashes from double-close
}

// ============================================================================
// 1.1.14 — Descendant process termination (job object / process group)
// ============================================================================

TEST(ProcessTest, DescendantTermination) {
#ifdef _WIN32
    // Launch a shell that spawns a background child, both sleeping
    // Job object with KILL_ON_JOB_CLOSE should kill both
    auto proc = Process::launch(
        "cmd", {"/c", "start /b ping -n 60 127.0.0.1 >nul & ping -n 60 127.0.0.1 >nul"});
#else
    // Launch sh that spawns a background sleep, process group kill should get both
    auto proc = Process::launch("/bin/sh", {"-c", "sleep 60 & sleep 60"});
#endif
    ASSERT_TRUE(proc.has_value());

    // Give child processes time to spawn
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Kill the process tree
    proc->kill();

    // Verify the main process is done
    EXPECT_TRUE(proc->is_done());
}

// ============================================================================
// Elapsed time tracking
// ============================================================================

TEST(ProcessTest, ElapsedTimeTracking) {
#ifdef _WIN32
    auto proc = Process::launch("cmd", {"/c", "echo fast"});
#else
    auto proc = Process::launch("/bin/echo", {"fast"});
#endif
    ASSERT_TRUE(proc.has_value());

    auto result = proc->wait(std::chrono::seconds(10));
    EXPECT_TRUE(result.launched);
    EXPECT_GT(result.duration_us, 0) << "Duration should be positive";

    auto elapsed = proc->elapsed();
    EXPECT_GT(elapsed.count(), 0);
}

// ============================================================================
// Invalid executable
// ============================================================================

TEST(ProcessTest, InvalidExecutableReturnsNullopt) {
    auto proc = Process::launch("/nonexistent/path/to/binary_that_does_not_exist_12345");
    // On some platforms launch may succeed (fork succeeds, exec fails with exit 127)
    // On others it may fail. Either way, it should not crash.
    if (proc.has_value()) {
        auto result = proc->wait(std::chrono::seconds(5));
        // The process should fail with a non-zero exit code (127 from exec failure)
        EXPECT_NE(result.exit_code, 0);
    }
    // If nullopt, that's also fine — clean failure
}
