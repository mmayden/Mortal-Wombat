# Divided States — Design Document

>
> **Status by section**, so nobody has to guess which parts are safe to build to.
>
> | Section | State |
> |---|---|
> | §1 §2 §3 §5 §6 §8 §9 §10 | **Binding.** §3 carries the thesis and is the anti-drift anchor. |
> | §4.1 §4.2 §4.3 | **Binding.** §4.1 is ADR 0021. |
> | §4.4 | **Binding in shape, provisional in numbers** — tune freely, re-record replays in the same commit. |
> | §4.5 | **Half superseded.** Its *move list* predates six buttons and is dead; its *frame values* are live and are what the game currently runs on. |
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

Divided States is a 2D one-on-one fighting game: six attack buttons, hold back
to block, grounded neutral, best-of-three rounds. It is set across a divided
United States, with each stage a different location in it, and is built on a
deterministic simulation for rollback netplay. The v1 target is two complete
characters that feel good to play.

**Tone is TODO and deliberately so.** Whether this is satire, comedy or played
straight does not change a single mechanic, and nothing in this document should
be read as having decided it.

---

## 2. Core loop

Two fighters face each other on a single-screen stage — **one per side, no tag
and no assists** (ADR 0022). Each has a health bar and a round timer. Players attack, block, and jump to reduce the opponent's
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

**The fighting system plays straight**, whatever the tone turns out to be.
Nothing in the mechanics is a gag: no random damage, no joke moves that break
the game state, no fourth-wall input.

This survives the change of setting because it was never really about the old
one. It is a *determinism* constraint wearing a tonal hat — random damage is
exactly the thing that makes a match irreproducible, and a mechanic that exists
for a laugh is a mechanic nobody can play around. A funny game that plays badly
is a bad game, and so is a bleak one.

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

- **No air blocking.** A fighter in the air cannot block anything, so jumping
  is a committed gamble. This is a root axis and it forecloses the air-dasher
  family deliberately (ADR 0022).
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

### 4.5 Starting frame data — ⚠️ HALF SUPERSEDED

**Read this before using the table. Half of it is dead and half of it is live,
and which half you are looking at matters.**

| | |
|---|---|
| **The move list is dead** | It describes a four-attack scheme, because there were no medium buttons when it was written. The real move set is **fourteen** — six buttons standing, six crouching, a jump attack and a special — and it lives in `data/characters/*.toml` under `framedata_schema.md` v2. **Do not treat this list as the moveset.** |
| **The frame values are live** | Startup, active, recovery, damage, hitstun and blockstun are what `data/characters/*.toml` currently ships and what the replay recordings encode. `ARCHITECTURE.md`, `framedata_schema.md` and `MECHANICS.md` all cite them, correctly. **Tune them freely** — they were always marked as starting points, and §4.4's re-record rule applies. |

The distinction matters because four other documents point here for numbers. A
blanket "superseded" would have told those readers to ignore values the game is
actually running on.

The dead half was closed on 2026-08-26. The eight missing definitions — MP and
MK, standing and crouching, for two characters — are **interpolated between the
light and heavy of the same limb**, which is no more of a guess than the values
on either side of them, and every one was checked in `ds_framedata_viewer`
before it landed. That tool was built first precisely so they could be: the jump
attack shipped with its hitbox at standing-punch height, unable to touch anyone
from any range at any timing, because four integers are plausible in a text file
and obvious in a picture. A test now asserts that every move can connect with
somebody, which is the guard that bug never had.

The table below is therefore kept, not archived. Its rows are the live starting
shape for light and heavy; medium sits between them and is only in the data.

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

- **Rig reuse** — the cast shares a skeleton, so character two costs a
  fraction of character one
- **Iteration is re-render, not redraw** — critical while tuning frame data
- **Free consistency** — lighting, proportions, and camera identical forever
- **Resolution-independent** — sprite size can be decided late

Rejected: hand-drawn pixel art (300+ frames per character is an animation-skill
bottleneck, and timing changes mean redrawing) and digitized live actors (MK's
own method, but lighting consistency across sessions is fragile and per-frame
cleanup is brutal).

### 5.3 Visual tone

- Chunky, exaggerated, low-detail. Silhouette is **TODO** (§5.4) — the previous
  entry described wombats, which are no longer the cast. What is kept is the
  *principle*: a shape that reads instantly at a glance and at speed —
  forgiving to model and animate, and the silhouette reads at small sizes.
- Deadpan presentation. The characters take themselves completely seriously.
- Period-appropriate UI: chunky health bars, a large centered timer, heavy
  drop-shadowed text.

### 5.4 The cast — PARTIALLY DECIDED

**Names exist so the files have something to be called. Everything else is
undecided — do not invent it.**

The two v1 characters are **George** and **Sue**. Character ids in code and data
are `george` and `sue`.

**They are placeholders, chosen to unblock the data files, and carry no
characterisation.** The previous names came from a 2024 prototype whose premise
has been dropped entirely; nothing of it is inherited. Do not read a personality
into "George" and "Sue" — there is not one yet, and inventing one here is what
§5.4 exists to prevent.

§5.4 asks for four things per character before art begins. One is filled in:

| | George | Sue |
|---|---|---|
| Name | **George** | **Sue** |
| Silhouette concept | **TODO** | **TODO** |
| One special move | **TODO** | **TODO** |
| Personality, one line | **TODO** | **TODO** |

The three TODO rows still bind: **do not invent them.** They are needed before
art begins, not before code — the frame-data loader and the character TOML
files can be built against the names alone, since every mechanical property
comes from `docs/framedata_schema.md` rather than from characterisation.

One open question the names raise: both characters get the **same** move set in
v1, which is correct for v1 scope — two characters exist to prove the engine
feels good, not to prove a matchup. Whether George and Sue eventually differ
mechanically is a post-v1 question (§7). The size of that shared set follows
from §4.1's six buttons and is being re-authored; §4.5's move list is
superseded, though its frame values are still live.

### 5.5 Stages — PARTIALLY DECIDED

**Settled:** stages **scroll**, in the manner of any conventional 2D fighter,
and the set of them is **locations across the United States** — the game's title
made literal. That matches what the code already does: the stage is 960 units
wide against a 480-unit screen (§4.4) and the camera tracks the fighters across
it.

**Still TODO, and not to be invented:** which locations, what each one looks
like, how many layers deep the background goes, and whether anything in a stage
is interactive.

**One stage for v1** regardless (§6). More locations are content work, and §6 is
explicit that a second character — or a second stage — only earns its cost once
the first pair play well. Until a location is chosen, a flat coloured backdrop
stands in.

A note for whoever picks the first one: it is a **feel** decision as much as an
art one. The floor plane and the depth cues are what make a fighter's position
readable at a glance, which is the readability work ADR 0017 puts in scope.

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

## 9. IP — what may be borrowed, and what may not

**This section used to carry a real constraint and no longer does.** The project
was once called "Mortal Wombat", a parody of a Warner Bros. property, which
meant every naming decision had to be checked against someone else's trademark.
That is gone: "Divided States" is an original title over an original cast, and
ADR 0018 had already dropped the mechanical inheritance.

What remains is the ordinary rule, kept because it is the reason the old
constraint was survivable: **mechanics are not copyrightable; specific
characters, names, assets and trade dress are.** Frame data, a six-button
layout, hold-back blocking and a scrolling stage may all be studied from any
game freely. Art, audio, names and likenesses may not.

The one live obligation is the dependency licences, not the design — see
`README.md`.

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
