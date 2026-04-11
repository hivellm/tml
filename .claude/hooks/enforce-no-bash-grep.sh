#!/usr/bin/env bash
# PreToolUse hook: block Bash calls that use grep/rg/head/tail/sed/awk
# or any output-filtering commands. Use dedicated tools instead:
#   - grep/rg → Grep tool
#   - head/tail → Read tool with offset/limit
#   - cat → Read tool
#   - sed/awk → Edit tool
#
# For test output: redirect to .sandbox/test_output.log and Read the file.
set -euo pipefail
input="$(cat)"

result="$(node -e "
const input = JSON.parse(process.argv[1]);
const tool = input.tool_name || '';
if (tool !== 'Bash') { console.log('ALLOW'); process.exit(0); }
const cmd = input.tool_input?.command || '';

// Block grep/rg anywhere in a pipeline
if (/(?:^|\|)\s*(?:grep|rg|egrep|fgrep)\b/.test(cmd)) {
  console.log('DENY_GREP');
  process.exit(0);
}

// Block head/tail used for filtering command output (piped)
// Allow: 'head -5 somefile.txt' (reading a file directly)
// Block: 'some_command | head -5' or 'some_command | tail -20'
if (/\|\s*(?:head|tail)\b/.test(cmd)) {
  console.log('DENY_FILTER');
  process.exit(0);
}

// Block sed/awk for text manipulation (use Edit tool)
if (/(?:^|\|)\s*(?:sed|awk)\b/.test(cmd)) {
  console.log('DENY_SED');
  process.exit(0);
}

// Block cat for reading files (use Read tool)
// Allow: cat with heredoc (cat <<), cat used in git commit
if (/(?:^|\|)\s*cat\b/.test(cmd) && !/<</.test(cmd)) {
  console.log('DENY_CAT');
  process.exit(0);
}

console.log('ALLOW');
" "$input" 2>/dev/null || echo "ALLOW")"

case "$result" in
  DENY_GREP)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"BLOCKED: Do not use grep/rg via Bash. Use the dedicated Grep tool instead."}}'
    ;;
  DENY_FILTER)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"BLOCKED: Do not pipe into head/tail. Instead, redirect output to .sandbox/test_output.log and use the Read tool to inspect it. Example: ./tml.exe test --suite=compiler 2>&1 > .sandbox/test_output.log"}}'
    ;;
  DENY_SED)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"BLOCKED: Do not use sed/awk via Bash. Use the dedicated Edit tool instead."}}'
    ;;
  DENY_CAT)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"BLOCKED: Do not use cat via Bash. Use the dedicated Read tool instead."}}'
    ;;
  *)
    echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow"}}'
    ;;
esac
