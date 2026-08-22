# `data/characters/`

One TOML file per character. `docs/framedata_schema.md` is the contract, and
it is versioned — both the sim loader and `tools/framedata_editor/` read it.

Frame data as text is the highest-leverage decision in the project (ADR 0009):
it diffs, reviews, and tests like code, and **balance changes become data
commits rather than code commits**.

Empty until the character loader lands. The two v1 files will be
`frenchy.toml` and `wisdom.toml` (DESIGN.md §5.4).

Names are settled; silhouette, special move, and personality are still TODO in
§5.4 and must not be invented. None of them block the loader — every
mechanical property comes from the schema, not from characterisation.
