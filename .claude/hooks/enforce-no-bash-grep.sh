#!/usr/bin/env bash
# PreToolUse hook: block Bash calls that run grep/rg — use the Grep tool instead.
set -euo pipefail
input="$(cat)"

result="$(node -e "
const input = JSON.parse(process.argv[1]);
const tool = input.tool_name || '';
if (tool !== 'Bash') { console.log('ALLOW'); process.exit(0); }
const cmd = input.tool_input?.command || '';
// Strip leading whitespace and check if the command starts with or pipes into grep/rg
// Also catch: grep, rg, egrep, fgrep, and common patterns like '| grep', '| rg'
if (/(?:^|\|)\s*(?:grep|rg|egrep|fgrep)\b/.test(cmd)) { console.log('DENY'); process.exit(0); }
console.log('ALLOW');
" "$input" 2>/dev/null || echo "ALLOW")"

case "$result" in
  DENY)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"BLOCKED: Do not use grep/rg via Bash. Use the dedicated Grep tool instead."}}'
    ;;
  *)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow"}}'
    ;;
esac