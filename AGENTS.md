# Agent Instructions — Mortal Wombat

## Project

Mortal Wombat is a 2D one-on-one fighting game in the mechanical style of
*Mortal Kombat II* — five buttons, a dedicated block button, fixed jump arcs,
best-of-three rounds. It is written in orthodox C++ on SDL3, with a
deterministic fixed-point simulation at a fixed 60Hz that is architected for
rollback netcode from day one. The v1 target is two complete characters that
feel good to play, offline, rendered as colored rectangles.

## Commands

Windows needs the Visual Studio developer environment on PATH. `tools/dev.ps1`
does that for you; every command below is run through it.

| | Command |
|---|---|
| Configure | `powershell -File tools/dev.ps1 cmake --preset debug` |
| Build | `powershell -File tools/dev.ps1 cmake --build --preset debug` |
| Test (all) | `powershell -File tools/dev.ps1 ctest --preset debug` |
| Unit only | `powershell -File tools/dev.ps1 ctest --preset debug -L unit` |
| Smoke | `powershell -File tools/dev.ps1 ctest --preset debug -L smoke` |
| Replay | `powershell -File tools/dev.ps1 ctest --preset debug -L replay` |
| Sim boundary | `python tests/check_sim_boundary.py src/sim` |
| Re-record replays | `build/debug/bin/mw_replay_record.exe` |
| Per-frame state hashes | `build/debug/bin/mw_desync_probe.exe` |
| **Run the game** | `build/debug/bin/mortal_wombat.exe` |
| Headless boot (N sim frames) | `mortal_wombat.exe --frames 600` |
| Capture a frame | `mortal_wombat.exe --frames 200 --screenshot out.bmp` |

Controls, keyboard: P1 `WASD` + `F G C V` + `B` to block. P2 arrows + numpad
`4 5 1 2` + `0` to block. `ESC` quits, `F1` toggles the debug overlay.

Gamepads are supported and hot-pluggable; the first pad connected becomes
player one. Punches on the left face pair (X/Y), kicks on the right (A/B),
block on either shoulder or trigger. Pad input is OR-ed with the keyboard, so
one player can be on a pad and the other on keys with no mode to select.

On Linux/macOS (and in CI) drop the wrapper: `cmake --preset debug`,
`cmake --build --preset debug`, `ctest --preset debug`.

The sim is also exercised entirely headless through the test tiers, which is
possible because it has no platform dependency at all. `--frames` exists so CI
can boot the real binary too: set `SDL_VIDEODRIVER=dummy` and it runs on a
machine with no display.

**Not built yet:** the special-move input parser (`B,F+HP`), knockdown and
wakeup, and throws. Fighters walk, crouch, block, attack with all eight ground
normals, take damage, and suffer hitstun and blockstun; rounds resolve on KO or
timeout.

`F1` toggles the hitbox overlay — blue hurtboxes, red hitboxes (filled while
active), yellow pushboxes. It is the fastest way to answer "why did that
miss?".

**Before pushing, run everything CI runs:**

```
powershell -File tools/verify.ps1        # add -Fix to reformat in place
```

Format, sim boundary, build, tests, and a replay-drift warning, in the order a
failure is cheapest to fix. Install the hook once and it cannot be forgotten:

```
git config core.hooksPath tools/hooks
```

CI still owns what a single machine cannot check: the three-platform matrix,
the desync comparison, and the release build.

**The full suite must stay under 5 minutes and a full rebuild under 60s
(ADR 0010). If you exceed either, that is a bug — report it, do not absorb it.**

## The one rule everything else follows from

> **If it affects the outcome of a match, it is in `GameState` and it is
> deterministic. If it does not, it is in the render layer and it never
> touches the sim.**

Every ambiguous decision in this project resolves against that sentence.

## The sim boundary

```
src/sim/     DETERMINISTIC — fixed-point only, no float, no allocation,
             no I/O, no clock, no rand(), no std:: containers, POD state.
             Pure function: (GameState, Inputs) -> GameState
──────────────────────── one-way ────────────────────────
src/render/  src/audio/  src/ui/  tools/
             floats, interpolation, effects, std::string, allocation.
             Reads GameState. Never writes it.
```

Below the boundary, these are build-breaking errors, not style preferences:

- `float` or `double` of any kind
- `new`, `malloc`, `std::vector`, `std::string`, or any allocating container
- Any clock read, `rand()`, or file/network I/O
- Iteration over an unordered container
- Pointers in `GameState` (use indices)

## Hard rules

1. **Never write to `src/sim/**` without explicit human direction.** The sim is
   a human-led zone permanently. Desync is the ultimate green-but-broken
   failure — it compiles, tests pass locally, and it breaks only in real
   matches between real machines.
2. Never modify anything under `assets/`. Binary, unmergeable, human-only.
3. Never use a C++ feature outside the ADR 0001 subset. No exceptions, no RTTI,
   no inheritance beyond one level, no template metaprogramming, no iostreams.
4. Never add a dependency without asking. `CMakeLists.txt` dependency blocks
   are a serialized hotspot (ADR 0010).
5. Never change the frame-data TOML schema without updating
   `docs/framedata_schema.md` and bumping its version in the same commit.
   The schema is a contract between the sim and `tools/framedata_editor/`.
6. Every behavior change requires a test. No test, no merge.
7. If a task requires editing a file outside your stated scope, **STOP and
   report.** Do not refactor your way there.
8. When a balance change legitimately breaks a replay test, re-record it **in
   the same commit as the change** — never in a separate "fix tests" commit.

## Where to look before deciding anything

- `docs/DESIGN.md` — what the game is, how it must feel, what is cut from v1.
  §3 is the anti-drift anchor for any judgment call about feel.
- `docs/decisions/README.md` — the ADR index. Settled questions;
  **these do not get relitigated.**
  ADRs 0001–0013 cover language, determinism, platform, rendering, entity
  model, physics, netcode, transport, data format, build, testing, support
  libraries, and art pipeline.
- `docs/ARCHITECTURE.md` — module map, dependency rules, and the
  "to add a thing of type X, touch exactly these files" paths.
- `docs/CONVENTIONS.md` — naming, error handling, logging, commit format.
- `docs/framedata_schema.md` — the versioned TOML contract.

## What is safe for an agent to work on

**Green** — tooling and render-layer work, fully parallelizable:
`tools/framedata_editor/`, `tools/sprite_packer/`, `tools/replay_inspector/`,
`src/render/`, `src/audio/`, `src/ui/`, menus, input remapping, training-mode
overlays, test coverage backfill.

**Red** — human-led, serialized, never parallel:
`src/sim/**` (anything), the `GameState` layout, the move state machine, the
frame-data schema, netcode integration, fixed-point math types.

**Agent ceiling for this project is 2, tools only.** See ADR "Scope tier".

## When stuck

Do not guess. Do not invent an API. Report what you tried and stop.

An invented SDL3 or GekkoNet call that compiles is worse than no code, because
it costs review time to discover. "I could not determine X" is a good outcome.
