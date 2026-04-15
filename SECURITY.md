# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.3.x   | Yes       |
| 0.2.x   | Security fixes only |
| < 0.2   | No        |

## Reporting a Vulnerability

If you discover a security vulnerability in TML, please report it responsibly.

**Do NOT open a public GitHub issue for security vulnerabilities.**

### How to Report

1. Email: Open a private security advisory via [GitHub Security Advisories](https://github.com/hivellm/tml/security/advisories/new).
2. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Impact assessment
   - Affected versions
   - Any suggested fix (optional)

### What to Expect

- **Acknowledgment**: Within 48 hours of your report.
- **Assessment**: We will evaluate the severity and impact within 5 business days.
- **Fix timeline**: Critical vulnerabilities will be patched within 7 days. High-severity within 14 days. Medium/low within the next release cycle.
- **Disclosure**: We will coordinate with you on public disclosure timing. We follow a 90-day disclosure policy.
- **Credit**: Security researchers will be credited in the release notes unless they prefer anonymity.

## Scope

The following are in scope:

- The TML compiler (`tml.exe` / `tml`)
- The TML standard library (`lib/core/`, `lib/std/`)
- The MCP server (`tml mcp`)
- The compilation daemon (`tml daemon`)
- Build scripts and CI/CD pipelines

The following are **out of scope**:

- Third-party dependencies (report to their maintainers directly)
- Vulnerabilities in user-written TML programs
- Issues requiring physical access to the machine

## Security Best Practices for TML Users

- Keep your TML installation up to date.
- Use `--release` builds for production (includes additional hardening).
- The compilation daemon (`tml daemon`) listens only on local named pipes / Unix domain sockets and is not network-accessible.
- Review third-party packages before adding them to your project.
