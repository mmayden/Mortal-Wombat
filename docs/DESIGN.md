# Mortal Wombat — Design Document

> **Status:** v1 constraints — binding
> **Date:** 2026-08-22
> **Purpose:** This is a *constraints* document, not a pitch. It exists so that
> any contributor — human or agent — makes decisions compatible with everyone
> else's. Where something is genuinely undecided it is marked **TODO**, and
> those sections must not be invented around.

---

## 1. The game in three sentences

Mortal Wombat is a 2D one-on-one fighting game in the mechanical style of
*Mortal Kombat II* — five buttons, a dedicated block button, fixed jump arcs,
best-of-three rounds. It is a comedy fighter: the cast are wombats, and the
tone is deadpan rather than gritty. The v1 target is two complete characters
that feel good to play, offline, with rollback-ready architecture.

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

The feel target is *deliberate and weighty*, not fast and flowing. MK2's
defining property is that committing to an action means committing — jumps
have fixed arcs you cannot alter mid-air, big attacks have long recovery, and
whiffing a heavy is genuinely punishing. This is the feel we want.

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

### 4.1 Controls

Five buttons, MK-style:

| Button | Name |
|---|---|
| LP | Low Punch |
| HP | High Punch |
| LK | Low Kick |
| HK | High Kick |
| BL | **Block** |

Plus four directions.

**Block is a button, not hold-back.** This is a deliberate MK inheritance and
it is also mechanically simpler: there is no ambiguity between walking backward
and blocking, which removes an entire class of edge cases from the state
machine.

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

### 4.6 Explicitly cut from v1

Do not build these. Do not architect around them beyond what ADR 0009's data
format already permits.

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

- [ ] Two characters with the full §4.5 moveset — *9 of 10; the special is unimplemented*
- [ ] Block, hitstun, blockstun, knockdown, wakeup all correct — *knockdown and wakeup unbuilt*
- [x] Best-of-three rounds with timer and win conditions
- [x] Local versus on two gamepads — *one pad validated; never tested with two*
- [ ] Training mode: hitbox display, frame data readout, input display — *1 of 3*
- [ ] 50+ replay tests passing — *9 of 50*
- [x] Desync CI green across three platforms
- [ ] **It is fun** — *nobody has played it*

Status is mirrored in `ROADMAP.md`, which also carries what to work on next.
This list is the authority on what v1 *means*; that file tracks where it stands.

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
