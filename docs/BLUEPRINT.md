# Agent-Assisted Game Development — Master Blueprint

> **Governing principle:** Parallelize *thinking*. Serialize *writing*.
> The leverage is in the harness, not the agent count. A single agent with
> tests, specs, and CI beats five agents without them — every time.

---

## Table of Contents

- [Scaling This to Your Project](#scaling-this-to-your-project)
- [Phase 0 — Repo Foundation](#phase-0--repo-foundation)
- [Phase 1 — The Harness (Before Any Feature)](#phase-1--the-harness-before-any-feature)
- [Phase 2 — Context Engineering Artifacts](#phase-2--context-engineering-artifacts)
- [Phase 3 — The Single-Agent Loop](#phase-3--the-single-agent-loop)
- [Phase 4 — The Reviewer Subagent](#phase-4--the-reviewer-subagent)
- [Phase 5 — Controlled Parallelism](#phase-5--controlled-parallelism)
- [Integration & Merge Protocol](#integration--merge-protocol)
- [Git Workflow Reference](#git-workflow-reference)
- [Tooling & Model Choice](#tooling--model-choice)
- [Failure Modes & Guardrails](#failure-modes--guardrails)
- [Engine-Specific Adaptations](#engine-specific-adaptations)
- [Readiness Gates](#readiness-gates)

---

## Scaling This to Your Project

**Read this before Phase 0.** This blueprint is written at full weight, for a
project measured in months. Applied whole to a two-week game jam entry, the
setup becomes the project — which is the exact failure this document warns
against in the Failure Modes table. Don't let a process doc about avoiding
infra procrastination become your infra procrastination.

Pick a tier and commit to it:

| | **Small** (jam, prototype, <1 month) | **Medium** (solo, 1–6 months) | **Large** (team, 6+ months) |
|---|---|---|---|
| Setup budget | 2–4 hours | 1–2 days | 1 week |
| Docs | `AGENTS.md` only | + DESIGN, ARCHITECTURE, ADRs | Full set incl. OWNERSHIP |
| Tests | Smoke test only | Unit + smoke | Full pyramid + replay |
| CI | Optional | Required | Required + perf gates |
| Branch protection | Skip | Required | Required |
| Determinism | Skip | Decide deliberately | Usually yes |
| Reviewer subagent | No | Yes | Yes |
| Parallelism | Never | Rarely, 2 max | Yes, gated |
| Realistic ceiling | 1 agent | 2–3 agents | 3–5 agents |

**The irreducible minimum, at any scale:**

1. A command that tells you whether the game still runs
2. `AGENTS.md` with the exact commands and the hard rules
3. Version control with small commits

Everything else is proportional to how long you'll live with the codebase.
If you're not sure which tier you're in, pick the smaller one. Scaling a
process up when you feel the pain is easy. Scaling down after you've built
ceremony you don't need is something people almost never actually do.

---

## Phase 0 — Repo Foundation

**Goal:** A repo an agent can be dropped into and immediately understand.
**Exit criteria:** `main` exists, is protected, and contains structure + rules but no game features.

### 0.1 Directory layout

```
/
├── .roo/
│   ├── rules/                  # Global rules, always loaded
│   │   └── rules.md
│   ├── rules-code/             # Mode-specific: implementation
│   ├── rules-architect/        # Mode-specific: design
│   ├── rules-debug/
│   └── rules-orchestrator/     # Only populated in Phase 5
├── docs/
│   ├── DESIGN.md               # What the game IS
│   ├── ARCHITECTURE.md         # How the code is shaped
│   ├── OWNERSHIP.md            # Who/what may write where
│   ├── CONVENTIONS.md          # Style, naming, patterns
│   └── decisions/              # ADRs — one file per decision
│       └── 0001-engine-choice.md
├── src/                        # Game source
├── tests/
│   ├── unit/                   # Pure logic, no engine
│   ├── integration/            # Systems talking to each other
│   └── smoke/                  # Headless boot-and-run
├── tools/                      # Build scripts, asset pipeline
├── assets/                     # BINARY — agent no-go zone
├── .github/workflows/ci.yml
├── AGENTS.md                   # Entry point for any agent
└── README.md
```

### 0.2 The `AGENTS.md` contract

This is the first file any agent reads. Keep it under 200 lines. It must answer:

1. What is this project, in three sentences?
2. How do I run the tests? (exact command)
3. How do I run the game? (exact command)
4. What am I forbidden from touching?
5. Where do I look before making a design decision?

Example skeleton:

```markdown
# Agent Instructions

## Project
[Three sentences. Genre, engine, core loop.]

## Commands
- Test:  `<exact command>`
- Lint:  `<exact command>`
- Run:   `<exact command>`
- Smoke: `<exact command>`

## Hard Rules
1. Never modify files under `assets/` or `*.scene`/`*.tscn`/`*.prefab`.
2. Never change a public function signature without an ADR.
3. Never add a dependency without asking.
4. Every behavior change requires a test. No test, no merge.
5. If a task requires editing a file outside your stated scope, STOP and report.

## Before You Decide Anything
- Read `docs/DESIGN.md` for intent.
- Read `docs/decisions/` — settled questions do not get relitigated.
- Read `docs/CONVENTIONS.md` for style.

## When Stuck
Do not guess. Do not invent an API. Report what you tried and stop.
```

> **Why rule 5 matters:** agents over-refactor. A task scoped to "fix the
> timeout bug" comes back restructuring error handling across four files.
> An explicit stop-and-report rule is the cheapest guard against this.

### 0.3 Git setup

```bash
git init
git add .
git commit -m "chore: repo skeleton, agent rules, docs scaffolding"
git branch -M main
git remote add origin <url>
git push -u origin main
```

Then, in the host UI (GitHub/GitLab), set branch protection on `main`:

- [ ] Require pull request before merging
- [ ] Require status checks to pass (CI)
- [ ] Require branches be up to date before merging
- [ ] Block force pushes

**This constrains you, not just the agents.** That's the point. The moment
you carve yourself an exception, the gate stops meaning anything.

---

## Phase 1 — The Harness (Before Any Feature)

**Goal:** A fast, honest, automated signal that says "the game still works."
**Exit criteria:** CI green on `main`, one real test in each tier.

> This is the single highest-leverage phase and it is not close. An agent
> with tests self-corrects. An agent without them confabulates and you find
> out three days later.

### 1.1 The game testing pyramid

Games resist testing differently than web apps. Structure it in four tiers:

| Tier | What it covers | Speed | Agent value |
|---|---|---|---|
| **Unit** | Pure logic with no engine dependency | ms | ★★★★★ |
| **Integration** | Systems composed together, headless | ~1s | ★★★★ |
| **Smoke** | Boot game loop, run N frames, assert no crash | ~5s | ★★★★ |
| **Replay** | Recorded input + seed → state hash compare | ~10s | ★★★★★ |

**Tier 1 is where you win.** Aggressively push logic out of engine classes
into plain, engine-free modules: damage calculation, inventory rules,
state machines, save serialization, pathfinding, loot tables, RNG streams,
economy math. These are testable in milliseconds and are where bugs
actually live.

If your logic can't be tested without booting the engine, that is an
architecture defect, not a testing limitation.

### 1.2 Determinism — decide this now, deliberately

**This is a Phase 1 decision because retrofitting it is brutal.** It is not,
however, a universal mandate. Determinism has real cost: no ambient RNG, no
wall-clock reads in logic, disciplined iteration order everywhere, sometimes
fixed-point math. Pay it when it buys something.

**Strong yes if your game has:** replays, daily/shared seeds, spectating,
netcode with rollback, speedrun verification, competitive fairness claims,
or procedural generation you want to reproduce from a seed.

**Probably not worth it for:** narrative games, most puzzle games, anything
single-player and physics-heavy where "close enough" is fine.

Write the answer as ADR 0002 either way, so no agent relitigates it.

#### If yes, the four requirements

- **Seeded RNG.** One injectable source. No global/ambient random calls.
  Multiple named streams (`combat`, `loot`, `worldgen`) so consuming one
  doesn't shift the others.
- **Fixed timestep** for simulation, decoupled from render.
- **Injectable clock.** No wall-clock reads in game logic.
- **Input as data.** A replayable event stream, not polled engine state.

#### The replay test — the highest-value test a game can have

Once input is data and the sim is deterministic, this falls out nearly free
and is worth more than the rest of the suite combined:

```
1. Record a session: seed + ordered input events + final state hash
2. Commit the recording to tests/replays/
3. On every CI run: replay the inputs against the seed
4. Assert the final state hash matches
```

This is the closest thing that exists to an automated *"does the game still
play the same?"* check. It catches behavioral regressions that unit tests
structurally cannot see — an agent refactors pathfinding, everything compiles,
every unit test passes, and enemies now route differently. Only a replay
catches that.

Keep 5–10 recordings covering distinct scenarios (combat, traversal, a full
level, an edge case that once broke). When one fails legitimately because you
changed balance on purpose, re-record it in the same commit as the change —
never in a separate "fix tests" commit, which destroys the signal.

**Agents are the specific reason this matters.** Human refactors are cautious
and scoped. Agent refactors are broad and confident. A replay test is the only
gate that reliably catches confidently-wrong behavior change.

#### If you answered no

You still need *some* net for behavioral regression, since unit tests
structurally can't see it. Your substitutes, in order: the headless smoke test
(1.3), snapshot tests over any pure logic that produces structured output
(loot tables, worldgen, dialogue trees — these are testable even when the
game as a whole isn't deterministic), and the sixty-second manual play after
every merge (Integration & Merge Protocol). That last one stops being a nice
ritual and becomes your primary regression gate. Treat it as mandatory, not
optional.

### 1.3 The smoke test

Minimum viable, and worth more than it looks:

```
1. Boot the game headless
2. Load the default scene/world
3. Run 600 simulation ticks (10s at 60Hz)
4. Assert: no exceptions, no assertion failures
5. Assert: frame time stayed under budget
6. Exit 0
```

This catches the entire class of "agent made it compile but it explodes on
launch" — which is the most common agent failure in game code.

### 1.4 CI workflow

```yaml
name: CI
on:
  pull_request:
  push:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup toolchain
        run: <engine/language setup>
      - name: Lint
        run: <lint command>
      - name: Unit tests
        run: <unit test command>
      - name: Integration tests
        run: <integration test command>
      - name: Headless smoke
        run: <smoke command>
```

Keep the full suite under 5 minutes. Past that, agents (and you) start
skipping it, and a skipped gate is no gate.

### 1.5 Git for this phase

```bash
git checkout -b feat/test-harness
# ... build the harness ...
git add . && git commit -m "test: add unit test runner and first assertion"
git commit -m "ci: add GitHub Actions workflow"
git commit -m "test: add headless smoke test"
git push -u origin feat/test-harness
gh pr create --title "Test harness and CI" --body "Establishes the verification gate."
# Confirm CI is green, then merge.
```

**Do not proceed to Phase 2 until this PR is merged and green.**

---

## Phase 2 — Context Engineering Artifacts

**Goal:** Everything an agent needs to make a *coherent* decision is written down.
**Exit criteria:** All five docs exist and are non-trivial.

> The Flappy Bird failure — one agent builds a Mario-style background while
> another builds a bird from a different game — is not a coordination bug.
> It's a *shared-context* bug. Neither agent could see the implicit aesthetic
> and design decisions the other was making. Writing them down is the fix.

### 2.1 `docs/DESIGN.md` — what the game IS

Not a pitch document. A constraints document. Include:

- Core loop, in one paragraph
- Player fantasy / intended feel (this is the anti-Flappy-Bird section)
- Art direction: palette, resolution, perspective, tone — be specific enough
  that two people reading it produce compatible assets
- Explicit non-goals ("this is not an open world," "no multiplayer, ever")
- Scope ceiling

### 2.2 `docs/ARCHITECTURE.md` — how the code is shaped

- Module map with one-line responsibilities
- Dependency direction rules (what may import what)
- The core loop's structure
- Data flow: where state lives, how it's mutated
- Extension points: "to add a new enemy, you touch exactly these files"

That last one is critical. Agents produce far better work when the "how do
I add a thing of type X" path is documented rather than inferred.

### 2.3 `docs/OWNERSHIP.md` — the write map

```markdown
| Path | Writable by | Notes |
|---|---|---|
| `src/systems/**` | agent | one system per task |
| `src/core/**` | human-led only | core loop, ECS, scheduler |
| `src/registry.*` | human-led only | hotspot; serialize all edits |
| `assets/**` | human only | binary, unmergeable |
| `*.tscn` / `*.prefab` | human only | unmergeable in practice |
| `tests/**` | agent | required with every change |
| `docs/decisions/**` | append-only | never edit a merged ADR |
```

**Registry/manifest files are the #1 source of agent merge conflicts.**
Every new feature wants to register itself. Two strategies:

1. **Serialize** — only one agent may touch it, ever.
2. **Eliminate** — use file-system-based or attribute-based auto-discovery
   so there's no central list to conflict on. Strongly preferred if your
   engine supports it.

#### The asset & content pipeline

"Agents stay out of `assets/`" is necessary but incomplete. Real projects grow
an import → validate → manifest → build chain, and that chain sits at exactly
the boundary where agent work is both most useful and most dangerous.

Split it in three:

| Layer | Owner | Rationale |
|---|---|---|
| **Source assets** (`.psd`, `.blend`, `.wav`) | Human only | Binary, unmergeable, no agent value |
| **Pipeline tooling** (importers, validators, packers) | **Agent — excellent work** | Pure code, highly testable, tedious |
| **Generated output** (atlases, manifests, `.import` files) | **Neither — gitignore it** | Regenerable; committing it creates phantom conflicts |

That middle row is one of the best agent tasks in the whole project. Asset
validators are tedious, well-specified, and pure-functional — "reject any sprite
whose dimensions aren't a multiple of 16," "fail if any audio file isn't 44.1kHz
mono," "error if a manifest entry points at a missing file." Agents write these
well and they pay for themselves permanently.

The third row causes more agent merge pain than almost anything else. Generated
manifests and import metadata change on every build, conflict constantly, and
carry zero review value. If your engine forces you to commit them (Godot's
`.import`, Unity's `.meta`), add a `.gitattributes` merge strategy and mark
them agent-no-go:

```gitattributes
*.import  merge=ours
*.meta    merge=ours
```

**Content data is the opposite case.** Level definitions, dialogue, item stats,
and loot tables should live in *text* formats (JSON/TOML/YAML/CSV) precisely so
they merge, diff, review, and test like code. Every piece of game content you
move out of a binary editor and into text is a piece an agent can safely work
on. This is one of the highest-leverage architecture decisions available to you.

### 2.4 `docs/decisions/` — ADRs

One file per settled decision, numbered, append-only:

```markdown
# 0007 — Combat uses fixed-point integers, not floats

**Status:** Accepted
**Date:** 2026-08-21

## Context
Replay determinism requires bit-identical math across platforms.

## Decision
All combat math uses i32 fixed-point with 16 fractional bits.

## Consequences
- Replays are deterministic
- Combat code cannot use standard trig; use lookup tables in `src/math/`
- Rendering may still use floats — the boundary is at `CombatState::render_view()`
```

The purpose is to stop agents from relitigating. Without ADRs, every session
re-derives decisions from scratch and derives them differently.

### 2.5 `docs/CONVENTIONS.md`

Naming, error handling, logging, comment policy, test naming, commit format.
Short and prescriptive. Ambiguity here shows up as inconsistency in the diff.

### 2.6 Git for this phase

```bash
git checkout -b docs/context-foundation
git commit -m "docs: add DESIGN, ARCHITECTURE, OWNERSHIP, CONVENTIONS"
git commit -m "docs: add ADR 0001-0003 for foundational decisions"
git push -u origin docs/context-foundation
gh pr create
```

---

## Phase 3 — The Single-Agent Loop

**Goal:** Ship 2–3 vertical slices with one agent, single-threaded.
**Exit criteria:** You have rhythm, and you know where the agent reliably fails.

### 3.1 The loop

```
SPEC → RED → GREEN → REVIEW → PR → MERGE
```

1. **SPEC** — You (or Architect mode) write the task: goal, files in scope,
   files explicitly out of scope, acceptance criteria, and the test that
   proves it. Written *before* the agent starts.
2. **RED** — Agent writes the failing test first. You see it fail.
3. **GREEN** — Agent implements until the test passes and the full suite
   stays green.
4. **REVIEW** — You read the diff. All of it, at this stage.
5. **PR** — Push, CI runs, merge.

### 3.2 Task sizing

A well-sized agent task is:

- **One session.** If it needs context compaction mid-task, it's too big.
- **≤5 files touched.** More means the decomposition was wrong.
- **One testable assertion of value.** "Player takes fall damage proportional
  to distance" — not "improve the damage system."
- **Reversible.** `git revert` on one commit undoes it cleanly.

### 3.3 Commit discipline

Commit after each discrete unit — not at end of session. Conventional commits:

```
feat(combat): add fall damage scaled by fall distance
test(combat): cover fall damage boundary cases
fix(inventory): prevent stack overflow above max_stack
refactor(save): extract serialization from SaveManager
docs(adr): record fixed-point math decision
chore(ci): cache toolchain between runs
```

Small commits mean an agent going sideways costs you twenty minutes, not a day.

### 3.4 Bootstrap first, then vertical slices

"Always build vertical slices" is good advice that's wrong at the very start.
You cannot build a vertical slice of an enemy before something can render,
tick, and read input. A small amount of horizontal scaffolding has to come
first — the mistake is not *having* it, it's letting it expand.

**Bootstrap order (horizontal, deliberately minimal):**

```
B0  Window + render loop, clear to a color
B1  Fixed-timestep game loop, decoupled update/render
B2  Input abstraction (as data, if you chose determinism)
B3  One entity that exists, renders, and moves by input
B4  One scene/level loaded from a text file
B5  Save/load of that entity's state, round-tripped in a test
```

**The bootstrap rule:** each step ends the moment it's *demonstrable*, not
when it's *good*. B0 is done when you see a colored window — not when you have
a renderer abstraction. B4 is done when one hardcoded level loads — not when
you have a level editor. Generalize later, from three real use cases, not from
imagination.

Do the bootstrap yourself or with a single tightly-supervised agent. It's the
architectural spine and every later decision inherits from it.

**Then switch to vertical, and stay there.** Build one enemy end to end —
spawn, AI, animation hook, damage, death, loot drop, save/load — *not* "the AI
system" then "the damage system." Vertical slices surface integration problems
while they're cheap. Horizontal layers defer every integration problem to one
catastrophic week later.

**The tell that you've drifted back to horizontal:** you're building a system
with no caller. If nothing in the game uses it yet, you're writing speculative
architecture, and agents will happily generate hundreds of lines of it.

---

## Phase 4 — The Reviewer Subagent

**Goal:** Add the first second agent — read-only.
**Exit criteria:** Reviewer catches at least one real bug you'd have missed.

This is the first multi-agent addition worth making, because it perfectly
fits the governing principle: it contributes intelligence and writes nothing.

### 4.1 Setup

In `.roo/rules-*` (or `.claude/agents/reviewer.md`), define a reviewer with:

- **Read + git-diff tools only.** No write, no edit, no execute.
- A checklist grounded in *your* docs:

```markdown
# Reviewer Checklist

## Correctness
- Does every behavior change have a test?
- Do the tests actually test the behavior, or just the implementation?
- Edge cases: zero, negative, max, empty, null, concurrent?

## Scope
- Did the diff touch files outside the task's stated scope?
- Did it refactor anything not asked for?
- Did it add dependencies?

## Consistency
- Does it violate any ADR in docs/decisions/?
- Does it match docs/CONVENTIONS.md?
- Does it duplicate something that already exists? (search first)

## Game-specific
- Does it introduce nondeterminism? (unseeded RNG, wall-clock, iteration
  order over unordered collections)
- Does it allocate in the hot loop?
- Does it break save-file compatibility without a migration?

## Output
Report findings as BLOCKING / SHOULD-FIX / NIT. Do not edit. Do not fix.
```

### 4.2 Where it sits

Reviewer runs on the diff *before* PR. The lead agent only proceeds on
green-reviewed code. It functions as a quality gate inside the loop rather
than after it.

**The nondeterminism check alone justifies this.** Iteration order over a
hash map is the classic agent-introduced replay-breaking bug, and it is
nearly invisible in human review.

---

## Phase 5 — Controlled Parallelism

**Goal:** Run 2–3 agents concurrently without creating merge debt.
**Entry criteria:** Phases 0–4 complete. You have architecture, tests, and rhythm.

> **Do not start here.** Parallelism only pays once decomposition is clean,
> and decomposition is only clean once the architecture exists.

### 5.0 The binding constraint is you

Before any of the mechanics below: **agent capacity is cheap and elastic.
Your review capacity is fixed and small.** Every parallel agent produces
diffs that only you can judge for design coherence, and "does this feel
right" cannot be delegated to a reviewer subagent or a test suite.

Do the arithmetic honestly:

- A meaningful agent task produces roughly 150–500 lines of diff
- Reviewing that properly — reading it, running it, *playing* it — is 20–40 minutes
- Three agents cycling every ~90 minutes generates 2–3 hours of review per hour

**The ceiling is not how many agents you can run. It's how many diffs you can
actually understand in a day.** For a solo dev that is realistically 2–3
concurrent agents. Not because tooling can't do more, but because past that
you stop reviewing and start rubber-stamping — and a rubber-stamped merge is
strictly worse than not having run the agent, because now the bad code is in
`main` with your implicit approval on it.

**Symptoms you've exceeded your ceiling:**

- You approve a PR without running the game
- You can't recall what merged yesterday
- You're fixing agent output more than you're directing it
- The backlog is review, not work
- You've started trusting green CI as sufficient

**When you hit it, the fix is fewer agents, not better tooling.** Every
"agent orchestration" product will offer to solve this by adding more
automation on top. That deepens the problem: more output, same review budget.

Structural relief that actually works: make tasks *smaller* (a 100-line diff
reviews in five minutes, not thirty), push more verification into tests so
review is about design rather than correctness, and batch review into
dedicated blocks instead of interrupt-driven context-switching.

### 5.1 The three gates

Before any task is dispatched in parallel, all three must pass:

1. **File exclusivity** — does this task write any file another concurrent
   task writes? If yes, they are not independent.
2. **Interface stability** — does it change a function signature, API
   contract, or data schema another task depends on? If yes, sequence it.
3. **Semantic independence** — could two reasonable implementations of these
   two tasks contradict each other in behavior even with zero file overlap?

Gate 3 is the one people skip and the one that produces the Flappy Bird
failure. Two agents can touch disjoint files and still build incompatible
things.

### 5.2 What is safe to parallelize in a game

**Green — genuinely independent:**
- Build scripts, asset pipeline tooling
- Save/load serialization
- Audio bus and mixing
- Input rebinding and settings menu
- Localization scaffolding
- Procedural generation modules (pure functions)
- Debug overlays and dev tools
- Test coverage backfill

**Red — always serialize:**
- Core game loop / scheduler
- ECS or entity model
- Scene files, prefabs, any binary or semi-textual engine asset
- Central registries and manifests
- Anything touching the design's "feel"
- Shared config / dependency manifests

### 5.3 Worktree setup

```bash
# From repo root
git worktree add ../game-audio       -b feat/audio-bus
git worktree add ../game-save        -b feat/save-system
git worktree add ../game-input       -b feat/input-rebinding

# List / clean up
git worktree list
git worktree remove ../game-audio
```

Each worktree gets its own working directory, index, and HEAD while sharing
one object store. This moves conflicts to merge time — where normal git
tooling catches them — instead of letting agents silently overwrite each
other during active work.

It also, importantly, means each agent can run the full test suite without
fighting another agent's build artifacts.

### 5.4 Per-worktree task file

Drop a `TASK.md` in each worktree:

```markdown
# Task: Audio bus

## Goal
[one paragraph]

## In scope
- src/audio/**
- tests/unit/audio/**

## Out of scope — STOP AND REPORT if you need to touch these
- src/core/**
- src/registry.*
- Cargo.toml / package.json / project.godot
- assets/**

## Acceptance
- [ ] Test: mixing two sources produces expected sample output
- [ ] Test: muting a bus silences children
- [ ] Full suite green
- [ ] Smoke test green

## Do not
- Refactor anything outside src/audio/
- Add dependencies
```

### 5.5 Shared-config pre-step

If three tasks each need a new dependency, all three will conflict on the
manifest no matter how good your isolation is. **Land all shared config
changes in a single commit on `main` first**, then branch the worktrees from
that commit.

### 5.6 Draft PRs immediately

Have each agent open a draft PR the moment it starts. In-progress work
becomes visible and scope overlap surfaces before it becomes a conflict.

---

## Integration & Merge Protocol

Five agents finishing at once doesn't help if review takes five times as long.
Plan the merge, not just the work.

### The sequence

```bash
# 1. Order branches by dependency. Least-dependent first.

# 2. Pre-flight conflict check WITHOUT merging
git merge-tree $(git merge-base feat/audio feat/save) feat/audio feat/save

# 3. Merge one at a time
git checkout main
git merge --no-ff feat/audio
<run full test suite>          # ← non-negotiable
git push

# 4. Rebase remaining branches onto updated main
cd ../game-save
git fetch origin && git rebase origin/main
<run full test suite>
gh pr create

# 5. Repeat
```

### Rules

- **Never merge two agent branches simultaneously.** Conflicts compound.
- **Run the full suite after every merge, not just at the end.** The bug you
  want to catch is the *interaction*, and it only appears post-merge.
- **For 3+ branches, use an integration branch.** Merge everything into
  `integration/sprint-N` first, get it green, then fast-forward to `main`.
  Keeps `main` continuously shippable.
- **Rebase-before-PR** keeps history linear and shrinks each conflict surface.

### Post-merge smoke ritual

After each merge to `main`: run the game. Actually play it for sixty seconds.
Automated tests verify correctness; they cannot verify that the thing is
still a game. This is the step that catches "all green, feels broken."

---

## Git Workflow Reference

### Branch naming

```
feat/<scope>-<short-desc>      feat/audio-bus
fix/<scope>-<short-desc>       fix/inventory-stack-overflow
refactor/<scope>               refactor/save-serialization
test/<scope>                   test/combat-coverage
docs/<topic>                   docs/adr-fixed-point
chore/<topic>                  chore/ci-caching
integration/sprint-<n>         integration/sprint-3
```

### When to do what

| Trigger | Action |
|---|---|
| Starting any unit of work | **New branch** off latest `main` |
| A logical step compiles and tests pass | **Commit** |
| End of a work session | **Push** (even if incomplete — backup + visibility) |
| Agent starts a parallel task | **Draft PR** immediately |
| Task complete, suite green, reviewer clean | **Mark PR ready** |
| PR approved + CI green | **Merge** (squash for small, `--no-ff` for features) |
| Branch merged | **Delete branch**, `git worktree remove` |
| `main` moved while you were working | **Rebase** your branch onto it |
| Agent went badly sideways | `git reset --hard` to last good commit |

### Non-negotiables

- Never commit directly to `main`
- Never merge red CI
- Never merge a behavior change without a test
- Never force-push a shared branch
- Delete branches after merge — stale agent branches rot fast

---

## Tooling & Model Choice

This blueprint is deliberately process-first — the practices outlive any
specific tool. But one honest calibration is worth stating plainly:

> **The quality of your single agent loop matters more than the number of
> agents.** Moving from a weaker to a stronger model on one well-scoped task
> beats adding a second agent, usually by a wide margin. If you're choosing
> between "upgrade the loop" and "add parallelism," upgrade the loop.

**What actually moves the needle, ranked:**

1. **A strong model on a well-scoped task with tests.** Everything else is a
   rounding error next to this.
2. **Tool access quality.** Can it run your tests, read your docs, and see the
   failure output? An agent that can't run the suite is guessing.
3. **Context window management.** Long sessions degrade. Fresh session per
   task beats one heroic session, every time.
4. **Only then: more agents.**

**Tool-agnostic requirements.** Whatever you use, it must be able to run your
test suite and read the output, read `AGENTS.md` and `docs/` without you
pasting them, produce reviewable diffs rather than opaque rewrites, and stop
when it's stuck rather than inventing an API. If a tool can't do those four,
no amount of orchestration around it will help.

**On model tiering.** Cheaper models for mechanical work (test scaffolding,
boilerplate, doc updates) and stronger models for design-bearing work is a
real optimization — but it's a *late* one. Get the loop working with your best
available model first. Optimizing cost on a workflow that isn't yet producing
good code is optimizing the wrong variable.

**Skepticism is warranted toward the orchestration product category.** Tools
that promise to manage 10+ agents are solving a problem you probably don't
have and creating one you definitely will: output volume that exceeds your
review capacity (see 5.0). Adopt them if you hit a real bottleneck they
address — not in anticipation of one.

---

## Failure Modes & Guardrails

| Failure | Signal | Guard |
|---|---|---|
| **Scope creep** | Diff touches files outside the task | Explicit out-of-scope list + stop-and-report rule |
| **Context rot** | Quality degrades late in a long session | Cap task size; fresh session per task; ADRs carry decisions forward |
| **Telephone game** | Two agents build incompatible things | Written DESIGN.md; semantic-independence gate |
| **Confident fabrication** | Invented APIs, plausible nonsense | Tests as ground truth; "do not guess" rule |
| **Registry conflicts** | Same manifest edited by every branch | Serialize or eliminate the registry |
| **Binary conflicts** | Unmergeable scene/asset files | Human-only zone in OWNERSHIP.md |
| **Green but broken** | Tests pass, game feels wrong | Sixty-second manual play after every merge |
| **Merge bottleneck** | Branches pile up unreviewed | Cap concurrency at 3; draft PRs early |
| **Review saturation** | Approving without running the game | Fewer agents, smaller diffs (see 5.0) |
| **Behavioral regression** | Compiles, tests pass, plays differently | Replay tests (see 1.2) |
| **Infra procrastination** | Two weeks of orchestration, no game | Milestone gates below; scope tier table |
| **Blueprint as procrastination** | Perfecting process instead of shipping | Pick a tier up top and cap setup hours |
| **Nondeterminism creep** | Replays diverge, flaky tests | Reviewer determinism checklist; seeded RNG from day one |

### The honest warning

Building the orchestration layer is more fun than building the game. It's
novel systems design and it *feels* productive. The test harness is tedious.

If you find yourself six weeks in with a beautiful agent cluster and no
playable build, the cluster became the project. That's a legitimate thing to
build — but scope it as its own project rather than telling yourself it's a
means to a game.

**Checkpoint question, asked weekly:** *Can I play it?*

---

## Engine-Specific Adaptations

### Godot
- `.tscn`/`.tres` are text but merge badly in practice → human-only
- Keep logic in plain GDScript/C# classes, not attached to nodes, for testability
- `--headless` for smoke tests; GUT or GdUnit4 for unit tests
- Autoloads are a hidden registry — treat as serialized
- Parallelize: tooling, pure-logic modules, shaders. Never scenes.

### Unity
- Prefabs and scenes are effectively unmergeable → human-only, always
- Push logic into plain C# classes outside MonoBehaviour; test with NUnit
- ScriptableObjects reduce scene-file churn — lean on them hard
- `-batchmode -nographics` for CI
- The Asset Database is a global registry; expect serialization pressure

### Unreal
- Blueprints are binary → human-only, no exceptions
- Keep agents in C++ only; enforce a Blueprint/C++ boundary in ARCHITECTURE.md
- Automation Spec framework for tests; commandlets for headless CI
- Header/implementation split gives good file exclusivity for parallelism

### Rust / Bevy
- **Best case for this whole blueprint.** Cargo workspaces give real module
  boundaries; the borrow checker catches a large class of agent errors at
  compile time; `cargo test` is fast and honest.
- Parallelize by crate — near-perfect file exclusivity
- `Cargo.toml` is the shared-config hotspot → land dependency changes first
- Bevy systems are naturally decomposable; ECS schedules are the hotspot

### Web (TypeScript / Phaser / Three.js)
- Everything is text and merges well — highest safe parallelism
- Vitest/Jest for units, Playwright for smoke
- `package.json` is the hotspot
- Bundler config is a registry — serialize it

### Custom engine
- Highest parallelism ceiling, highest discipline requirement
- Module boundaries are *yours to define* — define them before Phase 5
- Invest extra in ARCHITECTURE.md; there's no engine convention to fall back on

---

## Readiness Gates

Do not advance until each is true.

### Gate A → may start Phase 3 (features)
- [ ] `main` protected, PRs required, CI blocking
- [ ] Test runner works; ≥1 real unit test, ≥1 smoke test
- [ ] CI green on `main`
- [ ] `AGENTS.md` written with exact commands
- [ ] DESIGN, ARCHITECTURE, OWNERSHIP, CONVENTIONS exist and are non-trivial
- [ ] Determinism decision made and recorded as an ADR (yes *or* no)
- [ ] If yes: seeded RNG, fixed timestep, input-as-data, one replay test
- [ ] Scope tier picked and setup budget respected

### Gate B → may add the reviewer subagent
- [ ] 2–3 vertical slices shipped through the full loop
- [ ] You know the agent's three most common failure modes on *this* codebase
- [ ] Test suite runs in under 5 minutes
- [ ] Twenty or more commits of real history

### Gate C → may run parallel agents
- [ ] Reviewer subagent is catching real issues
- [ ] Architecture is stable — no ADR churn for two weeks
- [ ] You can name 3+ tasks passing all three independence gates
- [ ] Merge protocol rehearsed at least once with two branches
- [ ] There is a playable build

### Gate D → may scale past three agents

**Most solo developers should never pass this gate, and that is not a
failure.** Three well-supervised agents on a codebase with real tests is a
genuinely strong setup. Past that, the constraint stops being agent capacity
and becomes your review capacity (see 5.0) — and adding agents doesn't relieve
that, it strains it.

Only pass this gate if *all* of these are true:

- [ ] Merge review is genuinely not your bottleneck
- [ ] Integration branch workflow is routine and boring
- [ ] Post-merge failures are rare
- [ ] You have enough provably independent work to fill the agents
- [ ] **More than one human is reviewing**

That last box is the real gate. Above roughly three agents, the arithmetic in
5.0 stops working for one person no matter how good the tooling is.

**The better question at Gate C:** not "can I add a fourth agent?" but "is the
game good?" Process capacity past this point is rarely what's limiting the
project.

---

## The One-Line Summary

Size the process to the project. Build the harness. Write the context down.
Bootstrap minimally, then ship vertical slices with one strong agent. Add a
reviewer that only reads. Parallelize only what is provably independent, and
only as far as you can actually review. Merge one branch at a time and play
the game after every merge.

**And the check that matters more than any of it:** can I play it?
