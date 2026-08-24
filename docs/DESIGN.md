# Mortal Wombat — Design Document

>
> **Status by section**, so nobody has to guess which parts are safe to build to.
>
> | Section | State |
> |---|---|
> | §1 §2 §3 §5 §6 §8 §9 §10 | **Binding.** §3 carries the thesis and is the anti-drift anchor. |
> | §4.1 §4.2 §4.3 | **Binding.** §4.1 is ADR 0021. |
> | §4.4 | **Binding in shape, provisional in numbers** — tune freely, re-record replays in the same commit. |
> | §4.5 | **Superseded and must be re-authored** — it predates six buttons. Do not build to it. |
> | §4.6 | **Direction binding, numbers open.** ADR 0020. |
> | §4.7 | **Void**, kept only for the record. |
> | §7 | **Non-binding by its own terms** — recorded to keep scope pressure off v1. |
>
> Mechanics still being decided live in `drawing-board/RULESET.md` and graduate
> into §4 as they settle. `ROADMAP.md` owns how far along each one is — this
> document deliberately carries no status.
>
> **Date:** revised 2026-08-24 (originally 2026-08-22)
> **Purpose:** This is a *constraints* document, not a pitch. It exists so that
> any contributor — human or agent — makes decisions compatible with everyone
> else's. Where something is genuinely undecided it is marked **TODO**, and
> those sections must not be invented around.

---

## 1. The game in three sentences

Mortal Wombat is a 2D one-on-one fighting game: six attack buttons, hold back
to block, grounded neutral, best-of-three rounds. It is a comedy fighter — the
cast are wombats and the tone is deadpan rather than gritty — built on a
deterministic simulation for rollback netplay. The v1 target is two complete
characters that feel good to play.

---

## 2. Core loop

Two fighters face each other on a single-screen stage. Each has a health bar
and a round timer. Players attack, block, and jump to reduce the opponent's
health to zero. First to win two rounds wins the match. A round ends on KO or
on timer expiry, in which case the fighter with more remaining health wins.

That is the entire loop. There is no meta-progression, no story mode, no
unlocks in v1.

---

## 3. Player fantasy and intended feel

**This section is the anti-drift anchor. Read it before making any judgment
call about feel.**

### The thesis — the one question the game asks

> ## *"Was that worth committing to?"*

Every exchange is that question. You chose a button, at a distance, at a moment,
and you are now living with it.

**This is the test every mechanical decision has to pass.** When two options are
both defensible, the one that makes that question sharper wins. A mechanic that
lets a player escape a bad commitment cheaply is working against the game, no
matter how well it works elsewhere.

**How this sits with §10.** Two rules in this document resolve ambiguity and
they answer different halves of the question. §3 decides *what a mechanic is
for*; §10 decides *how elaborate it is allowed to be*. Run §3 first: an option
that does not sharpen the question is out regardless of how simple it is. Among
the options that survive, **§10 breaks the tie, and it breaks it toward
simpler.** A mechanic that serves the thesis is not thereby licensed to be
complicated — the thesis is the reason to build something, never the excuse to
build more of it.

### What that means structurally

**Neutral is a distinct phase, not a continuous surface.** There is a state of
the match where nobody is committed — both fighters moving, looking for an
opening — and leaving it is a decision with a price. Attacking is how you leave
neutral, and if you leave it wrongly you should be punished for it.

This is a real fork in the genre and it is chosen deliberately. Street Fighter 6
takes the other branch: a whiffed poke and a full mixup are one input and some
meter apart, which makes neutral and offence a single continuous surface. That
is a legitimate design and it sells enormously. It is not this game.

### The feel that follows

The feel target is *deliberate and weighty*, not fast and flowing. Jumps have
fixed arcs you cannot alter mid-air, big attacks have long recovery, and
whiffing a heavy is genuinely punishing.

**We are NOT building:**
- A fast, cancel-heavy combo game (Street Fighter, Marvel, modern MK)
- A game with air control or double jumps
- A game where mashing produces good outcomes
- A technically demanding execution barrier

**We ARE building:**
- Slow, readable, commitment-based neutral
- Attacks that feel heavy when they land
- Something two people can pick up in ninety seconds
- Comedy through character and animation, never through unfair mechanics

**Comedy rule:** the joke is the wombats. The fighting system plays straight.
Nothing in the mechanics should be a gag — no random damage, no joke moves
that break the game state, no fourth-wall input. A funny game that plays badly
is a bad game.

---

## 4. Mechanical specification (v1)

### 4.1 Controls — settled (ADR 0021)

**Six attack buttons and four directions.** Blocking is a direction, not a
button.

| | Light | Medium | Heavy |
|---|---|---|---|
| **Punch** | LP | MP | HP |
| **Kick** | LK | MK | HK |

On a controller that is four face buttons plus two shoulders, which is the
conventional layout and needs no special hardware.

**Hold back to block.** Because *back* is relative to the opponent's position,
an attack that changes sides mid-animation forces the defender to reverse their
input. That is a crossup, and it is an entire axis of offence that a block
button deletes by giving every attack the same defensive answer.

**Directions resolve by SOCD rules**, because a keyboard and a leverless
controller can hold opposing directions at once where a stick cannot:

| Both held | Result |
|---|---|
| Up + Down | **Up wins.** A player crouching who presses up means to jump. |
| Left + Right | **Neutral.** |

**Two control schemes, chosen per player before a match.**

*Classic* uses all six buttons with motion inputs for specials. *Modern* uses
light, medium and heavy plus a dedicated Special button, performing specials as
a direction plus Special.

Modern is a **pure input adapter over one canonical action set** — the
simulation holds one set of move properties and never learns which scheme
produced an input. There is no damage penalty. The cost of the simpler scheme is
that it cannot express every action, which is a real cost that needs no
bookkeeping.

The one exception, and the rule that governs it: **if a scheme removes a
capability it needs no tax; if it grants one, it must be taxed** — and the tax is
paid in frames, not damage. A one-button command grab is strictly better than a
motion one, so it costs startup. A damage penalty would be invisible during
play and teach nothing; two extra startup frames change which situations the
move works in, which a player can see.

**Auto-combos are scripted canonical inputs**, not authored sequences. Pressing
light repeatedly performs the same moves the player would perform manually, in
order. This adds no move data, inherits combo scaling automatically, can never
be stronger than the manual route because it *is* the manual route, and teaches
that route by performing it.

### 4.2 Fighter states

The complete v1 state set. Adding a state requires an ADR.

```
RoundStart · Idle · WalkForward · WalkBackward · Crouch
JumpStartup · Airborne · Landing
Attack · Blocking · Hitstun · Blockstun
Knockdown · Wakeup · Win · Lose
```

### 4.3 Movement constraints

- **Fixed jump arcs.** Trajectory is committed at takeoff. No air control, no
  double jump, no air dash, no jump cancel.
- Three jump types, determined at takeoff: neutral, forward, backward.
- No run button in v1.
- No dashes.
- Fighters always face each other; facing flips when they cross up.

### 4.4 Starting numeric values

All values in integer frames at 60Hz. These are **starting points to tune, not
constants** — expect all of them to change, and re-record affected replay tests
in the same commit as the change.

| Property | Value |
|---|---|
| Logical resolution | 480 × 270, integer-scaled |
| Ground plane Y | 240 |
| Fighter standing height | ~140 units |
| Stage width | 960 units |
| Walk speed | 1.2 units/frame forward, 1.0 back |
| Jump duration | 44 frames total |
| Jump apex | ~90 units |
| Starting health | 100 |
| Round timer | 5400 frames (90s) |
| Rounds to win | 2 |

### 4.5 Starting frame data — ⚠️ SUPERSEDED

**This table predates ADR 0021 and must be re-authored. Do not build to it.**

It describes a four-attack scheme: there is no medium punch and no medium kick
in it, because when it was written there were no medium buttons. §4.1 is the
binding control spec, and it requires six.

What that costs is roughly eight new move definitions — MP and MK, standing and
crouching, for two characters — each needing hitbox geometry that has to be
*seen* to be reviewed. The jump attack shipped with its hitbox at standing-punch
height and could not touch anyone from any range at any timing, because the
numbers are plausible in a text file. So `tools/framedata_editor/` comes first;
`ROADMAP.md` sequences it.

The old table is kept below for the frame values, which are still a reasonable
starting shape for light and heavy. The move *set* is what is wrong.

Per character, v1. Twelve moves total.

| Move | Startup | Active | Recovery | Damage | Hitstun | Blockstun |
|---|---|---|---|---|---|---|
| LP | 4 | 2 | 8 | 3 | 12 | 8 |
| HP | 7 | 3 | 16 | 8 | 18 | 12 |
| LK | 5 | 3 | 10 | 4 | 13 | 9 |
| HK | 9 | 4 | 20 | 9 | 20 | 14 |
| Crouching variants | inherit, adjusted boxes | | | | | |
| Jump attack | 6 | until landing | 4 (landing) | 7 | 16 | 11 |
| Special | 12 | — | 24 | 6 | 18 | 12 |

**Special move input: `B, F + HP`** — a two-direction motion, deliberately
chosen over a quarter-circle. Simpler to parse, simpler to execute, and
period-appropriate.

### 4.6 System mechanics — direction settled, numbers open

**These four follow from the thesis in §3 and are recorded by ADR 0020.** The
*choice* is settled and is not reopened by tuning. Every number in them is still
open, and `drawing-board/RULESET.md` tracks what is left to pin down.

They are here rather than in the drawing board because they now constrain what
else may be built — which is the line between a proposal and a rule.

**One active defence: the Just Defend / instant block family.** A tighter timing
window on an action the defender is already performing. Not a parry.

The reason is the failure state. A missed Just Defend leaves you *having
blocked*; a missed parry leaves you *hit*. The first is a discipline reward, the
second is a second guess layered on top of the first — and under rollback, where
a defender may be reacting to a frame that gets re-simulated, a mechanic whose
failure is catastrophic punishes the network rather than the player.

**Exactly one commitment release, and it is expensive.** General purpose rather
than situational, so players find their own applications — that is the property
that makes the admired version admired. Priced so that buying out of a mistake
is itself a real commitment.

One, not one per situation. A second mechanic in an occupied role does not add
depth; it splits the importance of the role between two things (ADR 0019).

**Meter does three jobs, not five.** A multi-use resource is only as deep as the
gap between its best and second-best use, and keeping that gap honest is
permanent work, not a launch decision. One developer with no live-service patch
cadence has to be able to audit it by hand. Which three is open.

**The combo cap is hard, not soft.** A game whose whole question is *"was that
worth committing to?"* cannot answer *"yes, it won the round outright."* Scaling
that merely discourages length leaves the ceiling where it was. Where the cap
sits, and what shape it takes, is open.

---

### 4.7 The old cut list — ⚠️ VOID, kept for the record

**This list is void as written and is not binding.** It was drawn up when the
game had no meter, no active defence and no commitment release, and several
entries were cut as consequences of a design that has since been dropped
(ADR 0018).

Combo strings and cancels in particular are now open rather than excluded — a
combo system is implied by decisions already taken — and throws are back under
consideration, because hold-back blocking is free and always available, so a
defender who simply holds back needs an answer.

`drawing-board/RULESET.md` tracks what is actually decided. The list below is
kept for the record only:

- Juggle system
- Combo strings and cancels
- Chip damage
- Throws
- Run button
- Uppercut-to-pit / stage transitions
- **Fatalities** — see §7
- Multiple special moves per character
- More than two characters

---

## 5. Art direction

### 5.1 v1: placeholder boxes

**v1 ships with colored rectangles. This is a decision, not a placeholder
apology.**

Rationale: feel is entirely frame data. Startup, active, recovery, hitstun.
Colored rectangles feel identical to finished sprites. Tuning the game to
feeling good with zero art means we will know exactly which frames are needed
rather than guessing — and committing to an art pipeline before the engine
works is the most common way a solo fighting game dies.

Placeholder rendering:

| Element | Representation |
|---|---|
| Fighter body | Solid rectangle, per-player color |
| Hurtbox | Blue outline |
| Hitbox | Red outline, filled while active |
| Pushbox | Yellow outline |
| Facing | White notch on the leading edge |
| State | Text label above the fighter (debug builds) |

### 5.2 v2: 3D rendered to sprite sheets

The planned successor. Recorded here so no agent proposes an alternative.

Model and rig in Blender, animate, render from a locked orthographic camera to
sprite sheets. Chosen because:

- **Rig reuse** — every wombat shares a skeleton, so character two costs a
  fraction of character one
- **Iteration is re-render, not redraw** — critical while tuning frame data
- **Free consistency** — lighting, proportions, and camera identical forever
- **Resolution-independent** — sprite size can be decided late

Rejected: hand-drawn pixel art (300+ frames per character is an animation-skill
bottleneck, and timing changes mean redrawing) and digitized live actors (MK's
own method, but lighting consistency across sessions is fragile and per-frame
cleanup is brutal).

### 5.3 Visual tone

- Chunky, exaggerated, low-detail. Wombats are round, heavy, and short-limbed —
  forgiving to model and animate, and the silhouette reads at small sizes.
- Deadpan presentation. The characters take themselves completely seriously.
- Period-appropriate UI: chunky health bars, a large centered timer, heavy
  drop-shadowed text.

### 5.4 The cast — PARTIALLY DECIDED

**Names are settled. Everything else is still undecided — do not invent it.**

The two v1 characters are **Frenchy** and **Wisdom**. Character ids in code and
data are `frenchy` and `wisdom`, replacing the former `WOMBAT_A`/`WOMBAT_B`
placeholders.

Source: these are the names from the 2024 prototype, whose README read
*"Frenchy faces off against Wisdom and its gang of evil Wombatants."* Only the
names carry over. Nothing about that prototype's mechanics, tone, or
implementation is inherited — it was a different codebase in a different
language and this document supersedes it entirely.

§5.4 asks for four things per character before art begins. One is filled in:

| | Frenchy | Wisdom |
|---|---|---|
| Name | **Frenchy** | **Wisdom** |
| Silhouette concept | **TODO** | **TODO** |
| One special move | **TODO** | **TODO** |
| Personality, one line | **TODO** | **TODO** |

The three TODO rows still bind: **do not invent them.** They are needed before
art begins, not before code — the frame-data loader and the character TOML
files can be built against the names alone, since every mechanical property
comes from `docs/framedata_schema.md` rather than from characterisation.

The prototype README hints that Wisdom leads "a gang of evil Wombatants",
which would make Wisdom the antagonist. That is a hint, not a decision, and it
is recorded here so nobody re-derives it as fact.

One open question the names raise: both characters get the **same** move set in
v1, which is correct for v1 scope — two characters exist to prove the engine
feels good, not to prove a matchup. Whether Frenchy and Wisdom eventually differ
mechanically is a post-v1 question (§7). The size of that shared set follows
from §4.1's six buttons and is being re-authored; §4.5 is superseded.

### 5.5 TODO — stage design

**Undecided. Do not invent.**

One stage for v1. Required: dimensions confirmed against §4.4, background
layers, and whether it scrolls. Until filled in, use a flat colored backdrop.

---

## 6. Scope ceiling

**Two characters, complete, before anything else.** Full v1 movesets, hitstun,
blockstun, round flow, win conditions — and it must feel right.

If two characters play well, a third is content work. If they do not, twelve
will not help.

**The weekly check:** can two people sit down and play a match that is fun?

### Definition of done for v1

1. Two characters with the full v1 moveset — every §4.1 button, standing and
   crouching, plus a jump attack and a special
2. Block, hitstun, blockstun, knockdown, wakeup all correct
3. Best-of-three rounds with timer and win conditions
4. Local versus on two gamepads
5. Training mode: hitbox display, frame data readout, input display
6. 50+ replay tests passing
7. Desync CI green across three platforms
8. **It is fun**

**This list defines what v1 means. It deliberately carries no status.**
`ROADMAP.md` tracks how far along each line is — keeping the definition and the
progress in two places was already drifting, and a requirement that quietly
marks itself complete is worse than no checklist.

Rollback netplay is explicitly *not* in the v1 definition of done. The
architecture supports it from day one; the integration waits.

---

## 7. Post-v1 roadmap (non-binding)

Recorded to prevent scope pressure on v1, not as commitment.

1. **Fatalities.** Nearly free once the input parser exists — a special input
   accepted only during a win state. Likely the most fun part of the project.
2. Rollback netplay integration (ADR 0007)
3. Characters three and four
4. 3D art pipeline (§5.2)
5. Additional stages
6. Juggles and combo strings — **only** if v1 feel is proven first

---

## 8. Non-goals

Permanent. Changing these requires an ADR superseding this section.

- No story mode, cutscenes, or narrative
- No meta-progression, unlocks, or currency
- No online matchmaking service or ranked ladder
- No mobile or console ports
- No character creator
- No modding API
- No 3D gameplay — the sim is strictly 2D
- No monetization

---

## 9. IP constraint

Mortal Kombat is Warner Bros. property. This project uses **no** MK characters,
names, assets, sound, or trade dress. "Mortal Wombat" is a parody title over an
original cast.

**The title is the only thing that refers to Mortal Kombat.** The mechanical
inheritance it once implied was dropped by ADR 0018, so the game is no longer
"MK-like" in any sense a reader should carry into a design decision — §4.1's
six buttons and hold-back blocking are the opposite of what that lineage
specified. Anyone reasoning from the name is reasoning from a pun.

Mechanics are not copyrightable; specific characters and assets are. The
fighting engine is identical either way.

---

## 10. The tiebreaker

**When a design question is unclear, choose the simpler option and the one that
makes the game more readable to a new player.**

**This runs second.** §3 asks what a mechanic is *for* and eliminates anything
that does not sharpen the game's question. This section then picks among what
survives, and it picks the simpler one. Serving the thesis is a reason to build
something; it is never a licence for that something to be complicated.

This project's purpose is to build a fighting game engine well. Mechanical
depth is not the goal. Simplicity is the constraint that makes finishing
possible.
