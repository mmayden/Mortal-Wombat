// Camera behaviour. The one piece of the render layer that can be tested,
// because it is arithmetic over two positions and touches no SDL.
//
// This file exists because a human reported the same defect twice -- "if I move
// right, the other character moves left towards me" -- and there was nothing to
// run either time. The first fix was verified by measuring pixels by hand and
// then deleted from memory; the second report had to start from scratch.
//
// Two cases carry the weight. "An idle fighter holds exactly still" is the
// user's sentence turned into an assertion. "Neither fighter leaves the screen"
// is the defect this file found on its first run.

#include <doctest/doctest.h>

#include "render/camera.h"
#include "sim/constants.h"
#include "sim/state.h"

using mw::render::Camera;
using mw::render::camera_update;
using namespace mw::sim;

namespace {

// Screen position of a fighter: world position minus where the view sits.
// This is exactly what renderer.cpp computes before drawing.
float screen_x(const GameState& state, int32_t index, const Camera& camera) {
    return static_cast<float>(state.fighters[index].x.raw) / static_cast<float>(FIXED_ONE) -
           camera.x;
}

// Both snapshots identical, so alpha cannot matter and the test measures the
// camera rather than the interpolation.
void settle(Camera& camera, const GameState& state) {
    camera_update(camera, state, state, 1.0f);
}

GameState round_start() {
    GameState state{};
    init_state(state, 1u);
    return state;
}

void set_x(GameState& state, int32_t index, int32_t x) {
    state.fighters[index].x = Fixed::from_int(x);
}

}  // namespace

TEST_CASE("Camera opens centred on the two fighters") {
    GameState state = round_start();
    Camera camera{};
    settle(camera, state);

    const float left = screen_x(state, 0, camera);
    const float right = screen_x(state, 1, camera);

    // Symmetric about the middle of the screen: equal margins on both sides.
    const float screen = static_cast<float>(SCREEN_WIDTH);
    CHECK(left == doctest::Approx(screen - right));
    CHECK(left > 0.0f);
    CHECK(right < screen);
}

TEST_CASE("A fighter walking inside the deadzone does not move the camera") {
    GameState state = round_start();
    Camera camera{};
    settle(camera, state);
    const float opened_at = camera.x;

    // Walk player one 40 units toward player two, well inside the margin.
    for (int32_t i = 0; i < 40; ++i) {
        set_x(state, 0, static_cast<int32_t>(state.fighters[0].x.to_int() + 1));
        settle(camera, state);
    }

    CHECK(camera.x == doctest::Approx(opened_at));
}

// THE REPORTED BUG, as an assertion.
//
// The naive camera -- centre on the midpoint -- moves by X/2 when one fighter
// moves by X, so a completely idle opponent slides X/2 toward you. It reads
// unmistakably as the other character walking at you under its own power.
//
// Stated as the property rather than the implementation: while the camera is
// not obliged to move, a fighter that is not moving must hold EXACTLY still on
// screen. Not approximately -- exactly. Any drift at all is the bug.
TEST_CASE("An idle fighter holds exactly still while the camera need not move") {
    for (int32_t direction = -1; direction <= 1; direction += 2) {
        CAPTURE(direction);

        GameState state = round_start();
        Camera camera{};
        settle(camera, state);

        const int32_t idle_world_x = state.fighters[1].x.to_int();
        const float idle_at_start = screen_x(state, 1, camera);
        const float opened_at = camera.x;

        for (int32_t step = 0; step < 50; ++step) {
            set_x(state, 0, state.fighters[0].x.to_int() + direction);
            settle(camera, state);

            REQUIRE(state.fighters[1].x.to_int() == idle_world_x);
            if (camera.x != opened_at) {
                break;  // Camera was forced to move; the property no longer applies.
            }
            CHECK(screen_x(state, 1, camera) == doctest::Approx(idle_at_start));
        }
    }
}

// The defect this file was written for, and the reason the render layer needed
// a test at all: the old camera applied two clamps in order and let the second
// win, so the LEFT fighter was always the one sacrificed. Measured before the
// fix -- player one backing away left the screen entirely after about three and
// a half seconds of walking, while player two sat pinned at the right edge.
TEST_CASE("Neither fighter leaves the screen while both can fit on it") {
    GameState state = round_start();
    Camera camera{};
    settle(camera, state);

    // Walk player one away until the pair are nearly a full screen apart --
    // the widest separation at which keeping both visible is still possible.
    while (state.fighters[1].x.to_int() - state.fighters[0].x.to_int() < SCREEN_WIDTH - 4) {
        set_x(state, 0, state.fighters[0].x.to_int() - 1);
        settle(camera, state);

        CAPTURE(state.fighters[0].x.to_int());
        CAPTURE(state.fighters[1].x.to_int() - state.fighters[0].x.to_int());
        CHECK(screen_x(state, 0, camera) >= 0.0f);
        CHECK(screen_x(state, 1, camera) <= static_cast<float>(SCREEN_WIDTH));
    }
}

// Whatever the camera does when they no longer fit, it must not pick a side.
TEST_CASE("Being off screen costs both players the same") {
    GameState state = round_start();
    Camera camera{};
    set_x(state, 0, 40);
    set_x(state, 1, 920);
    settle(camera, state);

    const float left_overhang = 0.0f - screen_x(state, 0, camera);
    const float right_overhang = screen_x(state, 1, camera) - static_cast<float>(SCREEN_WIDTH);
    CHECK(left_overhang == doctest::Approx(right_overhang));
}

TEST_CASE("Camera follows once a fighter reaches the edge margin") {
    GameState state = round_start();
    Camera camera{};
    settle(camera, state);
    const float opened_at = camera.x;

    // Far enough left to leave the deadzone, but still close enough that both
    // fighters fit on screen -- so following is both required and possible.
    set_x(state, 0, state.fighters[1].x.to_int() - (SCREEN_WIDTH - 40));
    settle(camera, state);

    CHECK(camera.x < opened_at);  // it actually followed
    CHECK(screen_x(state, 0, camera) >= 0.0f);
    CHECK(screen_x(state, 1, camera) <= static_cast<float>(SCREEN_WIDTH));
}

TEST_CASE("Camera never shows past either end of the stage") {
    GameState state = round_start();
    Camera camera{};

    set_x(state, 0, STAGE_LEFT_BOUND);
    set_x(state, 1, STAGE_LEFT_BOUND + 1);
    settle(camera, state);
    CHECK(camera.x >= 0.0f);

    Camera far_side{};
    set_x(state, 0, STAGE_RIGHT_BOUND - 1);
    set_x(state, 1, STAGE_RIGHT_BOUND);
    settle(far_side, state);
    CHECK(far_side.x <= static_cast<float>(STAGE_WIDTH - SCREEN_WIDTH));
}
