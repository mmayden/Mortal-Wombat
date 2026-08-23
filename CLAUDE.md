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
Every change goes through a PR that passes six checks: three platform builds,
the desync comparison, the sim-boundary script, and clang-format.

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

## State of the build

**Working:** fixed-point sim at 60Hz, jumping with fixed arcs and jump attacks, seeded RNG, input-as-data, fixed-timestep
loop with interpolation, SDL3 window and rendering, keyboard and gamepad input,
TOML frame data for Frenchy and Wisdom, walking, crouching, blocking, all eight
ground normals, hit detection, damage, hitstun, blockstun, pushbox separation,
round flow, the `F1` hitbox overlay, per-state fighter tinting, a deadzone camera, and
`--input-test` for checking which device produced which input.

**Not built:** the special-move input parser (`B,F+HP`), knockdown and wakeup
(two of the sixteen states in DESIGN.md §4.2 are still unreachable), throws,
audio, netcode, and every tool under `tools/`.

**Unvalidated:** the gamepad path compiles and detects zero pads correctly, but
no pad has ever been connected to this machine. Do not describe it as working.

**Nothing is animated.** Fighter state is shown by tint, an extended limb while
attacking, and a guard plate while blocking. DESIGN.md §5.1 asks for a debug
text label instead; there is no font path yet (ADR 0012 puts Dear ImGui in the
debug UI), so the tinting substitutes for that line rather than replacing it.

---

## Open questions — do not invent answers to these

Each is flagged in code or docs where it bites. They need a human decision.

1. **The move count.** `DESIGN.md` §4.5 says "Twelve moves total"; its table
   lists ten. The table is implemented because it carries the numbers.
2. **`DESIGN.md` §5.4** still has silhouette, special move, and personality as
   TODO for both characters, and says not to invent them. This gates the
   special-move parser: a special that "fits the character" needs a character.
3. **`DESIGN.md` §5.5**, the stage, is untouched TODO.
4. **Every hitbox coordinate is provisional.** `DESIGN.md` specifies fighter
   height and nothing else about shape. The stack decision says build
   `tools/framedata_editor/` *before* authoring content; the current numbers
   exist only to reach the first connecting hit.
5. **The `PROVISIONAL` block in `src/sim/constants.h`** is the list of engine
   values no design document specifies. It should shrink, never grow.
6. **Nobody has played it.** Feel is the one thing the test suite structurally
   cannot check, and `BLUEPRINT.md` calls the sixty-second manual play the
   primary regression gate for it.
