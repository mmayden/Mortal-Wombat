# Ruleset — working draft

**Not binding.** This is the design phase for the mechanical rules, replacing
`DESIGN.md` §4, which was written against a Mortal Kombat II basis that has been
dropped. When this stabilises it graduates into `DESIGN.md` and this file
becomes history.

Decisions are worked in the order `2d-fighter-mechanics-deep-dive.md` §6 gives,
because each one narrows the next.

**Sources:** `drawing-board/2d-fighters-breakdown.txt` (video transcript,
taxonomy of subgenres) and `drawing-board/2d-fighter-mechanics-deep-dive.md`
(designer's analysis of which mechanics are loved and why). Both converge
independently on the same root axes, which is why they are treated as root.

---

## Decided

### 1. Hold back to block

Blocking is holding away from the opponent. There is no block button.

**Why.** Because *back* is relative to the opponent's position, an attack that
changes sides mid-animation forces the defender to reverse their input. That is
a crossup, and it is the entire left/right mixup axis — which a block button
deletes outright, since every attack then has the same defensive answer.

It is close to free here: facing is already resolved every frame, and fighters
already flip when they cross.

**Forecloses:** nothing we want. It ends the MK inheritance in §4.1.

**Cost in this codebase:** small and contained.

- `Button::Block` leaves the input bitfield. That changes `InputFrame`, so every
  replay recording is invalidated and must be re-recorded.
- Blocking stops being a state entered from a button. Holding away already
  produces `WalkBackward`; the block is decided *at hit resolution* by asking
  whether the defender was holding away, grounded, and able to act.
- `resolve_hits` needs the defender's input, which it does not currently
  receive.

**Consequence to design next:** hold-back only pays off with a **high/low axis**
to go with the left/right one. That needs attack heights — see Open below.

### 2. No air blocking — grounded

A fighter in the air cannot block anything.

**Why.** It makes jumping a committed gamble, which makes anti-airs matter,
which makes ground spacing the centre of the game. That is exactly what
`DESIGN.md` §3 asks for and it is still binding: *slow, readable,
commitment-based neutral*.

The alternative is expensive in a way that is easy to miss. Full air blocking
removes the high/low game in the air, because there is no standing or crouching
state up there — so the design immediately owes the game a replacement mixup
axis. That is where air dashes, super jumps, and eventually assists come from.
It is a different genre, not a setting.

**Forecloses:** the anime / air-dasher family, deliberately.

**Cost in this codebase:** none. Fixed jump arcs already fit this, and there is
no air blocking to remove.

### 3. One fighter per side

**Why.** It keeps `GameState` as it is, and it keeps the system count low.

The deep dive's §5 is the argument that matters: execution accessibility and
system accessibility are different problems, and system layers *multiply*
rather than add. Removing hard inputs does not help a player who cannot answer
"what should I be doing right now". `DESIGN.md` §3's ninety-second onboarding is
the same conclusion reached from the other direction.

**Forecloses:** tag and assists as a v1 feature. Not permanently — a tag mode
can arrive later, but as a *root* axis this is fixed.

**Cost in this codebase:** none. This is what exists.

---

## What these three imply

A **grounded 1v1 hold-back fighter** — the Street Fighter II / King of Fighters
lineage. The best-understood shape in the genre, the one with the most prior art
to learn from, and the cheapest to reach from where the code already is.

Everything already built survives: fixed-point determinism, walking, jump arcs,
hitbox resolution, hitstun and blockstun, round flow, and the whole test and CI
layer. The only built thing any of this touches is the block input.

---

## Open, in decision order

Next up, from the deep dive's §6:

| # | Decision | Why it is next |
|---|---|---|
| 4 | **Attack heights** (high / mid / low / overhead) and stance blocking | Hold-back only pays off with a high/low axis alongside left/right. Directly follows decision 1. |
| 5 | **Knockdown → okizeme loop** | The deep dive calls it "the engine; everything else decorates it". Already roadmap item 2 and a v1 definition-of-done line. |
| 6 | **One active-defence mechanic** — parry, Just Defend, or instant block | Rated the highest-return decision available. Exactly one. |
| 7 | **One general-purpose commitment release**, priced in a contested resource | Not five specific ones. Meaningless unless the resource has other uses. |
| 8 | **Meter: how many jobs does it do?** | Opportunity cost is depth per byte. One meter doing three to five jobs beats three meters. |
| 9 | **Movement tiers** | Loved in proportion to how *differentiated* they are, not how many there are. |
| 10 | **Combo determinism cap** | Decide before shipping, not in a patch. |
| 11 | **System count audit** | Count the pairwise interactions a new player must hold. |

## Still unanswered from earlier

- **Is rollback netcode still a goal?** It is the reason for most of the
  architecture. Nothing decided so far changes that, but it should be confirmed
  rather than assumed.
- **Selectable rulesets as a long-term mode.** CvS2's Groove system is the
  precedent, and it varied *leaf* axes only — every Groove was hold-back,
  grounded, 1v1. That is the rule: modes may vary leaf axes; root axes are the
  game.
