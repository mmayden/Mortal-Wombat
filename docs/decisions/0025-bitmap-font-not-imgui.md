# 0025 — A built-in bitmap font for the readout, not Dear ImGui yet

**Status:** Accepted
**Date:** 2026-08-26
**Relates to:** ADR 0012 (support libraries), ADR 0017 (readability is in scope),
`DESIGN.md` §6 (training mode is a v1 requirement)

## Context

A playtester pressed all six attack buttons and reported: *"I don't know the
difference in the inputs or attacks so I don't know what's happening."*

Half of that was the renderer drawing an identical limb for every attack, which
was a defect and is fixed. The other half is not a defect: **a fighting game is
unreadable without numbers.** Startup, active and recovery are the whole
vocabulary of the genre, and none of them were on screen. Frame data nobody can
see is frame data nobody can learn.

`DESIGN.md` §6 already requires this — *"training mode: hitbox display, frame
data readout, input display"* is a v1 line, and only the first third existed.

**ADR 0012 planned Dear ImGui for debug UI, and it has never been integrated.**
So the readout needed either that integration or something smaller. A 5x7
bitmap font already existed in `tools/framedata_viewer`, written because that
tool had the same problem and could not add a dependency either.

## Decision

**Promote the bitmap font to `src/render/font.h` and build the readout on it.
Do not integrate Dear ImGui for this.**

Roughly forty glyphs of five bits by seven rows, drawn as filled rectangles
through the renderer already in use. No new dependency, no new build step,
nothing to configure.

### Why not ImGui

- **It is a dependency, and rule 4 requires asking.** The playtester agreed to
  a readout, not to a library.
- **The job is four lines of text.** ImGui is an immediate-mode UI toolkit with
  windows, docking, input capture and its own render backend. Using it to print
  `HK 8/33 ACTIVE` is not a proportionate trade.
- **ADR 0012 scoped ImGui to *debug UI*.** This is a v1 training-mode
  requirement, closer to a shipped feature than to a developer tool, and the
  distinction matters for what the game eventually ships with.
- **It is easy to reverse.** The readout's text is built by a pure function in
  `readout_text.h` and drawn separately. Replacing the drawing is replacing one
  file; the words and their tests are untouched.

### Why the font is acceptable rather than merely expedient

It is not a font system and does not pretend to be: no kerning, no lowercase, no
Unicode, one size scaled by integers. That is the correct amount of capability
for the job, and it is legible at 480x270, which is the resolution the game
renders at (§4.4).

## Consequences

- **The training-mode line in §6 is complete.** Hitbox display, frame data
  readout and input display, all bound to `F1`.
- **`--debug` starts with the overlay on**, so `--screenshot` can capture it.
  A diagnostic view nobody can photograph is one nobody reviews, and the first
  version of this readout printed straight through the round pips — found by
  screenshotting it, not by reading it.
- **The text is tested and the drawing is not.** `readout_text.h` is
  header-only and SDL-free, so `tests/unit/test_readout.cpp` asserts the words
  against the frame data they claim to describe. A readout that prints the
  *wrong* move is worse than none, because a player believes it.
- **Every switch in it is exhaustive with no default case**, so adding a
  `MoveId` or `FighterState` fails the build. That rule is here because the
  opposite cost three defects in a single day.
- **Revisit when ImGui arrives for real** — a frame-data editor with text entry,
  or a training mode with menus, are jobs this font genuinely cannot do. At that
  point the readout should move over and this file should go. Keeping it *and*
  ImGui would be two text paths, which is worse than either.
