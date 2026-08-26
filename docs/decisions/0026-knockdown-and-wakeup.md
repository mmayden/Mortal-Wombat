# 0026 — Knockdown and wakeup: soft and hard, with a timing choice

**Status:** Accepted
**Date:** 2026-08-26
**Relates to:** `DESIGN.md` §3 (the thesis), §4.2, §4.6, ADR 0019, ADR 0020,
ADR 0021, ADR 0022
**Settles:** `drawing-board/RULESET.md` decision 8

## Context

The research calls this the most important structural decision left:

> Knockdown isn't a reward, it's a *state transition into a rigged coin flip* —
> and most of the depth in SF, KOF and anime games lives in that transition.
> **It's the engine. Everything else decorates it.**

Knocking someone down makes landing one hit worth more than the damage it dealt.
Two of the sixteen states in §4.2 — `Knockdown` and `Wakeup` — have been
unreachable since the state set was written, and the v1 definition of done names
them explicitly.

The author researched the field and asked for a choice that matches community
consensus and fits a grounded fighter rather than an air-dash one.

## Decision

### A grounded fighter cannot be hit

No hitting a downed opponent. This is the rule that *creates* the guessing game:
if the attacker could simply keep hitting, there would be nothing to set up.
It is the 2D default the research names in §1.3, and it is consistent with every
other choice this game has made.

### Two kinds of knockdown

**Soft.** The defender chooses when to rise: **quick** or **delayed**. The
attacker keeps the advantage but must cover two timings rather than one.

**Hard.** Fixed timing, no choice. The attacker knows exactly when, and gets the
cleaner setup.

**Hard knockdowns are the reward for a harder thing to land.** That is the whole
reason the split exists: it gives "go for the knockdown" a gradient instead of a
single binary, so a player can commit *more* for a better situation. Which moves
cause which is per-move data, and adding that field is a schema change.

### The rise itself is invulnerable

Invulnerability covers the get-up animation and ends when the fighter is
actionable. Without it a perfectly-timed attack hits a defender who has no
option at all, which is not a coin flip — rigged or otherwise.

### No dedicated wakeup reversal

An invincible get-out-of-jail attack is **the commitment release wearing a hat**,
and ADR 0019 allows one mechanic per role. If a defender should be able to buy
their way out of okizeme, that must *be* decision 10's release, priced the same
as every other use of it. Bolting on a universal reversal inflates the option
count for nothing, which is exactly what decision 15 exists to prevent.

### Rolling is deferred, not rejected

Position choice — rolling to change distance or side — is left out for now. It
adds a second axis on top of the timing one, and decision 15's warning is that
every system multiplies what a new player holds in their head. If playtesting
shows the defender needs more escape, a restricted roll is the first thing to
try.

## Why this, and not simply "what SF6 does"

The author asked for community consensus and a fit with a grounded, SF6-adjacent
game rather than an anime one. Both are good instincts and neither is the reason
this decision is what it is.

**ADR 0019 forbids adopting a mechanic because it is popular.** The test is what
constraint it releases, whether the role is already filled, and whether it serves
§3. Timing choice passes on its own terms: the attacker's commitment still pays,
because they retain advantage and the defender is still guessing — while the
defender makes a decision rather than watching. Fixed timing maximises the payoff
but makes being knocked down passive, and a game whose thesis is *"was that worth
committing to?"* should not have a state where one player has nothing to commit
to at all.

**One factual caution.** "Timing choice" is the Street Fighter IV lineage;
quick rise, with delayed wakeup added in Ultra. Later Street Fighters lean more
on *position* — a back rise that resets spacing — and the exact shape of SF6's
options is not something this project should assert from memory. It does not
change the decision, because the decision rests on the thesis rather than on
copying an implementation, but "this is what SF6 does" is not load-bearing here
and should not be repeated as though it were.

**And position choice would fit this game worse than it looks.** A back rise
returns the defender to neutral, which in a game where neutral is a *distinct
phase* (ADR 0020) hands back most of what the knockdown was worth. Timing choice
keeps the attacker's advantage, which is what makes the commitment that earned it
worth having made.

## Consequences

- **Two unreachable states become reachable**, and a v1 definition-of-done line
  becomes buildable. `FighterState::Blocking` remains the only dead one.
- **The frame-data schema gains a per-move knockdown field** — none, soft, or
  hard — which is a version bump and both character files in the same commit
  (rule 5).
- **`GameState` grows a wakeup-choice field**, and the state machine gains the
  transitions. This is `src/sim`, so it is human-led work (rule 1).
- **Every replay recording is invalidated** when it lands, since knockdown
  changes what happens after a hit. Re-recorded in the same commit (rule 8).
- **Decisions 7 and 13 get easier.** Attack heights and throws are both about
  how offence opens a guard, and okizeme is where they pay off — a knockdown
  loop with no mixup axis is a loop with nothing in it.
- **Worth revisiting if** playtesting shows knockdowns are oppressive, in which
  case the restricted roll above is the lever, or that they are worth too little,
  in which case the soft/hard split is where to look first.
