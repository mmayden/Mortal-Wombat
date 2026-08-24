# 0022 — Grounded defence and one fighter per side

**Status:** Accepted
**Date:** 2026-08-24
**Relates to:** `DESIGN.md` §3, §4.3, ADR 0018, ADR 0020, ADR 0021

## Context

Two rules were settled during the redesign and then specified nowhere. They
appeared only as reasoning in `drawing-board/RULESET.md` — no ADR, and no line
in `DESIGN.md` stating the rule itself.

They surfaced while applying ADR 0023 — *the ADR owns the why once a decision
graduates* — because trimming the drawing board would have deleted the only
record of either. That is the failure mode the ownership map exists to prevent,
caught before it cost anything.

Both are **root axes**: they decide which family of fighting game this is, and
almost everything else is downstream of them. Neither is expensive to implement,
which is exactly why neither got written down — nothing forced the question.

## Decision

### A fighter in the air cannot block anything

Jumping is a committed gamble. That makes anti-airs matter, which makes ground
spacing the centre of the game — which is what `DESIGN.md` §3 asks for and what
ADR 0020's thesis requires. A jump you can defend out of is not a commitment.

**The alternative is expensive in a way that is easy to miss.** Air blocking
removes the high/low game in the air, because there is no standing or crouching
state up there. The design then immediately owes the game a replacement mixup
axis, and that is where air dashes, super jumps and eventually assists come
from. It is a different genre, not a setting.

**Forecloses:** the anime / air-dasher family, deliberately.

### One fighter per side

No tag, no assists, no partner. It keeps `GameState` as it is and it keeps the
system count low.

The argument that decides it is that **execution accessibility and system
accessibility are different problems, and system layers multiply rather than
add.** Removing hard inputs does nothing for a player who cannot answer "what
should I be doing right now" — and each additional system multiplies the
pairwise interactions that player has to hold in their head. §3's ninety-second
onboarding target is the same conclusion reached from the other direction.

**Forecloses:** tag and assists as a v1 feature. Not permanently — a tag mode
could arrive later — but as a *root* axis this is fixed, and anything built
before then may assume two fighters and exactly two.

## Consequences

- **Neither costs anything to implement.** Fixed jump arcs already fit grounded
  defence and there is no air blocking to remove; `GameState` already holds
  exactly two fighters. The cost of both decisions is entirely in what they
  foreclose, which is why they needed recording rather than building.
- **`GameState`'s two-fighter array is now a design commitment**, not an
  implementation convenience. Code may assume it.
- **Decision 7 (attack heights and stance blocking) inherits from this.** The
  high/low axis has to work on the ground, because there is no air defence for
  it to interact with.
- **Worth revisiting only as a mode, never as a default.** The rule from the
  CvS2 precedent recorded in `drawing-board/RULESET.md` applies: a selectable
  mode may vary leaf axes; root axes are the game. These are root axes.
