//! # Diagnostic Test Execution
//!
//! This file implements the diagnostic test mode for verifying compiler error messages.
//!
//! ## How It Works
//!
//! Diagnostic test files (`*.error.tml`) contain intentionally invalid code along
//! with `@expect-error` directives that specify which error codes the compiler
//! should emit.
//!
//! ```text
//! // @expect-error T001
//! let x: I32 = "hello"   // type mismatch
//! ```
//!
//! ## Test Outcomes
//!
//! | Scenario                        | Result |
//! |---------------------------------|--------|
//! | All expected errors are emitted | PASS   |
//! | Compilation succeeds (no error) | FAIL   |
//! | Wrong error code emitted        | FAIL   |
//! | Expected error not found        | FAIL   |
//!
//! ## Error Matching
//!
//! Errors are matched by error code (e.g., T001, B005). An optional message
//! pattern provides substring matching for additional validation.

#include "cli/builder/builder_internal.hpp"
#include "log/log.hpp"
#include "tester_internal.hpp"
#include "types/module_binary.hpp"

#include <regex>

namespace tml::cli::tester {

// ============================================================================
// Compile and Collect Errors
// ============================================================================

/// Attempt to compile a diagnostic test file through the full pipeline
/// (lex → parse → typecheck → borrow check), collecting all error codes.
///
/// Returns: vector of {error_code, message} pairs from all phases.
static std::vector<std::pair<std::string, std::string>>
collect_compilation_errors(const std::string& file_path, bool /*verbose*/) {
    std::vector<std::pair<std::string, std::string>> errors;

    // Read source
    std::string source_code;
    try {
        source_code = read_file(file_path);
    } catch (const std::exception& e) {
        errors.push_back({"E001", "Failed to read file: " + std::string(e.what())});
        return errors;
    }

    // Preprocess
    auto pp_config = preprocessor::Preprocessor::host_config();
    preprocessor::Preprocessor pp(pp_config);
    auto pp_result = pp.process(source_code, file_path);

    if (!pp_result.success()) {
        for (const auto& diag : pp_result.diagnostics) {
            if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                errors.push_back({"P001", diag.message});
            }
        }
        return errors;
    }

    // Lex
    auto source = lexer::Source::from_string(pp_result.output, file_path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& err : lex.errors()) {
            std::string code = err.code.empty() ? "L001" : err.code;
            errors.push_back({code, err.message});
        }
        return errors;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(file_path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& parse_errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& err : parse_errors) {
            std::string code = err.code.empty() ? "P001" : err.code;
            errors.push_back({code, err.message});
        }
        return errors;
    }
    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& type_errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& err : type_errors) {
            std::string code = err.code.empty() ? "T001" : err.code;
            errors.push_back({code, err.message});
        }
        return errors;
    }
    const auto& env = std::get<types::TypeEnv>(check_result);

    // Borrow check
    if (CompilerOptions::polonius) {
        borrow::polonius::PoloniusChecker polonius_checker(env);
        auto borrow_result = polonius_checker.check_module(module);
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            const auto& borrow_errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
            for (const auto& err : borrow_errors) {
                // Map BorrowErrorCode enum to string code
                std::string code = "B099";
                switch (err.code) {
                case borrow::BorrowErrorCode::UseAfterMove:
                    code = "B001";
                    break;
                case borrow::BorrowErrorCode::MoveWhileBorrowed:
                    code = "B002";
                    break;
                case borrow::BorrowErrorCode::AssignNotMutable:
                    code = "B003";
                    break;
                case borrow::BorrowErrorCode::AssignWhileBorrowed:
                    code = "B004";
                    break;
                case borrow::BorrowErrorCode::BorrowAfterMove:
                    code = "B005";
                    break;
                case borrow::BorrowErrorCode::MutBorrowNotMutable:
                    code = "B006";
                    break;
                case borrow::BorrowErrorCode::MutBorrowWhileImmut:
                    code = "B007";
                    break;
                case borrow::BorrowErrorCode::DoubleMutBorrow:
                    code = "B008";
                    break;
                case borrow::BorrowErrorCode::ImmutBorrowWhileMut:
                    code = "B009";
                    break;
                case borrow::BorrowErrorCode::ReturnLocalRef:
                    code = "B010";
                    break;
                case borrow::BorrowErrorCode::PartialMove:
                    code = "B011";
                    break;
                case borrow::BorrowErrorCode::OverlappingBorrow:
                    code = "B012";
                    break;
                case borrow::BorrowErrorCode::UseWhileBorrowed:
                    code = "B013";
                    break;
                case borrow::BorrowErrorCode::ClosureCapturesMoved:
                    code = "B014";
                    break;
                case borrow::BorrowErrorCode::ClosureCaptureConflict:
                    code = "B015";
                    break;
                case borrow::BorrowErrorCode::PartiallyMovedValue:
                    code = "B016";
                    break;
                case borrow::BorrowErrorCode::ReborrowOutlivesOrigin:
                    code = "B017";
                    break;
                case borrow::BorrowErrorCode::AmbiguousReturnLifetime:
                    code = "B031";
                    break;
                case borrow::BorrowErrorCode::InteriorMutWarning:
                    code = "W001";
                    break;
                default:
                    code = "B099";
                    break;
                }
                errors.push_back({code, err.message});
            }
            return errors;
        }
    } else {
        borrow::BorrowChecker borrow_checker(env);
        auto borrow_result = borrow_checker.check_module(module);
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            const auto& borrow_errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
            for (const auto& err : borrow_errors) {
                std::string code = "B099";
                switch (err.code) {
                case borrow::BorrowErrorCode::UseAfterMove:
                    code = "B001";
                    break;
                case borrow::BorrowErrorCode::MoveWhileBorrowed:
                    code = "B002";
                    break;
                case borrow::BorrowErrorCode::AssignNotMutable:
                    code = "B003";
                    break;
                case borrow::BorrowErrorCode::AssignWhileBorrowed:
                    code = "B004";
                    break;
                case borrow::BorrowErrorCode::BorrowAfterMove:
                    code = "B005";
                    break;
                case borrow::BorrowErrorCode::MutBorrowNotMutable:
                    code = "B006";
                    break;
                case borrow::BorrowErrorCode::MutBorrowWhileImmut:
                    code = "B007";
                    break;
                case borrow::BorrowErrorCode::DoubleMutBorrow:
                    code = "B008";
                    break;
                case borrow::BorrowErrorCode::ImmutBorrowWhileMut:
                    code = "B009";
                    break;
                case borrow::BorrowErrorCode::ReturnLocalRef:
                    code = "B010";
                    break;
                case borrow::BorrowErrorCode::PartialMove:
                    code = "B011";
                    break;
                case borrow::BorrowErrorCode::OverlappingBorrow:
                    code = "B012";
                    break;
                case borrow::BorrowErrorCode::UseWhileBorrowed:
                    code = "B013";
                    break;
                case borrow::BorrowErrorCode::ClosureCapturesMoved:
                    code = "B014";
                    break;
                case borrow::BorrowErrorCode::ClosureCaptureConflict:
                    code = "B015";
                    break;
                case borrow::BorrowErrorCode::PartiallyMovedValue:
                    code = "B016";
                    break;
                case borrow::BorrowErrorCode::ReborrowOutlivesOrigin:
                    code = "B017";
                    break;
                case borrow::BorrowErrorCode::AmbiguousReturnLifetime:
                    code = "B031";
                    break;
                case borrow::BorrowErrorCode::InteriorMutWarning:
                    code = "W001";
                    break;
                default:
                    code = "B099";
                    break;
                }
                errors.push_back({code, err.message});
            }
            return errors;
        }
    }

    // No errors at any phase - compilation succeeded
    return errors;
}

// ============================================================================
// Run Diagnostic Tests
// ============================================================================

int run_diagnostic_tests(const std::vector<std::string>& diag_files, const TestOptions& opts,
                         TestResultCollector& collector, const ColorOutput& /*c*/) {
    using Clock = std::chrono::high_resolution_clock;

    if (diag_files.empty()) {
        return 0;
    }

    // Pre-load library modules (needed for type checking)
    types::preload_all_meta_caches();

    int failures = 0;

    for (const auto& file_path : diag_files) {
        auto test_start = Clock::now();

        // Parse expectations from the file
        auto expectations = parse_diagnostic_expectations(file_path);

        if (expectations.empty()) {
            // File has no @expect-error directives - this is a test authoring error
            TestResult result;
            result.file_path = file_path;
            result.test_name = fs::path(file_path).stem().string();
            result.group = "diagnostic";
            result.passed = false;
            result.test_count = 1;
            result.error_message = "No @expect-error directives found in diagnostic test file";
            collector.add(std::move(result));
            ++failures;
            continue;
        }

        // Try to compile and collect errors
        auto actual_errors = collect_compilation_errors(file_path, opts.verbose);

        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - test_start)
                .count();

        // Match expected errors against actual errors
        // Strategy: for each expectation, find at least one actual error with matching code
        std::vector<DiagnosticExpectation> exps = expectations;
        std::vector<bool> actual_matched(actual_errors.size(), false);

        for (auto& exp : exps) {
            for (size_t i = 0; i < actual_errors.size(); ++i) {
                if (actual_matched[i])
                    continue;
                const auto& [code, message] = actual_errors[i];
                if (code == exp.error_code) {
                    // Code matches - check optional message pattern
                    if (exp.message_pattern.empty() ||
                        message.find(exp.message_pattern) != std::string::npos) {
                        exp.matched = true;
                        actual_matched[i] = true;
                        break;
                    }
                }
            }
        }

        // Determine pass/fail
        bool all_matched = true;
        std::ostringstream failure_msg;

        for (const auto& exp : exps) {
            if (!exp.matched) {
                all_matched = false;
                failure_msg << "  Expected error " << exp.error_code;
                if (!exp.message_pattern.empty()) {
                    failure_msg << " matching \"" << exp.message_pattern << "\"";
                }
                failure_msg << " (line " << exp.line_number << ") was NOT emitted\n";
            }
        }

        if (actual_errors.empty()) {
            all_matched = false;
            failure_msg << "  Compilation SUCCEEDED but errors were expected\n";
        }

        // Build result
        TestResult result;
        result.file_path = file_path;
        result.test_name = fs::path(file_path).stem().string();
        result.group = "diagnostic";
        result.test_count = static_cast<int>(exps.size());
        result.duration_ms = duration_ms;
        result.passed = all_matched;

        if (!all_matched) {
            result.error_message = "\n  FAILED: " + result.test_name + " (diagnostic)\n";
            result.error_message += failure_msg.str();
            if (!actual_errors.empty()) {
                result.error_message += "  Actual errors:\n";
                for (const auto& [code, message] : actual_errors) {
                    result.error_message += "    [" + code + "] " + message + "\n";
                }
            }
            ++failures;

            TML_LOG_ERROR("test",
                          "FAILED diagnostic test=" << result.test_name << " file=" << file_path);
        }

        if (opts.verbose || !result.passed) {
            TML_LOG_INFO("test", (result.passed ? "ok" : "FAILED")
                                     << " " << result.test_name << " (" << exps.size()
                                     << " expected error" << (exps.size() != 1 ? "s" : "") << ", "
                                     << actual_errors.size() << " actual, " << duration_ms
                                     << "ms)");
        }

        collector.add(std::move(result));

        // Fail fast
        if (opts.fail_fast && failures > 0) {
            break;
        }
    }

    return failures;
}

} // namespace tml::cli::tester
