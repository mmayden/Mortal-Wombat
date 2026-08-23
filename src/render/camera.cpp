#include "render/camera.h"

#include "sim/constants.h"
#include "sim/state.h"

namespace mw::render {
namespace {

// How close a fighter may come to the edge of the view before the camera
// starts following. With a 480-unit screen this leaves a 300-unit band in
// which both fighters can move freely and the view does not budge.
//
// PROVISIONAL: DESIGN.md says nothing about the camera, and 5.5 leaves the
// stage undecided entirely. This is a feel value -- too small and the view
// shoves back and forth, too large and a fighter can walk off screen.
constexpr float CAMERA_EDGE_MARGIN = 90.0f;

float to_float(mw::sim::Fixed value) {
    return static_cast<float>(value.raw) / static_cast<float>(mw::sim::FIXED_ONE);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float interpolated_x(const mw::sim::GameState& previous, const mw::sim::GameState& current,
                     int32_t index, float alpha) {
    return lerp(to_float(previous.fighters[index].x), to_float(current.fighters[index].x), alpha);
}

}  // namespace

void camera_update(Camera& camera, const mw::sim::GameState& previous,
                   const mw::sim::GameState& current, float alpha) {
    const float first = interpolated_x(previous, current, 0, alpha);
    const float second = interpolated_x(previous, current, 1, alpha);

    const float leftmost = first < second ? first : second;
    const float rightmost = first < second ? second : first;

    const float screen_width = static_cast<float>(mw::sim::SCREEN_WIDTH);
    const float furthest_left = 0.0f;
    const float furthest_right =
        static_cast<float>(mw::sim::STAGE_WIDTH) - static_cast<float>(mw::sim::SCREEN_WIDTH);

    if (!camera.initialized) {
        // Open centred, so the first frame of a round is symmetric.
        camera.x = (first + second) * 0.5f - screen_width * 0.5f;
        camera.initialized = true;
    }

    // Follow only far enough to pull a fighter back inside the margin. Both
    // clamps can fire at once when the fighters are further apart than the
    // deadzone; the second wins, which keeps the fighter being pushed toward
    // the right edge visible.
    if (leftmost - camera.x < CAMERA_EDGE_MARGIN) {
        camera.x = leftmost - CAMERA_EDGE_MARGIN;
    }
    if (rightmost - camera.x > screen_width - CAMERA_EDGE_MARGIN) {
        camera.x = rightmost - (screen_width - CAMERA_EDGE_MARGIN);
    }

    if (camera.x < furthest_left) {
        camera.x = furthest_left;
    }
    if (camera.x > furthest_right) {
        camera.x = furthest_right;
    }
}

}  // namespace mw::render
