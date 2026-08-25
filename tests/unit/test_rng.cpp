// Seeded RNG. ADR 0002.
//
// What matters here is not statistical quality — it is that the same seed
// produces the same sequence, that saving and restoring state resumes it
// exactly, and that streams do not interfere. Each of those is a rollback
// requirement before it is a randomness requirement.
#include <cstring>
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/rng.h"

using namespace ds::sim;

TEST_CASE("The same seed produces the same sequence") {
    RngState a{};
    RngState b{};
    rng_seed(a, 12345u);
    rng_seed(b, 12345u);

    for (int32_t i = 0; i < 1000; ++i) {
        CHECK(rng_next(a, RngStream::Combat) == rng_next(b, RngStream::Combat));
    }
}

TEST_CASE("Different seeds produce different sequences") {
    RngState a{};
    RngState b{};
    rng_seed(a, 1u);
    rng_seed(b, 2u);

    int32_t differences = 0;
    for (int32_t i = 0; i < 100; ++i) {
        if (rng_next(a, RngStream::Combat) != rng_next(b, RngStream::Combat)) {
            ++differences;
        }
    }
    // Not asserting all 100 differ: two sequences colliding on a single draw
    // is expected at roughly 1 in 4 billion, and a test that forbids it would
    // be a flake waiting to happen.
    CHECK(differences > 95);
}

TEST_CASE("Restoring saved state resumes the exact sequence") {
    // This is the rollback contract. RngState is part of GameState, so a
    // rollback restores it by memcpy and re-draws the same numbers. If this
    // fails, every rolled-back frame diverges.
    RngState rng{};
    rng_seed(rng, 999u);

    for (int32_t i = 0; i < 50; ++i) {
        rng_next(rng, RngStream::Combat);
    }

    RngState snapshot{};
    std::memcpy(&snapshot, &rng, sizeof(RngState));

    uint32_t original[20];
    for (uint32_t& value : original) {
        value = rng_next(rng, RngStream::Combat);
    }

    std::memcpy(&rng, &snapshot, sizeof(RngState));

    for (uint32_t expected : original) {
        CHECK(rng_next(rng, RngStream::Combat) == expected);
    }
}

TEST_CASE("Streams are independent") {
    // The reason named streams exist: adding a hit-spark effect that draws
    // from Effects must not shift which numbers Combat sees, or every replay
    // recorded before the effect existed breaks.
    RngState with_effects{};
    RngState without_effects{};
    rng_seed(with_effects, 4242u);
    rng_seed(without_effects, 4242u);

    for (int32_t i = 0; i < 100; ++i) {
        rng_next(with_effects, RngStream::Effects);
        rng_next(with_effects, RngStream::Effects);

        CHECK(rng_next(with_effects, RngStream::Combat) ==
              rng_next(without_effects, RngStream::Combat));
    }
}

TEST_CASE("Streams from the same seed differ from each other") {
    RngState rng{};
    rng_seed(rng, 7u);

    int32_t differences = 0;
    for (int32_t i = 0; i < 100; ++i) {
        if (rng_next(rng, RngStream::Combat) != rng_next(rng, RngStream::Effects)) {
            ++differences;
        }
    }
    CHECK(differences > 95);
}

TEST_CASE("rng_below stays within bounds") {
    RngState rng{};
    rng_seed(rng, 31337u);

    SUBCASE("every draw is inside the range") {
        for (int32_t i = 0; i < 10000; ++i) {
            const uint32_t value = rng_below(rng, RngStream::Combat, 6u);
            CHECK(value < 6u);
        }
    }

    SUBCASE("a bound of one always yields zero") {
        for (int32_t i = 0; i < 100; ++i) {
            CHECK(rng_below(rng, RngStream::Combat, 1u) == 0u);
        }
    }

    SUBCASE("a bound of zero yields zero rather than dividing by zero") {
        // The sim has no error path (CONVENTIONS.md 3), so this returns a
        // defined value instead of asserting.
        CHECK(rng_below(rng, RngStream::Combat, 0u) == 0u);
    }
}

TEST_CASE("rng_below covers its whole range") {
    // Guards against a rejection-sampling bug that silently excludes the top
    // value — the kind of thing that only shows up as "that outcome never
    // happens" months later.
    RngState rng{};
    rng_seed(rng, 55u);

    bool seen[6] = {};
    for (int32_t i = 0; i < 10000; ++i) {
        seen[rng_below(rng, RngStream::Combat, 6u)] = true;
    }

    for (bool value : seen) {
        CHECK(value);
    }
}

TEST_CASE("rng_range is inclusive on both ends") {
    RngState rng{};
    rng_seed(rng, 808u);

    SUBCASE("values stay inside the requested range") {
        for (int32_t i = 0; i < 10000; ++i) {
            const int32_t value = rng_range(rng, RngStream::Combat, -5, 5);
            CHECK(value >= -5);
            CHECK(value <= 5);
        }
    }

    SUBCASE("both endpoints are reachable") {
        bool saw_low = false;
        bool saw_high = false;
        for (int32_t i = 0; i < 10000; ++i) {
            const int32_t value = rng_range(rng, RngStream::Combat, -5, 5);
            saw_low = saw_low || value == -5;
            saw_high = saw_high || value == 5;
        }
        CHECK(saw_low);
        CHECK(saw_high);
    }

    SUBCASE("a degenerate range returns its single value") {
        CHECK(rng_range(rng, RngStream::Combat, 3, 3) == 3);
        CHECK(rng_range(rng, RngStream::Combat, 5, 1) == 5);
    }
}

TEST_CASE("RngState satisfies the GameState memcpy contract") {
    CHECK(std::is_trivially_copyable_v<RngState>);
    CHECK(std::is_standard_layout_v<RngState>);
    CHECK(sizeof(RngState) == 2 * sizeof(uint64_t));
}
