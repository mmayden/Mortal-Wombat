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

### 0. Rollback netcode is required (confirmed)

Not a mechanic, but it constrains every mechanic below, so it is recorded here
rather than left as an assumption.

**What it locks in.** ADR 0002 (deterministic fixed-point simulation) and ADR
0005 (flat, trivially-copyable `GameState`) become non-negotiable rather than
merely chosen. ADR 0007 already says as much: the netcode *library* is the most
replaceable decision in the stack, while getting 0002 or 0005 wrong is a
rewrite. Both are already built and proven — the simulation is byte-identical
across Linux, Windows and macOS, and identical between debug and release.

**What it means for the decisions below.** Rollback re-simulates recent frames
whenever a remote input arrives late, so:

- **Tight timing windows behave differently online.** A parry with a two-frame
  window is a different mechanic under rollback than in local play, because the
  defender is reacting to a picture that may be corrected. Active defence
  (decision 6) has to be chosen with that in mind — window size is a netcode
  decision as much as a design one.
- **State size is a running cost.** `GameState` is copied on every rollback,
  potentially several times a frame. This is another argument for the 1v1 choice
  already made, and against systems that add many simultaneous entities.
- **Nothing may depend on wall-clock time or unsynchronised randomness.** The
  sim boundary already enforces this mechanically.

**Transport is still open.** ADR 0008 defers internet play behind an interface.
"Multiplayer compatible" is satisfied by the architecture; actually shipping
online play needs NAT traversal or a relay, which is a distribution decision
that has not been made.

### 4. Six attack buttons

Light, medium and heavy punch; light, medium and heavy kick. The Street Fighter
layout, which is the family the three root axes already put this game in.

**Why.** It is the most widely understood layout in the genre, it is what nearly
every arcade stick and leverless controller is physically laid out for, and it
gives each attack a light/medium/heavy identity rather than a binary one.

**Cost, and it is the real one.** Six buttons times three stances is **eighteen
normals per character**, against twelve with a four-button layout. For two
characters that is thirty-six sets of frame data plus specials — all of it
hitbox geometry that has to be authored and, critically, *checked*.

This moves `tools/framedata_editor/` from "worth doing" to **required before
authoring content**. The jump attack already shipped with a hitbox that could
not hit anyone, at twelve moves. Eighteen is not a difference of degree.

**Cost in the input layer.** `InputFrame` becomes four directions plus six
attacks — ten bits, against nine today. `Button::Block` leaves (decision 1),
so the bitfield does not grow. Still comfortably inside the `uint16_t` that
rollback sends per player per frame.

### 4b. Controller support: leverless, pads, sticks

A hardware requirement rather than a mechanic, recorded here because one part of
it is a design decision that has to be made deliberately.

**SOCD cleaning is the whole problem.** SOCD is Simultaneous Opposing Cardinal
Directions — pressing Left and Right, or Up and Down, at the same time. On a
stick or a d-pad this is physically impossible. On a leverless controller, where
every direction is its own button, it is trivial, and the game must decide what
it means.

**What this project does today, and where it is wrong:**

| Input | Behaviour | Status |
|---|---|---|
| Left + Right | Neutral | Accepted scheme. Last-input-wins is the alternative and needs input history the sim does not keep. **Still open.** |
| Up + Down | **Up priority** | **Fixed.** Was neutral, which made a real input do nothing. |

The Up + Down case was a live defect, and not a hypothetical one about hardware
nobody owns yet — **it was reachable from the keyboard the game already ships
with.** Measured before the fix:

```
holding Down alone      -> Crouch        correct
Down + Up together      -> Idle          wrong; should be JumpStartup
Up alone                -> JumpStartup   correct
```

A player crouch-blocking holds Down; to jump they press Up without releasing it.
Neutral is neither a crouch nor a jump, so the fighter just stood there.

A gamepad cannot produce the input — a d-pad pivots, a stick has one position —
which is why it survived every play session. Fixed to Up priority, with unit
coverage for the resolution itself and a jump test for the crouch-to-jump case
that was actually broken.

All nine replay recordings hashed unchanged, because no scenario presses Up and
Down together. The fix is strictly additive.

**The rest of what "SF6-like controller support" means here:**

- Leverless devices, arcade sticks and pads all reaching the sim as the same
  bitfield. Input-as-data (ADR 0002) already guarantees this — the sim cannot
  tell what produced an input, which is the point.
- **Six attack buttons fit a standard pad comfortably**, and this is unrelated to
  the SOCD question above. The conventional mapping, which is what Street
  Fighter uses on a controller:

  | | Light | Medium | Heavy |
  |---|---|---|---|
  | Punch | Square / X | Triangle / Y | R1 / RB |
  | Kick | Cross / A | Circle / B | R2 / RT |

  Four face buttons plus two shoulders. Nothing about a six-button layout needs
  special hardware.
- Full button remapping. Needs a UI and a settings file; neither exists.
- An input buffer, so a slightly early press still comes out. Standard modern
  practice and currently absent.
- An input display to verify a mapping. `--input-test` is the console ancestor
  of this.

**Still to decide:** whether to offer a simplified control scheme alongside the
six-button one, in the manner of SF6's Modern controls. That is an accessibility
*mechanic*, not hardware support, and the research is pointed about it — see the
deep dive's §5 on execution accessibility versus system accessibility.

---

## Open, in decision order

Next up, from the deep dive's §6:

| # | Decision | Why it is there |
|---|---|---|
| 5 | **SOCD scheme for horizontal (neutral vs last-input-wins)** | Vertical is settled and fixed. Horizontal is genuinely contested among leverless players, and last-input-wins would need input history the sim does not currently keep. |
| 6 | **Simplified control scheme? (SF6 Modern style)** | An accessibility mechanic, not hardware support. The deep dive's §5 argues execution accessibility and system accessibility are different problems. |
| 7 | **Attack heights (high / mid / low / overhead) and stance blocking** | Hold-back only pays off with a high/low axis alongside left/right. Follows directly from decision 1. |
| 8 | **Knockdown → okizeme loop** | The deep dive calls it "the engine; everything else decorates it". Already a v1 definition-of-done line. |
| 9 | **One active-defence mechanic — parry, Just Defend, or instant block** | Rated the highest-return decision available. Exactly one. Window size is a netcode decision too — see decision 0. |
| 10 | **One general-purpose commitment release, priced in a contested resource** | Not five specific ones. Meaningless unless the resource has other uses. |
| 11 | **Meter: how many jobs does it do?** | Opportunity cost is depth per byte. One meter doing three to five jobs beats three meters. |
| 12 | **Movement tiers** | Loved in proportion to how *differentiated* they are, not how many there are. |
| 13 | **Throws** | Without them, blocking has no downside. Cut by the old §4.6, which is no longer binding. |
| 14 | **Combo determinism cap** | Decide before shipping, not in a patch. |
| 15 | **System count audit** | Count the pairwise interactions a new player must hold in their head. |

## Dissolved by the redesign

Questions that were blocking work under the old basis and no longer exist:

- **"Twelve or ten moves?"** `DESIGN.md` §4.5's prose and its table disagreed.
  The moveset is being redesigned, so the mismatch is moot.
- **"Does the special spawn a projectile?"** Depends on specials that have not
  been designed yet. Returns as part of decision 7.

## Still unanswered from earlier

- **Selectable rulesets as a long-term mode.** CvS2's Groove system is the
  precedent, and it varied *leaf* axes only — every Groove was hold-back,
  grounded, 1v1. That is the rule: modes may vary leaf axes; root axes are the
  game.
