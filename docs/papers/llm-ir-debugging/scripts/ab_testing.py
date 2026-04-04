#!/usr/bin/env python3
"""A/B Testing Framework for prompt engineering experiments.

9.1  Condition alternator: alternate between conditions for each new session
9.2  Condition registry: named conditions in conditions.json
9.3  Statistical comparator: mean, std, Welch's t-test, Cohen's d, 95% CI

Usage:
    python scripts/ab_testing.py status                      # Show current condition
    python scripts/ab_testing.py alternate                   # Toggle to next condition
    python scripts/ab_testing.py compare baseline enhanced   # Statistical comparison
    python scripts/ab_testing.py conditions                  # List registered conditions
    python scripts/ab_testing.py init                        # Create default conditions.json

Condition state: docs/papers/llm-ir-debugging/.ab_state.json
Condition registry: docs/papers/llm-ir-debugging/conditions.json
"""
import json, sys, os, math, argparse
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Paths — resolve relative to the script's own directory so this works
# from any working directory
# ---------------------------------------------------------------------------
_SCRIPT_DIR = Path(__file__).parent
_PAPER_DIR  = _SCRIPT_DIR.parent
STATE_FILE      = _PAPER_DIR / ".ab_state.json"
CONDITIONS_FILE = _PAPER_DIR / "conditions.json"

DEFAULT_CONDITIONS = {
    "baseline": {
        "debug_layers": False,
        "check_before_test": False,
        "description": "No debug_layers auto-enable, no check enforcement",
    },
    "debug-layers": {
        "debug_layers": True,
        "check_before_test": False,
        "description": "debug_layers enabled on test failure",
    },
    "enhanced": {
        "debug_layers": True,
        "check_before_test": True,
        "description": "debug_layers + check enforced before every test",
    },
}


# ---------------------------------------------------------------------------
# JSONL helpers
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
    for candidate in [Path("mcp-call-log.jsonl"), Path("..") / "mcp-call-log.jsonl"]:
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


def load_jsonl(log_path: Path):
    sessions: dict = {}
    calls: list = []
    with open(log_path, encoding="utf-8") as f:
        for line in f:
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


# ---------------------------------------------------------------------------
# 9.2 Condition registry
# ---------------------------------------------------------------------------

def load_conditions() -> dict:
    """Load conditions from conditions.json, falling back to defaults."""
    if CONDITIONS_FILE.exists():
        with open(CONDITIONS_FILE, encoding="utf-8") as f:
            return json.load(f)
    return DEFAULT_CONDITIONS


def save_conditions(conditions: dict):
    CONDITIONS_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(CONDITIONS_FILE, "w", encoding="utf-8") as f:
        json.dump(conditions, f, indent=2)
    print(f"Saved conditions to: {CONDITIONS_FILE}")


# ---------------------------------------------------------------------------
# 9.1 Condition alternator
# ---------------------------------------------------------------------------

def load_state() -> dict:
    if STATE_FILE.exists():
        with open(STATE_FILE, encoding="utf-8") as f:
            return json.load(f)
    # Default: start with debug-layers (current active condition)
    return {
        "current": "debug-layers",
        "session_count": 0,
        "history": [],
    }


def save_state(state: dict):
    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


def cmd_status():
    state = load_state()
    conditions = load_conditions()
    current = state["current"]
    cfg = conditions.get(current, {})
    print(f"Current condition : {current}")
    print(f"Description       : {cfg.get('description', 'N/A')}")
    print(f"Session count     : {state['session_count']}")
    print(f"Settings          : {json.dumps({k: v for k, v in cfg.items() if k != 'description'})}")


def cmd_alternate():
    """Advance to the next condition in the registry, wrapping around."""
    state = load_state()
    conditions = load_conditions()
    names = list(conditions.keys())
    if not names:
        print("No conditions registered.")
        sys.exit(1)
    current = state.get("current", names[0])
    idx = names.index(current) if current in names else 0
    new_cond = names[(idx + 1) % len(names)]

    state["history"].append({
        "from": current,
        "to": new_cond,
        "at": datetime.now(tz=timezone.utc).isoformat(),
        "session_count": state["session_count"],
    })
    state["current"] = new_cond
    state["session_count"] += 1
    save_state(state)
    print(f"Switched: {current} → {new_cond}")
    print(f"Total alternations: {state['session_count']}")


def cmd_conditions():
    conditions = load_conditions()
    print(f"Registered conditions ({len(conditions)}):")
    for name, cfg in conditions.items():
        settings = {k: v for k, v in cfg.items() if k != "description"}
        print(f"  {name:<20} {cfg.get('description', 'no description')}")
        print(f"  {'':>20} settings: {json.dumps(settings)}")


def cmd_init():
    """Create default conditions.json if it does not exist."""
    if CONDITIONS_FILE.exists():
        print(f"Already exists: {CONDITIONS_FILE}")
        print("Delete it first if you want to recreate defaults.")
        sys.exit(1)
    save_conditions(DEFAULT_CONDITIONS)
    print("Created default conditions.json with: baseline, debug-layers, enhanced")


# ---------------------------------------------------------------------------
# 9.3 Statistical comparator
# ---------------------------------------------------------------------------

def _mean(vals: list) -> float:
    return sum(vals) / len(vals) if vals else 0.0


def _std(vals: list) -> float:
    if len(vals) < 2:
        return 0.0
    m = _mean(vals)
    return math.sqrt(sum((x - m) ** 2 for x in vals) / (len(vals) - 1))


def _welch_t_test(a: list, b: list) -> tuple:
    """
    Welch's t-test (unequal variances). Returns (t_statistic, df).
    Use scipy.stats.ttest_ind for exact p-values; we return t and df so
    the caller can either use scipy or interpret |t| directly.
    """
    na, nb = len(a), len(b)
    if na < 2 or nb < 2:
        return 0.0, 1.0
    ma, mb = _mean(a), _mean(b)
    sa, sb = _std(a), _std(b)
    se = math.sqrt(sa ** 2 / na + sb ** 2 / nb)
    if se == 0:
        return 0.0, float(na + nb - 2)
    t = (ma - mb) / se
    # Welch-Satterthwaite degrees of freedom
    num = (sa ** 2 / na + sb ** 2 / nb) ** 2
    denom = (sa ** 2 / na) ** 2 / (na - 1) + (sb ** 2 / nb) ** 2 / (nb - 1)
    df = num / denom if denom > 0 else float(na + nb - 2)
    return t, df


def _cohens_d(a: list, b: list) -> float:
    """Cohen's d using pooled standard deviation (equal-weight)."""
    na, nb = len(a), len(b)
    if na < 2 or nb < 2:
        return 0.0
    ma, mb = _mean(a), _mean(b)
    sa, sb = _std(a), _std(b)
    # Pooled SD (Hedges pooled formula)
    sp = math.sqrt(((na - 1) * sa ** 2 + (nb - 1) * sb ** 2) / (na + nb - 2))
    return (ma - mb) / sp if sp > 0 else 0.0


def _confidence_interval_95(vals: list) -> tuple:
    """95% CI for the mean using t-distribution approximation."""
    n = len(vals)
    if n < 2:
        m = _mean(vals)
        return m, m
    m = _mean(vals)
    s = _std(vals)
    se = s / math.sqrt(n)
    # t critical value approximation for common df (conservative: use 1.96 for large n,
    # scale up for small n using a lookup table)
    t_crit_approx = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571,
                     6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
                     15: 2.131, 20: 2.086, 30: 2.042, 60: 2.000}
    df = n - 1
    # Find closest df in table
    keys = sorted(t_crit_approx.keys())
    t_crit = 1.96
    for k in keys:
        if df <= k:
            t_crit = t_crit_approx[k]
            break
    half_width = t_crit * se
    return m - half_width, m + half_width


def _compute_p_value_approx(t_stat: float, df: float) -> float:
    """
    Approximate two-tailed p-value using the regularized incomplete beta function.
    Falls back to scipy if available for higher accuracy.
    """
    try:
        from scipy.stats import t as t_dist
        return float(2 * t_dist.sf(abs(t_stat), df))
    except ImportError:
        pass

    # Manual approximation via Abramowitz & Stegun 26.7.8
    # Only accurate for df >= 5; for df < 5 treat as approximate
    x = df / (df + t_stat ** 2)
    # Regularized incomplete beta approximation
    # I_x(a, b) where a = df/2, b = 0.5
    a, b = df / 2.0, 0.5
    # Use continued fraction (simple approximation)
    if x >= 1.0:
        return 1.0
    if x <= 0.0:
        return 0.0
    # Simple approximation: p ≈ 1 - F(|t|, df) using Wilson-Hilferty normal approx
    h = 2.0 / (9.0 * df)
    z = (abs(t_stat) ** (2.0 / 3.0) * (1 - h) - (1 - h)) / math.sqrt(h)
    # Standard normal CDF approximation (Abramowitz & Stegun 26.2.17)
    if z > 6:
        return 0.0
    if z < -6:
        return 1.0
    t2 = 1.0 / (1.0 + 0.2316419 * abs(z))
    poly = t2 * (0.319381530 + t2 * (-0.356563782 + t2 * (1.781477937 +
           t2 * (-1.821255978 + t2 * 1.330274429))))
    phi = math.exp(-z * z / 2) / math.sqrt(2 * math.pi) * poly
    p_one_tail = phi if z >= 0 else 1 - phi
    return 2 * min(p_one_tail, 1 - p_one_tail)


def _session_metrics_from_calls(session_calls: list) -> dict:
    n = len(session_calls)
    if n == 0:
        return {}
    errors = sum(1 for c in session_calls if c.get("error_type", "none") != "none" or c.get("is_error"))
    tests  = [c for c in session_calls if c.get("tool") == "test"]
    check_before = sum(1 for c in tests if c.get("preceded_by") == "check")
    dl_tests = sum(
        1 for c in tests
        if c.get("debug_layers") is True or c.get("debug_layers") == "true"
        or (isinstance(c.get("params"), dict) and c["params"].get("debug_layers"))
    )
    durs = [c.get("duration_ms", 0) for c in session_calls]
    return {
        "total_calls":    n,
        "error_rate":     errors / n,
        "check_adoption": check_before / len(tests) if tests else 0.0,
        "dl_adoption":    dl_tests / len(tests) if tests else 0.0,
        "mean_duration":  _mean(durs),
    }


def cmd_compare(cond_a: str, cond_b: str):
    """Full statistical comparison: mean, std, Welch's t, Cohen's d, 95% CI."""
    log = find_log()
    if not log:
        print("ERROR: mcp-call-log.jsonl not found. Pass path or set MCP_CALL_LOG.")
        sys.exit(1)

    sessions, calls = load_jsonl(log)

    # Group calls into sessions, then assign condition
    by_session: dict = defaultdict(list)
    for c in calls:
        by_session[c.get("session", "unknown")].append(c)

    groups: dict = {cond_a: [], cond_b: []}
    for sid, session_calls in by_session.items():
        cond = sessions.get(sid, {}).get("condition", "unknown")
        if cond in groups:
            m = _session_metrics_from_calls(session_calls)
            if m:
                groups[cond].append(m)

    na, nb = len(groups[cond_a]), len(groups[cond_b])
    if na == 0 or nb == 0:
        print(f"Insufficient data: {cond_a}={na} sessions, {cond_b}={nb} sessions")
        print("Use 'python ab_testing.py conditions' to see registered conditions.")
        sys.exit(1)

    print(f"\nA/B Comparison: '{cond_a}' (n={na}) vs '{cond_b}' (n={nb})\n")
    print(f"{'Metric':<22} {'A mean':>10} {'A std':>8} {'B mean':>10} {'B std':>8} "
          f"{'Delta':>8} {'t':>7} {'p':>8} {'d':>7} {'95% CI (B)':>22} {'Effect':>10}")
    print("-" * 120)

    metrics_labels = [
        ("error_rate",     "Error Rate"),
        ("check_adoption", "Check Adoption"),
        ("dl_adoption",    "DL Adoption"),
        ("mean_duration",  "Mean Duration (ms)"),
    ]

    for key, label in metrics_labels:
        a_vals = [m[key] for m in groups[cond_a] if key in m]
        b_vals = [m[key] for m in groups[cond_b] if key in m]
        if not a_vals or not b_vals:
            print(f"{label:<22} {'N/A':>10}")
            continue

        ma, mb   = _mean(a_vals), _mean(b_vals)
        sa, sb   = _std(a_vals), _std(b_vals)
        delta    = mb - ma
        t_stat, df = _welch_t_test(a_vals, b_vals)
        p_val    = _compute_p_value_approx(t_stat, df)
        d        = _cohens_d(a_vals, b_vals)
        ci_lo, ci_hi = _confidence_interval_95(b_vals)

        effect = (
            "negligible" if abs(d) < 0.2 else
            "small"      if abs(d) < 0.5 else
            "medium"     if abs(d) < 0.8 else
            "large"
        )

        is_rate = key in ("error_rate", "check_adoption", "dl_adoption")
        def fmt(v: float) -> str:
            return f"{v*100:.1f}%" if is_rate else f"{v:.1f}"

        ci_str = f"[{fmt(ci_lo)}, {fmt(ci_hi)}]"
        p_str  = f"{p_val:.4f}" if p_val >= 0.0001 else "<.0001"

        print(
            f"{label:<22} {fmt(ma):>10} {fmt(sa):>8} {fmt(mb):>10} {fmt(sb):>8} "
            f"{('+' if delta >= 0 else '') + fmt(delta):>8} "
            f"{t_stat:>7.3f} {p_str:>8} {d:>7.3f} {ci_str:>22} {effect:>10}"
        )

    print()
    print("Interpretation:")
    print("  p < 0.05: statistically significant difference")
    print("  Cohen's d: <0.2 negligible, 0.2–0.5 small, 0.5–0.8 medium, >0.8 large")
    print("  95% CI: range where true mean of B lies with 95% confidence")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)

    cmd = sys.argv[1]
    if cmd == "status":
        cmd_status()
    elif cmd == "alternate":
        cmd_alternate()
    elif cmd == "conditions":
        cmd_conditions()
    elif cmd == "init":
        cmd_init()
    elif cmd == "compare":
        if len(sys.argv) < 4:
            print("Usage: ab_testing.py compare <condition_a> <condition_b>")
            sys.exit(1)
        cmd_compare(sys.argv[2], sys.argv[3])
    else:
        print(f"Unknown command: {cmd}")
        print("Commands: status, alternate, compare, conditions, init")
        sys.exit(1)
