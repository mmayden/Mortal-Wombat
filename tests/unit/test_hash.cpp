// State hashing — the mechanism behind the desync test. ADR 0011.
//
// The property that matters is not that the hash is good, but that it is
// *sensitive*: any change to match-affecting state must change it. A hash that
// misses a field turns the desync test green forever, which is worse than not
// having the test, because it is trusted.
#include <cstring>

#include <doctest/doctest.h>

#include "sim/hash.h"
#include "sim/sim.h"

#include "match_data.h"

using namespace ds::sim;

namespace {
constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};
}  // namespace

TEST_CASE("Identical states hash identically") {
    GameState a;
    GameState b;
    init_state(a, 777u);
    init_state(b, 777u);

    CHECK(hash_state(a) == hash_state(b));
    CHECK(hash_state_visible(a) == hash_state_visible(b));
}

TEST_CASE("Hashing is stable across a memcpy round-trip") {
    // This is the rollback save/restore path. If a hash changed across it, the
    // desync detector would fire on every rollback.
    GameState state;
    init_state(state, 888u);
    for (int32_t i = 0; i < 120; ++i) {
        advance_frame(state, ds::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    }

    const uint64_t before = hash_state(state);

    GameState snapshot;
    std::memcpy(&snapshot, &state, sizeof(GameState));
    std::memset(&state, 0, sizeof(GameState));
    std::memcpy(&state, &snapshot, sizeof(GameState));

    CHECK(hash_state(state) == before);
}

TEST_CASE("Any change to match state changes the hash") {
    GameState state;
    init_state(state, 1u);
    const uint64_t baseline = hash_state(state);

    SUBCASE("position") {
        state.fighters[0].x += Fixed(1);
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("a single point of health") {
        state.fighters[1].health -= 1;
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("fighter state") {
        state.fighters[0].state = FighterState::Crouch;
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("one frame of hitstun") {
        state.fighters[1].hitstun_remaining = 1;
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("the round timer") {
        state.round_timer -= 1;
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("a projectile far down the array") {
        // Guards against a hash that walks only the first element, which would
        // pass every test written against fighter state alone.
        state.projectiles[MAX_PROJECTILES - 1].active = 1;
        CHECK(hash_state(state) != baseline);
    }

    SUBCASE("the RNG stream position") {
        rng_next(state.rng, RngStream::Combat);
        CHECK(hash_state(state) != baseline);
    }
}

TEST_CASE("The visible hash ignores the RNG but the full hash does not") {
    // Why both exist: a render-side effect drawing a random number would
    // advance a stream and change hash_state without changing anything a
    // player could observe. Replay tests compare the visible hash so they keep
    // asserting on behavior; the desync test compares the full hash, because
    // there a divergent stream position genuinely is a bug.
    GameState state;
    init_state(state, 1u);

    const uint64_t full_before = hash_state(state);
    const uint64_t visible_before = hash_state_visible(state);

    rng_next(state.rng, RngStream::Effects);

    CHECK(hash_state(state) != full_before);
    CHECK(hash_state_visible(state) == visible_before);
}

TEST_CASE("The hash changes on nearly every frame of a live match") {
    // A hash that only changed occasionally would let a desync hide until the
    // next time it happened to move.
    GameState state;
    init_state(state, 2024u);

    uint64_t previous = hash_state(state);
    int32_t changes = 0;
    for (int32_t i = 0; i < 300; ++i) {
        advance_frame(state, ds::test::shipped_match_data(), NO_INPUT, NO_INPUT);
        const uint64_t current = hash_state(state);
        if (current != previous) {
            ++changes;
        }
        previous = current;
    }

    // The frame counter alone guarantees this; the assertion exists so that
    // removing it from the hash fails loudly.
    CHECK(changes == 300);
}

TEST_CASE("hash_bytes chains so a partial hash can be built up") {
    const uint8_t data[4] = {1u, 2u, 3u, 4u};

    const uint64_t whole = hash_bytes(data, 4);
    const uint64_t chained = hash_bytes(data + 2, 2, hash_bytes(data, 2));

    CHECK(whole == chained);
}

TEST_CASE("hash_bytes distinguishes order") {
    const uint8_t forward[3] = {1u, 2u, 3u};
    const uint8_t backward[3] = {3u, 2u, 1u};

    CHECK(hash_bytes(forward, 3) != hash_bytes(backward, 3));
}
