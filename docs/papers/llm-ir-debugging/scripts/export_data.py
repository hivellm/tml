#!/usr/bin/env python3
"""Export MCP log data to CSV for pandas/jupyter analysis.

Usage:
    python export_data.py                    # Export to CSV (default)
    python export_data.py --format csv       # Explicit CSV
    python export_data.py --output dir/      # Custom output directory
"""
import json, csv, sys, os
from pathlib import Path

def find_log():
    for p in [Path("mcp-call-log.jsonl"), Path("..") / "mcp-call-log.jsonl",
              Path(os.environ.get("TML_MCP_LOG_DIR", "")) / "mcp-call-log.jsonl"]:
        if p.exists(): return p
    return None

def export_csv(jsonl_path, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    sessions, calls = [], []
    with open(jsonl_path) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: e = json.loads(line)
            except: continue
            if e.get("event") == "session_start": sessions.append(e)
            elif e.get("event") == "tool_call": calls.append(e)

    # Sessions CSV
    with open(output_dir / "sessions.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["session_id", "server", "version", "condition", "task", "model", "claude_md_hash"])
        for s in sessions:
            w.writerow([s.get("session"), s.get("server"), s.get("version"),
                        s.get("condition"), s.get("task", ""), s.get("model", ""),
                        s.get("claude_md_hash", "")])

    # Tool calls CSV
    with open(output_dir / "tool_calls.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["session_id", "seq", "ts", "tool", "duration_ms", "is_error",
                     "error_type", "preceded_by", "test_target", "debug_layers",
                     "output_tokens"])
        for c in calls:
            w.writerow([c.get("session"), c.get("seq"), c.get("ts"), c.get("tool"),
                        c.get("duration_ms"), c.get("is_error"), c.get("error_type", ""),
                        c.get("preceded_by", ""), c.get("test_target", ""),
                        c.get("debug_layers"), c.get("output_tokens", 0)])

    print(f"Exported {len(sessions)} sessions + {len(calls)} calls to {output_dir}/")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--format", default="csv", choices=["csv"])
    parser.add_argument("--output", default="docs/papers/llm-ir-debugging/data")
    args = parser.parse_args()

    jsonl = find_log()
    if not jsonl: print("ERROR: JSONL not found"); sys.exit(1)
    export_csv(jsonl, args.output)
