# 0024 — The game is Divided States; the Mortal Wombat premise is dropped

**Status:** Accepted
**Date:** 2026-08-25
**Relates to:** `DESIGN.md` §1, §5.3, §5.4, §5.5, §9, ADR 0018
**Supersedes:** the *Mortal Wombat* title, its cast, and its comedy premise

## Context

The project was called **Mortal Wombat**: a parody title over a cast of wombats,
with a deadpan comedy tone and two characters named Frenchy and Wisdom. ADR 0018
had already dropped the *mechanical* inheritance from Mortal Kombat II. What
remained was the name, the cast and the joke — and those are now dropped too.

This is a second, separate removal, and worth recording as one rather than
folding into 0018. That ADR answered "what mechanics is this built on". This one
answers "what game is this", which the earlier decision deliberately left alone.

## Decision

**The game is *Divided States*.** It is set across a divided United States, and
each stage is a location in it.

Three things follow.

### The cast is emptied, not renamed

Frenchy and Wisdom are gone along with the premise that produced them. **George**
and **Sue** are placeholders that exist so the data files have something to be
called; they carry no characterisation and nothing should be read into them.
`DESIGN.md` §5.4's do-not-invent rule still binds every row but the name.

### Stages scroll, and they are US locations

This settles half of §5.5, which was entirely TODO. Scrolling matches what the
engine already does — a 960-unit stage against a 480-unit screen, with the
camera tracking the fighters. *Which* locations, what they look like, and how
deep the backgrounds go remain undecided, and §6 still caps v1 at one stage.

### Tone is TODO, deliberately

Whether this is satire, comedy or played straight changes no mechanic, so it is
not being decided to unblock anything. It is recorded as open rather than left
ambiguous, so nobody infers one from the title.

**One half of the old comedy rule is kept**, because it was never really about
wombats: *the fighting system plays straight — no random damage, no joke moves
that break game state, no fourth-wall input.* That is a determinism constraint
wearing a tonal hat. Random damage is precisely what makes a match
irreproducible, and a mechanic that exists for a laugh is one nobody can play
around. It survives any tone the game ends up with.

### `mw` becomes `ds` throughout

The namespace, macro prefix, target names and binary were all initialled after
the old title. Renamed in the same change: `ds::`, `DS_`, `ds_*`,
`divided_states`. 423 references across roughly fifty files, verified by the
compiler and the full test suite rather than by reading.

## Consequences

- **`DESIGN.md` §9 stops being a constraint.** It existed because the old title
  parodied a Warner Bros. property, so every naming decision had to be checked
  against someone else's trademark. An original title over an original cast
  removes that entirely. The ordinary rule is kept and restated: mechanics are
  not copyrightable, specific characters and assets are.
- **The repository is renamed** to `Divided-States`. GitHub redirects the old
  URL, so existing clones and links keep working.
- **The directory on disk stays `game-dev-system`**, after the process blueprint
  it was seeded from. It has never matched the game's name and does not now.
- **ADRs 0018 and earlier keep their wording.** They are append-only and their
  reasoning is about a premise that was real at the time; rewriting them would
  destroy the record of why the mechanics are what they are.
- **Nothing mechanical changed.** Not one frame value, hitbox, constant or state
  transition. The rename is verified by the build; the design change touches
  only prose, and the replay recordings did not move.
- **Worth revisiting if** the title changes again before anything ships. It is
  cheap now, when the only cast is two placeholders and no art exists, and it
  gets steadily more expensive after that.
