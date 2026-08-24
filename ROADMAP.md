# Roadmap

**What is done, what is next, and what is blocked.** Start here if you are
picking the project up cold — `AGENTS.md` says how to work, this says what to
work on.

Kept current as work lands. If it disagrees with the code, the code is right and
this file is a bug.

> **Naming note:** the repository directory is `game-dev-system`, after the
> development-process blueprint it was seeded from. The project is **Mortal
> Wombat**. The remote is `github.com/mmayden/Mortal-Wombat`.

---

## Where the project is

The game runs and can be played by two people at one keyboard or on gamepads.
Fighters walk, crouch, block, jump with fixed arcs, and attack with all eight
ground normals plus a jump attack. Hits connect, deal damage, and apply hitstun
and blockstun. Rounds resolve on KO or timeout, best of three.

The verification layer is complete and is ahead of the game: determinism is
proven byte-identical across Linux, Windows and macOS, and proven unchanged
between debug and release.

**Nobody has played it for feel.** That is the gate everything below eventually
runs into.

---

## v1 definition of done

`DESIGN.md` §6 defines what v1 means and carries no status. **This file owns
the status.** Numbering matches that list.

| | Status |
|---|---|
| Two characters with the full §4.5 moveset | **9 of 10** — the special is unimplemented |
| Block, hitstun, blockstun, knockdown, wakeup all correct | **partial** — knockdown and wakeup are unbuilt |
| Best-of-three rounds with timer and win conditions | **done** |
| Local versus on two gamepads | **done, half-validated** — one pad tested, never two |
| Training mode: hitbox display, frame data readout, input display | **1 of 3** — `F1` hitboxes; no readout, no on-screen input display |
| 50+ replay tests passing | **9 of 50** |
| Desync CI green across three platforms | **done** |
| **It is fun** | **unknown — nobody has played it** |

---

## Design phase — active now

The mechanical basis is being redesigned. `DESIGN.md` §4 was written against a
Mortal Kombat II inheritance that has been dropped, and is **marked non-binding
until this settles**. Work lives in `drawing-board/`:

- `RULESET.md` — decisions made and the order of what is next
- `2d-fighter-mechanics-deep-dive.md`, `2d-fighters-breakdown.txt` — research

**Settled so far:** rollback required; hold back to block; grounded; one fighter
per side; six attack buttons; Up-priority SOCD; Classic and Modern schemes, with
Modern a pure input remap over one canonical action set and auto-combos as
scripted canonical inputs.

**Not yet settled, and it gates the rest:** the *thesis* — the single question
the game asks. The research is explicit that the thesis decides the remaining
mechanics, and picking them without one produces a game full of individually
good mechanics that answers nothing.

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

**Reconciling this is the first implementation task once the ruleset settles**,
and it is one change rather than four: the input layer, the block condition at
hit resolution, and an input buffer. It invalidates every replay recording,
which is expected — they encode the old input semantics.

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
readability pass.
- every hitbox coordinate in `data/characters/*.toml`

### 2. Knockdown and wakeup

The last two of the sixteen states in `DESIGN.md` §4.2 that are unreachable,
and a named line in the v1 definition of done. Self-contained sim work with the
frame-data and state-machine plumbing already in place.

### 3. Training mode: frame data readout and input display

`F1` already draws hitboxes. The other two thirds of the DoD line need text on
screen, which the project has no path for yet — ADR 0012 plans Dear ImGui for
debug UI and it is not integrated. **Decide that before starting.**

The console `--input-test` is the input display's ancestor and can be promoted.

### 4. Readability pass  *(in scope per ADR 0017; art is not)*

Making the game easier to *see*, without touching the art pipeline. Cheap,
reversible, and it directly serves the playtest — feel cannot be tuned through a
display that shows nothing happening.

Candidates, roughly by value: hit and block feedback (flash, a few frames of
hitstop), a stage with a real floor and depth cues rather than a flat backdrop,
clearer per-state poses, and a landing squash so the jump reads.

**Not** sprites, atlases, Blender, or anything that pins character proportions —
ADR 0013 owns those and they wait for the v1 definition of done.

### 5. Grow the replay library toward 50

9 of 50. Cheap to add, and the highest-value regression net this project has —
`ADR 0011` calls it the primary one. Every fixed bug should leave a recording
behind. The rule that makes them worth having: a recording must *assert on the
thing it is named after*, or it reproduces perfectly and proves nothing.

### 6. `tools/framedata_editor/`

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

## Blocked on a decision

These cannot proceed without an answer, and inventing one is forbidden by
`DESIGN.md` §10.

| Question | Blocks | Where |
|---|---|---|
| **Twelve or ten moves?** §4.5's prose says twelve; its table lists ten | The moveset DoD line | `src/sim/framedata.h`, `docs/framedata_schema.md` |
| **Cast details** — silhouette, one special move, personality per character | The special-move parser: a special that "fits the character" needs a character | `DESIGN.md` §5.4 |
| **Does the special spawn a projectile?** §4.5 leaves its "active" column blank | Whether `Projectile` and `MAX_PROJECTILES` stay in `GameState` at all | `src/sim/constants.h` |
| **The stage** | Backgrounds, whether it scrolls, camera behaviour | `DESIGN.md` §5.5 |
| **Fighter silhouette** — currently a 32×140 pillar, which contradicts §5.3's "round, heavy, short-limbed" | Art, and the hitbox geometry that follows from it | `src/render/sprite.cpp` |

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
- **Anything not yet decided.** §4.6's old cut list is void (ADR 0018) — it
  excluded combos, cancels and throws under a design that has been dropped.
  `drawing-board/RULESET.md` is the only place that says what is in or out now,
  and most of it is still open.

---

## Recently completed

Newest first. Enough to orient; `git log` has the detail.

| | |
|---|---|
| Jump attack fix | It could not hit anyone; three files shared one wrong assumption |
| Playtest guide | `docs/PLAYTEST.md` — what to look for and what each answer unblocks |
| Process automation | `tools/verify.ps1`, pre-push hook, PR template, Dependabot, weekly CI |
| Camera deadzone | Fixed the view dragging a stationary opponent across the screen |
| Gamepad | Hot-plug, dedupe, `--input-test`; one pad no longer claims both slots |
| Jumping | Fixed arcs, jump attacks, landing recovery |
| Combat | Hitboxes, damage, hitstun, blockstun, pushboxes |
| Frame data | TOML loader, validation, Frenchy and Wisdom as data |
| Bootstrap | SDL3 window, fixed-timestep loop, rendering |
| Harness | Four test tiers, three-platform CI, desync comparison |
