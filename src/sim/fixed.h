// Fixed-point scalar for the simulation. ADR 0002.
//
// The simulation must be bit-identical across Linux, Windows, and macOS, on
// x86-64 and ARM. Floating point is not, in practice: compilers contract
// multiply-add into FMA differently, evaluate at different intermediate
// precisions, and vectorize reductions in different orders. Every one of those
// is a desync that only appears in a real match between two machines.
//
// i32.16 removes the whole class of problem. Integer arithmetic is exactly
// specified, so two machines running the same inputs produce the same bytes.
#pragma once

#include <cstdint>

namespace mw::sim {

// Number of fractional bits. 16 gives a range of +/-32768 units with a
// resolution of 1/65536 — the stage is 960 units wide (DESIGN.md 4.4), so
// this is roughly 34x more range than positions need, which leaves headroom
// for intermediate results.
inline constexpr int32_t FIXED_SHIFT = 16;
inline constexpr int32_t FIXED_ONE = 1 << FIXED_SHIFT;

// A signed fixed-point number with 16 fractional bits.
//
// Trivially copyable and the same size as an int32_t, so it can live directly
// inside GameState without breaking the memcpy contract (ADR 0005).
struct Fixed {
    int32_t raw;

    // Defaulted, not user-provided. A hand-written `Fixed() : raw(0) {}` would
    // make Fixed non-trivially-default-constructible, and that property is
    // contagious: every struct holding one -- Fighter, Projectile, GameState --
    // stops being trivial too, and memset on a non-trivial type is a GCC
    // diagnostic (-Wclass-memaccess) even though it is well-defined for a
    // trivially copyable type.
    //
    // Value-initialization still zeroes: `Fixed()` and `Fixed{}` zero-initialize
    // because the default constructor is not user-provided. Only default
    // -initialization (`Fixed x;`) leaves raw indeterminate, and the sim never
    // does that -- init_state memsets the whole struct before anything reads it.
    Fixed() = default;
    constexpr explicit Fixed(int32_t raw_value) : raw(raw_value) {}

    // Construct from a whole number of units. from_int(3) is exactly 3.0.
    static constexpr Fixed from_int(int32_t whole) {
        return Fixed(static_cast<int32_t>(static_cast<uint32_t>(whole) << FIXED_SHIFT));
    }

    // Construct from an explicit numerator/denominator, so that data files and
    // constants can express "1.2 units per frame" without ever writing a float.
    // from_ratio(12, 10) is the fixed-point value closest to 1.2.
    static constexpr Fixed from_ratio(int32_t numerator, int32_t denominator) {
        return Fixed(
            static_cast<int32_t>((static_cast<int64_t>(numerator) << FIXED_SHIFT) / denominator));
    }

    // Truncate toward negative infinity, matching an arithmetic shift.
    // to_int(-0.5) is -1, not 0. This is deliberate: it is branch-free and
    // therefore identical on every platform, which >> already guarantees for
    // signed types since C++20.
    constexpr int32_t to_int() const { return raw >> FIXED_SHIFT; }

    // Round half away from zero.
    constexpr int32_t round_to_int() const {
        return raw >= 0 ? (raw + (FIXED_ONE / 2)) >> FIXED_SHIFT
                        : -((-raw + (FIXED_ONE / 2)) >> FIXED_SHIFT);
    }
};

// Addition and subtraction are exact within range. Overflow is wraparound on
// the underlying int32_t; positions are clamped to the stage every frame
// (ARCHITECTURE.md section 5, step 7), so reaching it means a logic error, not
// a value we intend to represent.
constexpr Fixed operator+(Fixed a, Fixed b) {
    return Fixed(static_cast<int32_t>(static_cast<uint32_t>(a.raw) + static_cast<uint32_t>(b.raw)));
}

constexpr Fixed operator-(Fixed a, Fixed b) {
    return Fixed(static_cast<int32_t>(static_cast<uint32_t>(a.raw) - static_cast<uint32_t>(b.raw)));
}

constexpr Fixed operator-(Fixed a) {
    return Fixed(static_cast<int32_t>(0u - static_cast<uint32_t>(a.raw)));
}

// Multiply widens to int64 before shifting back down. Doing it in 32 bits
// would overflow for any product of two values above ~181 units, which the
// stage width alone exceeds.
//
// Truncates toward negative infinity, for the same reason to_int() does: the
// arithmetic shift is the same instruction on every target.
constexpr Fixed operator*(Fixed a, Fixed b) {
    const int64_t product = static_cast<int64_t>(a.raw) * static_cast<int64_t>(b.raw);
    return Fixed(static_cast<int32_t>(product >> FIXED_SHIFT));
}

// Divide widens first for the same reason. Division by zero is undefined here
// and is a programming error — the sim has no error path (CONVENTIONS.md 3),
// so callers guarantee a nonzero divisor rather than checking at runtime.
constexpr Fixed operator/(Fixed a, Fixed b) {
    const int64_t numerator = static_cast<int64_t>(a.raw) << FIXED_SHIFT;
    return Fixed(static_cast<int32_t>(numerator / b.raw));
}

// Scaling by a plain integer is exact and much cheaper than converting the
// integer to Fixed first. Frame counts multiply speeds constantly.
constexpr Fixed operator*(Fixed a, int32_t scalar) {
    return Fixed(static_cast<int32_t>(static_cast<int64_t>(a.raw) * scalar));
}

constexpr Fixed operator*(int32_t scalar, Fixed a) {
    return a * scalar;
}

constexpr Fixed operator/(Fixed a, int32_t scalar) {
    return Fixed(a.raw / scalar);
}

constexpr Fixed& operator+=(Fixed& a, Fixed b) {
    a = a + b;
    return a;
}

constexpr Fixed& operator-=(Fixed& a, Fixed b) {
    a = a - b;
    return a;
}

constexpr bool operator==(Fixed a, Fixed b) {
    return a.raw == b.raw;
}
constexpr bool operator!=(Fixed a, Fixed b) {
    return a.raw != b.raw;
}
constexpr bool operator<(Fixed a, Fixed b) {
    return a.raw < b.raw;
}
constexpr bool operator<=(Fixed a, Fixed b) {
    return a.raw <= b.raw;
}
constexpr bool operator>(Fixed a, Fixed b) {
    return a.raw > b.raw;
}
constexpr bool operator>=(Fixed a, Fixed b) {
    return a.raw >= b.raw;
}

constexpr Fixed fixed_abs(Fixed a) {
    return a.raw < 0 ? -a : a;
}

constexpr Fixed fixed_min(Fixed a, Fixed b) {
    return a.raw < b.raw ? a : b;
}

constexpr Fixed fixed_max(Fixed a, Fixed b) {
    return a.raw > b.raw ? a : b;
}

constexpr Fixed fixed_clamp(Fixed value, Fixed low, Fixed high) {
    return fixed_min(fixed_max(value, low), high);
}

}  // namespace mw::sim
