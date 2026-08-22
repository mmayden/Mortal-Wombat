# `src/audio/`

miniaudio playback, driven by observed state change.

Audio state is **not** in `GameState` and is **not** rolled back. Restoring
audio and particle state is the classic rollback pain point; ADR 0012 avoids
the whole class of problem by keeping it structurally out of the sim.
