// Runtime assertions for the layers above the sim boundary.
//
// CONVENTIONS.md 3 lists three ways to handle a failure, in order of
// preference: make it impossible, return a status, or assert. This is the
// third, and it is for invariants whose violation means a programming error --
// not for conditions a user or a data file can cause, which get a status.
//
// DELIBERATELY NOT USABLE FROM src/sim/. Reporting a failed assertion is I/O,
// and under rollback the same frame is re-simulated up to 8x, so an assert
// firing inside advance_frame reports eight times for one frame. The sim gets
// its guarantees the other two ways instead: static_assert for anything
// checkable at compile time, and fixed-size arrays with compile-time bounds so
// the failure cannot be expressed. tests/check_sim_boundary.py enforces that
// separation by rejecting I/O below the line.
//
// NAMED mw_assert.h, NOT assert.h. src/ is on the include path for every target
// and every dependency compiled through it, so a header here named assert.h
// shadows the C standard <assert.h> -- toml++ includes it and got this file
// instead, and the build broke with "'assert': identifier not found" in
// somebody else's code. The same hazard applies to any standard C header name:
// math.h, time.h, string.h, stdio.h. Prefix anything at this level.
//
// It surfaced only on a clean build, because an incremental build had no reason
// to recompile the file that included it.
#pragma once

#include <cstdio>
#include <cstdlib>

namespace mw {

// Compiled out entirely in release builds, so the condition must be free of
// side effects -- MW_ASSERT(advance(x)) would stop advancing in release.
#ifdef NDEBUG
#define MW_ASSERT(condition, message) ((void) 0)
#else
#define MW_ASSERT(condition, message)                                                         \
    do {                                                                                      \
        if (!(condition)) {                                                                   \
            std::fprintf(stderr, "[assert] %s\n  %s:%d: %s\n", (message), __FILE__, __LINE__, \
                         #condition);                                                         \
            std::abort();                                                                     \
        }                                                                                     \
    } while (0)
#endif

}  // namespace mw
