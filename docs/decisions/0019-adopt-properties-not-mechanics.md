# 0019 — Adopt properties, not mechanics

**Status:** Accepted
**Date:** 2026-08-24
**Relates to:** `DESIGN.md` §3 (feel — the anti-drift anchor), ADR 0018

## Context

The stated goal is to take what players value most in Street Fighter 6 and
comparable games and build with it. The instinct is right — learning from what
works is the job — but the direct form of it has a failure mode both research
documents in `drawing-board/` identify by name, and it needs a rule rather than
vigilance.

**The failure mode.** A beloved mechanic is loved as an *answer to a constraint
its own game created*. Move it to a game without that constraint and it answers
nothing. Assemble several and they compete for the same job, so none of them
feels important — a game full of admired mechanics that plays like nothing in
particular.

**SF6 is the clearest case available, and it is our own case.** Its most-praised
mechanic and its most-complained-about mechanic are the same system. The
research describes the real complaint about Drive Rush not as raw strength but
as compressing neutral and offence into one action, so that a whiffed poke and a
full mixup are one input apart — SF6 deliberately treating neutral as a
continuous surface rather than a distinct phase.

`DESIGN.md` §3 asks for the opposite in as many words: *slow, readable,
commitment-based neutral*. Not a weaker version of SF6's answer — the other
answer.

So "adopt what SF6 players like most" is not a coherent target here. Its central
system is admired, is the top complaint, and implements the premise this game
rejects.

## Decision

**Adopt properties and structure. Do not adopt mechanics.**

Anything considered for adoption is sorted into one of three buckets, and the
bucket decides how freely it is taken.

### Take freely — tooling and presentation

Training mode, frame-data display, replay review, input display, netcode
quality, onboarding. These are widely and uncontroversially valued, and they
**do not compete for a role in the design**. Nothing about excellent training
tools constrains what the game is.

This is the cheapest value available and it should be taken without hesitation.

### Take the principle, never the instance — system structure

A single multi-use resource with genuine opportunity cost and a punishment state
for overspending is a good *structure*. Its pricing is not portable, because
pricing is what makes or breaks it.

The governing law, from the research:

> **A multi-use resource is only as deep as the gap between its best and
> second-best use.**

If one spend is right most of the time, there is no decision — there is one
answer and some exceptions. That is a continuous maintenance obligation, and
with no live-service patch cadence and one developer, it argues for **fewer
uses**, not more, so the gap stays auditable by hand.

### One per role, chosen for fit — mechanics

One active defence. One commitment release. One resource. A second mechanic in
an occupied role does not add depth; it divides the importance of the role
between two things.

Before any mechanic is adopted it must answer:

1. **What constraint does it release?** If this game has not created that
   constraint, the mechanic answers nothing here.
2. **Is that role already filled?** If so, the question is replacement, not
   addition.
3. **Does it serve `DESIGN.md` §3?** A mechanic that makes neutral faster or
   less readable is working against the anchor regardless of its pedigree.

The nine-property checklist in `drawing-board/2d-fighter-mechanics-deep-dive.md`
§4 is the portable part of any loved mechanic — counterplay, contested pricing,
legible availability, legible failure, creating a decision rather than resolving
one. **Those properties travel. The mechanics do not.**

## Consequences

- "People love X, can we have X?" is answerable without re-arguing the design:
  which bucket, which role, which constraint.
- What this project takes from SF6 is concrete and already large: rollback, an
  accessible control scheme, best-in-class practice tooling, a unified resource
  *structure*, and a punish state for overspending.
- What it does not take is equally concrete: the specific pricing that made one
  spend dominant, and the choice to make neutral continuous.
- This rule cannot be applied without a thesis. "Does it serve §3" is a real
  test only if the game knows what it is asking the player. The thesis remains
  the open decision that gates the rest.
- Being unable to name the constraint a mechanic releases is sufficient reason
  to leave it out.
