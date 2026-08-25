// Fixed-point arithmetic. ADR 0002.
//
// These tests are the foundation of every determinism guarantee in the
// project: if Fixed is wrong, nothing above it can be right, and the failure
// shows up as a desync in a real match rather than as a wrong number here.
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/fixed.h"

using namespace ds::sim;

TEST_CASE("Fixed represents whole numbers exactly") {
    CHECK(Fixed::from_int(0).raw == 0);
    CHECK(Fixed::from_int(1).raw == FIXED_ONE);
    CHECK(Fixed::from_int(-1).raw == -FIXED_ONE);
    CHECK(Fixed::from_int(960).to_int() == 960);
    CHECK(Fixed::from_int(-960).to_int() == -960);
}

TEST_CASE("Fixed round-trips whole numbers through to_int") {
    // Stage width is 960 units, so this covers the full range of real
    // positions plus an order of magnitude of headroom.
    for (int32_t value = -10000; value <= 10000; value += 7) {
        CHECK(Fixed::from_int(value).to_int() == value);
    }
}

TEST_CASE("from_ratio produces the nearest representable value") {
    // DESIGN.md 4.4 gives walk speed as 1.2 units/frame. The data format
    // forbids writing 1.2 as a decimal, so this is how it is expressed.
    const Fixed one_point_two = Fixed::from_ratio(12, 10);
    CHECK(one_point_two.raw == 78643);  // trunc(1.2 * 65536)

    CHECK(Fixed::from_ratio(1, 2).raw == FIXED_ONE / 2);
    CHECK(Fixed::from_ratio(-1, 2).raw == -FIXED_ONE / 2);
    CHECK(Fixed::from_ratio(3, 1) == Fixed::from_int(3));
}

TEST_CASE("Addition and subtraction are exact") {
    const Fixed a = Fixed::from_ratio(1, 2);
    const Fixed b = Fixed::from_ratio(1, 4);

    CHECK((a + b).raw == FIXED_ONE / 2 + FIXED_ONE / 4);
    CHECK((a - b).raw == FIXED_ONE / 4);
    CHECK((a - a).raw == 0);
    CHECK((-a).raw == -(FIXED_ONE / 2));

    SUBCASE("compound assignment matches the binary operator") {
        Fixed acc = a;
        acc += b;
        CHECK(acc == a + b);
        acc -= b;
        CHECK(acc == a);
    }
}

TEST_CASE("Multiplication widens before shifting") {
    // The whole reason operator* goes through int64. In raw i32.16, 180 * 180
    // is 13107200 * 13107200, which is about 1.7e14 -- five orders of
    // magnitude past what int32 holds. A 32-bit intermediate would silently
    // produce garbage at ordinary stage distances.
    //
    // 180 is near the top of the type's range: i32.16 represents up to 32767
    // units, so the largest square that fits is 181 * 181.
    const Fixed big = Fixed::from_int(180);
    CHECK((big * big).to_int() == 32400);

    CHECK((Fixed::from_int(3) * Fixed::from_int(4)).to_int() == 12);
    CHECK((Fixed::from_int(-3) * Fixed::from_int(4)).to_int() == -12);
    CHECK((Fixed::from_ratio(1, 2) * Fixed::from_int(8)).to_int() == 4);
    CHECK((Fixed::from_int(5) * Fixed()).raw == 0);
}

TEST_CASE("Division widens before dividing") {
    CHECK((Fixed::from_int(12) / Fixed::from_int(4)).to_int() == 3);
    CHECK((Fixed::from_int(1) / Fixed::from_int(2)).raw == FIXED_ONE / 2);
    CHECK((Fixed::from_int(-12) / Fixed::from_int(4)).to_int() == -3);

    SUBCASE("large numerators do not overflow the shift") {
        // 20000 << 16 exceeds int32; the int64 widening in operator/ is what
        // keeps this correct.
        const Fixed result = Fixed::from_int(20000) / Fixed::from_int(4);
        CHECK(result.to_int() == 5000);
    }
}

TEST_CASE("Integer scaling matches Fixed multiplication") {
    const Fixed speed = Fixed::from_ratio(12, 10);

    for (int32_t frames = 0; frames < 60; ++frames) {
        CHECK(speed * frames == speed * Fixed::from_int(frames));
    }

    CHECK(speed * -1 == -speed);
    CHECK(2 * speed == speed * 2);
}

TEST_CASE("to_int truncates toward negative infinity") {
    // This is deliberate and documented: an arithmetic shift is one branch-free
    // instruction that behaves identically on every target, whereas rounding
    // toward zero needs a sign test. Determinism beats intuition here.
    CHECK(Fixed::from_ratio(3, 2).to_int() == 1);
    CHECK(Fixed::from_ratio(-3, 2).to_int() == -2);
    CHECK(Fixed::from_ratio(-1, 2).to_int() == -1);
    CHECK(Fixed::from_ratio(1, 2).to_int() == 0);
}

TEST_CASE("round_to_int rounds half away from zero") {
    CHECK(Fixed::from_ratio(3, 2).round_to_int() == 2);
    CHECK(Fixed::from_ratio(-3, 2).round_to_int() == -2);
    CHECK(Fixed::from_ratio(1, 2).round_to_int() == 1);
    CHECK(Fixed::from_ratio(-1, 2).round_to_int() == -1);
    CHECK(Fixed::from_ratio(1, 3).round_to_int() == 0);
    CHECK(Fixed::from_int(7).round_to_int() == 7);
}

TEST_CASE("Comparisons order values correctly across zero") {
    const Fixed negative = Fixed::from_int(-5);
    const Fixed zero = Fixed();
    const Fixed positive = Fixed::from_int(5);

    CHECK(negative < zero);
    CHECK(zero < positive);
    CHECK(negative < positive);
    CHECK(negative <= negative);
    CHECK(positive >= positive);
    CHECK(negative != positive);
    CHECK(zero == Fixed::from_int(0));
}

TEST_CASE("Helper functions clamp and select") {
    const Fixed low = Fixed::from_int(-10);
    const Fixed high = Fixed::from_int(10);

    CHECK(fixed_abs(Fixed::from_int(-7)) == Fixed::from_int(7));
    CHECK(fixed_abs(Fixed::from_int(7)) == Fixed::from_int(7));
    CHECK(fixed_min(low, high) == low);
    CHECK(fixed_max(low, high) == high);

    SUBCASE("clamp holds values inside the stage bounds") {
        CHECK(fixed_clamp(Fixed::from_int(-50), low, high) == low);
        CHECK(fixed_clamp(Fixed::from_int(50), low, high) == high);
        CHECK(fixed_clamp(Fixed::from_int(3), low, high) == Fixed::from_int(3));
    }

    SUBCASE("clamp is exact at the bounds themselves") {
        CHECK(fixed_clamp(low, low, high) == low);
        CHECK(fixed_clamp(high, low, high) == high);
    }
}

TEST_CASE("Fixed is layout-compatible with int32_t") {
    // GameState holds Fixed directly, so it has to satisfy the same
    // memcpy contract the struct as a whole does (ADR 0005).
    CHECK(sizeof(Fixed) == sizeof(int32_t));
    CHECK(std::is_trivially_copyable_v<Fixed>);
    CHECK(std::is_standard_layout_v<Fixed>);
}

TEST_CASE("Fixed arithmetic is usable in constant expressions") {
    // constexpr is what lets constants.h express speeds as exact ratios
    // rather than as magic raw numbers.
    constexpr Fixed a = Fixed::from_ratio(12, 10);
    constexpr Fixed b = a * Fixed::from_int(2);
    static_assert(b.raw == 157286);
    CHECK(b.to_int() == 2);
}
