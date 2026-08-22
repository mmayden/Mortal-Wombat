// Logging for the layers above the sim boundary.
//
// CONVENTIONS.md 4. There is deliberately no sim variant of this: logging is
// I/O, and under rollback the same frame is re-simulated up to 8x, so a log
// line inside advance_frame prints eight times for one frame of gameplay.
// To observe the sim, watch it from outside — training mode overlays or
// tools/replay_inspector/.
#pragma once

#include <cstdio>

namespace mw {

#define MW_LOG_INFO(...)                       \
    do {                                       \
        std::fprintf(stdout, "[info] ");       \
        std::fprintf(stdout, __VA_ARGS__);     \
        std::fprintf(stdout, "\n");            \
    } while (0)

#define MW_LOG_WARN(...)                       \
    do {                                       \
        std::fprintf(stderr, "[warn] ");       \
        std::fprintf(stderr, __VA_ARGS__);     \
        std::fprintf(stderr, "\n");            \
    } while (0)

#define MW_LOG_ERROR(...)                      \
    do {                                       \
        std::fprintf(stderr, "[error] ");      \
        std::fprintf(stderr, __VA_ARGS__);     \
        std::fprintf(stderr, "\n");            \
    } while (0)

}  // namespace mw
