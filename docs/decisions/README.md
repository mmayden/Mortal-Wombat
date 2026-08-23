# Architecture Decision Records

**Settled questions. These do not get relitigated.** The purpose of an ADR is to
stop every session re-deriving the same decision and deriving it differently.

Merged ADRs are **append-only**. If a decision turns out to be wrong, write a
new ADR that supersedes it and say so in both — do not edit the old one. The
record of what was believed, and why, is most of the value.

---

## Index

| # | Decision | Where |
|---|---|---|
| 0001 | Language: orthodox C++, a documented subset | [stack](0001-0013-stack.md) |
| 0002 | **Determinism: mandatory, fixed-point i32.16 at 60Hz** | [stack](0001-0013-stack.md) |
| 0003 | Platform layer: SDL3 | [stack](0001-0013-stack.md) |
| 0004 | Rendering: SDL3 GPU API — *amended by 0016* | [stack](0001-0013-stack.md) |
| 0005 | **Entity model: no ECS, one flat POD `GameState`** | [stack](0001-0013-stack.md) |
| 0006 | Physics: none. AABB overlap, fixed jump arcs | [stack](0001-0013-stack.md) |
| 0007 | Netcode: GekkoNet rollback (deferred, not integrated) | [stack](0001-0013-stack.md) |
| 0008 | Transport: deferred behind an interface | [stack](0001-0013-stack.md) |
| 0009 | **Data format: TOML frame data** | [stack](0001-0013-stack.md) |
| 0010 | Build: CMake + CPM, with a hard compile-time budget | [stack](0001-0013-stack.md) |
| 0011 | **Testing: doctest + replay + cross-platform desync** | [stack](0001-0013-stack.md) |
| 0012 | Support libraries: ImGui, miniaudio, Tracy, tomlplusplus | [stack](0001-0013-stack.md) |
| 0013 | Art pipeline: placeholder boxes first, 3D-to-sprites later | [stack](0001-0013-stack.md) |
| 0014 | The sim boundary is enforced mechanically, not by review | [0014](0014-harness-boundary-enforcement.md) |
| 0015 | Bootstrap order deviation; provisional constants quarantined | [0015](0015-bootstrap-order-and-provisional-constants.md) |
| 0016 | SDL_Renderer for v1, not the GPU API (amends 0004) | [0016](0016-sdl-renderer-for-v1.md) |
| 0017 | Readability work is in scope; art is not (clarifies 0013) | [0017](0017-readability-is-not-art.md) |

ADRs 0001–0013 live in one file because they were decided together, as the
project's founding stack decision. That file also carries the scope tier, the
parallelism map, and the v1 scope statement — everything downstream of the same
sitting. Later ADRs get their own file.

**The four in bold are the ones the architecture actually rests on.** If you
read nothing else, read 0002 and 0005: they are why there is no ECS, no physics
engine, no `float` below `src/sim/`, and no `dt` anywhere in the simulation.
Everything else is replaceable; getting those two wrong is a rewrite.

---

## Writing a new one

Number it after the highest existing ADR and give it its own file. The format
is the one `BLUEPRINT.md` §2.4 specifies — context, decision, consequences —
and the consequences section is the part future readers actually need:

```markdown
# 00NN — One line, in the present tense

**Status:** Accepted
**Date:** YYYY-MM-DD
**Relates to:** / **Amends:** / **Supersedes:**

## Context
What was true that forced a decision. Include what was tried and rejected.

## Decision
What was chosen, stated plainly.

## Consequences
What this costs, what it forecloses, and what will now fail if someone
violates it. Say what would make it worth revisiting.
```

An ADR is warranted when a choice is **expensive to reverse** or will otherwise
be re-argued. A new dependency, a change to `GameState`'s shape, a change to the
`advance_frame` step order, or anything touching determinism all qualify. Adding
a move does not.
