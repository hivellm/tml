# HTML-facing JSON schema

The JSON file consumed by `src/template/app.js` is a compact,
denormalized view of `CoverageReport`. Its shape is optimized for the
SPA: the file tree is pre-computed, per-line hit arrays are inline,
zero-hit lines are stored as a compact index list (compact mode only).

## Top-level object

```jsonc
{
  "tool": "llvm-cov",               // string — producing tool
  "version": "22.1.0",              // string — tool version
  "generated_at": 1713398400,       // int    — unix seconds
  "summary": { ... },               // project totals (see below)
  "files": [ { ... }, ... ],        // per-file entries
  "tree": { ... }                   // pre-computed file tree
}
```

## `summary`

```jsonc
{
  "pct_lines": 87.3,
  "pct_branches": 64.1,
  "pct_functions": 92.7,
  "covered_lines": 1234,
  "total_lines": 1413,
  "covered_branches": 321,
  "total_branches": 501,
  "covered_functions": 789,
  "total_functions": 851
}
```

## `files[]`

```jsonc
{
  "p": "lib/core/src/str/basic.tml",   // path (short key in compact mode)
  "s": { "pct_lines": 95.0, ... },     // per-file summary (same shape as top-level)
  "l": [                                // per-line entries (covered lines only in compact)
    { "l": 1, "c": 42 },
    { "l": 2, "c": 42 },
    { "l": 5, "c": 7,  "b": [1, 1] }   // optional `b`: [branch_count, branch_taken]
  ],
  "z": [3, 4, 6],                      // zero-hit lines (compact mode only)
  "fn": [                               // functions
    { "n": "add", "sl": 1, "h": 42 }
  ],
  "br": [                               // branches (optional)
    { "l": 5, "k": 0, "r": 0, "h": 7 },
    { "l": 5, "k": 0, "r": 1, "h": 0 }
  ]
}
```

Short-key legend (compact mode uses single-letter keys to shrink payload):

| Key | Full name       | Type   |
|-----|-----------------|--------|
| `p` | path            | string |
| `s` | summary         | object |
| `l` | lines           | array  |
| `l` | line (inside)   | int    |
| `c` | count           | int    |
| `b` | branch[ct, tk]  | array  |
| `z` | zero-hit lines  | array  |
| `fn`| functions       | array  |
| `n` | name            | string |
| `sl`| start line      | int    |
| `h` | hit count       | int    |
| `br`| branches        | array  |
| `k` | block id        | int    |
| `r` | branch id       | int    |

In non-compact (pretty) mode every key is spelled out in full
(`path`, `summary`, `lines`, `count`, etc.) and every line — hit or not
— appears in `lines[]`.

## `tree`

The file tree is precomputed so `app.js` can render without walking
`files[]` every time. Each tree node is either a directory or a file:

```jsonc
{
  "n": "lib",                         // path segment
  "d": true,                          // directory flag (false => file)
  "s": { "pct_lines": 91.0, ... },    // aggregated summary for this subtree
  "c": [ { ... }, { ... } ]           // children (present on directories)
}
```

File nodes have `d: false` and point at an index into `files[]` via
`"i": <index>` instead of nesting duplicate data.

## Size budget

For the TML project's ~3,000-file corpus the compact payload must
stay under 5 MB gzip-uncompressed. Empirical compact-mode budget:

| Item                | Bytes/file (avg)   |
|---------------------|--------------------|
| file envelope       | ~80                |
| per-line entry      | ~18                |
| zero-hit index      | ~4                 |
| function entry      | ~40                |
| branch entry        | ~24                |

3,000 files × 500 lines average × 18 bytes ≈ 27 MB (pretty mode) →
compact mode dropping zero-hit lines typically collapses this by 4–8×,
keeping the total under the budget for typical codebases where > 60%
of lines are zero-hit in any single run.
