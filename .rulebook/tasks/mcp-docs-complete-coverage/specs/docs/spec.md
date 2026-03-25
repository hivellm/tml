# Documentation Coverage Spec Delta

## ADDED Requirements

### Requirement: Top-Level Function Indexing
The MCP documentation extractor SHALL index top-level `pub func` declarations alongside `impl` methods. When a top-level function has a `///` doc comment, the `docs_search` and `docs_get` tools MUST return that description.

#### Scenario: Top-level function with doc appears in search
Given a top-level function `pub func len(s: Str) -> I64` with `///` doc comment "Returns the length of a string in bytes"
When `docs_search("length bytes string")` is called
Then the function SHALL appear in results with its description text

#### Scenario: docs_get returns description and examples for top-level function
Given a function with `///` doc comment containing `@example` blocks
When `docs_get("core::str::len")` is called
Then the response SHALL include the description text AND code examples

### Requirement: Doc Comment Coverage
All public functions, methods, and types in `lib/core/` and `lib/std/` SHALL have `///` doc comments with at minimum a one-line description.

#### Scenario: Function has doc comment
Given a public function `split` in `lib/core/src/str.tml`
When the documentation extractor processes the file
Then the `DocItem` for `split` SHALL have a non-empty `summary` field

#### Scenario: Method has param docs
Given a method `insert(key: K, value: V)` in `HashMap`
When the documentation extractor processes the file
Then the `DocItem` SHALL have `params` entries for `key` and `value` with descriptions

### Requirement: Example Coverage
At least 80% of public types (structs, enums, behaviors) SHALL have at least one `@example` block showing basic usage.

#### Scenario: Type has example
Given a public struct `Mutex[T]` in `lib/std/src/sync/mutex.tml`
When the documentation is extracted
Then the `DocItem` SHALL have at least one entry in `examples` with compilable TML code

#### Scenario: Example is valid
Given an `@example` block in a doc comment
When the example code is compiled with `tml check`
Then the compilation SHALL succeed without errors

### Requirement: Cross-Reference Coverage
Related types SHALL be linked via `@see` tags. At minimum: Maybe↔Outcome, Mutex↔RwLock, List↔Slice, Arc↔Shared, HashMap↔BTreeMap.

#### Scenario: Related type has see-also
Given the doc comment for `Maybe[T]`
When the documentation is extracted
Then the `DocItem` SHALL have `see_also` containing `core::result::Outcome`

### Requirement: Search Quality
The MCP `docs_search` tool SHALL return items with descriptions (not just signatures) for all documented items.

#### Scenario: Search returns description
Given a search query "HashMap insert"
When `docs_search` is called
Then the results SHALL include the `HashMap::insert` method with its description text

### Requirement: Category Tags
Public types and functions SHALL include category information in their doc summary using bracket notation: `[Thread-safe]`, `[Pure TML]`, `[FFI wrapper]`, `[Iterator adapter]`.

#### Scenario: Thread-safe type is tagged
Given the doc comment for `Mutex[T]`
When the documentation is extracted
Then the `summary` SHALL contain `[Thread-safe]`
