# `data/characters/`

One TOML file per character. `docs/framedata_schema.md` is the contract, and
it is versioned — both the sim loader and `tools/framedata_editor/` read it.

Frame data as text is the highest-leverage decision in the project (ADR 0009):
it diffs, reviews, and tests like code, and **balance changes become data
commits rather than code commits**.

Empty until the character loader lands. DESIGN.md §5.4 has not named the cast
yet and says not to invent it, so the two v1 files will be `wombat_a.toml` and
`wombat_b.toml` until it does.
