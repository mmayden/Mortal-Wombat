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
| Re-record replays | `build/debug/bin/mw_replay_record.exe` |
| Per-frame state hashes | `build/debug/bin/mw_desync_probe.exe` |

On Linux/macOS (and in CI) drop the wrapper: `cmake --preset debug`,
`cmake --build --preset debug`, `ctest --preset debug`.

**There is no game binary yet.** The harness is complete and green; the
bootstrap (window, render loop, input, one fighter on screen) is the next
phase. Until then the sim is exercised entirely headless through the test
tiers — which is possible because the sim has no platform dependency at all.

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
- `docs/decisions/` — settled questions. **These do not get relitigated.**
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
