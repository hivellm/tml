#!/usr/bin/env python3
"""Generate research visualizations from MCP log data.

Usage: python visualize.py [--output charts/]

Generates PNG charts (requires matplotlib):
- tool_adoption_timeline.png
- transition_heatmap.png
- error_rate_area.png
- session_duration_hist.png
- test_latency_box.png
"""
import json, sys, os
from collections import defaultdict, Counter
from pathlib import Path
from datetime import datetime

def find_log():
    for p in [Path("mcp-call-log.jsonl"), Path("..") / "mcp-call-log.jsonl",
              Path(os.environ.get("TML_MCP_LOG_DIR", "")) / "mcp-call-log.jsonl"]:
        if p.exists(): return p
    return None

def load_calls(log_path):
    calls = []
    with open(log_path) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: e = json.loads(line)
            except: continue
            if e.get("event") == "tool_call": calls.append(e)
    return calls

def tool_adoption_timeline(calls, output_dir):
    """Line chart: tool usage % over time (by day)."""
    import matplotlib.pyplot as plt

    by_day = defaultdict(lambda: Counter())
    for c in calls:
        ts = c.get("ts", 0)
        day = datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d") if ts else "unknown"
        by_day[day][c["tool"]] += 1

    days = sorted(by_day.keys())
    if len(days) < 2:
        print("  Not enough days for timeline"); return

    top_tools = Counter()
    for c in calls: top_tools[c["tool"]] += 1
    top_5 = [t for t, _ in top_tools.most_common(5)]

    fig, ax = plt.subplots(figsize=(12, 6))
    for tool in top_5:
        pcts = []
        for day in days:
            total = sum(by_day[day].values())
            pcts.append(100 * by_day[day].get(tool, 0) / total if total else 0)
        ax.plot(range(len(days)), pcts, label=tool, marker="o", markersize=3)

    ax.set_xticks(range(len(days)))
    ax.set_xticklabels(days, rotation=45, ha="right", fontsize=8)
    ax.set_ylabel("% of calls")
    ax.set_title("Tool Adoption Timeline")
    ax.legend(loc="upper right")
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_dir / "tool_adoption_timeline.png", dpi=150)
    plt.close()
    print("  Created tool_adoption_timeline.png")

def transition_heatmap(calls, output_dir):
    """Heatmap: tool→tool transition probabilities."""
    import matplotlib.pyplot as plt
    import numpy as np

    transitions = defaultdict(lambda: defaultdict(int))
    for i in range(1, len(calls)):
        if calls[i]["session"] == calls[i-1]["session"]:
            transitions[calls[i-1]["tool"]][calls[i]["tool"]] += 1

    top_tools = Counter(c["tool"] for c in calls).most_common(8)
    tools = [t for t, _ in top_tools]

    matrix = []
    for from_tool in tools:
        row = []
        total = sum(transitions[from_tool].values())
        for to_tool in tools:
            row.append(transitions[from_tool][to_tool] / total * 100 if total else 0)
        matrix.append(row)

    fig, ax = plt.subplots(figsize=(10, 8))
    im = ax.imshow(matrix, cmap="YlOrRd")
    ax.set_xticks(range(len(tools)))
    ax.set_yticks(range(len(tools)))
    ax.set_xticklabels(tools, rotation=45, ha="right", fontsize=8)
    ax.set_yticklabels(tools, fontsize=8)
    ax.set_xlabel("To")
    ax.set_ylabel("From")
    ax.set_title("Tool Transition Probabilities (%)")

    for i in range(len(tools)):
        for j in range(len(tools)):
            val = matrix[i][j]
            if val > 0:
                ax.text(j, i, f"{val:.0f}", ha="center", va="center",
                        color="white" if val > 30 else "black", fontsize=7)

    plt.colorbar(im)
    plt.tight_layout()
    plt.savefig(output_dir / "transition_heatmap.png", dpi=150)
    plt.close()
    print("  Created transition_heatmap.png")

def error_rate_area(calls, output_dir):
    """Stacked area: error types over time."""
    import matplotlib.pyplot as plt

    by_day = defaultdict(lambda: Counter())
    for c in calls:
        ts = c.get("ts", 0)
        day = datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d") if ts else "unknown"
        et = c.get("error_type", "none")
        if et != "none": by_day[day][et] += 1

    days = sorted(by_day.keys())
    if len(days) < 2:
        print("  Not enough days for error chart"); return

    error_types = ["compile", "crash", "timeout", "assertion", "unknown"]
    colors = ["#e74c3c", "#c0392b", "#f39c12", "#e67e22", "#95a5a6"]

    fig, ax = plt.subplots(figsize=(12, 5))
    bottom = [0] * len(days)
    for et, color in zip(error_types, colors):
        vals = [by_day[d].get(et, 0) for d in days]
        ax.bar(range(len(days)), vals, bottom=bottom, label=et, color=color, width=0.8)
        bottom = [b + v for b, v in zip(bottom, vals)]

    ax.set_xticks(range(len(days)))
    ax.set_xticklabels(days, rotation=45, ha="right", fontsize=8)
    ax.set_ylabel("Error count")
    ax.set_title("Errors by Type Over Time")
    ax.legend(loc="upper right")
    plt.tight_layout()
    plt.savefig(output_dir / "error_rate_area.png", dpi=150)
    plt.close()
    print("  Created error_rate_area.png")

def session_duration_hist(calls, output_dir):
    """Histogram of session durations."""
    import matplotlib.pyplot as plt

    by_session = defaultdict(list)
    for c in calls: by_session[c["session"]].append(c.get("duration_ms", 0))

    durations = [sum(ds) / 1000 for ds in by_session.values() if ds]  # seconds
    if not durations:
        print("  No session data"); return

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.hist(durations, bins=30, color="#3498db", edgecolor="white", alpha=0.8)
    ax.set_xlabel("Session duration (seconds)")
    ax.set_ylabel("Count")
    ax.set_title(f"Session Duration Distribution (n={len(durations)})")
    ax.axvline(sorted(durations)[len(durations)//2], color="red", linestyle="--", label="Median")
    ax.legend()
    plt.tight_layout()
    plt.savefig(output_dir / "session_duration_hist.png", dpi=150)
    plt.close()
    print("  Created session_duration_hist.png")

def test_latency_box(calls, output_dir):
    """Box plot of test compilation time by suite."""
    import matplotlib.pyplot as plt

    by_target = defaultdict(list)
    for c in calls:
        if c["tool"] != "test": continue
        target = c.get("test_target", "unknown")
        if not target: target = "unknown"
        by_target[target].append(c.get("duration_ms", 0) / 1000)

    top = sorted(by_target.items(), key=lambda x: -len(x[1]))[:10]
    if not top:
        print("  No test data"); return

    fig, ax = plt.subplots(figsize=(12, 6))
    labels = [t[0][:25] for t in top]
    data = [t[1] for t in top]
    ax.boxplot(data, labels=labels, vert=True)
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=8)
    ax.set_ylabel("Duration (seconds)")
    ax.set_title("Test Compilation Time by Suite")
    plt.tight_layout()
    plt.savefig(output_dir / "test_latency_box.png", dpi=150)
    plt.close()
    print("  Created test_latency_box.png")

if __name__ == "__main__":
    try:
        import matplotlib
        matplotlib.use("Agg")
    except ImportError:
        print("ERROR: matplotlib required. Install: pip install matplotlib")
        sys.exit(1)

    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="docs/papers/llm-ir-debugging/charts")
    args = parser.parse_args()

    log = find_log()
    if not log: print("ERROR: JSONL not found"); sys.exit(1)

    calls = load_calls(log)
    print(f"Loaded {len(calls)} calls")

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("Generating charts...")
    tool_adoption_timeline(calls, output_dir)
    transition_heatmap(calls, output_dir)
    error_rate_area(calls, output_dir)
    session_duration_hist(calls, output_dir)
    test_latency_box(calls, output_dir)
    print("Done!")
