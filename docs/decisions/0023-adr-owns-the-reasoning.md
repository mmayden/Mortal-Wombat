# 0023 — The ADR owns the reasoning once a decision graduates

**Status:** Accepted
**Date:** 2026-08-24
**Relates to:** `CONTRIBUTING.md` (the ownership map), ADR 0021, ADR 0022

## Context

The ownership map in `CONTRIBUTING.md` assigns exactly one owner to each fact,
on the principle that a second copy of a rule is a copy that goes stale. It has
a row for *settled technical decisions* and none for **why a settled design
decision was made** — so three files could plausibly claim it, and two did.

The reasoning for the control scheme ended up written out twice: in ADR 0021 and
in `drawing-board/RULESET.md`. Same arguments — crossups, the remove/grant tax
rule, auto-combos, why the input buffer pays for itself — in both places. That
was introduced by writing the ADR without trimming the source.

A second problem sits underneath it. `drawing-board/` is a working area by name.
If reasoning lives only there, the durable record of *why* depends on a
scratch directory surviving, and merged ADRs are append-only — a citation into a
directory that later moves cannot be fixed.

## Decision

**When a decision graduates, its reasoning moves into its ADR. The drawing board
keeps a pointer, not a copy.**

Three homes, and the boundaries between them are now explicit:

| Holds | Lives in |
|---|---|
| What the rule **is** | `docs/DESIGN.md` — the specification |
| **Why** it was chosen, what it forecloses, what would reopen it | `docs/decisions/` — the ADR |
| **Research inputs**, and decisions not yet made | `drawing-board/` |

**`drawing-board/` is kept permanently**, as the research archive. Its files are
cited by merged ADRs, and merged ADRs are never edited — so those paths must not
move. That settles the durability question rather than deferring it.

An ADR must be **self-contained on its reasoning**. It may cite a research
document for evidence, but a reader who cannot open that file must still be able
to understand why the decision went the way it did.

## Consequences

- **`drawing-board/RULESET.md` shrinks to its real job**: the decisions not yet
  made, plus a graduation table pointing at the specification and the ADR for
  each one that is. The long-form arguments for decisions 0–5 move out.
- **Applying this rule immediately found a hole.** Two settled root axes — no
  air blocking, and one fighter per side — had no ADR *and* no line in
  `DESIGN.md`. They existed only as drawing-board reasoning, so the trim would
  have deleted the only record of either. ADR 0022 records them. That is one
  real defect per rule application, on the first application.
- **Writing an ADR now has a second step**: trim what it supersedes in the
  drawing board, in the same commit. An ADR that leaves its source intact has
  created the duplication rather than resolved it.
- **The cost is that RULESET stops reading as a narrative.** That is accepted:
  the narrative is what the ADRs are for, and they are the artifact that
  survives.
- **Worth revisiting if** ADRs start being written to dodge the trim — a
  decision recorded thinly so the real argument can stay somewhere editable.
  The check is whether an ADR alone answers "why not the other option".
