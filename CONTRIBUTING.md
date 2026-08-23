# Contributing

This file is deliberately short. It exists because GitHub surfaces it beside
every pull request, and it points at the documents that actually govern the
project rather than restating them — a second copy of a rule is a copy that
goes stale.

| You want to know | Read |
|---|---|
| The rules, the commands, the sim boundary | [AGENTS.md](AGENTS.md) |
| Environment quirks and traps that have cost time | [CLAUDE.md](CLAUDE.md) |
| What the game is and how it must feel | [docs/DESIGN.md](docs/DESIGN.md) |
| How the code is shaped | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| Style, naming, error handling, commits | [docs/CONVENTIONS.md](docs/CONVENTIONS.md) |
| Settled decisions — **not up for re-argument** | [docs/decisions/](docs/decisions/README.md) |

## The loop

```bash
git checkout -b feat/whatever          # never commit to main; it is protected
# ... work ...
powershell -File tools/verify.ps1      # everything CI checks, locally
gh pr create
```

Install the pre-push hook once and the verify step stops being something you
can forget:

```bash
git config core.hooksPath tools/hooks
```

`main` requires a pull request and six passing checks, **including for the repo
owner**. That is intentional: the moment an exception is carved out, the gate
stops meaning anything.

## What the checks are actually for

Not busywork. Each one exists because the failure it catches is invisible
locally:

| Check | Catches |
|---|---|
| Three-platform build | GCC-only defects — three so far were green on Windows |
| **desync** | The simulation diverging between platforms. The failure that ships broken and cannot be reproduced on the developer's machine |
| **release determinism** | Optimization changing simulation results |
| **sim boundary** | A float, allocation, clock read, or unordered iteration below the line |
| clang-format | Diff noise, which costs review attention — the scarcest resource here |

## Three rules that are easy to get wrong

**Re-record replays in the same commit as the change that invalidated them.** A
separate "fix tests" commit destroys the only signal that distinguishes a
deliberate balance change from a regression.

**A test that cannot fail is worse than no test.** Three combat recordings once
encoded attacks that whiffed; they reproduced perfectly and proved nothing.
Assert on the thing the test is named after.

**Do not invent a number.** If a design document does not specify it, it goes in
the `PROVISIONAL` block in `src/sim/constants.h` with a note saying what still
needs deciding. That block should shrink, never grow.

## Reporting something that feels wrong

Especially valuable, and hard to get from anywhere else. Feel is the one thing
the test suite structurally cannot check.

Say what you did, what happened, and what you expected — even roughly. A recent
report of "if I move right, the other character moves left towards me" was
enough to find a camera bug that had survived code review and every screenshot
taken of it, because it is invisible in a still image.
