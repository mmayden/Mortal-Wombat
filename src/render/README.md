# `src/render/`

Draws `GameState`. Never writes it.

Floats, interpolation, and effects are all fine here — this is above the sim
boundary (ARCHITECTURE.md §1). Render learns that something happened by
observing state change between frames, not by being called back into from the
sim: under rollback a side effect inside `advance_frame` fires up to 8x.

Empty until the bootstrap reaches "one fighter rendering".
