#!/usr/bin/env python3
"""Migrate MCP JSONL logs to SQLite for structured analysis.

Usage: python migrate_to_sqlite.py [--jsonl PATH] [--db PATH]

Defaults:
  --jsonl mcp-call-log.jsonl
  --db    docs/papers/llm-ir-debugging/mcp_research.db
"""
import json, sqlite3, sys, os
from pathlib import Path

def find_log():
    for p in [Path("mcp-call-log.jsonl"), Path("..") / "mcp-call-log.jsonl",
              Path(os.environ.get("TML_MCP_LOG_DIR", "")) / "mcp-call-log.jsonl"]:
        if p.exists(): return p
    return None

def create_schema(conn):
    conn.executescript("""
    CREATE TABLE IF NOT EXISTS sessions (
        session_id TEXT PRIMARY KEY,
        server TEXT, version TEXT, condition TEXT,
        task TEXT, model TEXT, claude_md_hash TEXT,
        start_ts INTEGER,
        total_calls INTEGER, tools_used INTEGER,
        error_count INTEGER, error_rate REAL, dominant_tool TEXT
    );
    CREATE TABLE IF NOT EXISTS tool_calls (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT, seq INTEGER, ts INTEGER,
        tool TEXT, params TEXT, duration_ms INTEGER,
        is_error BOOLEAN, error_type TEXT,
        preceded_by TEXT, test_target TEXT,
        debug_layers TEXT, output_tokens INTEGER,
        test_results TEXT,
        FOREIGN KEY (session_id) REFERENCES sessions(session_id)
    );
    CREATE INDEX IF NOT EXISTS idx_calls_session ON tool_calls(session_id);
    CREATE INDEX IF NOT EXISTS idx_calls_tool ON tool_calls(tool);
    CREATE INDEX IF NOT EXISTS idx_calls_ts ON tool_calls(ts);
    """)

def migrate(jsonl_path, db_path):
    conn = sqlite3.connect(db_path)
    create_schema(conn)

    sessions_seen = set()
    calls_count = 0

    with open(jsonl_path) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue

            event = e.get("event")
            sid = e.get("session", "")

            if event == "session_start" and sid not in sessions_seen:
                sessions_seen.add(sid)
                conn.execute(
                    "INSERT OR REPLACE INTO sessions (session_id, server, version, condition, task, model, claude_md_hash, start_ts) VALUES (?,?,?,?,?,?,?,?)",
                    (sid, e.get("server"), e.get("version"), e.get("condition"),
                     e.get("task"), e.get("model"), e.get("claude_md_hash"), e.get("ts"))
                )
            elif event == "session_end":
                conn.execute(
                    "UPDATE sessions SET total_calls=?, tools_used=?, error_count=?, error_rate=?, dominant_tool=? WHERE session_id=?",
                    (e.get("total_calls"), e.get("tools_used"), e.get("error_count"),
                     e.get("error_rate"), e.get("dominant_tool"), sid)
                )
            elif event == "tool_call":
                tr = e.get("test_results")
                if isinstance(tr, dict): tr = json.dumps(tr)
                conn.execute(
                    "INSERT INTO tool_calls (session_id, seq, ts, tool, params, duration_ms, is_error, error_type, preceded_by, test_target, debug_layers, output_tokens, test_results) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (sid, e.get("seq"), e.get("ts"), e.get("tool"),
                     json.dumps(e.get("params")) if isinstance(e.get("params"), dict) else str(e.get("params", "")),
                     e.get("duration_ms"), e.get("is_error"), e.get("error_type"),
                     e.get("preceded_by"), e.get("test_target"),
                     str(e.get("debug_layers")), e.get("output_tokens"), tr)
                )
                calls_count += 1

    conn.commit()
    conn.close()
    print(f"Migrated {calls_count} calls, {len(sessions_seen)} sessions to {db_path}")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--jsonl", default=None)
    parser.add_argument("--db", default="docs/papers/llm-ir-debugging/mcp_research.db")
    args = parser.parse_args()

    jsonl = Path(args.jsonl) if args.jsonl else find_log()
    if not jsonl or not jsonl.exists():
        print("ERROR: JSONL file not found"); sys.exit(1)

    migrate(jsonl, args.db)
