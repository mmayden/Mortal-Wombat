#include "render/camera.h"

#include "sim/constants.h"
#include "sim/state.h"

namespace ds::render {
namespace {

// How close a fighter may come to the edge of the view before the camera
// starts following. With a 480-unit screen this leaves a 300-unit band in
// which both fighters can move freely and the view does not budge.
//
// PROVISIONAL: DESIGN.md says nothing about the camera, and 5.5 leaves the
// stage undecided entirely. This is a feel value -- too small and the view
// shoves back and forth, too large and a fighter can walk off screen.
constexpr float CAMERA_EDGE_MARGIN = 90.0f;

float to_float(ds::sim::Fixed value) {
    return static_cast<float>(value.raw) / static_cast<float>(ds::sim::FIXED_ONE);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float interpolated_x(const ds::sim::GameState& previous, const ds::sim::GameState& current,
                     int32_t index, float alpha) {
    return lerp(to_float(previous.fighters[index].x), to_float(current.fighters[index].x), alpha);
}

}  // namespace

void camera_update(Camera& camera, const ds::sim::GameState& previous,
                   const ds::sim::GameState& current, float alpha) {
    const float first = interpolated_x(previous, current, 0, alpha);
    const float second = interpolated_x(previous, current, 1, alpha);

    const float leftmost = first < second ? first : second;
    const float rightmost = first < second ? second : first;

    const float screen_width = static_cast<float>(ds::sim::SCREEN_WIDTH);
    const float furthest_left = 0.0f;
    const float furthest_right =
        static_cast<float>(ds::sim::STAGE_WIDTH) - static_cast<float>(ds::sim::SCREEN_WIDTH);

    if (!camera.initialized) {
        // Open centred, so the first frame of a round is symmetric.
        camera.x = (first + second) * 0.5f - screen_width * 0.5f;
        camera.initialized = true;
    }

    // The two requirements, as an interval of acceptable camera positions:
    // keep the right fighter inside the right margin, and the left fighter
    // inside the left margin.
    const float at_least = rightmost - (screen_width - CAMERA_EDGE_MARGIN);
    const float at_most = leftmost - CAMERA_EDGE_MARGIN;

    if (at_least <= at_most) {
        // Both fit. Move only as far as needed, which is what makes this a
        // deadzone: inside it the view does not budge at all, so an idle
        // fighter holds exactly still on screen.
        if (camera.x < at_least) {
            camera.x = at_least;
        }
        if (camera.x > at_most) {
            camera.x = at_most;
        }
    } else {
        // They are further apart than the margins allow, so one requirement
        // must give. Split the difference and centre.
        //
        // The previous version applied both clamps in order and let the second
        // win, which always sacrificed the LEFT fighter. Measured: player one
        // backing away walked off the left edge of the screen after about
        // three and a half seconds while player two sat pinned at the right.
        // It was invisible in review because each clamp is correct alone, and
        // invisible in play to whoever happened to be on the right.
        //
        // Centring cannot keep both on screen once they are more than a screen
        // apart -- nothing can, without zooming, which SDL_Renderer scaling
        // would have to do at the whole-frame level (ADR 0016). What it does
        // guarantee is that the cost falls on both players equally.
        camera.x = (leftmost + rightmost) * 0.5f - screen_width * 0.5f;
    }

    if (camera.x < furthest_left) {
        camera.x = furthest_left;
    }
    if (camera.x > furthest_right) {
        camera.x = furthest_right;
    }
}

}  // namespace ds::render
