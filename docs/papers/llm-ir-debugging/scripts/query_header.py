#!/usr/bin/env python3
"""MCP Call Log Query Tool for LLM-IR-Debugging Research."""

import argparse, json, math, os, sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional


def find_log_file() -> Optional[str]:
    cwd_log = Path.cwd() / "mcp-call-log.jsonl"
    if cwd_log.exists(): return str(cwd_log)
    log_dir = os.environ.get("TML_MCP_LOG_DIR")
    if log_dir:
        env_log = Path(log_dir) / "mcp-call-log.jsonl"
        if env_log.exists(): return str(env_log)
    script_dir = Path(__file__).resolve().parent
    root_log = script_dir.parent.parent.parent / "mcp-call-log.jsonl"
    if root_log.exists(): return str(root_log)
    return None
