#!/usr/bin/env python3
"""
Weekly Digest Report + Anomaly Detection for LLM-IR-Debugging Research

Reads mcp-call-log.jsonl, computes the past 7 days of metrics, detects
anomalies vs the rolling baseline, and saves a markdown report.

Usage:
    python scripts/weekly_report.py                    # Auto-locate JSONL, current week
    python scripts/weekly_report.py <log_file>         # Explicit log file
    python scripts/weekly_report.py <log_file> --week 2026-14   # Specific ISO year-week
    python scripts/weekly_report.py <log_file> --stdout         # Print instead of save

Outputs:
    docs/papers/llm-ir-debugging/reports/week-YYYY-WW.md
"""

import json
import math
import sys
import argparse
import statistics
from collections import Counter, defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Optional


# ---------------------------------------------------------------------------
# Tool classification (mirrors analyze_logs.py)
# ---------------------------------------------------------------------------

DIAGNOSIS_TOOLS = {"emit-ir", "emit-mir", "check", "explain"}
EXECUTION_TOOLS = {"test", "run", "build", "compile"}
DOC_TOOLS = {"docs/search", "docs/get", "docs/list", "docs/resolve"}
MAINTENANCE_TOOLS = {"format", "lint", "cache/invalidate"}
PROJECT_TOOLS = {
    "project/build", "project/coverage", "project/structure",
    "project/affected-tests", "project/artifacts", "project/slow-tests",
}


def classify_tool(tool: str) -> str:
    if tool in DIAGNOSIS_TOOLS:
        return "diagnosis"
    if tool in EXECUTION_TOOLS:
        return "execution"
    if tool in DOC_TOOLS:
        return "documentation"
    if tool in MAINTENANCE_TOOLS:
        return "maintenance"
    if tool in PROJECT_TOOLS:
        return "project"
    return "other"


# ---------------------------------------------------------------------------
# JSONL helpers
# ---------------------------------------------------------------------------

def find_log_file() -> Optional[Path]:
    """Search for mcp-call-log.jsonl in CWD, parent dirs, or env var."""
    import os
    env_path = os.environ.get("MCP_CALL_LOG")
    if env_path:
        p = Path(env_path)
        if p.exists():
            return p

    candidate = Path("mcp-call-log.jsonl")
    if candidate.exists():
        return candidate

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


def parse_log(log_path: Path) -> list[dict]:
    events: list[dict] = []
    with open(log_path, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                events.append(event)
            except json.JSONDecodeError as e:
                print(f"Warning: Line {line_num}: {e}", file=sys.stderr)
    return events


def parse_ts(ts: str) -> Optional[datetime]:
    if not ts:
        return None
    try:
        dt = datetime.fromisoformat(ts)
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# Weekly window helpers
# ---------------------------------------------------------------------------

def week_window(year_week: Optional[str]) -> tuple[datetime, datetime]:
    """Return (start, end) for the given ISO year-week (e.g. '2026-14')."""
    if year_week is None:
        now = datetime.now(tz=timezone.utc)
        # ISO week: Monday = day 1
        start = now - timedelta(days=now.weekday(), hours=now.hour,
                                 minutes=now.minute, seconds=now.second,
                                 microseconds=now.microsecond)
    else:
        parts = year_week.split("-")
        if len(parts) != 2:
            raise ValueError(f"Invalid week format '{year_week}', expected YYYY-WW")
        year, week = int(parts[0]), int(parts[1])
        # ISO week 1 day 1 of given year
        jan4 = datetime(year, 1, 4, tzinfo=timezone.utc)
        start_of_week1 = jan4 - timedelta(days=jan4.weekday())
        start = start_of_week1 + timedelta(weeks=week - 1)

    end = start + timedelta(days=7)
    return start, end


def filter_events_in_window(events: list[dict], start: datetime, end: datetime) -> list[dict]:
    result = []
    for ev in events:
        ts = parse_ts(ev.get("ts", ""))
        if ts and start <= ts < end:
            result.append(ev)
    return result


def group_by_session(events: list[dict]) -> dict[str, list[dict]]:
    sessions: dict[str, list[dict]] = defaultdict(list)
    for ev in events:
        sid = ev.get("session", "unknown")
        sessions[sid].append(ev)
    return dict(sessions)


def group_by_day(events: list[dict]) -> dict[str, list[dict]]:
    """Group tool_call events by date string YYYY-MM-DD."""
    by_day: dict[str, list[dict]] = defaultdict(list)
    for ev in events:
        if ev.get("event") != "tool_call":
            continue
        ts = parse_ts(ev.get("ts", ""))
        if ts:
            day = ts.strftime("%Y-%m-%d")
            by_day[day].append(ev)
    return dict(by_day)


# ---------------------------------------------------------------------------
# Per-session metrics
# ---------------------------------------------------------------------------

def session_metrics(events: list[dict]) -> dict:
    calls = [e for e in events if e.get("event") == "tool_call"]
    if not calls:
        return {"total_calls": 0, "error_rate": 0.0, "check_adoption": 0.0,
                "debug_layers_adoption": 0.0, "diagnosis_ratio": 0.0}

    n = len(calls)
    errors = sum(1 for c in calls if c.get("is_error", False))
    check_calls = sum(1 for c in calls if c.get("tool") == "check")
    test_calls = sum(1 for c in calls if c.get("tool") == "test")

    # debug_layers: test call with params.debug_layers == True
    dl_calls = sum(
        1 for c in calls
        if c.get("tool") == "test" and c.get("params", {}).get("debug_layers")
    )

    diagnosis = sum(1 for c in calls if classify_tool(c.get("tool", "")) == "diagnosis")

    # check adoption: check / (check + test) — ratio of proactive type-checking
    check_plus_test = check_calls + test_calls
    check_adoption = check_calls / check_plus_test if check_plus_test > 0 else 0.0

    # debug_layers adoption: dl_calls / test_calls
    dl_adoption = dl_calls / test_calls if test_calls > 0 else 0.0

    return {
        "total_calls": n,
        "error_count": errors,
        "error_rate": errors / n,
        "check_calls": check_calls,
        "test_calls": test_calls,
        "dl_calls": dl_calls,
        "check_adoption": check_adoption,
        "debug_layers_adoption": dl_adoption,
        "diagnosis_ratio": diagnosis / n,
    }


# ---------------------------------------------------------------------------
# Weekly aggregate
# ---------------------------------------------------------------------------

def compute_weekly_stats(session_list: list[dict]) -> dict:
    """Aggregate session metrics into weekly summary."""
    if not session_list:
        return {
            "session_count": 0,
            "total_calls": 0,
            "error_rate": 0.0,
            "check_adoption": 0.0,
            "debug_layers_adoption": 0.0,
            "diagnosis_ratio": 0.0,
            "top_tools": [],
        }

    total_calls = sum(s["total_calls"] for s in session_list)
    non_empty = [s for s in session_list if s["total_calls"] > 0]

    def mean_metric(key: str) -> float:
        vals = [s[key] for s in non_empty if key in s]
        return sum(vals) / len(vals) if vals else 0.0

    return {
        "session_count": len(session_list),
        "total_calls": total_calls,
        "error_rate": mean_metric("error_rate"),
        "check_adoption": mean_metric("check_adoption"),
        "debug_layers_adoption": mean_metric("debug_layers_adoption"),
        "diagnosis_ratio": mean_metric("diagnosis_ratio"),
    }


def top_tools(events: list[dict], n: int = 10) -> list[tuple[str, int, float]]:
    """Return top n tools by usage count."""
    calls = [e for e in events if e.get("event") == "tool_call"]
    total = len(calls)
    counter: Counter = Counter(c.get("tool", "unknown") for c in calls)
    result = []
    for tool, count in counter.most_common(n):
        pct = count / total * 100 if total > 0 else 0.0
        result.append((tool, count, pct))
    return result


def top_test_targets(events: list[dict], n: int = 10) -> list[tuple[str, int]]:
    """Return top n test suite/path targets."""
    counter: Counter = Counter()
    for e in events:
        if e.get("event") != "tool_call" or e.get("tool") != "test":
            continue
        params = e.get("params", {})
        target = params.get("suite") or params.get("path") or params.get("filter") or "(default)"
        counter[str(target)] += 1
    return counter.most_common(n)


# ---------------------------------------------------------------------------
# Rolling baseline & anomaly detection (7.2)
# ---------------------------------------------------------------------------

def compute_rolling_stats(
    all_events: list[dict],
    end: datetime,
    window_days: int = 7,
    history_weeks: int = 8,
) -> dict[str, dict]:
    """
    Compute rolling mean and std for key metrics over the past `history_weeks`
    weeks ending at `end`. Returns dict keyed by metric name with
    {mean, std, weeks} for anomaly detection.
    """
    metric_series: dict[str, list[float]] = defaultdict(list)

    for week_offset in range(1, history_weeks + 1):
        w_end = end - timedelta(weeks=week_offset - 1)
        w_start = w_end - timedelta(days=window_days)
        week_events = filter_events_in_window(all_events, w_start, w_end)
        sessions = group_by_session(week_events)
        session_stats = [session_metrics(v) for v in sessions.values()]
        weekly = compute_weekly_stats(session_stats)

        if weekly["session_count"] > 0:
            metric_series["error_rate"].append(weekly["error_rate"])
            metric_series["check_adoption"].append(weekly["check_adoption"])
            metric_series["debug_layers_adoption"].append(weekly["debug_layers_adoption"])
            metric_series["diagnosis_ratio"].append(weekly["diagnosis_ratio"])

    result = {}
    for metric, values in metric_series.items():
        if len(values) >= 2:
            m = statistics.mean(values)
            s = statistics.stdev(values)
        elif len(values) == 1:
            m = values[0]
            s = 0.0
        else:
            m, s = 0.0, 0.0
        result[metric] = {"mean": m, "std": s, "n_weeks": len(values), "values": values}

    return result


def detect_anomalies(
    current: dict,
    rolling: dict[str, dict],
    sigma_threshold: float = 2.0,
    adoption_drop_threshold: float = 0.30,
) -> list[dict]:
    """
    Detect anomalies in current week vs rolling baseline.
    Returns list of anomaly dicts with {metric, current, mean, std, kind, message}.
    """
    anomalies = []

    for metric, stats in rolling.items():
        cur_val = current.get(metric)
        if cur_val is None:
            continue
        mean = stats["mean"]
        std = stats["std"]

        # High anomaly: current > mean + sigma_threshold * std
        if std > 0 and cur_val > mean + sigma_threshold * std:
            anomalies.append({
                "metric": metric,
                "current": cur_val,
                "mean": mean,
                "std": std,
                "kind": "spike",
                "message": (
                    f"{metric} spiked to {cur_val:.3f} "
                    f"(mean={mean:.3f}, +{(cur_val - mean) / std:.1f}σ)"
                ),
            })

        # Low anomaly: adoption metrics dropped >30% from mean
        if metric in ("check_adoption", "debug_layers_adoption", "diagnosis_ratio"):
            if mean > 0 and cur_val < mean * (1 - adoption_drop_threshold):
                drop_pct = (mean - cur_val) / mean * 100
                anomalies.append({
                    "metric": metric,
                    "current": cur_val,
                    "mean": mean,
                    "std": std,
                    "kind": "drop",
                    "message": (
                        f"{metric} dropped to {cur_val:.3f} "
                        f"(mean={mean:.3f}, -{drop_pct:.0f}%)"
                    ),
                })

    return anomalies


# ---------------------------------------------------------------------------
# Markdown report generation
# ---------------------------------------------------------------------------

def generate_report(
    week_label: str,
    start: datetime,
    end: datetime,
    week_events: list[dict],
    all_events: list[dict],
    rolling: dict[str, dict],
    anomalies: list[dict],
) -> str:
    sessions = group_by_session(week_events)
    session_stats = [session_metrics(v) for v in sessions.values()]
    weekly = compute_weekly_stats(session_stats)
    tools = top_tools(week_events)
    targets = top_test_targets(week_events)

    lines = []

    # Header
    lines.append(f"# Weekly MCP Research Digest — {week_label}")
    lines.append(f"")
    lines.append(f"**Period**: {start.strftime('%Y-%m-%d')} – {end.strftime('%Y-%m-%d')}  ")
    lines.append(f"**Generated**: {datetime.now(tz=timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}  ")
    lines.append(f"")

    # Summary table
    lines.append("## Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("|--------|-------|")
    lines.append(f"| Sessions | {weekly['session_count']} |")
    lines.append(f"| Total calls | {weekly['total_calls']} |")
    lines.append(f"| Error rate | {weekly['error_rate']:.1%} |")
    lines.append(f"| Check adoption | {weekly['check_adoption']:.1%} |")
    lines.append(f"| Debug-layers adoption | {weekly['debug_layers_adoption']:.1%} |")
    lines.append(f"| Diagnosis ratio | {weekly['diagnosis_ratio']:.1%} |")
    lines.append("")

    # Anomaly alerts
    if anomalies:
        lines.append("## Anomaly Alerts")
        lines.append("")
        for a in anomalies:
            icon = "🔺" if a["kind"] == "spike" else "🔻"
            lines.append(f"- {icon} **{a['kind'].upper()}**: {a['message']}")
        lines.append("")
    else:
        lines.append("## Anomaly Alerts")
        lines.append("")
        lines.append("_No anomalies detected this week._")
        lines.append("")

    # Tool distribution
    lines.append("## Tool Distribution")
    lines.append("")
    if tools:
        lines.append("| Tool | Calls | % |")
        lines.append("|------|-------|---|")
        for tool, count, pct in tools:
            lines.append(f"| `{tool}` | {count} | {pct:.1f}% |")
    else:
        lines.append("_No tool calls recorded._")
    lines.append("")

    # Top test targets
    lines.append("## Top Test Targets")
    lines.append("")
    if targets:
        lines.append("| Target | Calls |")
        lines.append("|--------|-------|")
        for target, count in targets:
            lines.append(f"| `{target}` | {count} |")
    else:
        lines.append("_No test calls recorded._")
    lines.append("")

    # Rolling baseline
    lines.append("## Rolling Baseline (past 8 weeks)")
    lines.append("")
    if rolling:
        lines.append("| Metric | Rolling Mean | Rolling Std | Weeks |")
        lines.append("|--------|-------------|-------------|-------|")
        for metric, stats in rolling.items():
            lines.append(
                f"| {metric} | {stats['mean']:.3f} | {stats['std']:.3f} | {stats['n_weeks']} |"
            )
    else:
        lines.append("_Insufficient history for rolling baseline._")
    lines.append("")

    # Per-session breakdown
    lines.append("## Session Breakdown")
    lines.append("")
    if sessions:
        lines.append("| Session | Calls | Error Rate | Check Adopt | DL Adopt |")
        lines.append("|---------|-------|-----------|-------------|---------|")
        for sid, evs in sorted(sessions.items()):
            m = session_metrics(evs)
            if m["total_calls"] == 0:
                continue
            lines.append(
                f"| `{sid[:16]}…` | {m['total_calls']} "
                f"| {m['error_rate']:.1%} "
                f"| {m['check_adoption']:.1%} "
                f"| {m['debug_layers_adoption']:.1%} |"
            )
    else:
        lines.append("_No sessions in this week._")
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate weekly MCP research digest with anomaly detection"
    )
    parser.add_argument(
        "log_file", nargs="?", help="Path to mcp-call-log.jsonl (auto-detected if omitted)"
    )
    parser.add_argument(
        "--week", metavar="YYYY-WW",
        help="Target ISO year-week (default: current week)"
    )
    parser.add_argument(
        "--stdout", action="store_true",
        help="Print report to stdout instead of saving to file"
    )
    args = parser.parse_args()

    # Locate log file
    if args.log_file:
        log_path = Path(args.log_file)
        if not log_path.exists():
            print(f"Error: Log file not found: {log_path}", file=sys.stderr)
            sys.exit(1)
    else:
        log_path = find_log_file()
        if log_path is None:
            print(
                "Error: mcp-call-log.jsonl not found. "
                "Set MCP_CALL_LOG env var or pass path as argument.",
                file=sys.stderr,
            )
            sys.exit(1)

    print(f"Reading: {log_path}", file=sys.stderr)
    all_events = parse_log(log_path)
    print(f"Parsed {len(all_events)} events", file=sys.stderr)

    # Determine week window
    start, end = week_window(args.week)
    iso_cal = start.isocalendar()
    week_label = f"{iso_cal[0]}-{iso_cal[1]:02d}"

    print(f"Week: {week_label} ({start.date()} – {end.date()})", file=sys.stderr)

    # Filter to this week
    week_events = filter_events_in_window(all_events, start, end)
    print(f"Events in window: {len(week_events)}", file=sys.stderr)

    # Rolling baseline (history BEFORE this week)
    rolling = compute_rolling_stats(all_events, start, window_days=7, history_weeks=8)

    # Current week metrics
    sessions = group_by_session(week_events)
    session_stats = [session_metrics(v) for v in sessions.values()]
    weekly = compute_weekly_stats(session_stats)

    # Anomaly detection
    anomalies = detect_anomalies(weekly, rolling)

    # Generate report
    report = generate_report(
        week_label, start, end, week_events, all_events, rolling, anomalies
    )

    if args.stdout:
        print(report)
        return

    # Determine output directory relative to the log file's project root
    # First try: locate reports/ next to scripts/
    script_dir = Path(__file__).parent
    reports_dir = script_dir.parent / "reports"
    reports_dir.mkdir(parents=True, exist_ok=True)

    out_path = reports_dir / f"week-{week_label}.md"
    out_path.write_text(report, encoding="utf-8")
    print(f"Report saved: {out_path}", file=sys.stderr)

    if anomalies:
        print(f"ANOMALIES DETECTED ({len(anomalies)}):", file=sys.stderr)
        for a in anomalies:
            print(f"  [{a['kind']}] {a['message']}", file=sys.stderr)


if __name__ == "__main__":
    main()
