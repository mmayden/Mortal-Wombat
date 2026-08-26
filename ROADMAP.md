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

## Where the project is

The game runs and can be played by two people at one keyboard or on gamepads.
Fighters walk, crouch, block, jump with fixed arcs, and attack with all eight
ground normals plus a jump attack. Hits connect, deal damage, and apply hitstun
and blockstun. Rounds resolve on KO or timeout, best of three.

The verification layer is complete and is ahead of the game: determinism is
proven byte-identical across Linux, Windows and macOS, and proven unchanged
between debug and release.

**It has been played, but not yet judged.** Sessions on 2026-08-24 covered
keyboard, one pad and two, and found a real camera defect that no test had
caught. What none of them produced is a verdict on *feel* -- whether attacking
is risky enough, whether jumping is worth it, whether blocking is worth doing.
`docs/PLAYTEST.md` asks those, and question 1 is the thesis in a form a person
can feel. That is still the gate everything below eventually runs into.

---

## v1 definition of done

`DESIGN.md` §6 defines what v1 means and carries no status. **This file owns
the status.** Numbering matches that list.

| | Status |
|---|---|
| Two characters with the full v1 moveset | **9 of ≈14** — the special is unimplemented, and medium punch and medium kick do not exist yet (ADR 0021) |
| Block, hitstun, blockstun, knockdown, wakeup all correct | **partial** — knockdown and wakeup are unbuilt |
| Best-of-three rounds with timer and win conditions | **done** |
| Local versus on two gamepads | **done** — two real pads, one per player, played 2026-08-24. Button-to-action mapping still unverified (`--input-test`, PLAYTEST §0) |
| Training mode: hitbox display, frame data readout, input display | **1 of 3** — `F1` hitboxes; no readout, no on-screen input display |
| 50+ replay tests passing | **9 of 50** |
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

**Still open:** six of the sixteen ruleset decisions, listed in
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
| Six attack buttons | Four, plus a block button |
| Hold back to block | Block is a button |
| Two control schemes | One |
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
- round-start separation (200 units) and the round freezes (90 / 120 frames)
- camera edge margin (90 units)

**Answered so far:** hit feedback is readable — the character lights up on a
hit, confirmed in play. That lowers the priority of extra hit effects in the
readability pass. The camera no longer shoves the view around, confirmed on
keyboard after the second fix, and the session that first used a real pad
reported nothing wrong with it either.

**Recording a session is now the preferred way to report one of these.**
`--record session.replay` captures what you did, and the file replays exactly.
- every hitbox coordinate in `data/characters/*.toml`

### 2. `tools/framedata_editor/`

The stack decision says build this **before** authoring content. That advice was
overrun — every hitbox in `data/characters/` was hand-written to reach the first
connecting hit — and the cost has already been paid once.

The jump attack shipped with its hitbox at standing-punch height, floating above
the opponent's head for its entire arc. It could not hit anyone from any range
at any timing. Nobody could see that, because the numbers are plausible in a
text file and the geometry is only obvious when drawn. A visual editor is not a
convenience here; it is the thing that makes frame data reviewable at all.

Pure tooling, no sim contact, cannot desync anything — the best parallel-agent
task in the project. Blocked on nothing.

Worth doing before any real balance pass, and before authoring the special.

---

### 3. Implement the ADR 0021 control scheme

Six attack buttons, hold back to block, and an input buffer in `GameState`. One
coherent change to the input layer and the block condition at hit resolution
— plus **eight new move definitions**, because medium punch and medium kick do
not exist in `MoveId`, in the schema, or in either character's TOML.

That is why the editor comes first. It invalidates every replay recording, which
is expected and gets re-recorded in the same commit (rule 8).

`src/sim/**` is a human-led zone (AGENTS rule 1), so this one needs direction,
not initiative.

---

### 4. Knockdown and wakeup

The last two of the sixteen states in `DESIGN.md` §4.2 that are unreachable,
and a named line in the v1 definition of done. Self-contained sim work with the
frame-data and state-machine plumbing already in place.

### 5. Training mode: frame data readout and input display

`F1` already draws hitboxes. The other two thirds of the DoD line need text on
screen, which the project has no path for yet — ADR 0012 plans Dear ImGui for
debug UI and it is not integrated. **Decide that before starting.**

The console `--input-test` is the input display's ancestor and can be promoted.

### 6. Readability pass  *(in scope per ADR 0017; art is not)*

Making the game easier to *see*, without touching the art pipeline. Cheap,
reversible, and it directly serves the playtest — feel cannot be tuned through a
display that shows nothing happening.

Candidates, roughly by value: hit and block feedback (flash, a few frames of
hitstop), a stage with a real floor and depth cues rather than a flat backdrop,
clearer per-state poses, and a landing squash so the jump reads.

**Not** sprites, atlases, Blender, or anything that pins character proportions —
ADR 0013 owns those and they wait for the v1 definition of done.

### 7. Grow the replay library toward 50

9 of 50. Cheap to add, and the highest-value regression net this project has —
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
