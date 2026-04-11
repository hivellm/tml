# Never pipe Bash output through grep, head, tail, sed, awk, or cat

## Rule

Do NOT use filtering commands to parse Bash output. Instead:

| Blocked | Use instead |
|---------|-------------|
| `cmd \| grep pattern` | `Grep` tool on the output file |
| `cmd \| head -N` | Redirect to `.sandbox/*.log`, then `Read` with `limit` |
| `cmd \| tail -N` | Redirect to `.sandbox/*.log`, then `Read` with `offset` |
| `cat file` | `Read` tool |
| `cmd \| sed ...` | `Edit` tool |
| `cmd \| awk ...` | `Edit` tool |

## Test output workflow

When running tests or builds that produce long output:

```bash
# Step 1: Run and save output
./build/debug/bin/tml.exe test --suite=compiler 2>&1 > .sandbox/test_output.log

# Step 2: Read the results with the Read tool
Read(.sandbox/test_output.log)
```

Never `| head -20` or `| tail -10` to truncate test output — it discards
critical information and wastes a tool call when the result is incomplete.

## Why

1. `head`/`tail` silently discard output — errors at the beginning or end are missed
2. `grep` on Bash output is unreliable — the Grep tool handles regex, context, and pagination
3. Redirecting to a file creates a persistent artifact that can be re-read without re-running
4. The Read tool supports `offset` and `limit` for efficient navigation of large files
