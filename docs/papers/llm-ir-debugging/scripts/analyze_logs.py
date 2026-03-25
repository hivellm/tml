#!/usr/bin/env python3
"""
MCP Call Log Analyzer for LLM-IR-Debugging Research

Parses NDJSON call logs from the TML MCP server, segments into debugging
episodes, computes metrics, and outputs analysis data for the paper.

Usage:
    python analyze_logs.py <log_file>                        # Summary stats
    python analyze_logs.py <log_file> --sessions             # List all sessions
    python analyze_logs.py <log_file> --session <id>         # Detail for one session
    python analyze_logs.py <log_file> --metrics              # Compute research metrics
    python analyze_logs.py <log_file> --export <out.json>    # Export structured data
    python analyze_logs.py <log_file> --transitions          # Tool transition matrix
    python analyze_logs.py <log_file> --label <labels.json>  # Interactive session labeling
    python analyze_logs.py <log_file> --compare <labels.json># Compare conditions A vs B
    python analyze_logs.py <log_file> --stats <labels.json>  # Statistical tests (t-test, Cohen's d)
"""

import json
import math
import sys
import argparse
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path
from typing import Any


# =============================================================================
# Tool Classification (matches tool-taxonomy.md)
# =============================================================================

DIAGNOSIS_TOOLS = {"emit-ir", "emit-mir", "check", "explain"}
NAVIGATION_TOOLS = {"Read", "Grep", "Glob"}  # These are Claude tools, not MCP
EXECUTION_TOOLS = {"test", "run", "build", "compile"}
DOC_TOOLS = {"docs/search", "docs/get", "docs/list", "docs/resolve"}
MAINTENANCE_TOOLS = {"format", "lint", "cache/invalidate"}
PROJECT_TOOLS = {
    "project/build", "project/coverage", "project/structure",
    "project/affected-tests", "project/artifacts", "project/slow-tests",
}


def classify_tool(tool_name: str) -> str:
    """Classify a tool call into a category."""
    if tool_name in DIAGNOSIS_TOOLS:
        return "diagnosis"
    if tool_name in EXECUTION_TOOLS:
        return "execution"
    if tool_name in DOC_TOOLS:
        return "documentation"
    if tool_name in MAINTENANCE_TOOLS:
        return "maintenance"
    if tool_name in PROJECT_TOOLS:
        return "project"
    return "other"


# =============================================================================
# Log Parsing
# =============================================================================

def parse_log(log_path: str) -> list[dict]:
    """Parse NDJSON log file into a list of events."""
    events = []
    with open(log_path, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                event["_line"] = line_num
                events.append(event)
            except json.JSONDecodeError as e:
                print(f"Warning: Line {line_num}: {e}", file=sys.stderr)
    return events


def group_by_session(events: list[dict]) -> dict[str, list[dict]]:
    """Group events by session ID."""
    sessions = defaultdict(list)
    for event in events:
        sid = event.get("session", "unknown")
        sessions[sid].append(event)
    return dict(sessions)


# =============================================================================
# Metrics Computation
# =============================================================================

def compute_session_metrics(calls: list[dict]) -> dict:
    """Compute metrics for a single session's tool calls."""
    tool_calls = [e for e in calls if e.get("event") == "tool_call"]

    if not tool_calls:
        return {"total_calls": 0}

    # Category counts
    categories = Counter()
    tools_used = Counter()
    for call in tool_calls:
        tool = call.get("tool", "unknown")
        tools_used[tool] += 1
        categories[classify_tool(tool)] += 1

    # Duration stats
    durations = [c.get("duration_ms", 0) for c in tool_calls]
    total_duration_ms = sum(durations)
    avg_duration_ms = total_duration_ms / len(durations) if durations else 0

    # Error rate
    errors = sum(1 for c in tool_calls if c.get("is_error", False))

    # IR preference rate
    diagnosis = categories.get("diagnosis", 0)
    # Navigation tools are not MCP tools, so we can't count them from MCP logs
    # We use "other" as a proxy (Read/Grep/Glob would show as non-MCP)
    ir_preference = diagnosis / len(tool_calls) if tool_calls else 0

    # Time span
    timestamps = []
    for c in tool_calls:
        ts = c.get("ts", "")
        if ts:
            try:
                timestamps.append(datetime.fromisoformat(ts))
            except ValueError:
                pass

    time_span_s = 0
    if len(timestamps) >= 2:
        time_span_s = (max(timestamps) - min(timestamps)).total_seconds()

    # Test cycle detection: count test→(non-test)→test patterns
    fix_attempts = 0
    last_was_test = False
    saw_non_test = False
    for call in tool_calls:
        tool = call.get("tool", "")
        if tool == "test":
            if last_was_test or (not last_was_test and saw_non_test):
                fix_attempts += 1
            last_was_test = True
            saw_non_test = False
        else:
            last_was_test = False
            saw_non_test = True

    return {
        "total_calls": len(tool_calls),
        "categories": dict(categories),
        "tools_used": dict(tools_used),
        "diagnosis_calls": categories.get("diagnosis", 0),
        "execution_calls": categories.get("execution", 0),
        "doc_calls": categories.get("documentation", 0),
        "maintenance_calls": categories.get("maintenance", 0),
        "error_count": errors,
        "error_rate": errors / len(tool_calls) if tool_calls else 0,
        "ir_preference": ir_preference,
        "total_duration_ms": total_duration_ms,
        "avg_duration_ms": round(avg_duration_ms, 1),
        "time_span_s": round(time_span_s, 1),
        "fix_attempts": fix_attempts,
    }


def compute_transition_matrix(calls: list[dict]) -> dict[str, Counter]:
    """Compute tool-to-tool transition counts (Markov chain)."""
    tool_calls = [e for e in calls if e.get("event") == "tool_call"]
    transitions = defaultdict(Counter)

    for i in range(len(tool_calls) - 1):
        from_tool = tool_calls[i].get("tool", "unknown")
        to_tool = tool_calls[i + 1].get("tool", "unknown")
        transitions[from_tool][to_tool] += 1

    return {k: dict(v) for k, v in transitions.items()}


# =============================================================================
# Reporting
# =============================================================================

def print_summary(sessions: dict[str, list[dict]]):
    """Print overall summary statistics."""
    total_calls = 0
    total_sessions = len(sessions)
    all_tools = Counter()
    all_categories = Counter()

    for sid, events in sessions.items():
        tool_calls = [e for e in events if e.get("event") == "tool_call"]
        total_calls += len(tool_calls)
        for c in tool_calls:
            tool = c.get("tool", "unknown")
            all_tools[tool] += 1
            all_categories[classify_tool(tool)] += 1

    print(f"=== MCP Call Log Summary ===")
    print(f"Sessions:     {total_sessions}")
    print(f"Total calls:  {total_calls}")
    print()
    print(f"--- Tool Usage ---")
    for tool, count in all_tools.most_common():
        pct = count / total_calls * 100 if total_calls else 0
        print(f"  {tool:30s} {count:5d} ({pct:5.1f}%)")
    print()
    print(f"--- Category Breakdown ---")
    for cat, count in all_categories.most_common():
        pct = count / total_calls * 100 if total_calls else 0
        print(f"  {cat:20s} {count:5d} ({pct:5.1f}%)")


def print_session_detail(sid: str, events: list[dict]):
    """Print detailed view of a single session."""
    start = next((e for e in events if e.get("event") == "session_start"), None)
    end = next((e for e in events if e.get("event") == "session_end"), None)
    tool_calls = [e for e in events if e.get("event") == "tool_call"]

    print(f"=== Session: {sid} ===")
    if start:
        print(f"Started: {start.get('ts', '?')}")
        print(f"Server:  {start.get('server', '?')} v{start.get('version', '?')}")
    if end:
        print(f"Ended:   {end.get('ts', '?')}")
        print(f"Total:   {end.get('total_calls', '?')} calls")
    print()

    metrics = compute_session_metrics(events)
    print(f"--- Metrics ---")
    print(f"  Total calls:     {metrics['total_calls']}")
    print(f"  Diagnosis:       {metrics['diagnosis_calls']}")
    print(f"  Execution:       {metrics['execution_calls']}")
    print(f"  Documentation:   {metrics['doc_calls']}")
    print(f"  IR preference:   {metrics['ir_preference']:.2f}")
    print(f"  Fix attempts:    {metrics['fix_attempts']}")
    print(f"  Errors:          {metrics['error_count']}")
    print(f"  Time span:       {metrics['time_span_s']}s")
    print()

    print(f"--- Call Timeline ---")
    for c in tool_calls:
        seq = c.get("seq", "?")
        ts = c.get("ts", "?")
        tool = c.get("tool", "?")
        dur = c.get("duration_ms", 0)
        err = " [ERROR]" if c.get("is_error") else ""
        params_str = ""
        params = c.get("params", {})
        if isinstance(params, dict):
            # Show key params only (not full values)
            keys = list(params.keys())
            if keys:
                params_str = f" ({', '.join(keys)})"
        print(f"  [{seq:3}] {ts} {tool:25s} {dur:6d}ms{err}{params_str}")


def print_transitions(sessions: dict[str, list[dict]]):
    """Print tool transition matrix across all sessions."""
    all_calls = []
    for events in sessions.values():
        all_calls.extend([e for e in events if e.get("event") == "tool_call"])

    transitions = compute_transition_matrix(all_calls)

    # Get all unique tools
    all_tools = set()
    for from_t, tos in transitions.items():
        all_tools.add(from_t)
        all_tools.update(tos.keys())
    tools = sorted(all_tools)

    print(f"=== Tool Transition Matrix ===")
    print(f"{'FROM \\ TO':>20s}", end="")
    for t in tools:
        print(f" {t[:8]:>8s}", end="")
    print()

    for from_t in tools:
        print(f"{from_t:>20s}", end="")
        for to_t in tools:
            count = transitions.get(from_t, {}).get(to_t, 0)
            if count > 0:
                print(f" {count:>8d}", end="")
            else:
                print(f" {'·':>8s}", end="")
        print()


def export_transitions_csv(sessions: dict[str, list[dict]], output_path: str):
    """Export tool transition matrix as CSV (for heatmap visualization)."""
    all_calls = []
    for events in sessions.values():
        all_calls.extend([e for e in events if e.get("event") == "tool_call"])

    transitions = compute_transition_matrix(all_calls)

    all_tools = set()
    for from_t, tos in transitions.items():
        all_tools.add(from_t)
        all_tools.update(tos.keys())
    tools = sorted(all_tools)

    with open(output_path, "w", encoding="utf-8") as f:
        # Header
        f.write("from," + ",".join(tools) + "\n")
        # Rows
        for from_t in tools:
            row = [str(transitions.get(from_t, {}).get(to_t, 0)) for to_t in tools]
            f.write(from_t + "," + ",".join(row) + "\n")
    print(f"Transition matrix exported to {output_path}")


def export_data(sessions: dict[str, list[dict]], output_path: str):
    """Export structured analysis data to JSON."""
    result = {
        "generated_at": datetime.now().isoformat(),
        "total_sessions": len(sessions),
        "sessions": [],
    }

    for sid, events in sessions.items():
        start = next((e for e in events if e.get("event") == "session_start"), None)
        end = next((e for e in events if e.get("event") == "session_end"), None)
        metrics = compute_session_metrics(events)
        transitions = compute_transition_matrix(events)

        session_data = {
            "session_id": sid,
            "started_at": start.get("ts") if start else None,
            "ended_at": end.get("ts") if end else None,
            "metrics": metrics,
            "transitions": transitions,
            "calls": [
                {
                    "seq": c.get("seq"),
                    "ts": c.get("ts"),
                    "tool": c.get("tool"),
                    "params_keys": list(c.get("params", {}).keys()) if isinstance(c.get("params"), dict) else [],
                    "duration_ms": c.get("duration_ms"),
                    "is_error": c.get("is_error", False),
                    "category": classify_tool(c.get("tool", "")),
                }
                for c in events if c.get("event") == "tool_call"
            ],
        }
        result["sessions"].append(session_data)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(f"Exported to {output_path}")


# =============================================================================
# Session Labeling (8.3)
# =============================================================================

LAYER_OPTIONS = [
    "lexer", "parser", "type-system", "hir", "thir", "mir",
    "codegen", "runtime", "library", "unknown",
]
OUTCOME_OPTIONS = ["fixed", "partial", "abandoned", "workaround"]


def label_sessions(sessions: dict[str, list[dict]], labels_path: str):
    """Interactive labeling of sessions for post-hoc classification."""
    # Load existing labels if any
    labels = []
    if Path(labels_path).exists():
        with open(labels_path, "r", encoding="utf-8") as f:
            labels = json.load(f)
    labeled_sids = {l["session_id"] for l in labels}

    unlabeled = [(sid, events) for sid, events in sessions.items()
                 if sid not in labeled_sids]

    if not unlabeled:
        print(f"All {len(sessions)} sessions already labeled.")
        return

    print(f"=== Session Labeling ({len(unlabeled)} unlabeled) ===")
    print("Commands: [s]kip, [q]uit, or enter values when prompted\n")

    for sid, events in unlabeled:
        start = next((e for e in events if e.get("event") == "session_start"), None)
        tool_calls = [e for e in events if e.get("event") == "tool_call"]

        if not tool_calls:
            continue

        # Show session summary
        metrics = compute_session_metrics(events)
        condition = start.get("condition", "unknown") if start else "unknown"
        ts = start.get("ts", "?") if start else "?"

        print(f"--- Session: {sid} ---")
        print(f"  Time: {ts}  Condition: {condition}  Calls: {metrics['total_calls']}")
        print(f"  Tools: {metrics.get('tools_used', {})}")
        print(f"  Categories: {metrics.get('categories', {})}")

        # Show first 10 calls
        for c in tool_calls[:10]:
            tool = c.get("tool", "?")
            err = " [ERR]" if c.get("is_error") else ""
            print(f"    {c.get('seq', '?'):3} {tool}{err}")
        if len(tool_calls) > 10:
            print(f"    ... ({len(tool_calls) - 10} more)")

        # Ask for label
        print(f"\n  Bug layer? {LAYER_OPTIONS}")
        layer = input("  > ").strip()
        if layer == "q":
            break
        if layer == "s":
            continue
        if layer not in LAYER_OPTIONS:
            print(f"  Invalid layer, skipping.")
            continue

        print(f"  Outcome? {OUTCOME_OPTIONS}")
        outcome = input("  > ").strip()
        if outcome not in OUTCOME_OPTIONS:
            outcome = "unknown"

        notes = input("  Notes (optional): ").strip()

        label = {
            "session_id": sid,
            "episode_id": f"{sid}-e1",
            "bug_layer": layer,
            "condition": condition,
            "outcome": outcome,
            "metrics": metrics,
            "notes": notes,
        }
        labels.append(label)

        # Save after each label
        with open(labels_path, "w", encoding="utf-8") as f:
            json.dump(labels, f, indent=2)
        print(f"  Saved. ({len(labels)} total labels)\n")

    print(f"\nDone. {len(labels)} labels saved to {labels_path}")


# =============================================================================
# Condition Comparison (8.4)
# =============================================================================

def compare_conditions(sessions: dict[str, list[dict]], labels_path: str):
    """Compare tool usage patterns between conditions A (baseline) and B (debug-layers)."""
    if not Path(labels_path).exists():
        print(f"Error: Labels file not found: {labels_path}", file=sys.stderr)
        sys.exit(1)

    with open(labels_path, "r", encoding="utf-8") as f:
        labels = json.load(f)

    # Group labels by condition
    by_condition: dict[str, list[dict]] = defaultdict(list)
    for label in labels:
        cond = label.get("condition", "unknown")
        by_condition[cond].append(label)

    print(f"=== Condition Comparison ===\n")

    for cond, cond_labels in sorted(by_condition.items()):
        calls_list = [l["metrics"]["total_calls"] for l in cond_labels if "metrics" in l]
        ir_prefs = [l["metrics"].get("ir_preference", 0) for l in cond_labels if "metrics" in l]
        fix_attempts = [l["metrics"].get("fix_attempts", 0) for l in cond_labels if "metrics" in l]

        n = len(calls_list)
        if n == 0:
            continue

        avg_calls = sum(calls_list) / n
        avg_ir = sum(ir_prefs) / n
        avg_fixes = sum(fix_attempts) / n

        # Outcome distribution
        outcomes = Counter(l.get("outcome", "?") for l in cond_labels)
        layers = Counter(l.get("bug_layer", "?") for l in cond_labels)

        print(f"Condition: {cond} (n={n})")
        print(f"  Avg tool calls:    {avg_calls:.1f}")
        print(f"  Avg IR preference: {avg_ir:.2f}")
        print(f"  Avg fix attempts:  {avg_fixes:.1f}")
        print(f"  Outcomes:          {dict(outcomes)}")
        print(f"  Bug layers:        {dict(layers)}")
        print()


# =============================================================================
# Statistical Tests (9.1-9.3)
# =============================================================================

def _mean(data: list[float]) -> float:
    return sum(data) / len(data) if data else 0.0


def _std(data: list[float]) -> float:
    if len(data) < 2:
        return 0.0
    m = _mean(data)
    return math.sqrt(sum((x - m) ** 2 for x in data) / (len(data) - 1))


def _cohens_d(group_a: list[float], group_b: list[float]) -> float:
    """Compute Cohen's d effect size between two groups."""
    na, nb = len(group_a), len(group_b)
    if na < 2 or nb < 2:
        return 0.0
    ma, mb = _mean(group_a), _mean(group_b)
    sa, sb = _std(group_a), _std(group_b)
    # Pooled standard deviation
    sp = math.sqrt(((na - 1) * sa**2 + (nb - 1) * sb**2) / (na + nb - 2))
    if sp == 0:
        return 0.0
    return (ma - mb) / sp


def _welch_t_test(group_a: list[float], group_b: list[float]) -> tuple[float, float]:
    """Welch's t-test (unequal variances). Returns (t_statistic, approximate p_value)."""
    na, nb = len(group_a), len(group_b)
    if na < 2 or nb < 2:
        return (0.0, 1.0)
    ma, mb = _mean(group_a), _mean(group_b)
    sa, sb = _std(group_a), _std(group_b)
    se = math.sqrt(sa**2 / na + sb**2 / nb)
    if se == 0:
        return (0.0, 1.0)
    t = (ma - mb) / se
    # Approximate degrees of freedom (Welch-Satterthwaite)
    num = (sa**2 / na + sb**2 / nb) ** 2
    denom = (sa**2 / na)**2 / (na - 1) + (sb**2 / nb)**2 / (nb - 1)
    df = num / denom if denom > 0 else 1

    # Approximate two-tailed p-value using Student's t CDF approximation
    # (simplified — for proper p-values use scipy.stats)
    x = df / (df + t**2)
    # Rough approximation for small samples
    p = 1.0  # Placeholder — report t and df, compute exact p with scipy
    return (t, p)


def run_statistical_tests(sessions: dict[str, list[dict]], labels_path: str):
    """Run statistical tests comparing conditions A and B."""
    if not Path(labels_path).exists():
        print(f"Error: Labels file not found: {labels_path}", file=sys.stderr)
        sys.exit(1)

    with open(labels_path, "r", encoding="utf-8") as f:
        labels = json.load(f)

    # Separate by condition
    baseline_metrics = [l["metrics"] for l in labels if l.get("condition") == "baseline" and "metrics" in l]
    enhanced_metrics = [l["metrics"] for l in labels if l.get("condition") == "debug-layers" and "metrics" in l]

    if not baseline_metrics or not enhanced_metrics:
        print("Need labeled data from BOTH conditions (baseline and debug-layers).")
        print(f"  Baseline sessions: {len(baseline_metrics)}")
        print(f"  Debug-layers sessions: {len(enhanced_metrics)}")
        return

    print(f"=== Statistical Analysis ===")
    print(f"  Baseline (A): n={len(baseline_metrics)}")
    print(f"  Debug-layers (B): n={len(enhanced_metrics)}")
    print()

    metrics_to_compare = [
        ("total_calls", "Tool Calls"),
        ("fix_attempts", "Fix Attempts"),
        ("ir_preference", "IR Preference Rate"),
        ("error_count", "Error Count"),
    ]

    for metric_key, metric_name in metrics_to_compare:
        a_vals = [m.get(metric_key, 0) for m in baseline_metrics]
        b_vals = [m.get(metric_key, 0) for m in enhanced_metrics]

        ma, mb = _mean(a_vals), _mean(b_vals)
        sa, sb = _std(a_vals), _std(b_vals)
        d = _cohens_d(a_vals, b_vals)
        t_stat, _ = _welch_t_test(a_vals, b_vals)

        # Effect size interpretation
        if abs(d) < 0.2:
            effect = "negligible"
        elif abs(d) < 0.5:
            effect = "small"
        elif abs(d) < 0.8:
            effect = "medium"
        else:
            effect = "large"

        print(f"--- {metric_name} ---")
        print(f"  Baseline:     mean={ma:.2f}, std={sa:.2f}")
        print(f"  Debug-layers: mean={mb:.2f}, std={sb:.2f}")
        print(f"  Difference:   {mb - ma:+.2f} ({(mb - ma) / ma * 100:+.1f}%)" if ma > 0 else f"  Difference:   {mb - ma:+.2f}")
        print(f"  Cohen's d:    {d:.3f} ({effect})")
        print(f"  Welch's t:    {t_stat:.3f}")
        print()

    # Success rate comparison
    baseline_fixed = sum(1 for l in labels if l.get("condition") == "baseline" and l.get("outcome") == "fixed")
    enhanced_fixed = sum(1 for l in labels if l.get("condition") == "debug-layers" and l.get("outcome") == "fixed")
    baseline_total = len(baseline_metrics)
    enhanced_total = len(enhanced_metrics)

    print(f"--- Success Rate ---")
    print(f"  Baseline:     {baseline_fixed}/{baseline_total} ({baseline_fixed/baseline_total*100:.0f}%)" if baseline_total > 0 else "  Baseline: N/A")
    print(f"  Debug-layers: {enhanced_fixed}/{enhanced_total} ({enhanced_fixed/enhanced_total*100:.0f}%)" if enhanced_total > 0 else "  Debug-layers: N/A")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Analyze MCP call logs for LLM-IR-Debugging research")
    parser.add_argument("log_file", help="Path to NDJSON call log file")
    parser.add_argument("--sessions", action="store_true", help="List all sessions")
    parser.add_argument("--session", type=str, help="Show detail for a specific session ID")
    parser.add_argument("--metrics", action="store_true", help="Compute research metrics")
    parser.add_argument("--export", type=str, help="Export structured data to JSON file")
    parser.add_argument("--transitions", action="store_true", help="Show tool transition matrix")
    parser.add_argument("--label", type=str, metavar="LABELS_JSON",
                        help="Interactive session labeling (save to JSON file)")
    parser.add_argument("--compare", type=str, metavar="LABELS_JSON",
                        help="Compare conditions A vs B using labeled data")
    parser.add_argument("--stats", type=str, metavar="LABELS_JSON",
                        help="Run statistical tests (t-test, Cohen's d) on labeled data")
    parser.add_argument("--heatmap", type=str, metavar="OUTPUT_CSV",
                        help="Export tool transition matrix as CSV for heatmap visualization")

    args = parser.parse_args()

    if not Path(args.log_file).exists():
        print(f"Error: Log file not found: {args.log_file}", file=sys.stderr)
        sys.exit(1)

    events = parse_log(args.log_file)
    sessions = group_by_session(events)

    if not events:
        print("No events found in log file.", file=sys.stderr)
        sys.exit(1)

    if args.heatmap:
        export_transitions_csv(sessions, args.heatmap)
    elif args.label:
        label_sessions(sessions, args.label)
    elif args.compare:
        compare_conditions(sessions, args.compare)
    elif args.stats:
        run_statistical_tests(sessions, args.stats)
    elif args.session:
        if args.session in sessions:
            print_session_detail(args.session, sessions[args.session])
        else:
            print(f"Session not found: {args.session}", file=sys.stderr)
            print(f"Available sessions: {', '.join(sessions.keys())}", file=sys.stderr)
            sys.exit(1)
    elif args.sessions:
        print(f"=== Sessions ({len(sessions)}) ===")
        for sid, events_list in sessions.items():
            start = next((e for e in events_list if e.get("event") == "session_start"), None)
            calls = [e for e in events_list if e.get("event") == "tool_call"]
            ts = start.get("ts", "?") if start else "?"
            print(f"  {sid:30s}  {ts:20s}  {len(calls):4d} calls")
    elif args.transitions:
        print_transitions(sessions)
    elif args.export:
        export_data(sessions, args.export)
    elif args.metrics:
        print(f"=== Research Metrics (all sessions) ===")
        print()
        for sid, events_list in sessions.items():
            metrics = compute_session_metrics(events_list)
            if metrics["total_calls"] == 0:
                continue
            print(f"Session: {sid}")
            print(f"  Calls: {metrics['total_calls']}, "
                  f"IR pref: {metrics['ir_preference']:.2f}, "
                  f"Fix attempts: {metrics['fix_attempts']}, "
                  f"Errors: {metrics['error_count']}, "
                  f"Duration: {metrics['time_span_s']}s")
            print(f"  Categories: {metrics['categories']}")
            print()
    else:
        print_summary(sessions)


if __name__ == "__main__":
    main()
