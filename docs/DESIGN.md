# Mortal Wombat — Design Document

>
> **§4.1 and §4.3 are settled and binding.** §4.2, §4.4, §4.5 and §4.6 are
> **still under revision** — do not build to them. Work in progress lives in
> `drawing-board/RULESET.md`, which graduates into this section when complete.
>
> The Mortal Kombat II basis this section was originally written against has
> been dropped (ADR 0018). Everything else in this document stands: §1, §2,
> §3 (feel — the anti-drift anchor), §5, §6, §8, §9 and §10.
> **Date:** 2026-08-22
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

The feel target is *deliberate and weighty*, not fast and flowing. Committing
to an action means committing: jumps have fixed arcs you cannot alter mid-air,
big attacks have long recovery, and whiffing a heavy is genuinely punishing.

This is the anti-drift anchor. When a mechanical decision is unclear, the
question is which option better serves it.

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

### 4.1 Controls — settled

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

### 4.5 Starting frame data

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

### 4.6 Cut from v1 — ⚠️ UNDER REVISION

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

One open question the names raise: DESIGN §4.5 gives both characters an
identical twelve-move set, which is correct for v1 scope. Whether Frenchy and
Wisdom eventually differ mechanically is a post-v1 question (§7), not a v1 one.

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

1. Two characters with the full §4.5 moveset
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
original cast in a similar mechanical style.

Mechanics are not copyrightable; specific characters and assets are. The
fighting engine is identical either way.

---

## 10. The rule that resolves ambiguity

**When a design question is unclear, choose the simpler option and the one that
makes the game more readable to a new player.**

This project's purpose is to build a fighting game engine well. Mechanical
depth is not the goal. Simplicity is the constraint that makes finishing
possible.
