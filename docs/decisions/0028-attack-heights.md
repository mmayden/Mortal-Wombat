# 0028 — Attack heights: mid, low and overhead

**Status:** Accepted
**Date:** 2026-08-26
**Relates to:** `DESIGN.md` §4.6, ADR 0019, ADR 0021, ADR 0026, ADR 0027
**Settles:** `drawing-board/RULESET.md` decision 7

## Context

Hold-back blocking (ADR 0021) was adopted because *back* is relative to the
opponent, which makes crossups an axis of offence. It bought a left/right axis
and nothing else — and on its own that left a hole: **holding back answered
every attack in the game.** Blocking was a decision made once at the start of a
sequence and never revisited.

That hole matters more now than it did last week. ADR 0026 built the knockdown
loop, which the research calls the engine, and a knockdown whose follow-up can
be answered by holding one direction is a loop that opens into an empty room.

Consensus was asked for, and here it is unusually settled — this is one of the
least controversial structures in the genre.

## Decision

**Three heights, and the stance must match.**

| Height | Must be blocked | Assigned to |
|---|---|---|
| **Mid** | standing *or* crouching | everything not listed below — the default |
| **Low** | crouching | crouching kicks, including the sweep |
| **Overhead** | standing | jump attacks |

Guarding is now necessary but not sufficient. Holding back with the wrong stance
is **not** a partial block: the defender is hit, for full damage. Half-blocking
is a mechanic this game does not have and should not acquire by accident.

### On the word "high"

Deliberately absent. It is the most confused term in the genre's vocabulary — in
most games a "high" attack is still blockable crouching — so naming a category
that way would invite exactly the wrong guess from a player who half-remembers
it. Three names, each meaning one thing.

### Why crouching kicks and not crouching punches

The distinction is the limb, not the stance, and this is the convention nearly
every 2D fighter shares. A player arriving from any of them already knows it.
Per ADR 0027 that consensus is a legitimate basis; the §3 check it still has to
pass is below.

### Why jump attacks are overheads

Universal, and structurally necessary: without it, crouch-blocking answers
everything on the ground *and* in the air, and jumping stops being a threat at
all. It is also what makes ADR 0022's grounded defence a real cost rather than a
technicality — you must stand up to deal with a jump, and standing up is what a
sweep is waiting for.

## How it serves the thesis

The thesis is *"was that worth committing to?"* — and a mixup is that question
asked of the **defender**. Choosing a stance is a commitment, made in advance,
that can be wrong.

Before this, the defender had no commitment to make. Holding back was free and
correct against everything, so the attacker's own commitment bought a small
amount of chip time and nothing else. Now an attacker who earned a knockdown has
something to threaten with, and a defender has something to guess at. That is
the same question from both sides of the screen, which is the shape the thesis
is meant to have.

## Consequences

- **The ground mixup is real but thin, deliberately.** Low and overhead exist,
  but the only overhead is the jump attack, which is slow and visible. A fast
  ground overhead is what a *command normal* is for, and there are none yet.
  This is the strongest argument so far for decision 13 (throws): a defender who
  can crouch-block and simply wait still needs an answer, and a throw is the
  conventional one.
- **The frame-data schema gains a per-move `height`** — v4, spelled rather than
  numbered, defaulting to mid.
- **Not one existing replay changed.** Every recorded scenario blocks a *mid*,
  which behaves identically either side of this change. That is a coverage hole
  rather than good news, so `low_vs_standing_block` was added and carries an
  assertion that its sweeps actually land — a recording nobody gets hit in would
  reproduce perfectly and prove nothing.
- **Worth revisiting if** playtesting shows crouch-blocking is simply correct
  most of the time. The lever is a faster overhead, and the honest fix is a
  command normal rather than reassigning an existing move.
