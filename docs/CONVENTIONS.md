# Conventions — Mortal Wombat

> Short and prescriptive on purpose. Ambiguity here shows up as inconsistency
> in the diff, and review is this project's binding constraint.

---

## 1. The C++ subset (ADR 0001)

**Use:** RAII, references, `constexpr`, namespaces, `enum class`, plain
structs, operator overloading (fixed-point types only), templates for
containers only.

**Forbidden in shipped code** — the game binary and every library it links
(`mw_sim`, `mw_data`). Tests and the `tools/` binaries are exempt from the first
two rows and only those: doctest reports failures by throwing, and neither ships
or can desync anything. The exemption is expressed in the build as the
`mw_no_exceptions` / `mw_exceptions` interface targets, not as a convention
anyone has to remember.

| Forbidden | Instead |
|---|---|
| Exceptions (`-fno-exceptions` / `/EHs-c-`) | Return a status enum or `bool` |
| RTTI (`-fno-rtti` / `/GR-`) | Tagged unions, `enum class` discriminants |
| Inheritance beyond one level | Composition, plain structs |
| Template metaprogramming | Write the code out |
| `<iostream>` | `std::fprintf` in tools; the log macros in game code |
| `shared_ptr` as a default | Value types; `unique_ptr` only where ownership is genuinely dynamic |

**Additionally forbidden below the sim boundary** (`src/sim/**`):

`float`, `double`, `new`, `malloc`, any `std::` container, `std::string`,
`<cmath>`, `<chrono>`, `rand()`, file or network I/O, and pointers inside
`GameState`.

Compilers are configured to reject the first two categories. The sim-boundary
rules are enforced three ways, none of which rely on anyone remembering them:
`static_assert` in `state.h`, the sim target's compile flags, and
`tests/check_sim_boundary.py`, which greps every file under `src/sim/` for each
forbidden construct and runs both as a ctest case and as its own CI job.

They are still written here because the reviewer sees a diff before CI runs, and
catching it there is cheaper. See ADR 0014 for why these are mechanical rather
than review items: every rule in the list is a desync that compiles, passes
every local test, and fails only in a real match between two machines.

---

## 2. Naming

| Kind | Style | Example |
|---|---|---|
| Types | `PascalCase` | `GameState`, `Fixed`, `InputFrame` |
| Functions | `snake_case` | `advance_frame`, `boxes_overlap` |
| Variables, fields | `snake_case` | `round_timer`, `hitstun_remaining` |
| Constants, `constexpr` | `SCREAMING_SNAKE` | `MAX_PROJECTILES`, `FRAME_RATE` |
| Enum class members | `PascalCase` | `FighterState::JumpStartup` |
| Namespaces | `snake_case`, short | `mw`, `mw::sim` |
| Files | `snake_case.h` / `.cpp` | `fixed.h`, `state.h`, `sim.cpp` |
| Test files | `test_<subject>.cpp` | `test_fixed.cpp` |
| Macros | `SCREAMING_SNAKE`, `MW_` prefix | `MW_ASSERT` |

**Frame counts carry their unit in the name** when the type does not:
`startup_frames`, `hitstun_remaining`, `round_timer` (all `int32_t` frames).
There is no `dt`, no `seconds`, and no `_ms` anywhere below the boundary. If
you find yourself wanting one, you are on the wrong side of the line.

Everything is in namespace `mw`. The sim is in `mw::sim`.

---

## 3. Error handling

**No exceptions.** Three patterns, in order of preference:

1. **Make it impossible.** Fixed-size arrays with a compile-time bound and a
   `static_assert` beat a runtime check.
2. **Return a status.** `enum class LoadResult { Ok, FileMissing, BadSchema };`
   Callers must handle every case — no `default:` that swallows.
3. **Assert.** `MW_ASSERT(cond, "message")` from `src/mw_assert.h`, for invariants
   whose violation means a programming error — not for anything a user or a data
   file can cause, which gets a status instead. Active in debug, compiled out
   entirely in release, so **the condition must have no side effects**.

   **Not usable below the sim boundary.** Reporting a failed assertion is I/O,
   and under rollback the same frame re-simulates up to 8x. The sim gets the
   same guarantees the other two ways: `static_assert` for anything checkable at
   compile time, and fixed-size arrays with compile-time bounds so the failure
   cannot be expressed at all.

**The sim never fails.** `advance_frame` has no error path — it cannot open a
file, cannot allocate, and cannot encounter a missing resource. Everything it
needs was validated at load time. If you are adding an error path to the sim,
you are adding something that does not belong there.

Loading, parsing, and I/O all happen at load time, outside the sim, where
failure is reportable and recoverable.

---

## 4. Logging

`MW_LOG_INFO` / `MW_LOG_WARN` / `MW_LOG_ERROR`, defined in `src/mw_log.h`.

**No logging inside `src/sim/`.** It is I/O, and under rollback the same frame
logs up to eight times. To observe the sim, use the training-mode overlay or
`tools/replay_inspector/` — both read state from outside.

---

## 5. Comments

Comment the **why**, never the **what**. The code says what it does.

Comment density should match the risk:

- **Sim code:** comment every non-obvious determinism constraint. "Iteration
  order here is load-bearing — do not switch to a hash map" is exactly the
  comment that saves a week.
- **Frame-data-derived constants:** cite the source. `// DESIGN.md §4.4`
- **Render, tools:** sparse. It is ordinary code.

No commented-out code. Git remembers.

---

## 6. Tests

- One `TEST_CASE` per behavior, named as a sentence:
  `TEST_CASE("Fixed multiplication round-trips through division")`
- `SUBCASE` for variations of the same behavior.
- Test the behavior, not the implementation. If a refactor that preserves
  behavior breaks the test, the test was wrong.
- **Every test tier gets a CTest label:** `unit`, `smoke`, `replay`, plus
  `boundary` on the sim-boundary check. This is what makes `ctest -L unit`
  work, and CI depends on it.
- **Sim tests use the shipped `data/characters/*.toml`**, via
  `tests/match_data.h` — not invented fixtures. Frame data is the behaviour of
  a fighting game, so a test against made-up timings proves the code works on
  data that will never ship.
- Boundary cases are not optional in sim tests: zero, negative, min, max,
  overflow, first frame, last frame.

---

## 7. Commits

Conventional commits. Scope is the module.

```
feat(sim): add fixed-point multiply with round-to-nearest
test(sim): cover Fixed overflow boundaries
fix(render): correct facing flip on cross-up
refactor(input): extract bitfield decode from event pump
docs(adr): record fixed-point math decision
chore(ci): cache CPM downloads between runs
data(frenchy): tune HP recovery from 16 to 14 frames
```

Scopes: `sim`, `render`, `audio`, `ui`, `platform`, `input`, `net`, `tools`,
`ci`, `docs`, `data`, `build`.

**Rules:**

- Commit after each discrete unit, not at end of session.
- A behavior change and its test go in the **same** commit.
- A balance change and the replay recordings it invalidates go in the **same**
  commit. Never a separate "fix tests" commit — that destroys the signal that
  makes replays worth having.
- One commit should be cleanly `git revert`-able.

---

## 8. Branches

```
feat/<scope>-<desc>       feat/sim-hitbox-overlap
fix/<scope>-<desc>        fix/render-facing-flip
test/<scope>              test/sim-coverage
docs/<topic>              docs/framedata-schema
chore/<topic>             chore/ci-caching
data/<character>          data/frenchy-tuning
```

Never commit directly to `main`. Never merge red CI. Delete branches after
merge.

---

## 9. Formatting

`.clang-format` is the authority; run it before committing. Four spaces, no
tabs, 100 column limit, braces on the same line.

**Use the pinned version**, because clang-format changes its output between
releases and a different one will fight CI:

```
pip install clang-format==22.1.8
clang-format -i $(find src tests -name '*.h' -o -name '*.cpp')
```

CI runs the same version and the check blocks merges.

Do not reformat code you are not otherwise changing. Formatting noise in a
diff costs review attention, and review attention is the scarce resource.
