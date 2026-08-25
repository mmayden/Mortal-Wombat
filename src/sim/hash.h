// State hashing — the desync check. ADR 0011.
//
// This is the mechanism behind the most important test in the project. Two
// machines run the same replay; if any per-frame hash differs, the simulation
// diverged, and we know the exact frame it happened on.
//
// FNV-1a over the raw bytes of GameState. Byte-level rather than field-level
// on purpose: a field-level hash has to be updated whenever a field is added,
// and the one time someone forgets is the one time a desync slips through.
// Walking the bytes cannot forget.
//
// This is only sound because GameState is proven padding-free by the
// static_asserts in state.h. Read that comment before changing the struct.
#pragma once

#include <cstdint>

#include "sim/state.h"

namespace ds::sim {

inline constexpr uint64_t FNV64_OFFSET_BASIS = 14695981039346656037ULL;
inline constexpr uint64_t FNV64_PRIME = 1099511628211ULL;

inline uint64_t hash_bytes(const void* data, int32_t size, uint64_t seed = FNV64_OFFSET_BASIS) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    uint64_t hash = seed;
    for (int32_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= FNV64_PRIME;
    }
    return hash;
}

// The full-state hash, used for the per-frame desync comparison.
inline uint64_t hash_state(const GameState& state) {
    return hash_bytes(&state, static_cast<int32_t>(sizeof(GameState)));
}

// Hashes only what a replay's final assertion cares about, excluding the RNG
// stream position.
//
// Why this exists separately: a render-side effect that draws a random number
// would advance a stream and change hash_state without changing anything a
// player could observe. Comparing this in replay tests keeps them asserting on
// behavior. The desync test still compares the full hash_state, because there
// a divergent RNG position genuinely is a bug.
inline uint64_t hash_state_visible(const GameState& state) {
    uint64_t hash = FNV64_OFFSET_BASIS;
    hash = hash_bytes(state.fighters, static_cast<int32_t>(sizeof(state.fighters)), hash);
    hash = hash_bytes(state.projectiles, static_cast<int32_t>(sizeof(state.projectiles)), hash);
    hash = hash_bytes(&state.frame, static_cast<int32_t>(sizeof(state.frame)), hash);
    hash = hash_bytes(&state.round_timer, static_cast<int32_t>(sizeof(state.round_timer)), hash);
    hash = hash_bytes(&state.round_number, static_cast<int32_t>(sizeof(state.round_number)), hash);
    hash = hash_bytes(state.rounds_won, static_cast<int32_t>(sizeof(state.rounds_won)), hash);
    hash = hash_bytes(&state.round_phase, static_cast<int32_t>(sizeof(state.round_phase)), hash);
    return hash;
}

}  // namespace ds::sim
