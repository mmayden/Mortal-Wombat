# 0018 — The Mortal Kombat II basis is dropped

**Status:** Accepted
**Date:** 2026-08-24
**Supersedes:** the MK2 framing throughout `0001-0013-stack.md` and in
`DESIGN.md` §1, §3 and §4
**Does not affect:** ADRs 0001–0013 themselves, all of which stand

## Context

The project began as a Mortal Kombat II clone, chosen as a deliberately simple
starting point rather than as a design goal. `DESIGN.md` §4 and the stack
decision's title both carry that inheritance.

The goal has changed. Rather than inherit one game's mechanics wholesale, the
ruleset is being chosen mechanic by mechanic from what the genre has learned,
using research kept in `drawing-board/`.

The stack decision is append-only, as merged ADRs must be, so its title and its
MK2 asides cannot be edited. This ADR records the supersession instead.

## Decision

**MK2 is no longer the basis for any mechanical decision.** Where `DESIGN.md`
or the stack decision describes a mechanic as inherited from MK2, that
inheritance is void, and the mechanic is decided on its own merits or is open.

Concretely, three MK2 inheritances are already reversed:

| Was, from MK2 | Now | Because |
|---|---|---|
| Block is a dedicated button | **Hold back to block** | A block button deletes the entire left/right mixup axis by giving every attack the same defensive answer |
| Five buttons, four of them attacks | **Six attack buttons** | Blocking no longer costs a button, and six is what sticks and leverless controllers are laid out for |
| One control scheme | **Classic and Modern** | Execution accessibility as a growth lever |

## What survives, and why that matters

**Every ADR from 0001 to 0013 stands unchanged.** They follow from "2D fighter
with rollback netcode", not from MK2 — fixed-point determinism, the flat POD
`GameState`, no ECS, no physics engine, TOML frame data, the test tiers, the
desync matrix. Two of them, 0002 and 0005, have since been reinforced rather
than weakened: rollback is now a confirmed requirement rather than an
assumption.

**Every line of the simulation survives too**, except the block input. Walking,
jump arcs, hit resolution, hitstun and blockstun, pushboxes, round flow and the
entire test and CI layer are all mechanically neutral.

That the basis could change without disturbing the architecture is the clearest
evidence available that the boundary in `ARCHITECTURE.md` §1 was drawn in the
right place. The parts that changed were the parts that were supposed to be able
to change.

## Consequences

- `DESIGN.md` §4 stays under revision until `drawing-board/RULESET.md` is
  complete, at which point it graduates wholesale rather than piecemeal.
- §4.6's cut list is void as written. It cut combo strings and cancels at a time
  when the game had no meter, no active defence and no commitment release. Those
  are now open questions rather than settled exclusions.
- The IP position in `DESIGN.md` §9 is *strengthened*, not weakened. The game is
  no longer patterned on a specific Warner Bros. title at all — only the parody
  title remains, over an original cast, in a genre nobody owns.
- The title of `0001-0013-stack.md` remains "MK2-style" and is now inaccurate.
  That is the cost of an append-only record, and it is the right trade: the
  document says what was believed when the stack was chosen, and this ADR says
  what changed.
