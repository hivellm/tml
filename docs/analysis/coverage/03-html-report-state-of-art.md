# 03 — HTML coverage report: state of the art

**Goal**: compare the dominant HTML reporters in 2026, extract best practices, and define the UX contract TML must meet (or exceed) in its rewrite.

---

## Reference reporters

### `genhtml` (LCOV)

- **Input**: `coverage.info` (LCOV).
- **Output**: directory with `index.html` (summary table), one HTML per directory (mid-level summary), one HTML per source file.
- **Rendering**: server-side, Perl-generated static HTML. No JS runtime. Plain CSS.
- **Features**: line coverage, branch coverage (`+`/`-` markers), function coverage. "Lo/Med/Hi" threshold colouring via CSS classes. Cyclomatic complexity via the optional `--branch-coverage` flag.
- **Strengths**: zero JS dependency; renders on any browser going back to 2000; extremely stable.
- **Weaknesses**: dated UI; no file-tree navigation (flat directory listing); no search; no syntax highlighting; cannot be an SPA.
- **Verdict**: still the baseline fallback; not an aspirational target.

### Istanbul `html` reporter

- **Input**: Istanbul JSON (`coverage-final.json`) + the source files on disk.
- **Output**: static directory with `index.html`, a `prettify.css/js` bundle and one HTML per file (each embedding the source).
- **Rendering**: mostly static HTML, some JS for sort/filter in the index. The per-file HTML embeds the source already tokenised with `<span class="cline-...">` wrappers.
- **Features**: per-line hit counter, branch counter with `I`/`E` markers, function counter, clickable file/folder breadcrumb, sortable columns (line %, branch %, function %). Row colouring by threshold (`low`/`medium`/`high`).
- **Strengths**: the **quality benchmark**. Clean, fast, readable, works offline.
- **Weaknesses**: no file-tree sidebar (navigation is breadcrumb-only); no diff-vs-baseline; no search.
- **Verdict**: the best Pareto-optimal reference. Any TML replacement should match its readability.

### `llvm-cov show --format=html`

- **Input**: `.profdata` + the original binary and source tree.
- **Output**: static HTML files with inline source; since LLVM 15 a simple `index.html` with per-file rows.
- **Features**: line hit count, branch coverage (since LLVM 14), region counters, MC/DC (since LLVM 18), source rendered with light syntax highlighting. Navigation by hyperlink.
- **Strengths**: unmatched precision (regions, MC/DC); ships with LLVM; zero runtime JS.
- **Weaknesses**: UX is austere — tables, no tree, no search, no sticky filters. Source rendering is functional but not beautiful.
- **Verdict**: excellent as a data source; a TML SPA can simply load its JSON and re-skin.

### JaCoCo report

- **Input**: `.exec`.
- **Output**: heavy static-HTML directory tree (`packages/.../class.html`).
- **Features**: five counter dimensions (instruction, line, branch, method, complexity). Per-class page with highlighted source, inline branch annotations. Drill-down through the package tree.
- **Strengths**: completeness (5 dimensions is unique).
- **Weaknesses**: dated look; heavy (hundreds of HTML files for large projects).
- **Verdict**: niche for JVM; irrelevant as a visual reference for TML.

### `coverage.py` HTML

- **Input**: SQLite `.coverage`.
- **Output**: a single-directory static report, client-side navigation via `coverage_html.js`.
- **Features**: file tree on the left, file view with toggles ("executed / missed / both"), search box, Pygments syntax highlighting, sticky header, per-line hit counts, branch marks on the gutter, keyboard shortcuts (`n`/`p` next/previous file, `/` search).
- **Strengths**: probably the **best UX in the coverage space today**. Light, fast, efficient, accessible, zero external JS dependencies.
- **Verdict**: the product-design reference to copy, with attribution.

### ReportGenerator (.NET)

- **Input**: almost anything — LCOV, Cobertura, OpenCover, JaCoCo, Clover.
- **Output**: multiple themes (`Html`, `HtmlSummary`, `HtmlChart`, `HtmlInline`, `HtmlInline_AzurePipelines`, `HtmlDark`).
- **Features**: file tree, per-file source view, diff vs baseline, history graphs (coverage over time, when fed sequential reports), risk-hotspot list, branch/line/method counters, attribute filter.
- **Strengths**: the most complete reporter in the industry. Multi-theme. History tracking.
- **Weaknesses**: large runtime (shipped as a .NET tool, ~30 MB); heavy for simple use; theme switching is coarse-grained.
- **Verdict**: the feature upper bound TML can *optionally* target, but not the baseline.

### `grcov` HTML

- **Input**: `.profraw` / `.gcda`.
- **Output**: Handlebars-based static HTML.
- **Features**: per-file view with highlighted source, line counter, summary index. No branch/function split. Used by Mozilla for Firefox CI.
- **Verdict**: proves that a simple template engine is enough when the data model is LCOV-derived.

### tarpaulin `--out Html`

- **Input**: internal tarpaulin format.
- **Output**: Tera-template static HTML.
- **Features**: summary index, per-file source view, line counters.
- **Verdict**: confirms that "template + JSON" beats string concatenation at every project size.

---

## Feature matrix

| Feature | genhtml | Istanbul | llvm-cov | coverage.py | ReportGenerator | **TML (current)** | **TML (target)** |
|---------|:-------:|:--------:|:--------:|:-----------:|:---------------:|:-----------------:|:----------------:|
| File tree sidebar | ✗ | ✗ | ✗ | ✓ | ✓ | ✗ | ✓ |
| Source syntax highlight | ✗ | ✓ | ~ | ✓ | ✓ | ✓ | ✓ |
| Per-line hit count | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | ✓ |
| Branch coverage | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | ✓ |
| MC/DC | ✗ | ✗ | ✓ (L18+) | ✗ | ✗ | ✗ | ◻ (stretch) |
| Function coverage | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Search | ✗ | ✗ | ✗ | ✓ | ✓ | ✗ | ✓ |
| Keyboard nav | ✗ | ~ | ✗ | ✓ | ~ | ✗ | ✓ |
| Diff vs baseline | ✗ | ✗ | ✗ | ◻ | ✓ | ✗ | ◻ (stretch) |
| Git blame link | ✗ | ✗ | ✗ | ✗ | ~ | ✗ | ◻ (stretch) |
| PR annotation export | LCOV | LCOV | LCOV/SARIF | LCOV | any | none | LCOV |
| Offline / no backend | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Data-JS separation | partial | ✓ | ✓ | ✓ | ✓ | ✗ (inline) | ✓ |

Legend: ✓ yes, ~ partial, ◻ planned stretch, ✗ no.

---

## 2026 best practices

1. **Static client-side only.** No backend, no build step at view time. `index.html` + `coverage.json` + `app.js` + `app.css` — that is the entire deliverable. Browsers open it via `file://` or any static host (GitHub Pages, S3, nginx).
2. **Separate data from presentation.** Data lives in one JSON (ideally `llvm-cov` JSON or a compact derivative). Presentation lives in HTML/JS/CSS authored by humans, not string-concatenated by the build.
3. **File tree + file view, two-pane layout.** Clickable tree on the left (directory hierarchy), source view on the right. This is the minimum expected in 2026.
4. **Per-line gutter with counters and branch markers.** Gutter shows hit count (dim when 0), branch markers (`I`/`E` or `→`/`✗`) beside branch lines.
5. **Syntax highlighting via Prism.js or highlight.js.** ~15 KB gz; works offline; supports TML via a custom grammar (one-time 50-line job).
6. **Keyboard nav.** `/` search, `n`/`p` next/prev file, `g` go-to-line, `Esc` close dialog.
7. **Search.** Fuzzy file-path search (Fuse.js ~6 KB gz), and optional inline source search.
8. **Deterministic JSON.** Sorted keys, canonical form, so that CI diff between two runs is stable and reviewable.
9. **PR integration via service.** The local HTML is the "detail view". For PRs, upload LCOV to Codecov/Coveralls and let the service post line-by-line annotations.
10. **Hyperlink to VCS.** A per-line "view on GitHub/GitLab" link (`https://github.com/org/repo/blob/<sha>/<file>#L<line>`) made configurable via `tml.coverage.toml`.

---

## Recommended stack for TML

### Architecture

```
compiler/src/testing/coverage_report/
├── template/
│   ├── index.html        # shell: left tree, right file view
│   ├── app.js            # vanilla JS, ~300 LOC
│   ├── app.css           # light + dark themes, ~150 LOC
│   ├── prism.min.js      # syntax highlight (vendored, ~15 KB gz)
│   ├── prism.min.css     # ~3 KB
│   └── tml.prism.js      # TML grammar, ~50 LOC
└── emit_html.cpp         # packer: reads LCOV, writes JSON + copies template
```

At build time `emit_html.cpp` does three things, nothing more:
1. Parse LCOV (or consume the `llvm-cov` JSON directly if available).
2. Normalise into a compact internal JSON (file paths relative to project root, counters per line, branch annotations, function summaries).
3. Copy `template/` to `coverage-html/` and write `coverage.json` next to `index.html`.

Target: ~300 LOC in C++ (parsing + packing) versus the current 1,397 LOC. The rest becomes HTML/JS/CSS files in a template directory, maintained by humans.

### JSON shape (proposal)

```json
{
  "version": "1",
  "generated_at": "2026-04-17T18:00:00Z",
  "project_root": "/e/HiveLLM/Tml",
  "totals": { "lines": { "hit": 48210, "total": 64000 },
              "branches": { "hit": 12030, "total": 18000 },
              "functions": { "hit": 7120, "total": 8500 } },
  "files": [
    {
      "path": "compiler-tml/src/parser/parse_decl.tml",
      "lines": { "hit": 412, "total": 488 },
      "branches": { "hit": 98, "total": 140 },
      "functions": { "hit": 34, "total": 41 },
      "line_hits": { "42": 3, "43": 3, "44": 0, "45": 3 },
      "branches_per_line": { "42": [ [1,1], [0,3] ] },
      "source": "..."
    }
  ]
}
```

Notes on the shape:
- `line_hits` is a sparse map: only lines with a counter appear (empty lines, comments, `}` closers are absent and the renderer draws them plain).
- `branches_per_line` uses pairs `[taken, not_taken]` per branch on that line.
- `source` can be inlined (small files) or replaced by `"source_url": "..."` for lazy fetch. For TML size (858 + 1,984 `.tml` files) inline is fine — total gzipped payload stays well under 10 MB.

### Why vanilla JS (not React / Vue)

- 1 deliverable, no build step. Mount the folder, open `index.html`.
- No framework lock-in. Anyone can maintain the template.
- The UX is static enough (tree + file view) that a framework adds cost without value.
- Istanbul and coverage.py both use vanilla JS for the same reason.
- If a framework becomes genuinely useful later (interactive diff tooling), it can be added behind the same JSON contract.

### What to throw away from the current HTML emitter

- The 1,397 LOC of `testing_coverage_html.cpp`: **delete** in full.
- The string-concatenation helpers (`emit_header`, `emit_file_section`, `emit_line_row` and friends).
- The embedded CSS (currently ~400 LOC of inline `<style>`) and inline `<script>` blocks: externalised to files.
- The JSON-per-test dump (`.sandbox/coverage-*.json`): no longer produced; LLVM `.profraw` takes its place.

---

## Acceptance criteria for the new report

- [ ] Single URL (`coverage-html/index.html`) opens in any browser without a server.
- [ ] Renders for the entire TML codebase (858 source + 1,984 test files) in under 1 s after initial JSON parse.
- [ ] File tree on the left matches the directory structure from `lib/`, `compiler/`, `compiler-tml/`.
- [ ] Per-file view shows source with line numbers, gutter hits, branch markers, and syntax highlighting via Prism.
- [ ] Search (`/`) filters files by path.
- [ ] Dark-mode toggle with preference persisted in `localStorage`.
- [ ] Total gzipped payload ≤ 10 MB for the full codebase.
- [ ] Identical output between two runs with identical profile data (byte-equal `coverage.json`).
- [ ] LCOV companion file (`coverage.lcov`) produced alongside the HTML, without extra invocation — single `tml test --coverage` does both.

Continues in [`04-hybrid-cpp-tml-architecture.md`](./04-hybrid-cpp-tml-architecture.md).
