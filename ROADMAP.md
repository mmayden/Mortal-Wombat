# Roadmap

**What is done, what is next, and what is blocked.** Start here if you are
picking the project up cold — `AGENTS.md` says how to work, this says what to
work on.

Kept current as work lands. If it disagrees with the code, the code is right and
this file is a bug.

> **Naming note:** the repository directory on disk is `game-dev-system`, after
> the development-process blueprint it was seeded from. The game is **Divided
> States**; the remote is `github.com/mmayden/Divided-States`. It was called
> *Mortal Wombat* until 2026-08-25 — that name and its whole premise are gone,
> not renamed around.

---

## Picking this up cold

**Paused 2026-08-28.** Read this section, then `## Where the project is`, then
stop — everything else is detail you can reach for when you need it.

| | |
|---|---|
| **What it is** | A 2D fighting game. Six buttons, hold back to block, best of three. `DESIGN.md` §1. |
| **The one question it asks** | *"Was that worth committing to?"* Every design call resolves against it. `DESIGN.md` §3. |
| **What runs today** | Two people can play a full match. Movement, six attack strengths, blocking with high/low, knockdown and the setup after it, throws, rounds. |
| **What is decided but unbuilt** | Just Defend, one commitment release, three meter jobs, a hard combo cap. `DESIGN.md` §4.6. |
| **What is still undecided** | Three ruleset decisions, none load-bearing. `drawing-board/RULESET.md`. Plus the cast, the stage and the fighters' shape — those need the author. |
| **The blocker** | Nobody has judged whether it is any good, and testing it alone is not currently possible. |

**Run it:**

```
powershell -File tools/dev.ps1 cmake --build --preset debug
build\debugin\divided_states.exe
```

Press `F1` first — it shows hitboxes, what each fighter is doing, which frame of
which move, and your recent inputs. Almost every question about "did that work?"
is answerable from it.

**If you change anything, run `tools/verify.ps1` before pushing.** `main` is
protected and takes a pull request with seven passing checks, including for the
repo owner.

**Two things that will save you an hour.** The traps in `CLAUDE.md` are all real
and all cost time once. And the honest summary of where this stands: everything
built is defensible, nothing built is confirmed — the design has been running
ahead of validation, and closing that gap is item 1 below rather than more
features.

---

## Where the project is

The game runs and can be played by two people at one keyboard or on gamepads.
Fighters walk, crouch, block, jump with fixed arcs, and attack with all twelve
ground normals, a jump attack and a throw. Hits connect and deal damage; attacks
come at three heights and the defender's stance has to match; sweeps and throws
knock down, and the loop that follows a knockdown is built. Rounds resolve on KO
or timeout, best of three.

**The mechanical design is essentially complete.** Three of the four systems
`DESIGN.md` §4.6 names are built — heights, knockdown and throws — and the
fourth group (Just Defend, the commitment release, meter, the combo cap) is
decided but unbuilt. Three ruleset decisions remain open, none of them
load-bearing.

The verification layer is complete and is ahead of the game: determinism is
proven byte-identical across Linux, Windows and macOS, and proven unchanged
between debug and release.

**It has been played, but not yet judged, and that gap has widened.** Sessions
across 2026-08-24 to 08-26 covered keyboard, one pad and two, and found four
real defects no test had caught — a camera that dragged the view, six attack
buttons that drew one identical limb, invisible knockdowns, and a strength
spread so narrow that light and medium were the same move.

Every one of those was reported as *"is this working?"*, and every one was real.
**Playtesting has the best defect-finding rate of anything in this project.**

What none of them produced is a verdict on *feel* — whether attacking is risky
enough, whether jumping is worth it, whether blocking is worth doing.
`docs/PLAYTEST.md` asks those, and question 1 is the thesis in a form a person
can feel.

**The honest state of it:** three interacting systems landed on 2026-08-26 and
none has been judged. Everything built is defensible; nothing built is
confirmed.

---

## v1 definition of done

`DESIGN.md` §6 defines what v1 means and carries no status. **This file owns
the status.** Numbering matches that list.

| | Status |
|---|---|
| Two characters with the full v1 moveset | **14 of 15** — all six buttons standing and crouching, plus the jump attack. The special exists as data but has no input to trigger it |
| Block, hitstun, blockstun, knockdown, wakeup all correct | **done** — knockdown and wakeup landed 2026-08-26 (ADR 0026) |
| Best-of-three rounds with timer and win conditions | **done** |
| Local versus on two gamepads | **done** — two real pads, one per player, played 2026-08-24. Button-to-action mapping still unverified (`--input-test`, PLAYTEST §0) |
| Training mode: hitbox display, frame data readout, input display | **done** — all three on `F1` (ADR 0025) |
| 50+ replay tests passing | **11 of 50** |
| Desync CI green across three platforms | **done** |
| **It is fun** | **unknown** — played, but the feel questions in `PLAYTEST.md` are unanswered |

---

## Design phase — the basis is settled, the details are not

The mechanical basis was redesigned and has **landed in `DESIGN.md`**. The
notice at the top of that file gives the binding status of every section. §4.5
is half superseded — its move list is dead, its frame values are what the game
runs on — and §4.7 is void. Everything else in §4 binds.

Remaining detail work lives in `drawing-board/`:

- `RULESET.md` — decisions made and the order of what is next
- `2d-fighter-mechanics-deep-dive.md`, `2d-fighters-breakdown.txt` — research

**The thesis is settled:** *"Was that worth committing to?"* — neutral is a
distinct phase, and leaving it is a priced decision (`DESIGN.md` §3, ADR 0020).

**Settled, and every one recorded as an ADR:**

| Decision | ADR |
|---|---|
| Rollback netcode is required | 0007 |
| The thesis, and the four it decided — Just Defend over parry, one commitment release, three meter jobs, a hard combo cap | 0020 |
| Hold back to block; six attack buttons; Up-priority SOCD; Classic and Modern schemes; input history | 0021 |
| No air blocking — grounded; one fighter per side | 0022 |

`drawing-board/RULESET.md` has the table pointing at where each is specified.

**Still open:** five of the sixteen ruleset decisions, listed in
`drawing-board/RULESET.md`. Their *direction* is now constrained by §3, which is
why several of them got easier rather than merely later.

Everything built survives it — determinism, movement, jump arcs, hit resolution,
stun, rounds, the whole test layer. The only built thing affected is the block
input.

### The design is now ahead of the build

This is the important thing to know when reading anything below. Decided but not
implemented:

| Decided | Built today |
|---|---|
| Six attack buttons | **done** |
| Hold back to block | **done** |
| Two control schemes | One — Classic only; Modern needs input history |
| Input history in `GameState` | Only the current and previous frame |
| Just Defend, one commitment release, three meter jobs, hard combo cap | None of it |

**This is a content change as well as an input change**, which an earlier version
of this file under-counted. The input layer, the block condition at hit
resolution and an input buffer are one coherent change — but **medium punch and
medium kick do not exist in `MoveId`, in the schema, or in either character's
TOML.** That is roughly eight new move definitions, each needing hitbox geometry
that has to be *seen* to be reviewed, which is why the frame-data editor moved
ahead of it below.

It invalidates every replay recording, which is expected — they encode the old
input semantics, and they get re-recorded in the same commit (rule 8).

Nothing else in the simulation is affected. Movement, jump arcs, hit resolution,
stun, pushboxes and round flow are all mechanically neutral (ADR 0018).

**The build items below are paused where they depend on unsettled mechanics.**
Knockdown and wakeup in particular is now a *design* decision first: the deep
dive calls the knockdown loop the engine of a 2D fighter, so it is worth
deciding deliberately rather than implementing the simplest version.

---

## Next, in order

Ordered by what unblocks the most. Each is one branch.

### 1. Play it and report on feel  *(needs a human — nothing else does)*

**`docs/PLAYTEST.md` says what to try and what to watch for.**
`docs/MECHANICS.md` explains the terms behind it.

Everything below is guesswork until this happens. The test suite structurally
cannot check feel, and `DESIGN.md` §3 makes feel the anti-drift anchor.

Specifically unvalidated, all in the `PROVISIONAL` block of
`src/sim/constants.h` or noted beside their code:

- jump distance (1.8× walk speed) and jump startup (3 frames)
- round-start separation (200 units) and the round freezes (45 / 120 frames)
- camera edge margin (90 units)
- knockdown, rise and wakeup-delay durations (40 / 12 / 20 frames)
- every frame value in `data/characters/*.toml`, twice retuned and never felt

**The single biggest obstacle is that you need two people.** Testing the mixup
game — knockdown, high/low, throws — against a stationary opponent is not
possible. A training dummy with a few settable behaviours is the unblocker and
is item 2 below.

**Answered so far:** hit feedback is readable — the character lights up on a
hit, confirmed in play. That lowers the priority of extra hit effects in the
readability pass. The camera no longer shoves the view around, confirmed on
keyboard after the second fix, and the session that first used a real pad
reported nothing wrong with it either.

**Recording a session is now the preferred way to report one of these.**
`--record session.replay` captures what you did, and the file replays exactly.
- every hitbox coordinate in `data/characters/*.toml`

### 2. A training dummy  *(the unblocker for everything above)*

**You cannot test this game alone**, and that is now the binding constraint.
Knockdown, the high/low guess and throws are all *interactions*, and a
stationary opponent cannot participate in one. Every playtest so far has been a
person pressing buttons at a statue.

A dummy with settable behaviour fixes it: stand, block everything, block
standing only (so a sweep gets through), block at random, attack back, jump.
That is enough to feel okizeme, to find out whether a blocked heavy really is
punishable, and to see whether the throw is too strong.

Architecturally it is clean and cannot break anything: **the dummy is an input
source**, producing `InputFrame`s from what it can see. It never writes
`GameState`, so the sim boundary holds and determinism is untouched. It belongs
beside the platform layer's keyboard and pad, not below the line.

Not a CPU opponent. That is a bigger thing and it wants a balanced game first,
which this is not yet.

---

### 3. Readability pass  *(in scope per ADR 0017; art is not)*

Making the game easier to *see*, without touching the art pipeline. Cheap,
reversible, and it directly serves the playtest — feel cannot be tuned through a
display that shows nothing happening.

Candidates, roughly by value: hit and block feedback (flash, a few frames of
hitstop), a stage with a real floor and depth cues rather than a flat backdrop,
clearer per-state poses, and a landing squash so the jump reads.

**Not** sprites, atlases, Blender, or anything that pins character proportions —
ADR 0013 owns those and they wait for the v1 definition of done.

### 4. Grow the replay library toward 50

11 of 50. Cheap to add, and the highest-value regression net this project has —
`ADR 0011` calls it the primary one. Every fixed bug should leave a recording
behind. The rule that makes them worth having: a recording must *assert on the
thing it is named after*, or it reproduces perfectly and proves nothing.

## Blocked on a decision

**These need a human decision.** Every one is a `DESIGN.md` §5 TODO marked
*do not invent*, and none of them is answerable from the design as written.

That is what separates this table from the open decisions in
`drawing-board/RULESET.md`: those are calls the design can make and has not yet;
these are calls only the author can make.

| Question | Blocks | Where |
|---|---|---|
| **Cast details** — silhouette, one special move, personality per character | The special-move parser: a special that "fits the character" needs a character | `DESIGN.md` §5.4 |
| **Fighter silhouette** — currently a 32×140 pillar, which is a guess rather than a decision | Art, and the hitbox geometry that follows from it | `src/render/sprite.cpp` |
| **Which US locations** are stages, and what each looks like | Backgrounds and the readability pass. That stages *scroll* is settled | `DESIGN.md` §5.5 |

---

## Deliberately not being done

Recorded so they are not proposed as improvements.

- **Rollback netplay.** ADR 0007 defers integration until two characters play
  well offline. The architecture supports it; the integration waits.
- **Audio.** ADR 0012 puts it in the render layer, never in the sim. Nothing to
  play yet.
- **Art.** ADR 0013 is explicit that boxes are a decision, not a placeholder
  apology, and that the v1 definition of done comes first.
- **More than two characters.** §6: if two play well, a third is content work.
  If they do not, twelve will not help.
- **Anything not yet decided.** §4.7's old cut list is void (ADR 0018) — it
  excluded combos, cancels and throws under a design that has been dropped.
  `drawing-board/RULESET.md` is the only place that says what is in or out now,
  and most of it is still open.

---

## Recently completed

Newest first. Enough to orient; `git log` has the detail.

| | |
|---|---|
| Feel pass 1 | Widened the light/medium/heavy spread from 1-2 frame gaps to 3-4, cut the round-start freeze in half, and made knockdowns visible |
| Throws | Unblockable, short-ranged, not tech-able. Closes the rock-paper-scissors: attack beats throw, throw beats block, block beats attack |
| Attack heights | Mid, low and overhead, with the stance required to match. Gives the knockdown loop something to threaten with |
| Knockdown and wakeup | Soft and hard, with a timing choice on soft. `FighterState::Blocking` is now the only unreachable state |
| Training readout | `F1` now shows state, move, frame, phase and held buttons — the other half of "I don't know what's happening" |
| Readable attacks | The limb is the move's hitbox now, so all six buttons look different — reported as "I don't know the difference in the attacks" |
| Frame-data viewer | `ds_framedata_viewer` draws the hitboxes, and `test_reach` asserts every move can connect |
| Session recorder | `--record` captures a played session as a replay file, so a feel report stops being guesswork |
| Camera, second fix | Backing away walked you off the left edge; the render layer had no tests at all |
| Jump attack fix | It could not hit anyone; three files shared one wrong assumption |
| Playtest guide | `docs/PLAYTEST.md` — what to look for and what each answer unblocks |
| Process automation | `tools/verify.ps1`, pre-push hook, PR template, Dependabot, weekly CI |
| Camera deadzone | Fixed the view dragging a stationary opponent across the screen |
| Gamepad | Hot-plug, dedupe, `--input-test`; one pad no longer claims both slots. Validated on real hardware at one pad and two |
| Jumping | Fixed arcs, jump attacks, landing recovery |
| Combat | Hitboxes, damage, hitstun, blockstun, pushboxes |
| Frame data | TOML loader, validation, George and Sue as data |
| Bootstrap | SDL3 window, fixed-timestep loop, rendering |
| Harness | Four test tiers, three-platform CI, desync comparison |
