# 0027 — Popular consensus is a legitimate input, not a shortcut

**Status:** Accepted
**Date:** 2026-08-26
**Relates to:** ADR 0019, ADR 0020, ADR 0026
**Clarifies:** ADR 0019

## Context

ADR 0019 says *"adopt properties and structure, do not adopt mechanics"*, and
warns that a beloved mechanic is loved as an answer to a constraint its own game
created. That is sound and it stands.

It has been read more strictly than it was written. ADR 0026 stated that the
author asking for community consensus was *"not the reason"* the decision came
out as it did, on the grounds that 0019 forbids adopting anything because it is
popular. The author corrected this directly: consensus is what a great deal of
this project is deliberately based on.

They are right, and the misreading is worth closing before it recurs — the same
caveat had already been written twice.

## Decision

**What the wider fighting-game community has converged on is a legitimate and
valuable input to a design decision here.** It is evidence: thousands of players
across decades have already run the experiment this project cannot afford to
run, and ignoring the result in favour of reasoning from first principles is
worse engineering, not purer engineering.

ADR 0019's actual constraint is narrower than "popularity is not a reason", and
survives intact:

- **Consensus is a strong prior, not a conclusion.** A mechanic still has to
  answer what constraint it releases here, whether its role is already filled,
  and whether it serves `DESIGN.md` §3.
- **Pricing does not travel.** The structure of a loved mechanic is portable;
  the numbers that made it work in its own game are not.
- **One mechanic per role, still.** Two admired things doing the same job
  divide the importance of that job between them.

So the order is: start from what the field has settled on, then check it against
the thesis. Where they agree, that is the answer and the reasoning is short.
Where they disagree — as with SF6's continuous neutral in ADR 0020 — the thesis
wins, and *that* is the case ADR 0019 was written for.

## Consequences

- **"The community landed here" is a sufficient opening argument** and does not
  need apologising for. It still needs the §3 check, which is usually a
  paragraph rather than an essay.
- **ADR 0020 is unaffected.** Rejecting SF6's continuous neutral was never
  about disregarding its popularity; it was a case where the evidence and the
  thesis genuinely pointed different ways, and the ADR says so at length.
- **This does not license copying.** Taking a mechanic's *pricing* because it is
  popular is the failure ADR 0019 names, and remains forbidden.
- **Worth revisiting if** decisions start arriving with consensus cited and no
  §3 check attached. The check is the part that keeps the game a specific game
  rather than an average of other games.
