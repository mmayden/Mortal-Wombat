# `tools/`

Separate build targets. **Never linked into the game binary.**

Because a tool cannot desync anything, tools may freely use floats,
`std::string`, allocation, exceptions, and everything else ADR 0001 forbids
below the sim boundary. That is exactly what makes this directory the safe
zone for parallel agent work — the sim is human-led permanently, this is not.

| Tool | Status | Purpose |
|---|---|---|
| `dev.ps1` | working | Runs a command inside the VS developer environment |
| `framedata_editor/` | empty | Build this **before** authoring content — hand-writing hitbox TOML for two characters will break you |
| `replay_inspector/` | empty | Steps a recording frame by frame; worth building once the replay library grows |
| `sprite_packer/` | empty | v2, with the 3D-to-sprites pipeline (ADR 0013) |

The one coupling between an editor and the game is `docs/framedata_schema.md`.
Changing it requires bumping its version and updating both consumers in the
same commit (AGENTS.md rule 5) — it is the one place in the project where a
coordination failure is silent.
