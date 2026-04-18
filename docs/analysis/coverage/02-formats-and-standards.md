# 02 — Coverage formats and standards

**Goal**: map the textual/binary coverage formats used in modern ecosystems, measure SaaS interoperability (Codecov, Coveralls, Sonar), and recommend which TML should emit natively.

---

## Why formats matter

Coverage is almost worthless if the data never reaches the reader — PR reviewers, QA teams, CI dashboards. That reach requires emitting a format the rest of the ecosystem already understands. A project that only emits its own JSON isolates itself; a project that emits LCOV plugs into everything.

Historically the industry absorbed ~6 main formats and discarded dozens. The survivors are:

| Format | Origin | Current use | Native producers |
|--------|--------|-------------|------------------|
| **LCOV** (`.info`) | GCC gcov / LCOV perl (2002) | **Textual lingua franca**. SaaS, IDEs, merge tools. | gcov/lcov, llvm-cov export, coverage.py, Istanbul, c8, grcov, tarpaulin, Coverlet, slather |
| **Cobertura XML** | Cobertura/Java (2003) | Jenkins, GitLab, Azure DevOps. Still standard in Java legacy and .NET. | Cobertura, Coverlet, tarpaulin, grcov, coverage.py |
| **JaCoCo XML** | JaCoCo (2010) | JVM, SonarQube. | JaCoCo |
| **Clover XML** | Atlassian (2005) | Bamboo, Bitbucket. Less used today. | Clover, Istanbul |
| **llvm-cov JSON** | LLVM (2016) | Internal consumption by LLVM/Rust tooling. | llvm-cov export, cargo-llvm-cov |
| **OpenCover XML** | OpenCover (.NET, 2012) | Legacy .NET ecosystem. | OpenCover, Coverlet |
| **SARIF 2.1.0** | OASIS / Microsoft (2019) | Primarily static analysis; GitHub accepts `coverage`-tagged SARIF for PR annotations. | CodeQL, a few linters |

---

## LCOV `.info` in detail

### Format (textual, line-oriented)

```
TN:test_name
SF:/abs/path/to/source.c
FN:<line>,<function_name>
FNF:<total_functions>
FNH:<hit_functions>
FNDA:<hit_count>,<function_name>
BRDA:<line>,<block>,<branch>,<taken_or_dash>
BRF:<total_branches>
BRH:<hit_branches>
DA:<line>,<hit_count>
LF:<total_lines>
LH:<hit_lines>
end_of_record
```

Each `SF:...end_of_record` block is a file. Simple textual format, diff-friendly, mergeable line-by-line (with `lcov -a`).

### Why it won

- **Text**. Grep/sed/awk work. PR diffs work.
- **One section per file**. Order does not matter; merging is associative.
- **Lowest common denominator**. Has line (`DA`), branch (`BRDA`), function (`FNDA`). No region, no MC/DC — but no one outside safety-critical needs it.
- **Viral adoption**. Around 2010 GitHub/Coveralls/Codecov picked LCOV as the reference. By 2015 everyone was emitting it. Network effect.

### Known limitations

- No metadata for timestamp, commit, branch — SaaS wraps it in its own POST body.
- No MC/DC (non-standard extensions exist).
- Absolute paths cause pain in CI with different workdirs; the convention is `lcov --strip N` or post-processing.

### Tools that emit LCOV natively (sample)

| Tool | Command |
|------|---------|
| `lcov` | direct |
| `llvm-cov` | `llvm-cov export -format=lcov` |
| `grcov` | `grcov . -t lcov` |
| `coverage.py` | `coverage lcov` (since 6.0, 2021) |
| Istanbul/nyc | `lcov` reporter (emits `lcov.info` + HTML) |
| `c8` | `--reporter=lcov` |
| Jest/Vitest | `coverageReporters: ['lcov']` |
| Coverlet | `/p:CoverletOutputFormat=lcov` |
| cargo-llvm-cov | `--lcov` |
| tarpaulin | `-o Lcov` |

Effectively universal. Any tool that does not emit LCOV is considered a niche one.

---

## Cobertura XML

### Format

XML with the Cobertura (Java) schema — hierarchy `packages > package > classes > class > lines`. Born in Java but the schema is neutral enough (directory + file + line) to be adopted outside the JVM. `tarpaulin -o Xml`, `grcov -t cobertura`, `pytest --cov --cov-report=xml` all emit Cobertura XML.

### Minimal example

```xml
<?xml version="1.0" ?>
<coverage line-rate="0.75" branch-rate="0.5" timestamp="..." version="1.0">
  <packages>
    <package name="core" line-rate="0.75">
      <classes>
        <class name="foo.c" filename="src/foo.c" line-rate="0.75">
          <lines>
            <line number="10" hits="3"/>
            <line number="11" hits="0"/>
          </lines>
        </class>
      </classes>
    </package>
  </packages>
</coverage>
```

### Where it is expected

- Jenkins Cobertura plugin.
- GitLab: `coverage_report: { coverage_format: cobertura, path: coverage.xml }`.
- Azure DevOps: `PublishCodeCoverageResults@1 codeCoverageTool: Cobertura`.
- SonarQube (accepted, but prefers LCOV or JaCoCo).

### When to emit

If the target CI is GitLab or Azure DevOps and the project requires native, yes. For TML it is redundant with LCOV — but the cost of also emitting it is low (a deterministic transform).

---

## JaCoCo XML

JaCoCo's own schema, hierarchy `report > sessioninfo + package > class + method + counter`. Counters for `INSTRUCTION`, `LINE`, `BRANCH`, `METHOD`, `CLASS`, `COMPLEXITY`. The richest textual format after LLVM's `.profdata`.

Outside the JVM, practically no one emits JaCoCo. Consumers (SonarQube, IntelliJ) accept it but prefer the language's native format. For TML, not worth implementing.

---

## `llvm-cov export` — LCOV and JSON

LLVM is the only toolchain with a **rich binary format** (`.profdata`) that exports to two textual forms:

### LCOV export

`llvm-cov export -format=lcov -instr-profile=merged.profdata binary` produces standard LCOV. Used as a bridge to non-LLVM toolchains.

### JSON export

`llvm-cov export -format=text -instr-profile=merged.profdata binary` emits structured JSON with:
- `version`: JSON schema version (was `2.0.0` on LLVM 15, `2.0.1` on LLVM 18 [verify exact]).
- `type`: `"llvm.coverage.json.export"`.
- `data`: array of per-binary objects. Each object has `files` (region counters) and `functions` (name, hit count, regions, **branches**, **MC/DC** since LLVM 18).

**Utility for TML**: the richest format. Preserves regions (nested AST blocks), branch coverage and MC/DC. If TML is going to emit JSON for the HTML reporter, the best path is **to start from this JSON as input**, not reinvent it.

Official schema: `llvm/tools/llvm-cov/CoverageExporterJson.cpp` (versioned with LLVM).

---

## SARIF (Static Analysis Results Interchange Format)

OASIS standard, JSON, version 2.1.0 (2020). Designed for **static-analysis findings**, but the schema's `regions`/`hits` allow describing coverage. GitHub accepts SARIF via `upload-sarif` for PR annotations.

**Important caveat**: coverage in SARIF is a secondary use case. No coverage tool **emits** SARIF natively today [verify — possibly CodeQL in coverage mode]. The real use case is LCOV→SARIF conversion in projects that centralise all quality signals in GitHub Code Scanning.

**For TML**: ignore in the first iteration. It can become a secondary output later.

---

## Compatibility matrix with SaaS services

| Service | LCOV | Cobertura XML | JaCoCo | llvm-cov JSON | Other |
|---------|------|---------------|--------|---------------|-------|
| **Codecov** | yes (default) | yes | yes | via conversion | 25+ formats; autodetect |
| **Coveralls** | yes (default) | yes | yes | via conversion | Istanbul JSON, LCOV, Cobertura |
| **SonarQube** | yes (`sonar.coverageReportPaths`) | yes | yes (preferred for JVM) | not native | "Generic Test Coverage" XML of its own |
| **GitLab CI** | via `cobertura` badge | yes (native) | yes | no | needs conversion |
| **Azure DevOps** | via conversion | yes (native) | yes | no | PublishCodeCoverageResults@1 |
| **GitHub Code Coverage** (new, 2024) | yes | yes | yes | via export | [verify — feature still in gradual rollout] |

**Conclusion**: **LCOV is the only format accepted by all of them**. Cobertura XML is second but loses on Sonar. JaCoCo is JVM-only. SARIF is niche.

---

## Recommended outputs for TML

### Primary: LCOV `.info`

Reasons:
1. Universal (every SaaS accepts it).
2. Text, diff-friendly, mergeable.
3. Generated natively by `llvm-cov export -format=lcov` — zero extra work if we adopt LLVM source-based coverage (see [`04-hybrid-cpp-tml-architecture.md`](./04-hybrid-cpp-tml-architecture.md)).
4. Covers the three dimensions that matter: line, branch, function.

### Secondary: `llvm-cov export` JSON

Reasons:
1. Preserves regions (useful for rich reports).
2. Natural **input** for the HTML reporter (converted to a compact frontend format).
3. Format defined and versioned by LLVM — we invent nothing.

### Tertiary (optional, on demand): Cobertura XML

Via an LCOV→Cobertura converter (several off-the-shelf: `lcov_cobertura.py`, `lcov-to-cobertura-xml`). Emit only if a specific user asks (e.g., a GitLab pipeline without an LCOV plugin).

### Do not emit

- Current TML proprietary JSON: **discontinue**.
- JaCoCo XML: irrelevant outside the JVM.
- SARIF: defer.
- Clover: dead.

---

## Format migration path

1. **Step 0** (today): TML emits ad-hoc JSON. Codecov/Coveralls ignore. Zero external integration.
2. **Step 1**: adopt LLVM source-based → `.profraw` → `.profdata` → `llvm-cov export -format=lcov` → `coverage.lcov`. **From this moment Codecov works.**
3. **Step 2**: add `llvm-cov export -format=text` → `coverage.json`. HTML reporter consumes this JSON.
4. **Step 3**: optional LCOV→Cobertura converter (20-line Python script, or C++ if we keep the monolith).

The structural advantage: step 1 is **one external-binary invocation**. We do not write a parser, we do not write a serializer. We inherit the entire LLVM ecosystem.

---

## On the current `.sandbox/coverage-*.json`

Today each test process writes `coverage-<pid>.json` via `tml_coverage_write_file`, aggregated in C++ by `testing_coverage.cpp`. This file:

- Contains only **mangled function names** (no hit counts, no lines).
- Is incompatible with every external consumer.
- Duplicates LLVM profile runtime work (which already serialises atomically, merges concurrently, handles crashes).

The entire format should be **discarded**. The natural replacement is LLVM's `.profraw`, which is written atomically via `mmap`, supports multiple concurrent processes, and is merged with a single call to `llvm-profdata merge`.

Continues in [`03-html-report-state-of-art.md`](./03-html-report-state-of-art.md).
