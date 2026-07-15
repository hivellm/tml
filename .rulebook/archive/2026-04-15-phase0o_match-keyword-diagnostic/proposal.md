# Proposal: phase0o_match-keyword-diagnostic

## Why

Developers coming from Rust, Swift, or C# use `match` as muscle memory. In TML the
correct keyword is `when`. Currently, using `match` produces confusing parser noise
(`Expected expression`, `Expected declaration`) with no hint about the correct syntax.
Reported by an external AI agent (UzDB author) as a sharp edge that damages MCP
stability because in older versions it caused an ICE that crashed the subprocess.
The ICE is gone, but the diagnostic quality is still poor — a clear "did you mean
`when`?" message with the correct span would prevent wasted iteration.

## What Changes

- Parser: detect `match` as a keyword alias (currently parsed as an identifier,
  then fails when `{` follows with arms). Emit `E001`-style diagnostic:
  `error[S001]: 'match' is not valid TML — use 'when' instead`.
- The fix should point at the `match` token span and suggest `when`.
- No semantic change — `match` remains unsupported; this is diagnostic-only.

## Impact

- Affected specs: none (diagnostic change only)
- Affected code: `compiler/src/parser/` (expression parser, keyword detection)
- Breaking change: NO
- User benefit: First-time TML users from Rust/Swift/C# get actionable error
  instead of cascading parse noise.
