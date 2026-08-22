# 0015 — Bootstrap order taken out of sequence; provisional constants quarantined

**Status:** Accepted
**Date:** 2026-08-22
**Relates to:** ADR 0002, ADR 0009, ADR 0011, the stack decision's bootstrap order and scope tier

## Context

Three findings from reviewing the Phase 0 + Phase 1 work against the design
docs. All three are recorded together because they share a cause: the harness
needed the simulation to *do something* before the game had a window, and each
is a place where that pressure pushed past what the documents specify.

### 1. The bootstrap order was not followed

The stack decision ends with:

> Then bootstrap in order: `Fixed` type → `GameState` struct → fixed-timestep
> loop → input-as-data → one fighter rendering → one fighter moving → hitbox
> overlap → the first hit that connects.

What was actually built: `Fixed`, `GameState`, input-as-data, and **one fighter
moving** — skipping the fixed-timestep loop and rendering entirely. Movement,
facing resolution, and the round-flow state machine (~130 lines of `sim.cpp`)
landed two steps early.

This also sits awkwardly against the parallelism map, which says "the sim is a
human-led zone permanently," and against `AGENTS.md` rule 1.

### 2. Six constants were invented

`ROUND_START_X_P1`/`P2`, the stage edge bound, the two round-freeze durations,
and `MAX_PROJECTILES` appear in no design document. `DESIGN.md` §10 and §5.5
are explicit that undecided things must not be invented around. Worse, the
header of `constants.h` claimed all its values came from `DESIGN.md` §4.4,
which was false for these six — a reader had no way to tell a transcribed
number from a guessed one.

### 3. Two documents disagreed about a perf gate

`BLUEPRINT.md` §1.3 step 5 asks the smoke test to assert frame time stayed
under budget. The stack decision's "Explicitly cut from Medium" list says "No
CI perf gates yet." The smoke test was written following the blueprint.

## Decision

**1. Keep the early sim code; record the deviation.** It is correct, tested,
covered by replays, and matches `DESIGN.md` §4.2 and §4.4 exactly. Reverting it
to rebuild the same code in a different order would be ceremony, and
`BLUEPRINT.md` warns specifically against process that displaces shipping.

What the deviation actually costs is recorded here rather than left implicit:
the movement and round-flow code has never been *played*, only tested. The
sixty-second manual play after every merge — which `BLUEPRINT.md` calls the
primary regression gate for feel — has not happened and cannot happen until
there is a window. Every feel-bearing number in that code is therefore
unvalidated, which is exactly why the constants below are quarantined.

The remaining bootstrap steps run in the documented order from here: the
fixed-timestep loop and rendering come next, before hitbox overlap.

**2. Split `constants.h` into a cited half and a `PROVISIONAL` half.** Values
transcribed from `DESIGN.md` §4.4 cite their source. Values with no source sit
in their own block, each labeled with what still needs deciding and why. The
false file header is corrected.

Where invention was unavoidable it is now minimized rather than hidden: the two
round-start positions became one `ROUND_START_SEPARATION` derived against
`STAGE_WIDTH`, and the stage bound carries a note that it should not be a
constant at all — the real bound comes from a per-character pushbox that ADR
0009 puts in TOML.

**3. The stack decision wins on the perf gate.** It is project-specific and was
written later; `BLUEPRINT.md` is the generic process document. The smoke test
still measures and prints per-frame timing, but asserts nothing about it. A
wall-clock assertion on a shared CI runner is a flake generator, and a test
that goes red for reasons unrelated to the code teaches people to ignore red.

## Consequences

- `PROVISIONAL` in `constants.h` is the project's list of open engine
  decisions. It should shrink, never grow. A new value belongs there only if it
  genuinely has no design source, and adding one is a signal that a design
  question is being deferred rather than answered.
- Every provisional value is already baked into committed replay checkpoint
  hashes. Changing one requires re-recording — which is the intended workflow
  (AGENTS.md rule 8), not an obstacle.
- Two provisional values (`ROUND_START_SEPARATION`, the freeze durations) are
  pure feel and cannot be validated until the game is playable. They are the
  first things to revisit once it is.
- `STAGE_EDGE_MARGIN` and `MAX_PROJECTILES` are both expected to be *deleted*
  rather than tuned — the first when the character loader supplies real
  pushboxes, the second if the v1 special turns out not to spawn a projectile
  (`DESIGN.md` §4.5 is ambiguous; §4.6 does not settle it).
- Timing regressions in the sim will not fail CI. Tracy under the profile
  preset is the intended instrument, per ADR 0012.
- The reviewer subagent gains a checklist item: does this diff add a constant
  with no design-document source, outside the `PROVISIONAL` block?
