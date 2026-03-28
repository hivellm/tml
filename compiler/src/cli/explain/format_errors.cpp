TML_MODULE("compiler")

//! # Formatter/Linter Error Explanations
//!
//! Error codes F001-F010 for formatting and linting rules.
//! These correspond to the style rules enforced by `tml lint` and `tml format`.
//!
//! ## Style Rule Codes
//!
//! | Code | Rule          | Lint Code | Description                     |
//! |------|---------------|-----------|---------------------------------|
//! | F001 | indentation   | S001      | Tabs instead of spaces          |
//! | F002 | trailing-ws   | S002      | Trailing whitespace             |
//! | F003 | final-newline | —         | Missing newline at end of file  |
//! | F004 | line-length   | S003      | Line too long                   |
//! | F005 | mixed-indent  | S001      | Mixed tabs and spaces           |
//! | F006 | func-naming   | S010      | Function not in snake_case      |
//! | F007 | type-naming   | S011      | Type not in PascalCase          |
//! | F008 | const-naming  | S012      | Constant not in UPPER_SNAKE_CASE|
//! | F009 | var-naming    | S013      | Variable not in snake_case      |
//! | F010 | complexity    | C001-C003 | Code complexity limit exceeded  |

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_format_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"F001", R"EX(
Indentation error: tabs instead of spaces [F001]

TML source files must use spaces for indentation, not tabs. The linter
rule S001 (and formatter) enforce this.

Auto-fix available: `tml lint --fix` replaces each tab with 4 spaces.
The formatter (`tml format`) also fixes this automatically.

Example:
  // Wrong — tab character before `let`
  func example() {
  	let x = 1
  }

  // Correct — 4 spaces
  func example() {
      let x = 1
  }

Related: S001 lint rule, F005 (mixed tabs/spaces)
)EX"},

        {"F002", R"EX(
Trailing whitespace [F002]

Lines in TML source files must not have trailing spaces or tabs after
the last non-whitespace character. The linter rule S002 enforces this.

Auto-fix available: `tml lint --fix` trims trailing whitespace from all
lines. The formatter (`tml format`) also fixes this automatically.

This is a common issue introduced by editors that don't strip trailing
whitespace on save. Configure your editor to trim whitespace on save to
avoid this.

Related: S002 lint rule
)EX"},

        {"F003", R"EX(
Missing newline at end of file [F003]

TML source files must end with a newline character. Files without a
final newline cause issues with some tools and differ from POSIX
convention.

Auto-fix available: `tml lint --fix` appends a newline if missing.
The formatter (`tml format`) also fixes this automatically.

Most editors can be configured to always add a trailing newline on save.

Related: F002 (trailing whitespace)
)EX"},

        {"F004", R"EX(
Line too long [F004]

A line in the source file exceeds the maximum allowed line length
(default: 120 characters). The linter rule S003 reports this as a
warning.

How to fix:
1. Break long function calls across multiple lines
2. Introduce intermediate variables to shorten expressions
3. Split long string literals

Example:
  // Too long (hypothetical 150-char line)
  let result = some_function_with_long_name(argument_one, argument_two, argument_three, argument_four)

  // Better — break arguments across lines
  let result = some_function_with_long_name(
      argument_one,
      argument_two,
      argument_three,
      argument_four,
  )

The max line length can be configured in `tml.toml`:
  [lint]
  max_line_length = 100

Related: S003 lint rule
)EX"},

        {"F005", R"EX(
Mixed tabs and spaces [F005]

The source file mixes tab characters and space characters for
indentation. This causes inconsistent rendering across different
editors and tools.

Auto-fix: use `tml lint --fix` to convert all tabs to spaces (4 spaces
per tab). After fixing, the file will use spaces consistently.

TML convention: always use spaces, never tabs.

Related: F001 (tabs instead of spaces), S001 lint rule
)EX"},

        {"F006", R"EX(
Function naming convention: use snake_case [F006]

TML function names must use snake_case: all lowercase letters,
with words separated by underscores. The linter rule S010 enforces this.

Examples:
  // Wrong
  func MyFunction() {}
  func myFunction() {}
  func my-function() {}

  // Correct
  func my_function() {}
  func calculate_total_price() {}
  func read_line() {}

Exception: single-character names and names starting with `_` (to
indicate intentionally unused parameters) are allowed.

Related: S010 lint rule, F007 (type naming), F009 (variable naming)
)EX"},

        {"F007", R"EX(
Type naming convention: use PascalCase [F007]

TML type names (structs, enums, behaviors/traits, type aliases) must
use PascalCase: each word starts with an uppercase letter. The linter
rule S011 enforces this.

Examples:
  // Wrong
  struct my_struct { }
  struct myStruct { }
  enum http_method { }

  // Correct
  struct MyStruct { }
  enum HttpMethod { }
  behavior Printable { }

Related: S011 lint rule, F006 (function naming)
)EX"},

        {"F008", R"EX(
Constant naming convention: use UPPER_SNAKE_CASE [F008]

TML constants declared with `const` must use UPPER_SNAKE_CASE: all
uppercase letters with words separated by underscores. The linter rule
S012 enforces this.

Examples:
  // Wrong
  const maxRetries = 3
  const MaxRetries = 3

  // Correct
  const MAX_RETRIES = 3
  const BUFFER_SIZE = 4096
  const PI = 3.14159

Related: S012 lint rule, F006 (function naming), F007 (type naming)
)EX"},

        {"F009", R"EX(
Variable naming convention: use snake_case [F009]

TML variable names must use snake_case: all lowercase letters, with
words separated by underscores. The linter rule S013 enforces this.

Examples:
  // Wrong
  let myVariable = 1
  let MyVariable = 1

  // Correct
  let my_variable = 1
  let total_count = 0

Exception: names starting with `_` (e.g., `_unused`) are allowed
for intentionally unused variables without triggering the unused
variable warning (W001).

Related: S013 lint rule, F006 (function naming)
)EX"},

        {"F010", R"EX(
Code complexity limit exceeded [F010]

A function in the source file exceeds one or more complexity limits
enforced by the linter:

- **C001** — Function length: more than 50 lines (default)
- **C002** — Cyclomatic complexity: more than 10 decision points
- **C003** — Nesting depth: more than 4 levels of nesting

These limits encourage smaller, more focused functions that are easier
to read, test, and maintain.

How to fix:
1. Extract sub-tasks into separate, well-named helper functions
2. Replace deeply nested conditions with early returns (guard clauses)
3. Use pattern matching (`when`) to reduce branching

Thresholds can be configured in `tml.toml`:
  [lint]
  max_function_lines = 80
  max_cyclomatic_complexity = 15
  max_nesting_depth = 5

Related: C001, C002, C003 lint rules
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
