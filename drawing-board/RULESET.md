# Ruleset — working draft

**Nothing here is binding, and nothing settled stays here.** This file is the
working surface for mechanical rules that have not been decided yet. The moment
one is decided it graduates: the rule into `DESIGN.md`, the reasoning into an
ADR, and a pointer back into the table below (ADR 0023).

**This file is kept permanently** — not as a record of decisions, which the ADRs
hold, but because merged ADRs cite the research beside it and merged ADRs are
never edited. Those paths must not move.

Decisions are worked in the order `2d-fighter-mechanics-deep-dive.md` §6 gives,
because each one narrows the next.

**Sources:** `drawing-board/2d-fighters-breakdown.txt` (video transcript,
taxonomy of subgenres) and `drawing-board/2d-fighter-mechanics-deep-dive.md`
(designer's analysis of which mechanics are loved and why). Both converge
independently on the same root axes, which is why they are treated as root.

---

## Where the settled decisions live — not here

**Everything settled has graduated out of this file.** `DESIGN.md` says what
each rule *is*; its ADR says *why*, including what it forecloses and what would
reopen it. This file keeps a pointer and nothing more (ADR 0023).

| Settled | Specified in | Reasoning in |
|---|---|---|
| Rollback netcode is required | `ARCHITECTURE.md`; out of the v1 DoD by `DESIGN.md` §6 | ADR 0007 |
| Hold back to block | `DESIGN.md` §4.1 | ADR 0021 |
| Six attack buttons | `DESIGN.md` §4.1 | ADR 0021 |
| Classic and Modern schemes; Modern is a pure remap | `DESIGN.md` §4.1 | ADR 0021 |
| Up-priority SOCD (vertical) | `DESIGN.md` §4.1 | ADR 0021 |
| Input history in `GameState` | `DESIGN.md` §4.1 | ADR 0021 |
| No air blocking — grounded | `DESIGN.md` §4.3 | ADR 0022 |
| One fighter per side | `DESIGN.md` §2 | ADR 0022 |
| The thesis — *"was that worth committing to?"* | `DESIGN.md` §3 | ADR 0020 |
| Just Defend over parry; one commitment release; three meter jobs; a hard combo cap | `DESIGN.md` §4.6 | ADR 0020 |
| How to borrow from other games at all | — | ADR 0019 |
| The Mortal Kombat II basis is dropped | — | ADR 0018 |

**If this file and `DESIGN.md` ever disagree about what a rule *is*,
`DESIGN.md` is right and this file is stale.** If this file and an ADR disagree
about *why*, the ADR is right.

Two things were lost to the trim and are worth keeping, because they are rules
about *this file* rather than about the game:

**Decision numbering is historical, not priority.** Rollback is numbered 0
because it was confirmed after 1–3 but constrains all of them — it is the
precondition, not the fourth decision. Numbers are never reused, so a citation
to "decision 7" stays valid.

**Modes may vary leaf axes; root axes are the game.** Selectable rulesets are a
long-term idea and CvS2's Groove system is the precedent — but every Groove was
hold-back, grounded, 1v1. Hold-back blocking, grounded defence, one fighter per
side and the thesis are root. A mode that varies one of those is a different
game wearing this one's name.

---

## What we take from other games — ADR 0019

Three buckets — tooling and presentation, system structure, mechanics — and the
bucket decides how freely a thing is taken. Before adopting any mechanic: what
constraint does it release, is that role already filled, and does it serve
`DESIGN.md` §3?

**[ADR 0019](../docs/decisions/0019-adopt-properties-not-mechanics.md) is the
record**, including the SF6 case that is the reason the rule exists.

---

## The thesis — ADR 0020

***"Was that worth committing to?"*** Neutral is a distinct phase; leaving it is
priced. Specified in `DESIGN.md` §3.

**[ADR 0020](../docs/decisions/0020-thesis-commitment.md) is the record** — why
popular consensus did not resolve it directly, why the phase-versus-surface
binary was partly false, and what the choice forecloses.

---

## Settled by the thesis — numbers still to pin down

**`DESIGN.md` §4.6 is the specification for these four; ADR 0020 is why.** The
choice is made and is not reopened by tuning. What is left is arithmetic.

| # | Mechanic | What is still open |
|---|---|---|
| 9 | **Active defence — Just Defend / instant block** | How tight the window is, and what a success grants. |
| 10 | **One general-purpose commitment release** | What it costs, and what it releases you from. |
| 11 | **Meter — three jobs** | Which three, and how the bar fills. |
| 14 | **A hard combo cap** | Where the cap sits, and what shape it takes. |

---

## Open, in decision order

**Six decisions, none of them directed yet.** From the deep dive's §6, in the
order that unblocks the most.

| # | Decision | Why it is there |
|---|---|---|
| 6 | **SOCD scheme for horizontal (neutral vs last-input-wins)** | Vertical is settled and fixed. Horizontal is genuinely contested among leverless players, and last-input-wins would need input history the sim does not currently keep. |
| 7 | **Attack heights (high / mid / low / overhead) and stance blocking** | Hold-back only pays off with a high/low axis alongside left/right. Follows directly from decision 1. |
| 8 | **Knockdown → okizeme loop** | The deep dive calls it "the engine; everything else decorates it". Already a v1 definition-of-done line. |
| 12 | **Movement tiers** | Loved in proportion to how *differentiated* they are, not how many there are. |
| 13 | **Throws** | Without them, blocking has no downside — hold-back blocking is free and always available. Cut by the old cut list (§4.7), which is void. |
| 15 | **System count audit** | Count the pairwise interactions a new player must hold in their head. |

---

## Dissolved by the redesign

Questions that were blocking work under the old basis and no longer exist:

- **"Twelve or ten moves?"** `DESIGN.md` §4.5's prose and its table disagreed.
  Moot: the count now falls out of §4.1's six buttons rather than from prose —
  six standing normals, six crouching, a jump attack and a special.
- **"Does the special spawn a projectile?"** Depends on specials that have not
  been designed yet. Returns as part of decision 7.
