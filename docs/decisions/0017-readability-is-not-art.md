# 0017 — Readability work is in scope; art is not

**Status:** Accepted
**Date:** 2026-08-23
**Clarifies:** ADR 0013 (art pipeline: boxes first, 3D-to-sprites later)

## Context

ADR 0013 is deliberately blunt: v1 renders colored rectangles, and *"do not
propose replacing boxes with art before the v1 definition of done in DESIGN.md
§6 is met."* That rule exists because committing to an art pipeline before the
engine works is the most common way a solo fighting game dies.

Read strictly, it also forbids making the game easier to *see* — and that
reading has already cost something. Fighter state was invisible for weeks:
blocking, attacking, hitstun and blockstun all rendered identically to idle, so
pressing block produced no visible change and the game read as ignoring input.
It was reported as a bug. It was a rendering gap.

The question keeps arriving as "can we make it look better", which conflates two
different activities with very different risk.

## Decision

**Readability work is in scope at any time. Art is not, until §6 is met.**

The distinction is not how good it looks. It is what the work commits us to.

**In scope** — cheap, reversible, no pipeline, no external assets:

- Distinguishing states by colour, shape, or a drawn limb
- Hit and block feedback: flashes, shakes, pauses
- Stage legibility: floor, backdrop, depth cues, wall markers
- Anything drawn from `GameState` with primitives already available
- Debug and training-mode overlays

**Out of scope** — pipeline decisions, deferred to ADR 0013:

- Sprite sheets, texture atlases, or any imported image asset
- Blender, rigs, animation, the 3D-to-sprites path
- Fixed sprite dimensions, or anything that pins character proportions
- Any renderer change that assumes art exists

## Rationale

The three reasons this is not a loosening of 0013:

**Feel cannot be tuned through a display that shows nothing happening.**
`DESIGN.md` §3 makes feel the anti-drift anchor, and `BLUEPRINT.md` makes the
manual play session the primary regression gate for it. Both are worthless if
the player cannot see what the simulation did.

**Readability failures are indistinguishable from bugs.** Every rendering gap
so far has been reported as broken behaviour, and each cost a diagnosis. The
jump attack's missing limb, its hidden hitbox in the debug overlay, and the
camera dragging a stationary opponent were all *seen* before they were
understood.

**None of it forecloses anything.** Every item in the in-scope list is deleted
by replacing one manifest implementation (ADR 0013 already requires the renderer
to draw from a swappable manifest and to assume nothing about rectangles). The
out-of-scope list is where the irreversible choices live.

## Consequences

- "Make it look better" is answerable without reopening ADR 0013: readability,
  yes; assets, no.
- The v1 definition of done is unchanged. This does not license art.
- The placeholder fighter stays a box. It may become a *more legible* box —
  clearer state, better feedback — but its proportions remain provisional until
  `DESIGN.md` §5.4 settles the cast, because body shape drives hitbox geometry.
- If a piece of readability work starts requiring an imported asset or a fixed
  sprite size, it has crossed into ADR 0013's territory and stops.
- Revisit when §6 is met, at which point 0013's pipeline decision takes over and
  this ADR is spent.
