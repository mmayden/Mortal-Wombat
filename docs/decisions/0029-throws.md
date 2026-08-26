# 0029 — Throws: unblockable, short, and not tech-able

**Status:** Accepted
**Date:** 2026-08-26
**Relates to:** `DESIGN.md` §3, §4.6, ADR 0019, ADR 0021, ADR 0026, ADR 0027, ADR 0028
**Settles:** `drawing-board/RULESET.md` decision 13

## Context

Decision 13 has said the same thing since it was written: *without throws,
blocking has no downside — hold-back blocking is free and always available.*

Two changes since have made that sharper rather than softer. ADR 0026 built the
knockdown loop, and ADR 0028 gave offence a high/low axis. Together they leave
exactly one hole: **a defender who crouch-blocks and waits.** Crouching covers
lows and mids, and the only overhead is a jump attack visible from across the
screen. Waiting was close to free, and free is the one thing `DESIGN.md` §3 does
not allow an option to be.

## Decision

**Throws exist, and they cannot be blocked.** That is the whole mechanism;
everything else is pricing.

- **Light punch + light kick together.** The genre's convention, and it costs no
  button — which matters, because six is already the ceiling §4.1 committed to.
- **Short reach.** Measured, 51 units against a light punch's 61 — the shortest
  attack in the game. Being unblockable is paid for by only working up close.
- **Fast to start, slow to recover.** Four frames of startup and twenty-two of
  recovery, which is longer than a light punch's entire animation. A whiffed
  throw costs a turn.
- **Cannot catch an airborne fighter.** Jumping beats a throw.

### The triangle

This is what the decision is actually for:

```
attack beats throw   (throws are short, and slow to recover)
throw  beats block   (unblockable)
block  beats attack  (that is what blocking is)
```

Every edge is now a real edge, and a test asserts the whole shape. If one breaks,
a single option becomes correct and the guessing stops.

### No throw tech, and a soft knockdown instead

**This is the one place this decision departs from consensus, deliberately.**

Most modern fighters let a defender break a throw by inputting one at the same
time. It is popular, and ADR 0027 says popularity is a legitimate basis — but
0027 also says the `DESIGN.md` §3 check is what it still has to pass, and here
it does not cleanly:

- **A tech is a cheap escape from a commitment**, which §3 names as the thing a
  mechanic must not be. The attacker committed to a throw; a defender who can
  undo it by pressing the same button has not made a decision so much as
  cancelled one.
- **It is another system**, and decision 15 exists to count them. The game
  already has hold-back blocking, heights, a wakeup choice, and three more
  settled mechanics not yet built.
- **The triangle already provides counterplay.** A defender is not helpless
  without a tech: attacking beats a throw, and so does jumping. Those are the
  answers, and they are commitments in their own right.

What a tech normally guards against is the **throw loop** — throw, they get up,
throw again, with no way out. That is a real danger and it is answered directly:
**a throw causes a SOFT knockdown**, not the hard one convention gives it. The
defender keeps the timing choice on the way up, so the loop does not close.

Convention says hard knockdown *and* tech-able. This takes neither half rather
than one, which is coherent — what it must not be is hard knockdown with no tech,
which is the genuinely oppressive combination.

## Consequences

- **Blocking now has a downside**, which is what decision 13 was about, and the
  knockdown loop finally opens into a real guess.
- **`MoveId` gains a fifteenth entry, appended.** Tools and tests iterate
  `0..Count` and TOML keys map by name, so inserting one mid-list would be a
  data migration wearing the clothes of a rename. Schema v5.
- **The move-name switches are not as safe as claimed.** Adding `Throw` should
  have been a build error under the no-`default` rule from ADR 0028's work; MSVC
  did not warn, and a *test* caught it printing `?`. The guarantee holds on GCC
  and Clang, so CI would have caught it — but "adding a MoveId is a build error"
  is only true on two of three compilers, and should be stated that way.
- **`throw_vs_block` is recorded with the feature**, not after it, and carries
  an assertion that somebody actually gets thrown. The lesson is from ADR 0028:
  every existing recording blocked a mid, so not one changed, and the new rule
  went into the regression net uncovered.
- **Worth revisiting if** throws dominate in play. The first lever is reach or
  recovery, not a tech — and if a tech does become necessary, it should be
  weighed against the commitment release, which is the mechanic already assigned
  the role of buying out of a bad situation.
