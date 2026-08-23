// Where the view sits. Render-layer state, never rolled back.
//
// The camera is NOT part of GameState (ARCHITECTURE.md 3): it cannot affect the
// outcome of a match, two players can in principle be looking at different
// views of the same match, and rollback re-simulating a frame must not disturb
// where anyone is looking.
#pragma once

namespace mw::sim {
struct GameState;
}

namespace mw::render {

struct Camera {
    float x;
    bool initialized;
};

// Moves the camera only as much as it must to keep both fighters on screen.
//
// The obvious implementation -- centre the view on the midpoint between the
// fighters -- is what a fighting game camera is usually described as doing, and
// it is wrong in a way that is very hard to name when you see it. Moving one
// fighter by X shifts the midpoint by X/2, so the camera shifts by X/2 too:
// you gain X/2 on screen and your STATIONARY opponent loses X/2. Equal and
// opposite. It reads unmistakably as the other character sliding toward you
// under its own power, and it was reported as exactly that.
//
// Measured before the fix: player one walking right moved itself +144px and
// moved a completely idle player two -144px.
//
// So the camera holds still while both fighters are comfortably inside the
// view, and scrolls only when one of them nears an edge -- the deadzone every
// side-on fighting game actually uses.
void camera_update(Camera& camera, const mw::sim::GameState& previous,
                   const mw::sim::GameState& current, float alpha);

}  // namespace mw::render
