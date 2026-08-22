# 0016 — SDL_Renderer for v1, not the SDL3 GPU API

**Status:** Accepted
**Date:** 2026-08-22
**Amends:** ADR 0004 (Rendering: SDL3 GPU API)

## Context

ADR 0004 chose the SDL3 GPU API, with an explicit escape clause: "Fall back to
SDL_Renderer if it becomes a time sink — the game is textured quads and this is
not where the difficulty lies."

That clause is written for a future discovery. This ADR takes the fallback up
front instead, so the choice is a recorded decision rather than a quiet
divergence from 0004 that a later reader has to reverse-engineer.

## Decision

The bootstrap renderer is SDL_Renderer.

**Why now rather than after hitting a wall:**

- v1 renders colored rectangles (DESIGN.md §5.1) and v2 renders textured quads
  from sprite sheets (ADR 0013). Neither needs shaders, custom pipelines,
  render passes, or a swapchain the GPU API exists to expose.
- SDL_Renderer supplies logical presentation with integer scaling in one call.
  DESIGN.md §4.4 requires 480x270 integer-scaled, and a non-integer scale would
  blur every edge of a game made of hard-edged boxes. Reimplementing that over
  the GPU API is real work with no gameplay payoff.
- The GPU API would have to be written before there was anything to draw. ADR
  0004's own reasoning — "rendering is genuinely not the hard part of a
  fighter... do not spend engineering budget here" — argues against that more
  strongly than it argues for the GPU API.
- SDL_Renderer sits on D3D11, Metal, and Vulkan backends. This is not a
  software-rendering compromise; on this machine it selects direct3d11.

**What this does not change.** ADR 0004's substantive rules still hold in full:
rendering reads `GameState` and never writes it, and interpolation between sim
frames happens in the render layer only.

## Consequences

- `src/render/renderer.cpp` uses `SDL_RenderFillRect` and `SDL_RenderTexture`.
  Switching to the GPU API later means rewriting that one file.
- The `SpriteManifest` seam (ADR 0013) is unaffected — it deals in quads and
  tints, not in draw calls, so the v2 sprite pipeline lands against the same
  interface either way.
- Revisit if something actually needs the GPU API: full-screen shader effects,
  a custom blend mode, or a measured throughput problem. "It is more modern" is
  not a reason, and neither is "we said we would in 0004."
- If it is revisited, this ADR is superseded rather than edited — merged ADRs
  are append-only (BLUEPRINT.md §2.3).
