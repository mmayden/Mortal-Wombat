# 0014 — The sim boundary is enforced mechanically, not by review

**Status:** Accepted
**Date:** 2026-08-22
**Relates to:** ADR 0002 (determinism), ADR 0005 (flat POD state), ADR 0011 (testing)

## Context

ADR 0002 forbids floats, allocation, I/O, clock reads, ambient RNG, and
unordered iteration below the sim boundary. ADR 0011 makes the cross-platform
desync test the most important test in the project.

Both were written as rules. Rules of that shape are enforced by a reviewer
noticing an *absence* — that a `float` slipped into a sim file, that a hash map
is being iterated. Noticing an absence is the thing human review is worst at,
and it degrades exactly when review is under load, which is the project's
stated binding constraint (`BLUEPRINT.md` §5.0).

Every rule in question also has the same failure signature: the code compiles,
every local test passes, and it breaks only in a real match between two
different machines. That is the worst possible feedback loop to leave to
attention.

## Decision

Three mechanical guards, all running in CI on every commit.

1. **`tests/check_sim_boundary.py`** greps every file under `src/sim/` for each
   forbidden construct and fails with the rule, the line, and why it matters.
   It runs as a ctest case (label `unit`) and as its own CI job. An escape
   hatch exists — `// sim-boundary-allow: <rule> -- <why>` — so that a genuine
   exception is annotated and reviewable rather than achieved by deleting the
   check.

2. **Padding assertions in `state.h`.** The desync check hashes `GameState`
   byte by byte, so compiler-inserted padding would be uninitialized and would
   differ between machines — reporting a desync on a frame where behavior
   matched exactly. `GameState` therefore carries an explicit `reserved` field,
   and `static_assert`s require each struct's size to equal the sum of its
   members.

3. **`mw_desync_probe` plus the `desync` CI job.** The probe prints a state
   hash for every frame of every committed recording. CI runs it on Linux,
   Windows, and macOS and diffs the three outputs. A divergence names the frame
   it began on rather than reporting only that the platforms disagree.

Where the compiler supports `-mgeneral-regs-only`, the sim also builds with it,
making a float below the boundary a compile error. Support is uneven across
compilers and targets, so this is treated as a bonus rather than the guarantee.

## Consequences

- The reviewer checklist's mechanical items are now redundant with CI. They
  stay in `.claude/agents/reviewer.md` anyway: the reviewer sees a diff before
  CI runs, and catching it there is cheaper.
- `check_sim_boundary.py` is lexical, not a parse. It will not catch a float
  reached through a `typedef` or a macro. It catches the realistic failure —
  someone reaching for a familiar tool without thinking about which side of the
  line they are on — at almost no cost.
- Comment lines are exempt from the check. The sim's own headers name the
  forbidden constructs constantly while explaining why they are forbidden, and
  checking comments would mean deleting the documentation that makes the rules
  followable.
- Adding a field to `GameState` may now fail the build on a padding assertion.
  That is the intended behavior: the fix is to adjust the explicit padding, not
  to raise the expected size.
- The desync job depends on the three-platform matrix, so CI cannot be reduced
  to a single Linux runner without silently removing the project's most
  important test.
