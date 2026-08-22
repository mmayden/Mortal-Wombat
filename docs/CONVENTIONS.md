# Conventions — Mortal Wombat

> Short and prescriptive on purpose. Ambiguity here shows up as inconsistency
> in the diff, and review is this project's binding constraint.

---

## 1. The C++ subset (ADR 0001)

**Use:** RAII, references, `constexpr`, namespaces, `enum class`, plain
structs, operator overloading (fixed-point types only), templates for
containers only.

**Forbidden project-wide:**

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
rules are enforced by `static_assert`, by the sim target's compile flags, and
by the reviewer checklist — the last of which is why they are written here.

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
3. **Assert.** `MW_ASSERT(cond, "message")` for invariants that indicate a
   programming error. Active in debug, compiled out in release.

**The sim never fails.** `advance_frame` has no error path — it cannot open a
file, cannot allocate, and cannot encounter a missing resource. Everything it
needs was validated at load time. If you are adding an error path to the sim,
you are adding something that does not belong there.

Loading, parsing, and I/O all happen at load time, outside the sim, where
failure is reportable and recoverable.

---

## 4. Logging

`MW_LOG_INFO` / `MW_LOG_WARN` / `MW_LOG_ERROR`, defined in `src/log.h`.

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
- **Every test tier gets a CTest label:** `unit`, `smoke`, `replay`. This is
  what makes `ctest -L unit` work, and CI depends on it.
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

Do not reformat code you are not otherwise changing. Formatting noise in a
diff costs review attention, and review attention is the scarce resource.
