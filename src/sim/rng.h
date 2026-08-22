// Seeded deterministic RNG for the simulation. ADR 0002.
//
// PCG32. Chosen over xorshift for a better distribution at the same cost, and
// over anything in <random> because the standard library's generators are
// specified but its *distributions* are not — std::uniform_int_distribution
// produces different values on libstdc++, libc++, and MSVC from the same
// engine and seed. That is a desync.
//
// The state lives inside GameState, so it is saved and restored by rollback
// like every other piece of match state. A generator with hidden global state
// would replay differently on a rollback, which is exactly the bug this file
// exists to make impossible.
#pragma once

#include <cstdint>

namespace mw::sim {

// Named streams, so that consuming randomness in one system does not shift the
// sequence another system sees. Adding a hit-spark effect must not change which
// numbers the round-start logic draws.
enum class RngStream : int32_t {
    Combat = 0,
    Effects = 1,
    Count = 2,
};

struct RngState {
    uint64_t state[static_cast<int32_t>(RngStream::Count)];
};

// Each stream gets a distinct odd increment, which is what makes PCG streams
// independent rather than merely offset.
inline constexpr uint64_t PCG_MULTIPLIER = 6364136223846793005ULL;
inline constexpr uint64_t PCG_INCREMENT_BASE = 1442695040888963407ULL;

constexpr uint64_t pcg_increment(RngStream stream) {
    return PCG_INCREMENT_BASE + (static_cast<uint64_t>(stream) << 1) + 1u;
}

constexpr void rng_seed(RngState& rng, uint64_t seed) {
    for (int32_t i = 0; i < static_cast<int32_t>(RngStream::Count); ++i) {
        const uint64_t increment = pcg_increment(static_cast<RngStream>(i));
        rng.state[i] = 0u;
        rng.state[i] = rng.state[i] * PCG_MULTIPLIER + increment;
        rng.state[i] += seed;
        rng.state[i] = rng.state[i] * PCG_MULTIPLIER + increment;
    }
}

// PCG32: advance the LCG, then permute the *old* state into the output. The
// permutation is what gives good statistical quality; the LCG alone is poor.
constexpr uint32_t rng_next(RngState& rng, RngStream stream) {
    const int32_t index = static_cast<int32_t>(stream);
    const uint64_t old_state = rng.state[index];
    rng.state[index] = old_state * PCG_MULTIPLIER + pcg_increment(stream);

    const uint32_t xorshifted = static_cast<uint32_t>(((old_state >> 18u) ^ old_state) >> 27u);
    const uint32_t rot = static_cast<uint32_t>(old_state >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
}

// Uniform in [0, bound). Rejection-sampled rather than taken modulo, because
// modulo biases toward low values when bound does not divide 2^32 — and
// because the rejection loop is itself deterministic, it costs nothing we care
// about here.
//
// A bound of zero or less returns zero; the sim has no error path.
constexpr uint32_t rng_below(RngState& rng, RngStream stream, uint32_t bound) {
    if (bound == 0u) {
        return 0u;
    }
    const uint32_t threshold = (~bound + 1u) % bound;
    for (;;) {
        const uint32_t value = rng_next(rng, stream);
        if (value >= threshold) {
            return value % bound;
        }
    }
}

// Inclusive on both ends, matching how ranges are written in design docs.
constexpr int32_t rng_range(RngState& rng, RngStream stream, int32_t low, int32_t high) {
    if (high <= low) {
        return low;
    }
    const uint32_t span = static_cast<uint32_t>(high - low) + 1u;
    return low + static_cast<int32_t>(rng_below(rng, stream, span));
}

}  // namespace mw::sim
