# Working notes for Claude Code

**`AGENTS.md` is the contract — read it first.** This file is the practical
layer underneath it: environment quirks, traps that have already cost time, and
the questions that are still open. Nothing here overrides `AGENTS.md` or an ADR.

---

## This machine

Visual Studio 2022 Community ships CMake and Ninja but does **not** put them, or
the MSVC toolchain, on `PATH`. `tools/dev.ps1` imports the developer
environment and then runs whatever you pass it:

```
powershell -File tools/dev.ps1 cmake --preset debug
powershell -File tools/dev.ps1 cmake --build --preset debug
powershell -File tools/dev.ps1 ctest --preset debug
```

`pwsh` is not installed — use `powershell`. On Linux/macOS and in CI, call
`cmake`/`ctest` directly.

**Run `tools/verify.ps1` before pushing.** It does format, sim boundary, build,
tests, and a replay-drift check — everything CI can check on one machine, in the
order a failure is cheapest to fix. `-Fix` reformats in place.

It exists because scripted edits kept skipping clang-format and CI kept catching
it two minutes after a push. `git config core.hooksPath tools/hooks` makes it
run on every push.

**clang-format is pinned at 22.1.8** and the check blocks merges. On this
machine the pip-installed binary lives at:

```
C:/Users/mj/AppData/Roaming/Python/Python310/Scripts/clang-format.exe
```

---

## The workflow is gated, including for you

`main` is protected with `enforce_admins: true`. **You cannot push to `main`.**
Every change goes through a PR that passes seven checks: three platform builds,
the desync comparison, the release-determinism build, the sim-boundary script,
and clang-format.

```
git checkout -b feat/whatever
# work, verify locally
gh pr create --title "..." --body "..."
gh pr checks <n> --watch
gh pr merge <n> --merge --delete-branch
```

That constraint is deliberate (`BLUEPRINT.md` Phase 0.3): the moment an
exception is carved out, the gate stops meaning anything.

---

## Expect Linux CI to fail when your Windows build is green

Development happens on MSVC; the strictest compiler in the matrix is GCC. Three
separate defects have reached CI green-on-Windows and red-on-Linux:

| Symptom | Cause |
|---|---|
| `-Werror=class-memaccess` | `memset` on a type with a user-provided default constructor. MSVC accepts it. |
| `-Werror=conversion` on toml++ | Constructing a `node_view` from a table is an ambiguous conversion under GCC. |
| Configure fails outright | A dependency declaring `cmake_minimum_required(VERSION 3.0)`; CMake 4.x removed <3.5 support, VS bundles 3.30 which only warns. |

Two habits that pay for themselves: prefer a `static_assert` over relying on a
compiler warning (the `is_trivial_v` assertions in `state.h` exist because
`is_trivially_copyable_v` alone missed the bug), and check a new dependency's
`cmake_minimum_required` before adding it.

---

## Traps that have already cost time

**Input is ignored for the first 90 frames.** `advance_frame` only acts on
player input while `round_phase` is `Fighting`, and the round-start freeze is 90
frames. A test or scenario that starts walking at frame 0 has not moved by frame
90 — and this fails *silently*: the fighters simply never close, every attack
whiffs, and the replay reproduces it perfectly forever. `tests/replay/scenarios.h`
waits it out via `FREEZE_FRAMES`.

**Re-record replays in the same commit as the change that invalidated them.**

```
build/debug/bin/mw_replay_record.exe
```

It prints what changed and where it first diverged. A separate "fix tests"
commit destroys the signal that makes replays worth having (`AGENTS.md` rule 8).

**`--screenshot` redraws before capturing** because `SDL_RenderReadPixels` reads
the back buffer, whose contents are undefined after `SDL_RenderPresent`. Do not
"simplify" that redraw away — it silently returns the previous frame, which is
exactly enough to make a hitbox look inactive on the frame it connected.

**Heredocs mangle backslashes in this environment.** Writing C or Python that
contains `\n` through a bash heredoc has repeatedly produced a literal newline
inside a string literal. Use the Write tool for such content, or build the
escape from character codes (`chr(92)`).

**A wrong assumption is never in one place. Grep for its siblings.**

`FighterState::Attack` was used as a proxy for "this fighter is attacking" in
three places. A jump attack runs while the state is `Airborne`, so all three
were wrong: the renderer hid its limb, the debug overlay hid its hitbox, and hit
resolution skipped it entirely — the move could not touch anyone from any range
at any timing.

The first two were found and fixed *in isolation*, weeks apart, without anyone
asking where else the assumption lived. Both fixes were correct and neither was
sufficient. When a bug turns out to rest on a wrong assumption, the fix is not
done until you have searched for every other site holding it.

**Diagnostics must not share assumptions with the code they diagnose.**

The `F1` hitbox overlay exists to answer "why did that miss?". It was blind to
jump attacks for exactly the same reason the sim was — so the one tool that
should have caught the bug instead reproduced it, and looked correct doing so.
A debug view derived from the same wrong premise as the code is worse than none:
it actively confirms the mistake.

**Never name a header after a standard C header.** `src/` is on the include
path for every target and every dependency built through it. A header briefly
named `src/assert.h` shadowed the C standard `<assert.h>` — toml++ included it, got ours, and the
build failed inside somebody else's code. Same hazard for `math.h`, `time.h`,
`string.h`, `stdio.h`. Everything at that level takes the `mw_` prefix — `src/mw_log.h` is the one
that remains.

It surfaced only on a **clean** build: an incremental build had no reason to
recompile the file that included it, so local testing was green.

**Scripted edits skip the formatter.** Files edited with `sed`/Python are just
as easy to forget as they are to change. Run clang-format over the whole branch
before pushing.

---

## Where things live

| Need | Place |
|---|---|
| **What to work on next, and what is blocked** | `ROADMAP.md` |
| The mechanics explained without jargon | `docs/MECHANICS.md` |
| What to try when playing it | `docs/PLAYTEST.md` |
| What the game is, how it must feel | `docs/DESIGN.md` (§3 is the anti-drift anchor) |
| Settled technical decisions | `docs/decisions/README.md` (index) — **do not relitigate** |
| Module map, the sim boundary, `advance_frame` step order | `docs/ARCHITECTURE.md` |
| The C++ subset, naming, commit format | `docs/CONVENTIONS.md` |
| The TOML contract (versioned) | `docs/framedata_schema.md` |
| The development process being followed | `docs/BLUEPRINT.md` |

The single rule everything resolves against:

> If it affects the outcome of a match, it is in `GameState` and it is
> deterministic. If it does not, it is in the render layer and it never touches
> the sim.

---

## What this file does not own

Two sections used to live here that belong elsewhere, and keeping local copies
is precisely how four documents ended up disagreeing about what was built.

**State of the build → [`ROADMAP.md`](ROADMAP.md).** What works, what does not,
and what is unvalidated. One of those is worth repeating because it is a trap
rather than a status: **the gamepad path is now partly validated, and the parts
still unproven are the ones that fail quietly.**

Real pads were connected and played on 2026-08-24 -- one first, then two at
once. Both are detected, each drives its own fighter, and the earlier bug where
a single pad claimed both slots does not recur at either count. Two-player local
versus on pads works.

One thing that run did *not* establish, because playing structurally cannot:

**Which physical button produces which attack.** A pad that moves and swings
feels like it works even if kick and punch are transposed -- the player simply
learns the wrong buttons and never reports it. `--input-test` is the only thing
that answers it, printing the decoded input per player next to each fighter's
position and state. `docs/PLAYTEST.md` §0 has the procedure.

Until someone runs that, the honest claim is "two pads, two players, movement
and attacks come out" -- not "the mapping is right".

**Open questions → [`drawing-board/RULESET.md`](drawing-board/RULESET.md)** for
mechanics still being decided, and `ROADMAP.md` for what they block. The rule
that governed the old list still stands and is the reason to point at all:

> **Do not invent an answer.** If a design document does not specify a number,
> it goes in the `PROVISIONAL` block in `src/sim/constants.h` with a note saying
> what still needs deciding. That block should shrink, never grow.

The one open question that is genuinely this file's business, because it is
about how work gets verified here rather than about the game:

**It has been played; its feel has not been judged.** Those are different, and
conflating them is how a guessed number becomes a settled one.

Sessions on 2026-08-24 covered keyboard, one pad and two. They found a real
camera defect that no test had caught -- which is the argument for playing, made
concretely. What they did not produce is an answer to any question in
`docs/PLAYTEST.md`: whether attacking is risky enough, whether jumping is worth
it, whether blocking is worth doing.

Feel is the one thing the test suite structurally cannot check, and
`BLUEPRINT.md` makes the sixty-second manual play the primary regression gate
for it. Every number in the `PROVISIONAL` block stays a guess until those
questions are answered.
