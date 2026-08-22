# Mortal Wombat

A 2D one-on-one fighting game in the mechanical style of *Mortal Kombat II* —
five buttons, a dedicated block button, fixed jump arcs, best-of-three rounds.
The cast are wombats and the tone is deadpan; the fighting system plays
completely straight.

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

The game runs: `build/debug/bin/mortal_wombat`. Two fighters walk, crouch,
block, and attack with all eight ground normals; hits connect, deal damage, and
apply hitstun and blockstun; rounds resolve on KO or timeout. `F1` shows the
hitbox overlay.

Not built yet: jumping and the airborne states, the special-move input parser,
and throws.

`--frames N` runs N simulation frames and exits, so CI can boot the real binary
headless via `SDL_VIDEODRIVER=dummy`; `--screenshot PATH` captures the final
frame.

Two extra binaries come out of the build. `mw_replay_record` regenerates the
committed recordings — run it when a deliberate behavior change invalidates
the replay tier, and commit its output in the same commit as the change.
`mw_desync_probe` prints per-frame state hashes and is what the CI desync job
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
| [AGENTS.md](AGENTS.md) | Entry point for any contributor, human or agent |
| [docs/DESIGN.md](docs/DESIGN.md) | What the game is and how it must feel |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the code is shaped |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | The C++ subset, naming, commits |
| [docs/framedata_schema.md](docs/framedata_schema.md) | The TOML contract |
| [docs/decisions/](docs/decisions/README.md) | ADRs — settled, not relitigated. Start with the index |
| [docs/BLUEPRINT.md](docs/BLUEPRINT.md) | The development process this repo follows |

---

## IP

Mortal Kombat is Warner Bros. property. This project uses no MK characters,
names, assets, sound, or trade dress. "Mortal Wombat" is a parody title over an
original cast in a similar mechanical style. Mechanics are not copyrightable;
specific characters and assets are.
