/* ==========================================================================
   TML coverage report — SPA entry point.
   Consumes `coverage.json` (compact schema from lib/coverage/emit/json.tml).
   Vanilla ES2022. No bundler. No framework.
   ========================================================================== */

(function () {
  const state = {
    data: null,        // parsed coverage.json
    index: {},         // path -> file entry
    filtered: null,    // search-filtered list of paths
    active: null,      // active file path
    sources: new Map() // path -> Promise<string>   (lazy fetch)
  };

  // Entry -----------------------------------------------------------------
  async function main() {
    try {
      const res = await fetch("coverage.json");
      state.data = await res.json();
    } catch (err) {
      document.getElementById("status").textContent = "failed to load coverage.json";
      return;
    }
    indexFiles();
    renderTotals();
    renderTree();
    bindKeys();
    bindSearch();
    document.getElementById("status").textContent = `${state.data.files.length} files`;
  }

  function indexFiles() {
    state.index = {};
    for (const f of state.data.files) {
      const path = f.p || f.path;
      state.index[path] = f;
    }
  }

  // Totals header ---------------------------------------------------------
  function renderTotals() {
    const s = state.data.summary;
    const el = document.getElementById("totals");
    el.innerHTML = "";
    addMetric(el, "lines", s.covered_lines, s.total_lines, s.pct_lines);
    addMetric(el, "branches", s.covered_branches, s.total_branches, s.pct_branches);
    addMetric(el, "functions", s.covered_functions, s.total_functions, s.pct_functions);
  }
  function addMetric(el, label, covered, total, pct) {
    const node = document.createElement("span");
    node.className = "metric " + gradeClass(pct);
    node.innerHTML = `${label}: <b>${fmtPct(pct)}%</b> (${covered}/${total})`;
    el.appendChild(node);
  }
  function gradeClass(pct) {
    if (pct >= 80) return "good";
    if (pct >= 50) return "meh";
    return "bad";
  }
  function fmtPct(n) { return (Math.round(n * 10) / 10).toFixed(1); }

  // File tree -------------------------------------------------------------
  function renderTree() {
    const root = buildTree(listPaths());
    const el = document.getElementById("tree");
    el.innerHTML = "";
    el.appendChild(renderTreeNode(root, 0));
  }

  function listPaths() {
    if (state.filtered) return state.filtered;
    return state.data.files.map(f => f.p || f.path).sort();
  }

  function buildTree(paths) {
    const root = { name: "", dir: true, children: new Map(), filePath: null };
    for (const p of paths) {
      const parts = p.split("/").filter(Boolean);
      let cur = root;
      for (let i = 0; i < parts.length; i++) {
        const part = parts[i];
        const isFile = i === parts.length - 1;
        if (!cur.children.has(part)) {
          cur.children.set(part, {
            name: part,
            dir: !isFile,
            children: new Map(),
            filePath: isFile ? p : null
          });
        }
        cur = cur.children.get(part);
      }
    }
    return root;
  }

  function renderTreeNode(node, depth) {
    const ul = document.createElement("ul");
    const entries = Array.from(node.children.values())
      .sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
    for (const child of entries) {
      const li = document.createElement("li");
      const row = document.createElement("div");
      row.className = "node";

      const caret = document.createElement("span");
      caret.className = "caret";
      caret.textContent = child.dir ? "▸" : "";
      row.appendChild(caret);

      const icon = document.createElement("span");
      icon.className = "icon";
      icon.textContent = child.dir ? "📁" : "📄";
      row.appendChild(icon);

      const name = document.createElement("span");
      name.className = "name";
      name.textContent = child.name;
      row.appendChild(name);

      const pct = document.createElement("span");
      pct.className = "pct";
      pct.textContent = child.filePath ? fmtPct(summaryFor(child.filePath).pct_lines) + "%" : "";
      row.appendChild(pct);

      if (child.filePath) {
        row.addEventListener("click", () => openFile(child.filePath, row));
      } else {
        row.addEventListener("click", () => {
          const sub = li.querySelector("ul");
          if (sub) sub.hidden = !sub.hidden;
          caret.textContent = (sub && sub.hidden) ? "▸" : "▾";
        });
      }

      li.appendChild(row);
      if (child.dir && child.children.size > 0) {
        const sub = renderTreeNode(child, depth + 1);
        li.appendChild(sub);
      }
      ul.appendChild(li);
    }
    return ul;
  }

  function summaryFor(path) {
    const f = state.index[path];
    return f && (f.s || f.summary) || { pct_lines: 0 };
  }

  // File viewer -----------------------------------------------------------
  async function openFile(path, rowEl) {
    state.active = path;
    const activePrev = document.querySelector(".tree .node.active");
    if (activePrev) activePrev.classList.remove("active");
    if (rowEl) rowEl.classList.add("active");

    const viewer = document.getElementById("viewer");
    viewer.innerHTML = "";
    const wrap = document.createElement("div");
    wrap.className = "fileview";

    const h = document.createElement("h2");
    h.textContent = path;
    wrap.appendChild(h);

    const c = document.createElement("div");
    c.className = "counters";
    const s = summaryFor(path);
    c.innerHTML =
      `lines: <b>${fmtPct(s.pct_lines)}%</b> (${s.covered_lines}/${s.total_lines}) · ` +
      `branches: <b>${fmtPct(s.pct_branches)}%</b> (${s.covered_branches}/${s.total_branches}) · ` +
      `functions: <b>${fmtPct(s.pct_functions)}%</b> (${s.covered_functions}/${s.total_functions})`;
    wrap.appendChild(c);

    const source = await loadSource(path);
    wrap.appendChild(renderSource(path, source));
    viewer.appendChild(wrap);
  }

  async function loadSource(path) {
    if (!state.sources.has(path)) {
      state.sources.set(path, (async () => {
        try {
          const res = await fetch(path);
          if (!res.ok) throw new Error(String(res.status));
          return await res.text();
        } catch {
          return ""; // source not available — show counters without code
        }
      })());
    }
    return state.sources.get(path);
  }

  function renderSource(path, source) {
    const wrap = document.createElement("div");
    wrap.className = "source";
    const table = document.createElement("table");
    const lines = source.split("\n");
    const hits = hitMap(path);
    const zeros = zeroSet(path);

    const lang = guessLang(path);
    const doHighlight = window.Prism && typeof Prism.highlight === "function";

    for (let i = 0; i < lines.length; i++) {
      const lineNo = i + 1;
      const count = hits.get(lineNo);
      const isZero = zeros.has(lineNo);

      let cls = "";
      if (count !== undefined && count > 0) cls = "hit";
      else if (isZero || count === 0) cls = "miss";

      const tr = document.createElement("tr");
      if (cls) tr.className = cls;
      tr.innerHTML =
        `<td class="lineno">${lineNo}</td>` +
        `<td class="gutter">${count !== undefined ? count : (isZero ? 0 : "")}</td>` +
        `<td class="code">${doHighlight ? Prism.highlight(lines[i], Prism.languages[lang] || Prism.languages.markup, lang) : escapeHtml(lines[i])}</td>`;
      table.appendChild(tr);
    }
    wrap.appendChild(table);
    return wrap;
  }

  function hitMap(path) {
    const f = state.index[path];
    const arr = (f && (f.l || f.lines)) || [];
    const m = new Map();
    for (const e of arr) m.set(e.l ?? e.line, e.c ?? e.count);
    return m;
  }
  function zeroSet(path) {
    const f = state.index[path];
    const arr = (f && f.z) || [];
    return new Set(arr);
  }

  function guessLang(path) {
    if (path.endsWith(".tml")) return "tml";
    if (path.endsWith(".cpp") || path.endsWith(".hpp") || path.endsWith(".cc") || path.endsWith(".h")) return "cpp";
    if (path.endsWith(".c")) return "c";
    if (path.endsWith(".rs")) return "rust";
    if (path.endsWith(".js")) return "javascript";
    return "markup";
  }

  function escapeHtml(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  // Search ----------------------------------------------------------------
  function bindSearch() {
    const input = document.getElementById("search");
    input.addEventListener("input", () => {
      const q = input.value.trim().toLowerCase();
      if (!q) {
        state.filtered = null;
      } else {
        state.filtered = Object.keys(state.index).filter(p => p.toLowerCase().includes(q)).sort();
      }
      renderTree();
    });
  }

  // Keyboard shortcuts ----------------------------------------------------
  function bindKeys() {
    document.addEventListener("keydown", (e) => {
      if (e.target.tagName === "INPUT") return;
      if (e.key === "/") {
        e.preventDefault();
        document.getElementById("search").focus();
      } else if (e.key === "g") {
        document.getElementById("viewer").scrollTo({ top: 0, behavior: "instant" });
      } else if (e.key === "j" || e.key === "k") {
        nextFile(e.key === "j" ? 1 : -1);
      }
    });
  }
  function nextFile(delta) {
    const paths = listPaths();
    const idx = paths.indexOf(state.active);
    const next = Math.max(0, Math.min(paths.length - 1, idx + delta));
    if (paths[next]) openFile(paths[next]);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", main);
  } else {
    main();
  }
})();
