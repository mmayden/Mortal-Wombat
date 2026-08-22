# Architecture — Mortal Wombat

> **Status:** v1, binding
> **Derives from:** `decisions/0001-0013-stack.md` (ADRs 0002, 0004, 0005, 0009)

This document describes how the code is shaped. It exists so that "where does
this go?" has one answer rather than a per-session guess.

---

## 1. The boundary

Everything in this project is organized around one line:

```
┌───────────────────────────────────────────────────────────┐
│  src/sim/          THE SIMULATION — deterministic          │
│                                                            │
│  fixed-point only · no float · no allocation · no I/O      │
│  no clock · no rand() · no std:: containers · POD state    │
│                                                            │
│  pure function:  advance_frame(GameState, Inputs) -> void  │
├──────────────────────── one-way ───────────────────────────┤
│  src/render/  src/audio/  src/ui/  src/platform/  tools/   │
│                                                            │
│  floats, interpolation, effects, std::string, allocation   │
│  reads GameState · never writes it                         │
└───────────────────────────────────────────────────────────┘
```

**Dependency direction is strictly one-way.** `src/sim/` includes nothing from
any other module. Everything else may include `src/sim/` headers to *read*
state. There is no callback from sim into render, ever — not for sound, not
for effects, not for logging.

**How the render layer learns that something happened.** It does not get told;
it observes. Render diffs the state it drew last frame against the state it is
drawing now, or reads a counter field the sim increments. A hit landing is
`fighter.hitstun_remaining` transitioning from 0 to nonzero, not a
`play_sound()` call from inside the sim.

This is not fussiness. Rollback re-simulates the same frames repeatedly — up to
8x per frame — and any side effect inside the sim fires that many times. Audio
and particle state are the classic rollback pain point, and we avoid the whole
class of problem structurally by keeping them out of `GameState`.

---

## 2. Module map

| Module | Responsibility | Layer |
|---|---|---|
| `src/sim/fixed.h` | `Fixed` — i32.16 fixed-point scalar type | sim |
| `src/sim/math/` | Deterministic math: lookup tables, no `<cmath>` | sim |
| `src/sim/rng.h` | PCG32 seeded RNG, state lives in `GameState` | sim |
| `src/sim/input.h` | `InputFrame` — per-player button bitfield | sim |
| `src/sim/state.h` | `GameState`, `Fighter`, `Projectile` — flat POD | sim |
| `src/sim/hash.h` | FNV-1a over `GameState` bytes — the desync check | sim |
| `src/sim/sim.h` `.cpp` | `advance_frame()` — the whole game, one function | sim |
| `src/platform/` | SDL3 window, event pump, gamepad, timing | platform |
| `src/render/` | Draws `GameState`. Interpolation lives here. | render |
| `src/audio/` | miniaudio playback, driven by observed state change | render |
| `src/ui/` | Health bars, timer, menus, training overlays | render |
| `src/main.cpp` | Wires platform + sim + render. Owns the loop. | app |
| `tools/framedata_editor/` | Separate binary. Reads/writes frame-data TOML. | tool |
| `tools/replay_inspector/` | Separate binary. Steps a replay frame by frame. | tool |
| `tools/sprite_packer/` | Separate binary. Asset pipeline (v2). | tool |

**Tools are separate build targets and never link into the game binary.** They
may freely use floats, `std::string`, allocation, and anything else ADR 0001
forbids below the boundary, because they cannot desync anything. This is what
makes them the safe parallel-agent zone.

---

## 3. Where state lives

**All match-affecting state is in exactly one place:** a single flat
`GameState` struct, trivially copyable, no pointers, no heap.

```cpp
struct GameState {
    Fighter    fighters[2];
    Projectile projectiles[MAX_PROJECTILES];
    int32_t    frame;
    int32_t    round_timer;
    int32_t    round_number;
    int32_t    rounds_won[2];
    RngState   rng;
};
static_assert(std::is_trivially_copyable_v<GameState>);
```

The `static_assert` is load-bearing. It is what makes rollback's `save_state` /
`load_state` a `memcpy` rather than a serialization pass, and it fails the
build the moment someone puts a `std::string` or a pointer in.

**Everything else is derived.** Frame data loaded from TOML is *immutable
config*, read-only during a match, and lives outside `GameState`. Render
interpolation state, audio voices, and particle systems live in the render
layer and are never rolled back.

**Indices, not pointers.** A projectile refers to its owner as
`owner_index` (`int32_t`), never `Fighter*`. Pointers break `memcpy` restore
and they break cross-machine determinism.

---

## 4. The core loop

```
main.cpp
  ├─ platform: poll SDL events -> InputFrame[2]      (data, not polled state)
  ├─ accumulate real time
  └─ while (accumulator >= FRAME_DURATION):
        advance_frame(state, inputs)                 ← the only sim call
        accumulator -= FRAME_DURATION
     render(state, prev_state, alpha)                ← interpolates, read-only
```

**The sim has no `dt`.** Every duration in the game is an integer frame count
at 60Hz. `advance_frame` takes state and inputs and nothing else — no time, no
delta, no clock. This is what makes it replayable.

`alpha` — the fractional position between the previous and current sim frame —
exists only in the render call signature. It never crosses the boundary.

---

## 5. `advance_frame` internal order

The order is fixed, and changing it is a behavior change that requires
re-recorded replays in the same commit:

```
1. Decode inputs         raw bitfield -> intent, per fighter
2. Resolve facing        fighters always face each other
3. State machine tick    Idle/Walk/Jump/Attack/Hitstun/... transitions
4. Apply movement        integer velocity, gravity, committed jump arcs
5. Resolve pushboxes     separate overlapping bodies
6. Resolve hitboxes      active hitbox vs hurtbox -> damage, hitstun/blockstun
7. Clamp to stage        wall bounds
8. Tick timers           round timer, hitstun, blockstun, move counters
9. Round transitions     KO, timeout, round win, match win
10. frame += 1
```

Hit resolution comes after movement so that a hitbox is tested at the position
it actually occupies this frame — the ordering that makes frame data mean what
the frame-data table says it means.

---

## 6. Extension points

### To add a move

1. `data/characters/<name>.toml` — add the `[moves.<name>]` block. Schema is
   `docs/framedata_schema.md`.
2. `src/sim/moves.h` — add the enum entry, if the move needs a distinct
   identity in the state machine.
3. `tests/unit/test_moves.cpp` — assert startup/active/recovery match the TOML.
4. Record a replay exercising it into `tests/replays/`.

**No other file.** If adding a move requires changing the state machine's
structure, the state machine is wrong — report that rather than patching
around it.

### To add a fighter state

Requires an ADR. The v1 state set in `DESIGN.md` §4.2 is closed.

### To add a rendering effect

`src/render/` only. It reads `GameState`, diffs against the previous frame, and
draws. It does not add a field to `GameState` — if you believe it must, then
the effect affects match outcome, and that needs an ADR.

### To add a tool

New directory under `tools/`, new `add_executable` target, own
`CMakeLists.txt`. It may link SDL3 and Dear ImGui. It does not inherit the
sim's build flags — tools build with exceptions and RTTI enabled.

---

## 7. Testing shape

| Tier | Location | What it proves |
|---|---|---|
| Unit | `tests/unit/` | Fixed-point, RNG, input decode, box overlap, damage |
| Smoke | `tests/smoke/` | Boot, 600 ticks, no crash, frame budget held |
| Replay | `tests/replay/` + `tests/replays/*.replay` | Behavior did not change |
| Desync | CI matrix, same replays | Bit-identical across Linux/Win/macOS |

Tests link the sim directly and never boot SDL. That is only possible because
the sim has no platform dependency, which is the practical payoff of §1.

---

## 8. What this architecture deliberately does not have

Recorded so no agent proposes them as improvements:

- **No ECS.** ~10 entities, and rollback wants a memcpy-able struct. (ADR 0005)
- **No physics engine.** AABB overlap and committed jump arcs. (ADR 0006)
- **No event bus or message queue in the sim.** Side effects break rollback.
- **No scripting layer.** Frame data is TOML; behavior is C++.
- **No scene graph.** One stage, two fighters, drawn in a fixed order.
- **No `dt` anywhere below the boundary.** Integer frames only.
