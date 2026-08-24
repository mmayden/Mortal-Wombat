<!--
  Delete any section that does not apply. The checklist is the project's real
  gates, not ceremony -- every line is something that has actually gone wrong
  here at least once.
-->

## What and why

<!-- What changed, and what problem it solves. Prefer the consequence over the
     mechanism: "hits now connect" over "added resolve_hits()". -->

## How it was verified

<!-- Not "tests pass" -- CI says that. What did you check that CI cannot?
     Ran the game? Captured a frame? Measured something? Deliberately broke it
     to prove the test fails? -->

---

## Checklist

- [ ] `powershell -File tools/verify.ps1` passes locally
- [ ] Every behaviour change has a test (AGENTS.md rule 6)

**If this touches `src/sim/`:**

- [ ] No float, allocation, `std::` container, clock read, I/O, or unordered iteration below the boundary
- [ ] `GameState` still satisfies `is_trivial_v` and the no-implicit-padding assertions
- [ ] The `advance_frame` step order in `ARCHITECTURE.md` §5 is unchanged, or the doc changed with it

**If behaviour or frame data changed:**

- [ ] Replays re-recorded **in this PR**, never in a follow-up (AGENTS.md rule 8)
- [ ] The recordings actually exercise the change — a scenario whose attacks whiff reproduces perfectly and proves nothing

**If it adds a constant:**

- [ ] It cites a design document, or it lives in the `PROVISIONAL` block in `constants.h` saying what still needs deciding

**If it adds a dependency:**

- [ ] Asked first (AGENTS.md rule 4), and landed **alone** ahead of the code using it
- [ ] Its `cmake_minimum_required` is ≥ 3.5 — CMake 4 rejects anything older, and the bundled VS CMake only warns

**If it settles a design decision:**

- [ ] The rule is in `DESIGN.md`, the reasoning is in an ADR, and `drawing-board/` is trimmed to a pointer — all in this PR (ADR 0023)
- [ ] The ADR alone answers "why not the other option" — it may cite research, but a reader who cannot open that file still understands the choice

**If it changes `docs/framedata_schema.md`:**

- [ ] `schema_version` bumped and both consumers updated in this PR (AGENTS.md rule 5)
