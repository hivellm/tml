# LCOV parity with `llvm-cov export`

This document records the phase0w 11.2 cross-validation procedure —
every release should re-run it to confirm the coverage library's LCOV
emit path stays byte-compatible (modulo section ordering) with the
output of `llvm-cov export -format=lcov`.

## Reference toolchain

| Tool | Path | Version |
|---|---|---|
| `clang++` | `F:/LLVM/bin/clang++.exe` | 22.1.0 |
| `llvm-profdata` | `F:/LLVM/bin/llvm-profdata.exe` | 22.1.0 |
| `llvm-cov` | `F:/LLVM/bin/llvm-cov.exe` | 22.1.0 |

## Procedure

```bash
# 1. Compile a small instrumented binary.
clang++ -O0 -fprofile-instr-generate -fcoverage-mapping add.cpp -o add.exe

# 2. Run it, producing a raw profile.
LLVM_PROFILE_FILE=add.profraw ./add.exe

# 3. Merge the raw profile (the minimum for llvm-cov export).
llvm-profdata merge -sparse add.profraw -o add.profdata

# 4. Emit the reference LCOV.
llvm-cov export add.exe -instr-profile=add.profdata -format=lcov \
    > reference.lcov

# 5. Round-trip through the TML coverage library.
coverage_cli --input=reference.lcov --format=lcov \
             --output=out

# 6. Structural diff.
sort reference.lcov > ref_sorted.txt
sort out/coverage.lcov > cli_sorted.txt
diff ref_sorted.txt cli_sorted.txt   # must exit 0
```

## Known, accepted differences

`llvm-cov export` emits the records for each file in the order
`FN` → `FNDA` → `FNF/FNH` → `DA` → `BRDA` → `BRF/BRH`, while the TML
writer emits `FN` → `FNDA` → `FNF/FNH` → `BRDA` → `BRF/BRH` → `DA`.
Both orderings are valid per the LCOV spec — every downstream consumer
parses by record prefix, not position — so the `sort | diff` check at
step 6 is the canonical acceptance gate.

## Aggregate counts match

On the reference fixture (`lib/coverage/tests/fixtures/lcov/llvm_cov_reference.info`,
27-line C++ with three functions and one branch):

| Metric | llvm-cov | TML LCOV | TML Cobertura | Match |
|---|---|---|---|---|
| `LF` / `lines-valid` | 16 | 16 | 16 | ✅ |
| `LH` / `lines-covered` | 14 | 14 | 14 | ✅ |
| `FNF` | 3 | 3 | — | ✅ |
| `FNH` | 3 | 3 | — | ✅ |
| `BRF` / `branches-valid` | 4 | 4 | 4 | ✅ |
| `BRH` / `branches-covered` | 2 | 2 | 2 | ✅ |

The same reference file also round-trips cleanly through the JSON and
Cobertura XML emitters — `coverage_cli --format=all` produces all four
artefacts with consistent totals.

## Regression guard

The reference LCOV is checked in at
`lib/coverage/tests/fixtures/lcov/llvm_cov_reference.info` and parsed
by the `test_llvm_cov_reference` case in
`lib/coverage/tests/ingest_lcov.test.tml`. That test asserts the
structural counts (3 FNs, 4 BRDAs, 16 DAs with 14 hit), so any future
change to `read_lcov` that breaks the llvm-cov format gets caught by
the normal test run.

To re-generate the fixture from a fresh LLVM build, replay steps 1-4
above and replace the checked-in file.
