# io_uring Risks and Mitigations

## 1. Security CVEs (High Severity)

io_uring has the highest CVE rate of any Linux networking subsystem since 2019.

### Representative CVEs

| CVE | Year | Type | Description |
|---|---|---|---|
| CVE-2022-29582 | 2022 | UAF / privilege escalation | Use-after-free in deferred cleanup path |
| CVE-2023-2598 | 2023 | UAF | Fixed-buffer registration race condition |
| CVE-2023-21400 | 2023 | Privilege escalation | io_uring worker thread privilege escape |
| CVE-2024-0582 | 2024 | UAF | Ring buffer memory corruption |

The root cause pattern: io_uring's deferred completion model makes kernel reference counting  
harder to reason about. When operations complete asynchronously, the kernel must track object  
lifetimes across an async boundary — a historically fertile ground for UAF bugs.

### Industry Response

- **Google**: Disabled io_uring system-wide in ChromeOS and Android (2023). Remains disabled in 2026.  
  Quote from Android kernel team: *"io_uring has resulted in a constant stream of security vulnerabilities,  
  including privilege escalation bugs, and it is too risky to enable on Android."*
- **Android kernel**: `CONFIG_IO_URING=n` by default. Explicitly rejected for inclusion.

### Mitigation for TML

- io_uring runs at userspace process privilege — not a higher-privilege context  
- An HTTP server using io_uring has the same kernel exposure as any other server using the feature  
- The CVE risk is primarily relevant for privilege-separated server contexts (e.g., browser sandboxes)  
- Keep liburing up to date — kernel patches the vulnerabilities; liburing exposes new safe APIs  
- Monitor kernel advisories; pin minimum kernel version to one with known fixes applied

**Risk level for TML**: Medium (not elevated vs other native HTTP servers using io_uring)

---

## 2. Container / Seccomp Restrictions (High — Most Production Environments)

### Docker Default Seccomp Profile

Docker 25.0 (2023) explicitly added a rule to **block** `io_uring_setup`, `io_uring_enter`,  
and `io_uring_register` in the default seccomp profile (PR #46762, moby/moby).

Issue #47532 ("allow io_uring in default profile") was closed as **"not planned"** in 2024.  
No change in 2025–2026. io_uring remains blocked in Docker default.

### Kubernetes

- RuntimeDefault seccomp profile inherits Docker's block
- Pod Security Admission does not address io_uring explicitly  
- Containers need explicit annotation: `securityContext.seccompProfile.type: Unconfined`  
  or a custom seccomp profile that allows the three io_uring syscalls

### Serverless / Cloud Functions

- AWS Lambda: blocked
- Google Cloud Run: blocked
- Azure Container Instances: blocked

io_uring is fundamentally incompatible with the serverless execution model  
due to the kernel ring setup requiring persistent kernel state across requests.

### Workarounds

**Option A**: Custom seccomp profile (minimal change)
```json
{
  "names": ["io_uring_setup", "io_uring_enter", "io_uring_register"],
  "action": "SCMP_ACT_ALLOW"
}
```
Pass via `--security-opt seccomp=custom-profile.json` in Docker.  
In Kubernetes: mount the profile as a ConfigMap and reference in PodSpec.

**Option B**: `securityContext.seccompProfile.type: Unconfined`  
Disables seccomp entirely — not recommended for production.

**Option C**: Runtime detection with epoll fallback (recommended for TML)  
`tml_iouring_create()` attempts `io_uring_setup()` — if it fails (EPERM or ENOSYS),  
silently falls back to epoll. No user configuration required.

### Confirmed Working (User Test, 2026-04-10)

The user confirmed that Swoole + liburing **ran successfully in Docker**.  
This means either: custom seccomp, `--privileged`, `seccomp=unconfined`, or Docker Desktop/WSL2  
(which may use a different seccomp profile than Docker Engine on bare Linux).

**In production K8s**: assume blocked unless the seccomp profile is explicitly modified.

---

## 3. Kernel Version Fragmentation

| Distribution | Kernel | io_uring HTTP (5.6+) | Multishot (5.19+) | Zero-copy send (6.0+) |
|---|---|---|---|---|
| RHEL 8 / CentOS 8 | 4.18 | **No** | No | No |
| Ubuntu 20.04 LTS | 5.4 | **No** | No | No |
| RHEL 9 | 5.14 | **Yes** (basic) | **No** | No |
| Ubuntu 22.04 LTS | 5.15 | **Yes** (basic) | **No** | No |
| Debian 12 | 6.1 | **Yes** | **Yes** | No |
| Ubuntu 24.04 LTS | 6.8 | **Yes** | **Yes** | **Yes** |

**Practical impact for TML**:
- Ubuntu 22.04 LTS (most common CI/CD target): basic io_uring only, no multishot  
- Ubuntu 24.04 LTS (growing adoption in 2026): full feature set  
- RHEL/CentOS 8: no io_uring at all — epoll fallback mandatory

Mitigation: runtime probe with graceful degradation (see implementation plan).

---

## 4. Buffer Ownership Complexity

io_uring requires that buffers remain valid (not freed, not moved) from SQE submission  
until the CQE arrives. This is the same constraint as IOCP (`WSARecv` buffers must stay valid).

For TML (which already handles this correctly in `iocp_worker.tml`), this is not a new problem.  
The per-worker recv buffer that is pre-allocated and reused across requests already satisfies this.

Languages with GC or ref-counting (Go, Python, Java) have more difficulty here.  
TML's manual memory model matches what io_uring expects.

---

## Risk Summary

| Risk | Severity | Mitigated? |
|---|---|---|
| Security CVEs (kernel attack surface) | Medium | Partially — keep kernels patched |
| Docker/K8s seccomp block | High | Yes — runtime fallback to epoll |
| Kernel version fragmentation | High | Yes — runtime probe + fallback |
| Buffer ownership complexity | Medium | Yes — same as IOCP, already handled |
| API complexity | Low | Yes — liburing 2.14 fully documented |
| musl/Alpine incompatibility | None | Fixed in liburing 2.11 |

The seccomp and kernel version risks are both mitigated by the same mechanism:  
**runtime detection with automatic epoll fallback**.  
This is what Swoole, monoio, and glommio all do.
