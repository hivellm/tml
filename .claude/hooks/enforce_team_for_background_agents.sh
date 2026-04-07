#!/usr/bin/env bash
# PreToolUse hook for the Agent tool.
#
# Policy:
#   - A single Agent call is fine (foreground or background).
#   - Spawning multiple standalone Agents for parallel work is FORBIDDEN.
#     Parallel multi-agent work MUST go through a Team so agents can
#     communicate via SendMessage.
#
# Enforcement strategy:
#   Block any Agent invocation that uses `run_in_background: true` UNLESS
#   it targets `team-lead` or uses the `team_name` field. This forces
#   background parallel work to be coordinated through a team.
#
# The hook reads the tool input JSON from stdin and emits a permission
# decision JSON on stdout.

set -euo pipefail

input="$(cat)"

tool_name="$(printf '%s' "$input" | jq -r '.tool_name // empty' 2>/dev/null || true)"
if [[ "$tool_name" != "Agent" ]]; then
  # Not our concern.
  printf '%s' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow"}}'
  exit 0
fi

run_in_background="$(printf '%s' "$input" | jq -r '.tool_input.run_in_background // false' 2>/dev/null || echo false)"
subagent_type="$(printf '%s' "$input" | jq -r '.tool_input.subagent_type // empty' 2>/dev/null || true)"
team_name="$(printf '%s' "$input" | jq -r '.tool_input.team_name // empty' 2>/dev/null || true)"

# Foreground single agent → always allowed.
if [[ "$run_in_background" != "true" ]]; then
  printf '%s' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow"}}'
  exit 0
fi

# Background agents must be part of a team OR be the team-lead coordinator.
if [[ "$subagent_type" == "team-lead" ]] || [[ -n "$team_name" ]]; then
  printf '%s' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow"}}'
  exit 0
fi

# Block standalone background agent.
reason="POLICY VIOLATION: Spawning standalone background Agents is forbidden. Multi-agent parallel work MUST use a Team so agents can communicate via SendMessage. Either (a) use TeamCreate to create a team, (b) spawn a team-lead that creates the team, or (c) set team_name on the Agent call. See CLAUDE.md section 'Multi-Agent Work MUST Use Teams'."

# Emit a deny decision with a user-visible reason.
jq -nc --arg reason "$reason" '{
  hookSpecificOutput: {
    hookEventName: "PreToolUse",
    permissionDecision: "deny",
    permissionDecisionReason: $reason
  }
}'
exit 0