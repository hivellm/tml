# 04 — Self-Hosting Reality Check and Strategic Misalignment

## F-009 — The self-hosted compiler (`compiler-tml`) cannot compile a real input; the native-backend roadmap is gated on it

**Confidence: High. Impact: High.**

The C-frontend (`compiler-tml/src/cc/`) — TML code compiling C — cannot get `essential.c`
(1465 lines) through even `--emit=ast`: **0/5 across every phase from 24a to 24n**
(`.rulebook/tasks/phase24m_essential-c-residual-segv`, CHANGELOG 0.3.51/0.3.52).

This matters beyond self-hosting pride: the C-frontend is **TML exercising TML's own memory
model at application scale** — a parser with nested `Shared[CExpr]`/`Shared[CStmt]`/`HashMap`
type environments, structurally identical to a database's row/index graphs. Its failure is
the most honest available proxy for "can you build a real app in TML," and the answer today
is **no**.

The tasks board (`phase31*`–`phase34*`) plans to make the native (self-hosted) compiler the
default backend. That entire track is gated on a defect class the team has not been able to
close in 14 phases.

## F-014 — Roadmap ambition dramatically exceeds foundation readiness

**Confidence: High. Impact: Strategic.**

`.rulebook/tasks/` contains ~40 phases including:

- `phase35a_db-mongodb`, `phase35b_db-redis-mysql`
- `phase36a_ia-cuda`, `phase36c_ia-inference`, `phase36d_ia-lora-finetune`
- `phase37a_http-performance`
- `phase38a_package-manager`, `phase38d_cross-compilation`

Meanwhile the project is pinned in phase24 fighting double-frees. Building MongoDB drivers,
CUDA inference, and LoRA fine-tuning on a runtime that cannot reliably free a `HashMap` is
inverted prioritization: **every one of these features will inherit F-001..F-008 the moment
it stores user data in a collection.** Each feature shipped now is future rework — it will be
written against band-aid APIs (`get_clone`, manual `.duplicate()`) that must be reverted when
the real fix lands.

The corrective move is to freeze the feature phases and land a memory-model + codegen-verifier
milestone first (file 06, Phases A–C).

## Collateral tech debt from the band-aid campaign

The phase24 workaround campaign has left the C-frontend salted with:

- `into_raw()`/`from_raw()` aliasing hacks (CHANGELOG 0.3.41/0.3.42)
- A deliberate leak: `declarator_name_value_leak` (0.3.44)
- ~70 hand-placed `.duplicate()` calls (0.3.51)
- Dual read APIs (`get` vs `get_clone` vs `get_ref`) whose correct choice depends on
  understanding the unsoundness being worked around (0.3.52)

When the real fix lands, **all of this must be reverted** (phase24g's plan already anticipates
this). Until then it is negative-value code: it obscures where the genuine defects are, and it
teaches any future TML author the wrong idioms.
