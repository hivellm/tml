#!/usr/bin/env python3
"""Generate self-contained HTML dashboard with all research charts.

Usage: python generate_dashboard.py [--charts charts/] [--output dashboards/index.html]

Embeds PNG charts as base64 inline images — no external dependencies.
"""
import base64, sys
from pathlib import Path

def generate(charts_dir, output_path):
    charts_dir = Path(charts_dir)
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    chart_files = sorted(charts_dir.glob("*.png"))
    if not chart_files:
        print(f"No charts found in {charts_dir}. Run visualize.py first.")
        sys.exit(1)

    html = ['<!DOCTYPE html><html><head><meta charset="utf-8">',
            '<title>MCP Research Dashboard</title>',
            '<style>body{font-family:system-ui;max-width:1200px;margin:0 auto;padding:20px;background:#f5f5f5}',
            'h1{color:#2c3e50}h2{color:#34495e;border-bottom:2px solid #3498db;padding-bottom:5px}',
            '.chart{background:white;border-radius:8px;padding:15px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}',
            '.chart img{max-width:100%;height:auto}',
            '.meta{color:#7f8c8d;font-size:0.85em}</style></head><body>',
            '<h1>MCP Research Dashboard</h1>',
            f'<p class="meta">Generated from {len(chart_files)} charts</p>']

    for chart in chart_files:
        name = chart.stem.replace("_", " ").title()
        with open(chart, "rb") as f:
            b64 = base64.b64encode(f.read()).decode()
        html.append(f'<div class="chart"><h2>{name}</h2>')
        html.append(f'<img src="data:image/png;base64,{b64}" alt="{name}">')
        html.append('</div>')

    html.append('</body></html>')

    with open(output_path, "w") as f:
        f.write("\n".join(html))
    print(f"Dashboard: {output_path} ({len(chart_files)} charts)")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--charts", default="docs/papers/llm-ir-debugging/charts")
    parser.add_argument("--output", default="docs/papers/llm-ir-debugging/dashboards/index.html")
    args = parser.parse_args()
    generate(args.charts, args.output)
