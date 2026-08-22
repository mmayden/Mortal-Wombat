# `src/platform/`

SDL3: window, event pump, gamepad, audio device, and frame timing.

Translates devices into `InputFrame` bitfields before the sim sees them, and
calls `input_sanitized` on the way through so a driver setting an unused bit
cannot alter a state hash.

Empty until bootstrap B0. Adding SDL3 is a dependency change — a serialized
hotspot in `CMakeLists.txt` (AGENTS.md rule 4).
