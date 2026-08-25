# Divided States

A 2D one-on-one fighting game: six attack buttons, hold back to block, fixed
jump arcs, best-of-three rounds. Set across a divided United States, with each
stage a location in it.

The whole design answers one question — **"was that worth committing to?"**
Neutral is a distinct phase, and leaving it costs something. You chose a button,
at a distance, at a moment, and you live with it.

Under the hood it is a deterministic fixed-point simulation running at a fixed
60Hz, with all match state in one flat memcpy-able struct, architected for
rollback netcode from day one.

**v1 target:** two complete characters that feel good to play, offline,
rendered as colored rectangles.

---

## Build

Requires CMake 3.25+, Ninja, and a C++20 compiler. On Windows, Visual Studio
2022 with the "Desktop development with C++" workload supplies all three —
`tools/dev.ps1` puts them on PATH for you.

```powershell
# Windows
powershell -File tools/dev.ps1 cmake --preset debug
powershell -File tools/dev.ps1 cmake --build --preset debug
powershell -File tools/dev.ps1 ctest --preset debug
```

```bash
# Linux / macOS
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Before pushing, `powershell -File tools/verify.ps1` runs everything CI can check
on one machine. See [CONTRIBUTING.md](CONTRIBUTING.md).

Presets: `debug` (ASan, and UBSan where the compiler supports it), `release`,
`profile` (Tracy enabled).

Dependencies are fetched by CPM at configure time. Set `CPM_SOURCE_CACHE` to a
persistent directory to avoid re-downloading across build trees.

---

## Test tiers

| Command | Tier | Budget |
|---|---|---|
| `ctest --preset debug -L unit` | Pure logic — fixed-point, RNG, boxes | < 1s |
| `ctest --preset debug -L smoke` | Boot, 600 ticks, no crash | < 5s |
| `ctest --preset debug -L replay` | Recorded inputs → state hash | < 10s |
| `ctest --preset debug -L boundary` | No forbidden construct in `src/sim/` | < 1s |
| `ctest --preset debug` | All of the above | < 5 min |

The game runs: `build/debug/bin/divided_states`. Two fighters walk, crouch,
block, jump and attack; hits connect, deal damage, and apply hitstun and
blockstun; rounds resolve on KO or timeout. `F1` shows the hitbox overlay.

**The design is ahead of the build**, deliberately — six attack buttons and
hold-back blocking are decided (ADR 0021) and the code still implements the
earlier four-attacks-plus-block-button scheme.

**[ROADMAP.md](ROADMAP.md) is the only place that says how far along anything
is.** It is not repeated here, because a status kept in two files is a status
that disagrees with itself within a month.

`--frames N` runs N simulation frames and exits, so CI can boot the real binary
headless via `SDL_VIDEODRIVER=dummy`; `--screenshot PATH` captures the final
frame.

Two extra binaries come out of the build. `ds_replay_record` regenerates the
committed recordings — run it when a deliberate behavior change invalidates
the replay tier, and commit its output in the same commit as the change.
`ds_desync_probe` prints per-frame state hashes and is what the CI desync job
diffs across platforms.

The **desync** tier runs in CI only: the same replays across Linux, Windows,
and macOS runners, asserting bit-identical per-frame state hashes. It is the
most important test in the project — cross-platform divergence is the failure
that ships broken and cannot be debugged after release.

---

## Layout

```
src/sim/        The simulation. Deterministic, fixed-point, POD state.
src/data/       Loads character TOML. I/O lives here, above the sim boundary.
src/render/     Draws GameState. Never writes it.
src/platform/   SDL3 window, input, gamepads, timing.
data/           Frame data and character definitions, in TOML.
tools/          Separate binaries (planned): frame-data editor, replay inspector.
tests/          unit / smoke / replay, plus the sim-boundary check
docs/           Design, architecture, conventions, ADRs.
```

The one rule everything follows from:

> **If it affects the outcome of a match, it is in `GameState` and it is
> deterministic. If it does not, it is in the render layer and it never
> touches the sim.**

---

## Documentation

| File | What it answers |
|---|---|
| [ROADMAP.md](ROADMAP.md) | What is done, what is next, what is blocked |
| [docs/MECHANICS.md](docs/MECHANICS.md) | What the mechanics are, in plain language |
| [docs/PLAYTEST.md](docs/PLAYTEST.md) | What to try when playing it, and what to watch for |
| [AGENTS.md](AGENTS.md) | Entry point for any contributor, human or agent |
| [docs/DESIGN.md](docs/DESIGN.md) | What the game is and how it must feel |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the code is shaped |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | The C++ subset, naming, commits |
| [docs/framedata_schema.md](docs/framedata_schema.md) | The TOML contract |
| [docs/decisions/](docs/decisions/README.md) | ADRs — settled, not relitigated. Start with the index |
| [docs/BLUEPRINT.md](docs/BLUEPRINT.md) | The development process this repo follows |

---

## IP

Original title, original cast. **Mechanics are not copyrightable; specific
characters, names, assets and trade dress are** — so frame data, a six-button
layout and hold-back blocking may be studied from any game freely, while art,
audio, names and likenesses may not. `DESIGN.md` §9 is the full statement.
