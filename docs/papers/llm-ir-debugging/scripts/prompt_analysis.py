#!/usr/bin/env python3
"""Prompt engineering meta-analysis — measure CLAUDE.md rule effectiveness.

Usage:
    python scripts/prompt_analysis.py                   # Auto-locate JSONL, run all sections
    python scripts/prompt_analysis.py <log_file>        # Explicit log file
    python scripts/prompt_analysis.py <log_file> --section 8.1  # Single section
    python scripts/prompt_analysis.py <log_file> --save         # Also save to reports/prompt-analysis.md

Analyzes:
  8.1  Rule half-life: how long each rule maintains adoption before decay
  8.2  Wording effect: compare adoption across CLAUDE.md versions (by hash)
  8.3  Data citation effect: adoption before/after adding quantitative data to rules
  8.4  Position bias: compliance rates by rule position in CLAUDE.md
  8.5  Cross-model comparison: tool adoption by model (opus vs sonnet vs haiku)

Output: prints to stdout + optionally saves to reports/prompt-analysis.md
"""
import json, sys, os, math, argparse
from collections import defaultdict, Counter
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional


# ---------------------------------------------------------------------------
# CLAUDE.md rule registry: (rule_id, description, position_0based, metric)
# Position reflects order of appearance in CLAUDE.md.
# ---------------------------------------------------------------------------
TRACKED_RULES = [
    ("check-before-test",  "Use check before test",        0, "check_adoption"),
    ("debug-layers",       "Use debug_layers on failure",  1, "dl_adoption"),
    ("docs-tools",         "Use MCP docs tools",           2, "docs_ratio"),
    ("no-full-suite",      "Only run specific suites",     3, "targeted_ratio"),
    ("mcp-over-bash",      "Use MCP not Bash",             4, "mcp_ratio"),
]

# Date when "observed: 8.8%" data was added to check-before-test rule
DATA_CITATION_CUTOFF = "2026-03-26"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def find_log() -> Optional[Path]:
    env = os.environ.get("MCP_CALL_LOG") or os.environ.get("TML_MCP_LOG_DIR")
    if env:
        p = Path(env)
        if p.is_file():
            return p
        candidate = p / "mcp-call-log.jsonl"
        if candidate.exists():
            return candidate
    for p in [Path("mcp-call-log.jsonl"), Path("..") / "mcp-call-log.jsonl"]:
        if p.exists():
            return p
    current = Path.cwd()
    for _ in range(5):
        candidate = current / "mcp-call-log.jsonl"
        if candidate.exists():
            return candidate
        parent = current.parent
        if parent == current:
            break
        current = parent
    return None


def _parse_ts(ts) -> Optional[datetime]:
    """Parse ISO 8601 or epoch-ms timestamp."""
    if not ts:
        return None
    if isinstance(ts, (int, float)):
        try:
            return datetime.fromtimestamp(ts / 1000, tz=timezone.utc)
        except Exception:
            return None
    try:
        dt = datetime.fromisoformat(str(ts))
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt
    except ValueError:
        return None


def load_all(log_path: Path):
    """Returns (sessions_dict, calls_list). sessions keyed by session ID."""
    sessions: dict = {}
    calls: list = []
    with open(log_path, encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue
            if e.get("event") == "session_start":
                sessions[e.get("session")] = e
            elif e.get("event") == "tool_call":
                calls.append(e)
    return sessions, calls


def _mean(vals: list) -> float:
    return sum(vals) / len(vals) if vals else 0.0


def _day(ts_raw) -> Optional[str]:
    dt = _parse_ts(ts_raw)
    return dt.strftime("%Y-%m-%d") if dt else None


def _week(ts_raw) -> Optional[str]:
    dt = _parse_ts(ts_raw)
    return dt.strftime("%Y-W%W") if dt else None


# ---------------------------------------------------------------------------
# Per-session metric extraction
# ---------------------------------------------------------------------------

def _compute_session_metrics(session_calls: list) -> dict:
    """Compute adoption metrics for a list of tool_calls in one session."""
    n = len(session_calls)
    if n == 0:
        return {}

    errors = sum(1 for c in session_calls if c.get("is_error", False) or c.get("error_type", "none") != "none")
    check_calls = sum(1 for c in session_calls if c.get("tool") == "check")
    test_calls  = sum(1 for c in session_calls if c.get("tool") == "test")
    doc_calls   = sum(1 for c in session_calls if (c.get("tool") or "").startswith("docs/"))

    # debug_layers: params field OR top-level field (different schema versions)
    dl_calls = sum(
        1 for c in session_calls
        if c.get("tool") == "test" and (
            c.get("debug_layers") is True
            or c.get("debug_layers") == "true"
            or (isinstance(c.get("params"), dict) and c["params"].get("debug_layers"))
        )
    )

    # targeted test: suite= or path= provided
    targeted = sum(
        1 for c in session_calls
        if c.get("tool") == "test" and (
            (isinstance(c.get("params"), dict) and (c["params"].get("suite") or c["params"].get("path")))
        )
    )

    check_plus_test = check_calls + test_calls
    return {
        "total_calls":     n,
        "error_rate":      errors / n,
        "check_adoption":  check_calls / check_plus_test if check_plus_test > 0 else 0.0,
        "dl_adoption":     dl_calls / test_calls if test_calls > 0 else 0.0,
        "docs_ratio":      doc_calls / n,
        "targeted_ratio":  targeted / test_calls if test_calls > 0 else 0.0,
        "mcp_ratio":       1.0,  # All calls in the MCP log are MCP calls by definition
    }


def build_session_records(sessions: dict, calls: list) -> list:
    """Returns list of session dicts with metrics + metadata."""
    by_session: dict = defaultdict(list)
    for c in calls:
        by_session[c.get("session", "unknown")].append(c)

    records = []
    for sid, session_calls in by_session.items():
        meta = sessions.get(sid, {})
        first_ts = _parse_ts(session_calls[0].get("ts")) if session_calls else None

        metrics = _compute_session_metrics(session_calls)

        records.append({
            "session_id":    sid,
            "start_ts":      first_ts,
            "date":          first_ts.strftime("%Y-%m-%d") if first_ts else "unknown",
            "week":          first_ts.strftime("%Y-W%W") if first_ts else "unknown",
            "model":         meta.get("model", "unknown"),
            "condition":     meta.get("condition", "unknown"),
            "claude_md_hash": meta.get("claude_md_hash"),
            "metrics":       metrics,
        })

    records.sort(key=lambda r: r["start_ts"] or datetime.min.replace(tzinfo=timezone.utc))
    return records


# ---------------------------------------------------------------------------
# 8.1 Rule half-life
# ---------------------------------------------------------------------------

def rule_half_life(sessions: dict, calls: list, records: list, output: list):
    output.append("\n## 8.1 Rule Half-Life Analysis")
    output.append("\nHow long does each rule maintain adoption before decaying to 50% of peak?\n")

    # Also show fine-grained day-level data for check-before-test (primary metric)
    by_day: dict = defaultdict(lambda: {"test": 0, "check": 0})
    session_first: dict = {}
    for c in calls:
        sid = c.get("session", "")
        ts = _parse_ts(c.get("ts"))
        if ts and sid not in session_first:
            session_first[sid] = ts
        day = _day(c.get("ts"))
        if not day:
            continue
        if c.get("tool") == "test":
            by_day[day]["test"] += 1
            if c.get("preceded_by") == "check" or (isinstance(c.get("params"), dict) and c["params"].get("preceded_by") == "check"):
                by_day[day]["check"] += 1

    output.append(f"### Check-Before-Test Daily Adoption\n")
    output.append(f"{'Day':<12} {'Tests':>6} {'Check->Test':>12} {'Adoption':>9}")
    output.append("-" * 42)
    peak = 0.0
    half_life_day: Optional[str] = None
    peak_day: Optional[str] = None
    for day in sorted(by_day):
        d = by_day[day]
        rate = d["check"] / d["test"] * 100 if d["test"] else 0.0
        if rate > peak:
            peak = rate
            peak_day = day
        if peak > 0 and rate < peak * 0.5 and half_life_day is None and day != peak_day:
            half_life_day = day
        output.append(f"{day:<12} {d['test']:>6} {d['check']:>12} {rate:>8.1f}%")

    if half_life_day:
        output.append(f"\n**Half-life reached**: {half_life_day} (peak={peak:.1f}% on {peak_day})")
    elif peak > 0:
        output.append(f"\n**No half-life decay detected** (peak={peak:.1f}% on {peak_day}, still above 50%)")
    else:
        output.append("\n_No check-before-test data found._")

    # Weekly series per rule
    output.append("\n### Weekly Adoption by Rule\n")
    week_map: dict = defaultdict(lambda: defaultdict(list))
    for r in records:
        w = r["week"]
        m = r["metrics"]
        for _, _, _, metric_key in [(x[0], x[1], x[2], x[3]) for x in TRACKED_RULES]:
            if metric_key in m:
                week_map[metric_key][w].append(m[metric_key])

    header = f"{'Week':<12}"
    for rule_id, desc, _, metric_key in TRACKED_RULES:
        header += f" {desc[:12]:>12}"
    output.append(header)
    output.append("-" * (12 + 13 * len(TRACKED_RULES)))
    all_weeks = sorted(set(w for wm in week_map.values() for w in wm.keys()))
    for w in all_weeks:
        row = f"{w:<12}"
        for _, _, _, metric_key in TRACKED_RULES:
            vals = week_map[metric_key].get(w, [])
            val = _mean(vals) * 100 if vals else 0.0
            row += f" {val:>11.1f}%"
        output.append(row)


# ---------------------------------------------------------------------------
# 8.2 Wording effect
# ---------------------------------------------------------------------------

def wording_effect(sessions: dict, calls: list, output: list):
    output.append("\n## 8.2 Wording Effect (by CLAUDE.md Hash)\n")
    output.append("Each hash represents a distinct version of the CLAUDE.md prompt rules.\n")

    by_hash: dict = defaultdict(lambda: {"calls": 0, "check_before_test": 0, "tests": 0,
                                          "debug_layers": 0, "sessions": 0})
    session_hash = {sid: s.get("claude_md_hash", "unknown") for sid, s in sessions.items()}

    for c in calls:
        h = session_hash.get(c.get("session", ""), "unknown")
        by_hash[h]["calls"] += 1
        if c.get("tool") == "test":
            by_hash[h]["tests"] += 1
            if c.get("preceded_by") == "check":
                by_hash[h]["check_before_test"] += 1
            if (c.get("debug_layers") is True or c.get("debug_layers") == "true"
                    or (isinstance(c.get("params"), dict) and c["params"].get("debug_layers"))):
                by_hash[h]["debug_layers"] += 1

    for sid, s in sessions.items():
        h = s.get("claude_md_hash", "unknown")
        by_hash[h]["sessions"] += 1

    output.append(f"{'Hash':<12} {'Sessions':>9} {'Calls':>6} {'Check%':>7} {'DL%':>7}")
    output.append("-" * 46)
    for h in sorted(by_hash, key=lambda x: -by_hash[x]["calls"]):
        d = by_hash[h]
        check_rate = d["check_before_test"] / d["tests"] * 100 if d["tests"] else 0
        dl_rate = d["debug_layers"] / d["tests"] * 100 if d["tests"] else 0
        output.append(f"{(h or 'None')[:12]:<12} {d['sessions']:>9} {d['calls']:>6} {check_rate:>6.1f}% {dl_rate:>6.1f}%")


# ---------------------------------------------------------------------------
# 8.3 Data citation effect
# ---------------------------------------------------------------------------

def data_citation_effect(records: list, output: list):
    output.append("\n## 8.3 Data Citation Effect\n")
    output.append(
        f"Compares adoption metrics **before** vs **after** `{DATA_CITATION_CUTOFF}` "
        f"(when 'observed: 8.8%' data was added to the check-before-test rule in CLAUDE.md).\n"
    )

    before = [r for r in records if r["date"] < DATA_CITATION_CUTOFF and r["date"] != "unknown"]
    after  = [r for r in records if r["date"] >= DATA_CITATION_CUTOFF and r["date"] != "unknown"]

    def stats(recs: list, key: str) -> tuple:
        vals = [r["metrics"].get(key, 0.0) for r in recs if r["metrics"]]
        if not vals:
            return None, None, len(recs)
        m = _mean(vals)
        std = math.sqrt(sum((x - m) ** 2 for x in vals) / max(len(vals) - 1, 1))
        return m, std, len(vals)

    metrics_to_compare = [
        ("check_adoption", "Check Adoption"),
        ("dl_adoption",    "Debug-Layers Adoption"),
        ("docs_ratio",     "Docs Tool Ratio"),
    ]

    output.append(
        f"{'Metric':<25} {'Before (n=' + str(len(before)) + ')':>20}  "
        f"{'After (n=' + str(len(after)) + ')':>20}  {'Delta':>8}"
    )
    output.append("-" * 78)
    for key, label in metrics_to_compare:
        bm, bs, bn = stats(before, key)
        am, as_, an = stats(after, key)
        bstr = f"{bm*100:.1f}% ±{bs*100:.1f}%" if bm is not None else "N/A"
        astr = f"{am*100:.1f}% ±{as_*100:.1f}%" if am is not None else "N/A"
        if bm is not None and am is not None:
            delta_str = f"{(am - bm)*100:+.1f}%"
        else:
            delta_str = "N/A"
        output.append(f"{label:<25} {bstr:>20}  {astr:>20}  {delta_str:>8}")


# ---------------------------------------------------------------------------
# 8.4 Position bias
# ---------------------------------------------------------------------------

def position_bias(records: list, output: list):
    output.append("\n## 8.4 Position Bias\n")
    output.append(
        "Rules earlier in CLAUDE.md may have higher compliance due to LLM primacy effects.\n"
    )

    output.append(f"{'Pos':>3} {'Rule':<25} {'Metric':<20} {'Mean Adopt':>11} {'Std':>7} {'N':>5}")
    output.append("-" * 74)

    position_vals = []
    adoption_vals = []

    for rule_id, desc, pos, metric_key in TRACKED_RULES:
        vals = [r["metrics"].get(metric_key, 0.0) for r in records if r["metrics"]]
        n = len(vals)
        m = _mean(vals)
        std = math.sqrt(sum((x - m) ** 2 for x in vals) / max(n - 1, 1)) if n > 1 else 0.0
        output.append(f"{pos:>3} {desc:<25} {metric_key:<20} {m*100:>10.1f}% {std*100:>6.1f}% {n:>5}")
        position_vals.append(float(pos))
        adoption_vals.append(m)

    # Pearson correlation
    if len(position_vals) >= 3:
        n = len(position_vals)
        mp = _mean(position_vals)
        ma = _mean(adoption_vals)
        cov = sum((position_vals[i] - mp) * (adoption_vals[i] - ma) for i in range(n)) / n
        vp = sum((p - mp) ** 2 for p in position_vals) / n
        va = sum((a - ma) ** 2 for a in adoption_vals) / n
        if vp > 0 and va > 0:
            corr = cov / math.sqrt(vp * va)
            interp = "negative (earlier = higher)" if corr < -0.3 else "positive (later = higher)" if corr > 0.3 else "no clear"
            output.append(f"\nPearson r (position vs adoption): {corr:.3f}  [{interp} position bias]")


# ---------------------------------------------------------------------------
# 8.5 Cross-model comparison
# ---------------------------------------------------------------------------

def cross_model(sessions: dict, calls: list, output: list):
    output.append("\n## 8.5 Cross-Model Comparison\n")

    session_model = {sid: s.get("model", "unknown") for sid, s in sessions.items()}

    by_model: dict = defaultdict(lambda: {
        "calls": 0, "tools": Counter(), "errors": 0, "is_error": 0,
        "tests": 0, "check_before_test": 0, "docs": 0, "dl": 0,
    })
    for c in calls:
        model = session_model.get(c.get("session", ""), "unknown")
        by_model[model]["calls"] += 1
        by_model[model]["tools"][c.get("tool", "?")] += 1
        if c.get("error_type", "none") != "none" or c.get("is_error"):
            by_model[model]["is_error"] += 1
        tool = c.get("tool", "")
        if tool == "test":
            by_model[model]["tests"] += 1
            if c.get("preceded_by") == "check":
                by_model[model]["check_before_test"] += 1
            if (c.get("debug_layers") is True or c.get("debug_layers") == "true"
                    or (isinstance(c.get("params"), dict) and c["params"].get("debug_layers"))):
                by_model[model]["dl"] += 1
        if tool.startswith("docs/"):
            by_model[model]["docs"] += 1

    output.append(f"{'Model':<15} {'Calls':>6} {'Error%':>7} {'Check%':>7} {'DL%':>5} {'Docs%':>6} {'Top Tool':<20}")
    output.append("-" * 70)
    for model in sorted(by_model, key=lambda m: -by_model[m]["calls"]):
        d = by_model[model]
        err_rate   = d["is_error"] / d["calls"] * 100 if d["calls"] else 0
        check_rate = d["check_before_test"] / d["tests"] * 100 if d["tests"] else 0
        dl_rate    = d["dl"] / d["tests"] * 100 if d["tests"] else 0
        docs_rate  = d["docs"] / d["calls"] * 100 if d["calls"] else 0
        top_tool   = d["tools"].most_common(1)[0][0] if d["tools"] else "-"
        output.append(
            f"{model[:15]:<15} {d['calls']:>6} {err_rate:>6.1f}% {check_rate:>6.1f}% "
            f"{dl_rate:>4.1f}% {docs_rate:>5.1f}% {top_tool:<20}"
        )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Prompt engineering meta-analysis")
    parser.add_argument("log_file", nargs="?", help="Path to mcp-call-log.jsonl")
    parser.add_argument("--section", metavar="N.N", help="Run only section 8.1–8.5")
    parser.add_argument("--save", action="store_true", help="Save to reports/prompt-analysis.md")
    args = parser.parse_args()

    log = Path(args.log_file) if args.log_file else find_log()
    if not log or not log.exists():
        print("ERROR: mcp-call-log.jsonl not found. Pass path as argument or set MCP_CALL_LOG.")
        sys.exit(1)

    sessions, calls = load_all(log)
    print(f"Loaded {len(calls)} calls from {len(sessions)} sessions", file=sys.stderr)

    records = build_session_records(sessions, calls)

    def should(s: str) -> bool:
        return args.section is None or args.section.startswith(s)

    output: list = [
        "# Prompt Engineering Meta-Analysis",
        f"",
        f"_Generated: {datetime.now(tz=timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}_",
        f"_Source: {log}_",
        f"_Sessions: {len(sessions)}, Calls: {len(calls)}_",
    ]

    if should("8.1"):
        rule_half_life(sessions, calls, records, output)
    if should("8.2"):
        wording_effect(sessions, calls, output)
    if should("8.3"):
        data_citation_effect(records, output)
    if should("8.4"):
        position_bias(records, output)
    if should("8.5"):
        cross_model(sessions, calls, output)

    report = "\n".join(output)
    print(report)

    if args.save:
        script_dir = Path(__file__).parent
        reports_dir = script_dir.parent / "reports"
        reports_dir.mkdir(parents=True, exist_ok=True)
        out_path = reports_dir / "prompt-analysis.md"
        out_path.write_text(report, encoding="utf-8")
        print(f"\nSaved: {out_path}", file=sys.stderr)
