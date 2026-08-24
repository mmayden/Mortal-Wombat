# 0020 — The thesis: "Was that worth committing to?"

**Status:** Accepted
**Date:** 2026-08-24
**Relates to:** `DESIGN.md` §3 (feel — the anti-drift anchor), ADR 0018, ADR 0019

## Context

ADR 0019 ruled that mechanics are adopted by role and must serve `DESIGN.md` §3.
It closed by naming its own precondition: *"This rule cannot be applied without a
thesis. 'Does it serve §3' is a real test only if the game knows what it is
asking the player."*

Ten mechanical decisions were blocked behind that. The instruction was to choose
by popular consensus.

**Consensus does not resolve this directly, and saying so is part of the
decision.** The fork is whether neutral is a *distinct phase* — a state of the
match you must pay to leave — or a *continuous surface* where neutral and
offence are one input apart.

- **Commercial popularity points at continuous.** Street Fighter 6 is the
  best-selling modern fighting game and chose that branch deliberately.
- **Player sentiment about that exact mechanic points the other way.** The
  research in `drawing-board/` records Drive Rush as the most consistently named
  complaint, and the objection is specifically that it compresses neutral and
  offence into one action. The games the community holds up as best-designed
  skew phase-based.

Attributing SF6's success to its most-complained-about system reads the wrong
signal. What that research praises without reservation is its netcode, training
tools, onboarding and presentation — all of which ADR 0019 already puts in the
"take freely" bucket, and none of which depend on this fork.

**The phase-versus-surface binary was also partly false, and the research says
so.** It describes Roman Cancel and Drive Rush as the same idea with a different
bill. Both are commitment releases. Roman Cancel is rated S+ and is beloved as a
general-purpose *buy out of your commitment* that players find their own uses
for; Drive Rush is resented.

So consensus is not against releasing commitment. It is against **underpricing**
it — the failure mode ADR 0019 already quotes as law: a multi-use resource is
only as deep as the gap between its best and second-best use.

## Decision

**The thesis is: *"Was that worth committing to?"***

Every exchange asks it. The player chose a button, at a distance, at a moment,
and lives with it.

Two things follow, and they are binding:

1. **Neutral is a distinct phase.** Leaving it is a decision with a price, and
   leaving it wrongly is punished.
2. **There is exactly one commitment release, and it is expensive.** General
   purpose rather than situational, so players find their own applications —
   the property that makes the loved version loved — but priced so that buying
   out of a mistake is itself a real commitment.

This overrules nothing. `DESIGN.md` §3 already asked for a slow, readable,
commitment-based neutral; the thesis names what that neutral is *for*.

## Consequences

- **The blocked decisions unblock, and several are simply answered.** Active
  defence takes the Just Defend / instant block family, not a parry — a tighter
  window on an action already being performed, whose failure state is *having
  blocked* rather than *being hit*, which also degrades far better under
  rollback. One commitment release, not one per situation. Three meter uses
  rather than five, so the best/second-best gap stays auditable by one person
  with no patch cadence. A hard combo cap: a game about whether a commitment was
  worth it cannot let one commitment end the round.
- **A mechanic that lets a player escape a bad commitment cheaply is now a
  design error**, not a balance number — regardless of pedigree. That is the
  test ADR 0019 could not previously apply.
- **This forecloses the SF6 branch deliberately.** Players who want neutral and
  offence to be one continuous surface will not want this game, and that is the
  intended outcome of choosing rather than splitting the difference.
- **Worth revisiting if** playtesting shows commitment reads as *stiff* rather
  than *weighty* — the failure mode of this branch is a game where nobody wants
  to press anything. `DESIGN.md` §3's feel target and the playtest question "is
  attacking risky enough?" are the early warning, in both directions.
