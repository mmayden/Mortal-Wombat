# Playtest guide

**What to look for, and what an answer unblocks.** Feel is the one thing the
test suite structurally cannot check, and `DESIGN.md` §3 makes it the anti-drift
anchor for the whole project. Every number below is provisional — a guess made
to get the engine running — and none of them can be settled any other way.

```
build\debug\bin\mortal_wombat.exe
```

`F1` shows hitboxes. Two people at one keyboard, or gamepads.
Controls are in `AGENTS.md`.

You do not need to answer everything. **"That felt bad" is a complete and useful
answer** — the last two real bugs both came from a rough description of
something looking wrong, not from a precise report.

---

## 1. Commitment — the thing the game is about

`DESIGN.md` §3 asks for *deliberate and weighty*: committing to an action means
committing, and whiffing a heavy should genuinely hurt.

- Throw a **high kick** (`V` / B button) at nothing. Does the recovery feel long
  enough to be punished, or can you get away with it?
- Can your opponent actually **react** and punish a whiffed heavy, or is it over
  before they can move?
- Does a **light punch** feel meaningfully safer than a heavy?

> If heavies feel free, the game's core promise is broken.
> **Unblocks:** whether the §4.5 frame data is close, or needs a real pass.

## 2. Jumping

Fixed arcs, no air control, committed at takeoff (§4.3). Three provisional
numbers live here.

- Hold up and jump. Does the **3-frame startup** let a defender see it coming,
  or does it feel unresponsive to the jumper?
- Jump forward. It travels **1.8× walk speed** — is a forward jump worth its
  landing recovery, or would you always rather walk?
- Can a grounded defender **anti-air** a jump-in, or is jumping in strictly the
  best option? *(This one matters most — a fighting game where jumping always
  wins has no ground game.)*
- Does the jump feel too **floaty** or too **heavy**? Apex is 90 units over 44
  frames.

> **Unblocks:** `JUMP_STARTUP_FRAMES`, `JUMP_HORIZONTAL_SCALE`, and whether the
> jump attack's new hitbox is right.

## 3. Blocking

Block is a button, not hold-back (§4.1). Chip damage is cut (§4.6), so a
blocked hit costs the defender only time.

- Does blocking feel **worth doing**, or is walking away better?
- After blocking a heavy, are you at a disadvantage — and does that feel fair?
- Can you tell **you are blocking**? The fighter goes pale with a guard plate on
  its leading edge; is that readable at speed?

> **Unblocks:** whether the blockstun numbers in §4.5 hold up.

## 4. Reading the screen

Everything is placeholder boxes on purpose (ADR 0013), but you still have to be
able to *see* what happened.

- When you get hit, can you **tell** — is the flash enough?
- Can you tell **which move** the opponent threw?
- Can you tell **why** something whiffed? Try it with `F1` on and off.
- Does the **camera** feel stable? It holds still until a fighter nears the edge
  (margin: 90 units).

> **Unblocks:** the camera margin, and how much of the readability problem is
> the placeholder art versus the game itself.

## 5. Pacing

- **90 frames (1.5s)** of freeze before a round starts — too long?
- **120 frames (2s)** on the KO pose — too long?
- Fighters start **200 units apart**. Does the opening approach drag?
- A round is **90 seconds**. Does that feel right for how fast health drains?

> **Unblocks:** `ROUND_START_SEPARATION` and both freeze durations.

## 6. The silhouette

Fighters are **32 × 140** — a narrow pillar. `DESIGN.md` §5.3 asks for wombats
that are "round, heavy, and short-limbed", which is close to the opposite.

The width is specified nowhere; the height is. Seeing them move may make the
right proportion obvious.

> **Unblocks:** `DESIGN.md` §5.4, the hitbox geometry that follows from body
> shape, and the art direction after that.

---

## Reporting

Rough is fine. What helps most:

1. **What you did** — "jumped in from about half a screen"
2. **What happened** — "the other guy walked backwards"
3. **What you expected** — even just "not that"

A report of *"if I move right, the other character moves left towards me"*
found a camera bug that had survived code review and every screenshot taken of
it, because it is invisible in a still image. That is the standard: describing
the wrongness is enough, and diagnosing it is my job.
