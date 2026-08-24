# 0021 — Six buttons, hold-back blocking, two schemes, input history

**Status:** Accepted
**Date:** 2026-08-24
**Relates to:** `DESIGN.md` §4.1, ADR 0018, ADR 0020
**Supersedes:** the five-button, dedicated-block-button scheme inherited from the
Mortal Kombat II basis dropped by ADR 0018

## Context

ADR 0018 dropped the MK2 mechanical basis and left the control scheme to be
redesigned. That redesign happened, was written into `DESIGN.md` §4.1, and was
marked settled and binding — but no ADR recorded it.

That was a gap, not a judgment. This decision meets the bar in
`docs/decisions/README.md` more clearly than most of the ADRs that do have one:
it is expensive to reverse, and it will be re-argued the moment someone reads
the old framing that survived in four other files.

**What it reaches.** Every replay recording (they encode the old input
semantics), the `MoveId` enum in `src/sim/framedata.h`, the button bits in
`src/sim/input.h`, `docs/framedata_schema.md`, and both character TOMLs. Almost
nothing else in the simulation is affected — movement, jump arcs, hit
resolution, stun, pushboxes and round flow are all mechanically neutral to it.

## Decision

### Six attack buttons

Light, medium and heavy, in punch and kick. Four face buttons plus two
shoulders on a conventional pad — no special hardware, no claw grip.

The old scheme spent five buttons on four attacks plus block. Moving block onto
a direction frees a button and buys a third strength, which is what makes a
move list have *reach* choices rather than just fast and slow.

### Hold back to block

Because *back* is relative to the opponent, an attack that changes sides
mid-animation forces the defender to reverse their input. That is a crossup —
an entire axis of offence that a block button deletes by giving every attack
the same defensive answer.

It also costs something real, which ADR 0020 requires: blocking and retreating
become the same input, so a defender cannot do both, and hold-back blocking has
no startup to punish. That is what puts throws back under consideration.

### Two schemes: Classic and Modern

*Classic* uses all six buttons with motion inputs for specials. *Modern* uses
light/medium/heavy plus a dedicated Special button.

**Modern is a pure input adapter over one canonical action set.** The
simulation holds one set of move properties and never learns which scheme
produced an input. No damage penalty.

The governing rule: **if a scheme removes a capability it needs no tax; if it
grants one, it must be taxed — and the tax is paid in frames, not damage.** A
one-button command grab is strictly better than a motion one, so it costs
startup. A damage penalty is invisible during play and teaches nothing; two
extra startup frames change which situations the move works in, which a player
can see and learn from.

**Auto-combos are scripted canonical inputs**, not authored sequences. They
perform the moves the player would perform manually, in order — so they add no
move data, inherit combo scaling automatically, can never beat the manual route
because they *are* it, and teach that route by performing it.

### Input history in `GameState`

Both schemes need it, and the simulation currently keeps only the current and
previous frame. Classic motion inputs are a pattern across recent frames;
Modern auto-combos need to know where in a sequence the player is.

It must live in `GameState` — a buffer outside it would not survive a rollback
restore, and the input that came out would depend on how many times the frame
had been re-simulated. That is a desync, and it is the exact class ADR 0011's
desync job exists to catch.

**It pays for itself twice:** the same buffer is what makes a slightly early
press still come out, which is a baseline expectation of any modern fighter and
one of the things that reads as *responsive*.

## Consequences

- **The replay tier is invalidated wholesale** when this lands. Expected, and
  re-recorded in the same commit per rule 8 — the recordings encode the old
  input semantics, not a regression.
- **`GameState` grows**, and the 4KB assertion in `state.h` is the budget. An
  input ring of N frames × 2 players × 2 bytes is small; the assertion is there
  to make the cost visible rather than to forbid it.
- **This is a content change as well as an input change**, which the roadmap
  originally under-counted. Medium punch and medium kick do not exist in
  `MoveId`, in the schema, or in either character's data — roughly eight new
  move definitions with hand-authored hitbox geometry. `tools/framedata_editor/`
  is therefore sequenced **before** that authoring, per the stack decision and
  the jump-attack bug that proved the point.
- **`src/sim/**` is a human-led zone (AGENTS rule 1)**, so this ADR records the
  decision; the implementation is a separate, directed change.
- **Horizontal SOCD stays open** (RULESET decision 6). Vertical is settled at
  Up-priority. Last-input-wins for horizontal becomes *possible* once input
  history exists, which is why it was not decidable before.
- **Worth revisiting if** playtesting shows six buttons is more than the
  ninety-second pickup target in `DESIGN.md` §3 can carry. Modern exists partly
  to absorb that, and it is the thing to lean on before cutting a strength.
