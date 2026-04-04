#!/usr/bin/env python3
"""
Regression Guard — CI-compatible error rate comparison.

Compares the current week's error_rate with the previous week's error_rate.
Exits with code 1 if error_rate regressed by more than 5 percentage points.

Usage:
    python scripts/check_regression.py                  # Auto-locate JSONL
    python scripts/check_regression.py <log_file>       # Explicit log file
    python scripts/check_regression.py <log_file> --threshold 0.10  # 10% threshold

CI integration:
    python scripts/check_regression.py && echo "OK"
"""

import json
import sys
import argparse
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Optional


# ---------------------------------------------------------------------------
# Minimal helpers (self-contained — no import from weekly_report)
# ---------------------------------------------------------------------------

DIAGNOSIS_TOOLS = {"emit-ir", "emit-mir", "check", "explain"}
EXECUTION_TOOLS = {"test", "run", "build", "compile"}
DOC_TOOLS = {"docs/search", "docs/get", "docs/list", "docs/resolve"}
MAINTENANCE_TOOLS = {"format", "lint", "cache/invalidate"}
PROJECT_TOOLS = {
    "project/build", "project/coverage", "project/structure",
    "project/affected-tests", "project/artifacts", "project/slow-tests",
}


def find_log_file() -> Optional[Path]:
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
                events.append(json.loads(line))
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


def week_start(offset_weeks: int = 0) -> datetime:
    """Return the Monday 00:00 UTC of (current - offset_weeks) week."""
    now = datetime.now(tz=timezone.utc)
    monday = now - timedelta(
        days=now.weekday(),
        hours=now.hour,
        minutes=now.minute,
        seconds=now.second,
        microseconds=now.microsecond,
    )
    return monday - timedelta(weeks=offset_weeks)


def filter_window(events: list[dict], start: datetime, end: datetime) -> list[dict]:
    result = []
    for ev in events:
        ts = parse_ts(ev.get("ts", ""))
        if ts and start <= ts < end:
            result.append(ev)
    return result


def compute_error_rate(events: list[dict]) -> tuple[float, int]:
    """Returns (error_rate, total_calls) for all tool_call events."""
    calls = [e for e in events if e.get("event") == "tool_call"]
    if not calls:
        return 0.0, 0
    errors = sum(1 for c in calls if c.get("is_error", False))
    return errors / len(calls), len(calls)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="CI regression guard: fail if error_rate regressed >5% vs previous week"
    )
    parser.add_argument(
        "log_file", nargs="?",
        help="Path to mcp-call-log.jsonl (auto-detected if omitted)"
    )
    parser.add_argument(
        "--threshold", type=float, default=0.05, metavar="FRACTION",
        help="Regression threshold as fraction of total calls (default: 0.05 = 5%%)"
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Show detailed per-session breakdown"
    )
    args = parser.parse_args()

    # Locate log
    if args.log_file:
        log_path = Path(args.log_file)
        if not log_path.exists():
            print(f"ERROR: Log file not found: {log_path}")
            sys.exit(1)
    else:
        log_path = find_log_file()
        if log_path is None:
            print(
                "ERROR: mcp-call-log.jsonl not found. "
                "Set MCP_CALL_LOG env var or pass path as argument."
            )
            sys.exit(1)

    all_events = parse_log(log_path)

    # Define windows
    this_start = week_start(0)
    this_end = this_start + timedelta(days=7)
    prev_start = week_start(1)
    prev_end = prev_start + timedelta(days=7)

    this_events = filter_window(all_events, this_start, this_end)
    prev_events = filter_window(all_events, prev_start, prev_end)

    current_rate, current_calls = compute_error_rate(this_events)
    prev_rate, prev_calls = compute_error_rate(prev_events)

    this_label = this_start.strftime("%Y-W%W")
    prev_label = prev_start.strftime("%Y-W%W")

    print(f"Regression check: error_rate")
    print(f"  Previous week ({prev_label}): {prev_rate:.1%}  ({prev_calls} calls)")
    print(f"  Current  week ({this_label}): {current_rate:.1%}  ({current_calls} calls)")

    if prev_calls == 0:
        print("  NOTE: No data for previous week — cannot compare. Passing.")
        sys.exit(0)

    if current_calls == 0:
        print("  NOTE: No data for current week — cannot compare. Passing.")
        sys.exit(0)

    regression = current_rate - prev_rate
    print(f"  Delta: {regression:+.1%}  (threshold: +{args.threshold:.1%})")

    if regression > args.threshold:
        print(
            f"REGRESSION DETECTED: error_rate increased by {regression:.1%} "
            f"(>{args.threshold:.1%} threshold)"
        )
        sys.exit(1)
    else:
        print("OK: No regression detected.")
        sys.exit(0)


if __name__ == "__main__":
    main()
