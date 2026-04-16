// Tests for the TTY-aware output helper (cli/tty_output.{hpp,cpp}).
//
// These tests cover the deterministic parts of the helper: ANSI stripping and
// mode semantics. The actual pipe-buffer behaviour is tested end-to-end via
// the bash script in `compiler/tests/cli/pipe_output.sh`, because it needs
// real stdout/stderr redirection that gtest cannot faithfully simulate.

#include "cli/tty_output.hpp"

#include <gtest/gtest.h>

using namespace tml::cli::tty;

TEST(TtyOutputStripAnsi, PassesPlainTextThrough) {
    EXPECT_EQ(strip_ansi("hello world"), "hello world");
    EXPECT_EQ(strip_ansi(""), "");
    EXPECT_EQ(strip_ansi("\n"), "\n");
}

TEST(TtyOutputStripAnsi, RemovesSimpleColorCodes) {
    // Red "error" reset.
    EXPECT_EQ(strip_ansi("\x1b[31merror\x1b[0m"), "error");
    EXPECT_EQ(strip_ansi("\x1b[1;31mbold red\x1b[0m"), "bold red");
}

TEST(TtyOutputStripAnsi, HandlesNestedAndMultipleSequences) {
    std::string input = "\x1b[32mgreen\x1b[0m plain \x1b[31mred\x1b[0m";
    EXPECT_EQ(strip_ansi(input), "green plain red");
}

TEST(TtyOutputStripAnsi, HandlesUnterminatedSequence) {
    // If the CSI never closes we drop everything after the \x1b[ — acceptable
    // behaviour for corrupted input; the parser must not hang.
    EXPECT_EQ(strip_ansi("\x1b[31m"), "");
    EXPECT_EQ(strip_ansi("text\x1b[3"), "text");
}

TEST(TtyOutputStripAnsi, PreservesNonCsiEscapes) {
    // Only CSI (`\x1b[`) sequences are stripped; other escapes pass through
    // untouched. We do not try to be smart about OSC/SOS/APC.
    EXPECT_EQ(strip_ansi("\x1b]0;title\x07 after"), "\x1b]0;title\x07 after");
}

TEST(TtyOutputMode, NonInteractiveForcesNoColors) {
    init_runtime_mode(/*force_no_color=*/false, /*force_non_interactive=*/true);
    EXPECT_TRUE(mode().non_interactive);
    EXPECT_FALSE(mode().colors_enabled);
}

TEST(TtyOutputMode, NoColorFlagDisablesColors) {
    init_runtime_mode(/*force_no_color=*/true, /*force_non_interactive=*/false);
    EXPECT_FALSE(mode().colors_enabled);
}
