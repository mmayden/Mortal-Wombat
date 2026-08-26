# Frame Data Schema

> **Schema version:** 3
> **Status:** contract — binding on both consumers
> **Consumers:** `src/sim/` (read) and `tools/framedata_editor/` (read + write)

This file is the *only* coupling between the game binary and the frame-data
editor. Both read this schema; neither reads the other's code.

**Changing this file requires bumping `schema_version` below, updating both
consumers, and doing all of it in one commit.** A schema change that lands in
the editor before the sim produces character files the game cannot load, with
no build error to catch it. This is the one place in the project where a
coordination failure is silent.

---

## Version history

| Version | Date | Change |
|---|---|---|
| 1 | 2026-08-22 | Initial schema |
| 3 | 2026-08-26 | Knockdown (ADR 0026). Optional per-move `knockdown` key: `"none"` (the default), `"soft"` or `"hard"`. A v2 file loads unchanged in meaning — every move simply causes no knockdown — but the version still bumps, because a v2 *reader* would silently ignore a key that changes what a move does. |
| 2 | 2026-08-26 | Six attack buttons (ADR 0021). Four new required move keys per character — `medium_punch`, `medium_kick` and their crouching variants — and `low_`/`high_` renamed to `light_`/`heavy_` throughout. A v1 file no longer loads. |

---

## File location and naming

```
data/characters/<character_id>.toml
```

`character_id` is lowercase snake_case and matches the `id` field inside the
file. The two v1 characters are `george` and `sue` (`DESIGN.md` §5.4).

---

## Units — read this before anything else

| Quantity | Unit | Type |
|---|---|---|
| Durations | **integer frames at 60Hz** | `int32` |
| Positions, sizes | logical units (480x270 screen space) | `int32` |
| Speeds | units per frame, in 1/65536ths (i32.16 fixed-point) | `int32` |
| Damage, health | integer points | `int32` |

**There are no seconds, no milliseconds, and no floats in this format.** A
speed of "1.2 units per frame" is written as `78643` (1.2 × 65536), or with
the `fixed()` helper the editor emits. Writing `1.2` is a schema violation and
the loader rejects it — a float in the data file is a float in the sim, and
ADR 0002 makes that a desync.

---

## Top-level document

```toml
schema_version = 3

[character]
id           = "george"
display_name = "George"         # DESIGN.md §5.4

[character.physics]
walk_forward_speed  = 78643      # 1.2 units/frame  (DESIGN.md §4.4)
walk_backward_speed = 65536      # 1.0 units/frame
jump_duration       = 44         # frames, total
jump_apex           = 90         # units
starting_health     = 100

[character.boxes]
# Default boxes, used by any state that does not override them.
standing_hurtbox  = { x = -16, y = -140, w = 32, h = 140 }
crouching_hurtbox = { x = -18, y =  -80, w = 36, h =  80 }
pushbox           = { x = -14, y = -130, w = 28, h = 130 }
```

**Coordinate space.** Origin is the fighter's ground point, between the feet.
`+x` is forward relative to facing; `-y` is up. The loader mirrors `x` for a
left-facing fighter, so authored data is always written facing right.

---

## Moves

One `[moves.<move_id>]` table per move. `move_id` must be one of the keys
below, which correspond one-to-one with the `MoveId` enum in
`src/sim/framedata.h`. An unrecognised key is a load-time error, not a warning:
a typo would otherwise leave a character silently missing a move.

| Key | Move |
|---|---|
| `light_punch` | LP |
| `medium_punch` | MP |
| `heavy_punch` | HP |
| `light_kick` | LK |
| `medium_kick` | MK |
| `heavy_kick` | HK |
| `crouch_light_punch` | Crouching LP |
| `crouch_medium_punch` | Crouching MP |
| `crouch_heavy_punch` | Crouching HP |
| `crouch_light_kick` | Crouching LK |
| `crouch_medium_kick` | Crouching MK |
| `crouch_heavy_kick` | Crouching HK |
| `jump_attack` | Jump attack |
| `special` | Special |

**All fourteen are required.** A file missing one fails to load.

### `knockdown` — optional, per move

`"none"` (the default), `"soft"` or `"hard"`. ADR 0026.

Spelled rather than numbered, because `knockdown = "hard"` says what it means at
the point of use where a `2` would send the reader to a header. Optional because
most moves do not knock down, and requiring the line on all fourteen would be
noise people stop reading.

A **soft** knockdown lets the defender choose when to rise; a **hard** one is
fixed timing and the cleaner setup for the attacker. Any clean hit on an
*airborne* fighter is a soft knockdown whatever this key says — that is a
property of the defender's state, not of the attack, so it lives in the
simulation rather than in the data.

The shipped assignment follows the convention nearly every 2D fighter shares:
the sweep (`crouch_heavy_kick`) and the `special` are hard, everything else on
the ground is none.

The names are **light / medium / heavy**, not low / mid / high. Attack *height*
is a separate axis and is still undecided (`drawing-board/RULESET.md` decision
7); these keys were `low_punch` and `high_punch` until the sixth button landed,
at which point they would have read as "a punch that hits low".

The count is not asserted anywhere in prose. It falls out of the button set —
six buttons times two stances, plus the two that are neither — which is what
dissolved the old "twelve or ten moves?" question rather than answering it.

```toml
[moves.heavy_punch]
input       = "HP"
startup     = 7                  # frames before the first active frame
active      = 3                  # frames the hitbox is live
recovery    = 16                 # frames after active before actionable
damage      = 8
hitstun     = 18                 # frames the opponent is stunned on hit
blockstun   = 12                 # frames the opponent is stunned on block
cancel_into = []                 # reserved: the combo system is undecided

[[moves.heavy_punch.hitboxes]]
frames = [8, 10]                 # inclusive frame range, 1-based within the move
x = 20
y = -90
w = 34
h = 26
```

### Field reference

| Field | Type | Required | Notes |
|---|---|---|---|
| `input` | string | yes | See input notation below |
| `startup` | int ≥ 1 | yes | |
| `active` | int ≥ 1 | yes | |
| `recovery` | int ≥ 0 | yes | |
| `damage` | int ≥ 0 | yes | |
| `hitstun` | int ≥ 0 | yes | |
| `blockstun` | int ≥ 0 | yes | |
| `cancel_into` | string[] | no | **Reserved — must be empty.** The combo system is undecided |
| `hurtbox_override` | box | no | Replaces the default for this move's duration |
| `hitboxes` | array of box+frames | yes | At least one |

**Total move duration is `startup + active + recovery`.** It is derived, never
written. An editor that writes a `duration` field is writing a field the sim
ignores, and the two will disagree the moment someone edits one and not the
other.

**`frames` on a hitbox is 1-based and inclusive**, counted from the first
frame of the move. `frames = [8, 10]` on a move with `startup = 7` means the
hitbox is live for the three active frames — 8, 9, and 10. A range that falls
outside `[startup + 1, startup + active]` is a validation error.

---

## Input notation

| Token | Meaning |
|---|---|
| `LP` `HP` `LK` `HK` `BL` | Buttons (`DESIGN.md` §4.1) |
| `U` `D` `F` `B` | Directions, relative to facing |
| `,` | Sequence separator |
| `+` | Simultaneous |
| `d` `df` | Diagonal down, down-forward |

Examples: `HP`, `D+LK` (crouching low kick), `B,F+HP` (the v1 special —
`DESIGN.md` §4.5).

Motion inputs have a buffer window defined in the sim, not in this file. It is
a global rule, not per-move.

---

## Validation

The loader rejects a file and reports which rule failed. These run at load
time, outside the sim, where failure is recoverable.

1. `schema_version` present and equal to the version this build supports
2. `character.id` matches the filename stem
3. Every duration is a positive integer; no float appears anywhere
4. Every `move_id` maps to a `MoveId` enum entry
5. Every move has at least one hitbox
6. Every hitbox `frames` range lies within the move's active window
7. Every box has `w > 0` and `h > 0`
8. `cancel_into` is empty — reserved until a combo system is designed

`tests/unit/test_framedata.cpp` asserts that both shipped character files pass
every rule, so a hand-edit that breaks the schema fails CI rather than the
game. It also pins the shipped timings to the DESIGN.md §4.5 table: frame data
is the entire feel of a fighting game, so a silent edit to a startup value is a
design change disguised as a data commit, and changing one now requires
changing both.

Each rule additionally has a test that feeds the loader a file violating only
that rule, so the rules are known to reject rather than merely to exist.

---

## What does not belong in this file

- Sprite paths and animation timing — that is the sprite manifest (ADR 0013),
  a separate file, so that frame data can be tuned without touching art
- Sound effects — render layer, never rolled back (`ARCHITECTURE.md` §1)
- Global constants like gravity and stage width — those are `DESIGN.md` §4.4
  and live in `src/sim/constants.h`, shared by both characters
- Anything the sim does not read
