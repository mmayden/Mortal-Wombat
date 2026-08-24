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

## Where the settled decisions live

**Decisions 1 to 5 have graduated. `DESIGN.md` §4.1 is now the specification and
the only place the rules are stated.** What follows is the *reasoning* behind
them, which a specification should not carry — kept because knowing why a
decision was made is what stops it being re-argued.

If this file and `DESIGN.md` ever disagree about what a rule *is*, `DESIGN.md`
is right and this file is stale.

## Decided — reasoning

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

### 5. Two control schemes: Classic and Modern

Both offered, chosen per player.

**Classic.** Six attack buttons, motion inputs for specials. The full move list
and the full damage.

**Modern.** Fewer attack buttons — light, medium, heavy, plus a dedicated
Special button — with specials performed as a direction plus Special rather than
a motion, and repeated presses producing an authored combo sequence.

**Where the scheme lives.** Per player, fixed for the match, chosen before it
starts. That makes it *config*, so it belongs in `MatchData` alongside frame
data rather than in `GameState` — nothing about it changes during a round.

---

#### The consequence worth knowing before building it

**Both schemes need input history, and the simulation currently keeps none.**

`advance_frame` receives exactly two frames: the current input and the previous
one. That is enough for button edges and nothing else. But:

- **Classic motion inputs** are a pattern across recent frames. Recognising a
  quarter-circle means remembering the last several directions.
- **Modern auto-combos** need to know *where in a sequence* the player is, which
  is history by another name.

So both require a rolling buffer of recent inputs per player — and **it has to
live in `GameState`.** Rollback re-simulates recent frames from a restored
snapshot; anything the input translator remembers must be restored with it, or a
rolled-back frame recognises a different move than the original did. That is a
desync, and one that would only appear in real matches.

**The cost is small and worth stating so it is not feared.** Sixteen frames of
two-byte inputs for two players is sixty-four bytes, against a four-kilobyte
budget that currently sits at just over four hundred. It is the *placement* that
matters, not the size.

**It pays for itself twice.** An input buffer is also what makes a slightly
early press still come out, which is standard modern practice and currently
absent — listed separately under controller support. One structure serves both.

**This will need an ADR when implemented**, because it changes the shape of
`GameState`, which ADR 0005 governs.

---

#### 5a. Modern is a pure remap. No damage tax.

The simulation holds **one canonical action set**, and a control scheme is an
input adapter over it. The sim never learns which scheme produced an input.

**Why no damage penalty.** It would make the scheme a simulation input rather
than a presentation detail, and it forks the single source of truth ADR 0009
established: every move property in `data/characters/*.toml`, every validation
rule, every test asserting "high punch does 8", and the schema contract with the
frame-data editor would need a scheme qualifier. The balance surface roughly
squares — with twenty characters, a matchup matrix goes from four hundred
entries to sixteen hundred, and every claim about a move becomes two claims.

The return is close to nothing. The reported community verdict is that the
damage penalty is barely felt, while losing access to half the normals is what
actually limits the simpler scheme at higher levels. That is full engineering
and balance cost for something nobody experiences.

**The tax is option breadth, and it is automatic.** A scheme that cannot express
an action pays for its simplicity by not having that action. No data, no
bookkeeping, self-evident to the player.

#### 5b. The rule for the exception, and it is a small one

> **If a scheme removes a capability it needs no tax. If it grants one, it must
> be taxed.**

A one-button command grab is not a worse version of a motion command grab — it
is strictly better. Same for charge moves performed without charging. Those are
the only places a scheme-aware property is warranted, and there should be a
handful of them rather than a global multiplier.

**Tax frames, not damage.** A damage penalty is invisible during play and
teaches nothing. Two extra startup frames change which situations the move works
in — visible, situational, and a real reason to learn the harder input. This is
also the `DESIGN.md` §3 argument: readability is the anchor.

#### 5c. Auto-combos are scripted canonical inputs

Pressing light repeatedly performs the same moves the player would perform
manually, in order — the scheme queues real inputs rather than triggering an
authored sequence.

- **No new move data.** No animations to tune, no separate scaling curve, no
  corner cases to author.
- **Inherits the combo system for free**, including scaling. It can never be
  stronger than the manual route, because it *is* the manual route.
- **Self-teaching.** The player watches the route they should learn, performed
  correctly, every time they mash.

The alternative — an authored sequence with its own properties — teaches a habit
that has to be unlearned later, which makes it a plateau rather than a ramp. It
also costs a full combo route per character, maintained through every balance
change.

**And it is nearly free for us architecturally.** A queue of canonical inputs is
just input: it flows through the same history buffer motion inputs already
require. No new category of state to snapshot and restore for rollback.

#### One question Modern leaves open

**Which layer is being simplified?** Simplifying inputs while leaving the system
mechanics untouched is reported to be the worst of both — the simplified scheme
still meets the same wall, just with fewer options in hand. Whatever the hardest
system mechanic turns out to be, Modern has to have an answer for it or the
scheme is only half a ramp.

---

#### An honest note on what this buys

The research is pointed here, and it is worth recording rather than discovering:
**execution accessibility and system accessibility are different problems.** Two
control schemes lower the barrier to *performing* a move. They do nothing for a
player who cannot answer "what should I be doing right now".

That is not an argument against this decision — being unable to execute is a
real wall, and removing it is a real kindness. It is an argument against
expecting it to carry retention on its own. The thing that does carry retention,
per the same research, is keeping the *system count* low, which the 1v1 and
single-meter decisions are already aimed at.

---

## What we take from other games, and how

Settled as **ADR 0019**, because "people love X, can we have X?" will otherwise
be re-argued every time.

Three buckets, and the bucket decides how freely a thing is taken:

| Bucket | Rule | Examples for this project |
|---|---|---|
| **Tooling and presentation** | Take freely — it competes for no role | Training mode, frame-data display, replay review, input display, netcode, onboarding |
| **System structure** | Take the principle, never the pricing | One multi-use resource with real opportunity cost, plus a punish state for overspending |
| **Mechanics** | One per role, chosen for fit | One active defence. One commitment release. One resource. |

Before adopting any mechanic: what constraint does it release, is that role
already filled, and does it serve `DESIGN.md` §3?

### The SF6 case, which is the reason the rule exists

Its most-praised system and its most-complained-about system are the same one.
The research puts the real objection to Drive Rush not at raw strength but at
compressing neutral and offence into a single action — SF6 deliberately treating
neutral as a **continuous surface**.

`DESIGN.md` §3 asks for a *slow, readable, commitment-based neutral* — a
**distinct phase**. That is the opposite answer to the same question, chosen on
purpose by both.

So SF6 is a game to learn a great deal from and not a game to copy the centre
of. What this project takes is already substantial: rollback, an accessible
control scheme, best-in-class practice tooling, a unified-resource *structure*,
and a punish state for overspending. What it does not take is the pricing that
made one spend dominant, and the continuous-neutral premise.

---

## The thesis — settled

**`DESIGN.md` §3 owns the statement.** This is the reasoning behind it.

### Why "popular consensus" did not resolve it directly

The question was whether neutral is a **distinct phase** or a **continuous
surface**. Two signals point opposite ways.

**Commercial popularity says continuous.** Street Fighter 6 is the best-selling
modern fighting game and it chose continuous deliberately.

**Player sentiment says the opposite, about that exact mechanic.** The research
records Drive Rush as the most consistently named complaint, and the specific
objection is precisely that it compresses neutral and offence into one action.
The games the community holds up as best-designed skew phase-based.

Attributing SF6's success to its most-complained-about system would be reading
the wrong signal. Its netcode, tooling, onboarding and presentation are what the
same research praises without reservation — and those are all in the "take
freely" bucket of ADR 0019 anyway.

### The binary was false, and the research says so

It describes Roman Cancel and Drive Rush as *the same idea with a different
bill*. Both are commitment releases. Roman Cancel is rated S+ and beloved as a
general-purpose "buy out of your commitment" that players find their own uses
for. Drive Rush is the top complaint.

**So consensus is not against compressing commitment. It is against underpricing
it.** The failure mode named in the research is one spend among several being
dramatically better, which collapses the opportunity cost that made the resource
interesting.

### What was chosen, and why it is the consensus answer

**Distinct-phase neutral, with exactly one general-purpose commitment release,
priced expensively.**

That takes the loved property — agency to buy your way out of a mistake — while
avoiding the failure mode, and it is what the higher-rated design actually does.
It also suits this project's constraints: one developer with no patch cadence
cannot continuously audit five competing spends, and the research is explicit
that auditing that gap is a permanent obligation rather than a launch decision.

It required overruling nothing. `DESIGN.md` §3 already asked for a slow,
readable, commitment-based neutral.

### What the thesis now decides

These stop being open questions and become consequences. Each still needs
writing up, but the answer is no longer in doubt.

| Open decision | What the thesis implies |
|---|---|
| Active defence | The **Just Defend / instant block** family — tighter timing on an action you are already performing, which rewards discipline. A parry is a separate commitment made on reflex, and it degrades badly under rollback: its fallback is being hit, where Just Defend's fallback is having blocked. |
| Commitment release | **Exactly one**, general-purpose, expensive. Not one per situation. |
| Meter | **Few uses** — three rather than five — so the gap between best and second-best stays auditable by one person. |
| Combo cap | **Hard.** A game about whether a commitment was worth it cannot let a single commitment end the round. |
| Projectiles | Likely **yes**, as a spacing tool. Distinct-phase neutral wants tools for controlling ground you are not standing on. This eventually answers whether `Projectile` stays in `GameState`. |

---

## Open, in decision order

Next up, from the deep dive's §6.

**▸ marks a decision whose *direction* the thesis already settled** (ADR 0020).
What remains there is the specifics — windows, costs, numbers — not the choice.

| # | Decision | Why it is there |
|---|---|---|
| 6 | **SOCD scheme for horizontal (neutral vs last-input-wins)** | Vertical is settled and fixed. Horizontal is genuinely contested among leverless players, and last-input-wins would need input history the sim does not currently keep. |
| 7 | **Attack heights (high / mid / low / overhead) and stance blocking** | Hold-back only pays off with a high/low axis alongside left/right. Follows directly from decision 1. |
| 8 | **Knockdown → okizeme loop** | The deep dive calls it "the engine; everything else decorates it". Already a v1 definition-of-done line. |
| 9 ▸ | **Active defence: the Just Defend / instant block family** | Direction settled: a tighter window on an action already being performed, whose failure state is *having blocked*. A parry is a separate commitment made on reflex and degrades badly under rollback. Open: the window, and what it grants. |
| 10 ▸ | **One general-purpose commitment release** | Direction settled: exactly one, general-purpose rather than situational, priced expensively in a resource with other uses. Open: the price and what it releases you from. |
| 11 ▸ | **Meter: how many jobs does it do?** | Direction settled: **three**, not five — one developer with no patch cadence has to keep the best/second-best gap auditable by hand. Open: which three, and how it fills. |
| 12 | **Movement tiers** | Loved in proportion to how *differentiated* they are, not how many there are. |
| 13 | **Throws** | Without them, blocking has no downside. Cut by the old §4.6, which is no longer binding. |
| 14 ▸ | **Combo determinism cap** | Direction settled: **hard cap**. A game about whether a commitment was worth it cannot let one commitment end the round. Open: the cap and its shape. |
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
