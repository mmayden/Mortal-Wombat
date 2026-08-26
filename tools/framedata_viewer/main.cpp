// Frame-data viewer: draws what the TOML says, so the geometry can be reviewed.
//
// The stack decision said to build this BEFORE authoring content. That advice
// was overrun -- every hitbox in data/characters/ was hand-written to reach the
// first connecting hit -- and the cost has already been paid once. The jump
// attack shipped with its hitbox at standing-punch height, floating above the
// opponent's head for its entire arc. It could not hit anyone from any range at
// any timing.
//
// Nobody saw it, because "x = 16, y = -110, w = 30, h = 20" is plausible as
// text and a miss is the absence of an event. Drawn, it is obvious in a second.
// That is the whole argument for this program.
//
// It is a VIEWER, not yet an editor. Editing needs text entry, which needs a UI
// toolkit, which is ADR 0012's unintegrated Dear ImGui and a dependency this
// cannot add on its own (AGENTS rule 4). Viewing is where the value was anyway:
// the bug that motivated it was a review failure, not a typing failure.
//
// tools/ is above the sim boundary and may use anything (tools/README.md).

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "data/framedata_loader.h"
#include "data/reach.h"
#include "ds_log.h"
#include "framedata_viewer/font.h"
#include "sim/constants.h"
#include "sim/framedata.h"
#include "sim/sim.h"
#include "sim/state.h"

namespace {

using namespace ds::sim;
using ds::data::measure_reach;
using ds::data::MoveReach;
using ds::tools::draw_text;

constexpr int32_t VIEW_W = 640;
constexpr int32_t VIEW_H = 360;

// Where the ground sits on screen, and how much of the world fits across it.
// The fighters are ~140 units tall against a 270-unit screen in game; here the
// view is zoomed so a hitbox twenty units high is actually inspectable.
// 1.15 rather than something rounder: at 1.6 a 140-unit fighter stood 224
// pixels tall and its hurtbox ran up through the readout, which the first
// screenshot of this tool showed immediately and reading the code never would
// have. The panel below covers the rest.
constexpr float ZOOM = 1.15f;
constexpr float GROUND_SCREEN_Y = 300.0f;
constexpr float ATTACKER_SCREEN_X = 150.0f;

constexpr SDL_Color COL_BG{18, 20, 26, 255};
constexpr SDL_Color COL_GROUND{60, 64, 74, 255};
constexpr SDL_Color COL_TEXT{226, 230, 238, 255};
constexpr SDL_Color COL_DIM{130, 138, 152, 255};
constexpr SDL_Color COL_HURT{70, 130, 220, 255};
constexpr SDL_Color COL_HIT{225, 70, 70, 255};
constexpr SDL_Color COL_PUSH{215, 190, 60, 255};
constexpr SDL_Color COL_GOOD{110, 210, 130, 255};
constexpr SDL_Color COL_BAD{240, 110, 110, 255};

const char* move_name(MoveId move) {
    // No default case, deliberately. MSVC /W4 and GCC -Wswitch both warn on an
    // unhandled enumerator, and both are errors here -- so adding a MoveId
    // fails the build until it is named. The medium buttons shipped as
    // "UNKNOWN" in this very function because a default silently absorbed them.
    switch (move) {
        case MoveId::StandLightPunch: return "STAND LP";
        case MoveId::StandMediumPunch: return "STAND MP";
        case MoveId::StandHeavyPunch: return "STAND HP";
        case MoveId::StandLightKick: return "STAND LK";
        case MoveId::StandMediumKick: return "STAND MK";
        case MoveId::StandHeavyKick: return "STAND HK";
        case MoveId::CrouchLightPunch: return "CROUCH LP";
        case MoveId::CrouchMediumPunch: return "CROUCH MP";
        case MoveId::CrouchHeavyPunch: return "CROUCH HP";
        case MoveId::CrouchLightKick: return "CROUCH LK";
        case MoveId::CrouchMediumKick: return "CROUCH MK";
        case MoveId::CrouchHeavyKick: return "CROUCH HK";
        case MoveId::JumpAttack: return "JUMP ATTACK";
        case MoveId::Special: return "SPECIAL";
        case MoveId::Count: break;
    }
    return "INVALID";
}

// Which third of the move a frame falls in. This is the vocabulary in
// docs/MECHANICS.md, shown here so the drawing teaches it.
const char* phase_of(const MoveData& move, int32_t frame) {
    if (frame <= move.startup) {
        return "STARTUP";
    }
    if (frame <= move.startup + move.active) {
        return "ACTIVE";
    }
    return "RECOVERY";
}

void set_color(SDL_Renderer* renderer, SDL_Color c, uint8_t alpha = 255) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, alpha);
}

// World box -> screen rect. World +y is down and the ground is y = GROUND_Y,
// matching ARCHITECTURE 3, so this only scales and shifts.
SDL_FRect to_screen(const Box& box, float origin_screen_x, float world_origin_x) {
    return SDL_FRect{
        origin_screen_x + (static_cast<float>(box.x) - world_origin_x) * ZOOM,
        GROUND_SCREEN_Y + (static_cast<float>(box.y) - static_cast<float>(GROUND_Y)) * ZOOM,
        static_cast<float>(box.w) * ZOOM, static_cast<float>(box.h) * ZOOM};
}

void draw_box(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color, bool filled) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (filled) {
        set_color(renderer, color, 90);
        SDL_RenderFillRect(renderer, &rect);
    }
    set_color(renderer, color, 255);
    SDL_RenderRect(renderer, &rect);
}

struct View {
    int32_t attacker = 0;
    int32_t move_index = 0;
    int32_t frame = 1;
    int32_t gap = 90;
    bool defender_crouching = false;
    bool playing = true;
    int32_t play_counter = 0;
};

// Prints every move's measured reach and exits. The headless half of the tool:
// the same answer as the picture, in a form a terminal or a CI log can carry.
int print_report(const MatchData& data) {
    for (int32_t c = 0; c < 2; ++c) {
        std::printf("\ncharacter %d\n", c);
        std::printf("  %-14s %7s %7s %8s %8s   %s\n", "move", "start", "active", "nearest",
                    "furthest", "verdict");
        for (int32_t m = 0; m < MOVE_COUNT; ++m) {
            const MoveId move = static_cast<MoveId>(m);
            const MoveData& md = move_of(data.characters[c], move);
            const MoveReach reach = measure_reach(data.characters[c], data.characters[1 - c], move);

            std::printf("  %-14s %7d %7d %8d %8d   %s\n", move_name(move), md.startup, md.active,
                        reach.connects ? reach.nearest : 0, reach.connects ? reach.furthest : 0,
                        reach.connects ? "ok"
                                       : "CANNOT CONNECT AT ANY RANGE -- check the geometry");
        }
    }
    std::printf(
        "\nJump attack is measured on the ground here, so its numbers are not\n"
        "meaningful -- it is performed from the air. See tests/unit/test_jump_attack_reach.cpp.\n");
    return 0;
}

void draw_frame(SDL_Renderer* renderer, const MatchData& data, const View& view) {
    const CharacterData& attacker = data.characters[view.attacker];
    const CharacterData& defender = data.characters[1 - view.attacker];
    const MoveId move = static_cast<MoveId>(view.move_index);
    const MoveData& md = move_of(attacker, move);
    const int32_t total = move_total_frames(md);

    set_color(renderer, COL_BG);
    SDL_RenderClear(renderer);

    const SDL_FRect ground{0.0f, GROUND_SCREEN_Y, static_cast<float>(VIEW_W), 2.0f};
    set_color(renderer, COL_GROUND);
    SDL_RenderFillRect(renderer, &ground);

    Fighter attacking{};
    attacking.facing = Facing::Right;
    attacking.x = Fixed::from_int(0);
    attacking.y = Fixed::from_int(GROUND_Y);

    Fighter defending{};
    defending.facing = Facing::Left;
    defending.x = Fixed::from_int(view.gap);
    defending.y = Fixed::from_int(GROUND_Y);

    const bool crouching_attack = move >= MoveId::CrouchLightPunch && move <= MoveId::CrouchHeavyKick;
    const Box& attacker_hurt =
        crouching_attack ? attacker.crouching_hurtbox : attacker.standing_hurtbox;
    const Box& defender_hurt =
        view.defender_crouching ? defender.crouching_hurtbox : defender.standing_hurtbox;

    draw_box(renderer, to_screen(world_box(attacking, attacker.pushbox), ATTACKER_SCREEN_X, 0.0f),
             COL_PUSH, false);
    draw_box(renderer, to_screen(world_box(attacking, attacker_hurt), ATTACKER_SCREEN_X, 0.0f),
             COL_HURT, true);
    draw_box(renderer, to_screen(world_box(defending, defender.pushbox), ATTACKER_SCREEN_X, 0.0f),
             COL_PUSH, false);

    const Box defender_world = world_box(defending, defender_hurt);
    draw_box(renderer, to_screen(defender_world, ATTACKER_SCREEN_X, 0.0f), COL_HURT, true);

    // The hitboxes live on this frame, and whether any of them is touching the
    // defender right now. This is the question the tool exists to answer.
    bool touching = false;
    for (int32_t i = 0; i < md.hitbox_count; ++i) {
        const HitboxSpan& span = md.hitboxes[i];
        if (view.frame < span.first_frame || view.frame > span.last_frame) {
            continue;
        }
        if (box_is_empty(span.box)) {
            continue;
        }
        const Box hit_world = world_box(attacking, span.box);
        if (boxes_overlap(hit_world, defender_world)) {
            touching = true;
        }
        draw_box(renderer, to_screen(hit_world, ATTACKER_SCREEN_X, 0.0f), COL_HIT, true);
    }

    const MoveReach reach =
        measure_reach(attacker, defender, move, view.defender_crouching);

    // Opaque panel behind the readout. A fighter is drawn wherever the data
    // says, so the text cannot simply be placed where nothing else goes.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const SDL_FRect panel{0.0f, 0.0f, 336.0f, 118.0f};
    set_color(renderer, COL_BG, 232);
    SDL_RenderFillRect(renderer, &panel);
    set_color(renderer, COL_GROUND, 255);
    SDL_RenderRect(renderer, &panel);

    float y = 12.0f;
    const float line = 13.0f;
    draw_text(renderer, 12.0f, y,
              std::string(view.attacker == 0 ? "GEORGE" : "SUE") + "  " + move_name(move),
              2.0f, COL_TEXT);
    y += line + 6.0f;

    draw_text(renderer, 12.0f, y,
              "FRAME " + std::to_string(view.frame) + "/" + std::to_string(total) + "  " +
                  phase_of(md, view.frame),
              1.5f, COL_TEXT);
    y += line;
    draw_text(renderer, 12.0f, y,
              "STARTUP " + std::to_string(md.startup) + "  ACTIVE " + std::to_string(md.active) +
                  "  RECOVERY " + std::to_string(md.recovery),
              1.5f, COL_DIM);
    y += line;
    draw_text(renderer, 12.0f, y,
              "DAMAGE " + std::to_string(md.damage) + "  HITSTUN " + std::to_string(md.hitstun) +
                  "  BLOCKSTUN " + std::to_string(md.blockstun),
              1.5f, COL_DIM);
    y += line;
    draw_text(renderer, 12.0f, y, "GAP " + std::to_string(view.gap) + " UNITS", 1.5f, COL_DIM);
    y += line + 4.0f;

    if (reach.connects) {
        draw_text(renderer, 12.0f, y,
                  "REACHES " + std::to_string(reach.nearest) + "-" +
                      std::to_string(reach.furthest) + " UNITS",
                  1.5f, COL_GOOD);
    } else {
        draw_text(renderer, 12.0f, y, "CANNOT CONNECT AT ANY RANGE!", 1.5f, COL_BAD);
    }
    y += line;
    draw_text(renderer, 12.0f, y, touching ? "TOUCHING NOW" : "NOT TOUCHING", 1.5f,
              touching ? COL_GOOD : COL_DIM);

    draw_text(renderer, 12.0f, static_cast<float>(VIEW_H) - 30.0f,
              "TAB CHARACTER   ARROWS MOVE/FRAME   A D GAP   S CROUCH   SPACE PLAY", 1.0f,
              COL_DIM);
    draw_text(renderer, 12.0f, static_cast<float>(VIEW_H) - 18.0f,
              "R RESET GAP TO MAX REACH   ESC QUIT", 1.0f, COL_DIM);

    SDL_RenderPresent(renderer);
}

}  // namespace

int main(int argc, char** argv) {
    MatchData data{};
    {
        const std::string dir = std::string(DS_DATA_DIR) + "/characters/";
        std::string error;
        const ds::data::LoadResult result =
            ds::data::load_match(dir + "george.toml", dir + "sue.toml", data, error);
        if (result != ds::data::LoadResult::Ok) {
            DS_LOG_ERROR("could not load frame data (%s): %s", ds::data::load_result_name(result),
                         error.c_str());
            return 1;
        }
    }

    const char* screenshot = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--report") == 0) {
            return print_report(data);
        }
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DS_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Divided States - frame data", VIEW_W * 2, VIEW_H * 2,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        DS_LOG_ERROR("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, VIEW_W, VIEW_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    View view;

    // Capture one frame and exit. This is how a drawing tool gets reviewed:
    // reading its source proves nothing about what appears on screen.
    if (screenshot != nullptr) {
        view.playing = false;
        view.move_index = static_cast<int32_t>(MoveId::StandMediumKick);
        view.frame = move_of(data.characters[0], MoveId::StandMediumKick).startup + 1;
        view.gap = 70;

        draw_frame(renderer, data, view);

        // Redraw before reading. SDL_RenderReadPixels reads the back buffer,
        // whose contents are undefined after SDL_RenderPresent -- capturing
        // straight after a present returns the PREVIOUS frame, which is exactly
        // enough to make a hitbox look inactive on the frame it connected.
        // Learned the hard way in the game's own --screenshot path.
        draw_frame(renderer, data, view);

        SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
        const bool ok = surface != nullptr && SDL_SaveBMP(surface, screenshot);
        if (surface != nullptr) {
            SDL_DestroySurface(surface);
        }
        if (!ok) {
            DS_LOG_ERROR("screenshot failed: %s", SDL_GetError());
        } else {
            DS_LOG_INFO("wrote %s", screenshot);
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return ok ? 0 : 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                const CharacterData& attacker = data.characters[view.attacker];
                const MoveData& md = move_of(attacker, static_cast<MoveId>(view.move_index));
                const int32_t total = move_total_frames(md);

                switch (event.key.scancode) {
                    case SDL_SCANCODE_ESCAPE: running = false; break;
                    case SDL_SCANCODE_TAB: view.attacker = 1 - view.attacker; break;
                    case SDL_SCANCODE_LEFT:
                        view.move_index = (view.move_index + MOVE_COUNT - 1) % MOVE_COUNT;
                        view.frame = 1;
                        break;
                    case SDL_SCANCODE_RIGHT:
                        view.move_index = (view.move_index + 1) % MOVE_COUNT;
                        view.frame = 1;
                        break;
                    case SDL_SCANCODE_UP:
                        view.playing = false;
                        view.frame = view.frame <= 1 ? total : view.frame - 1;
                        break;
                    case SDL_SCANCODE_DOWN:
                        view.playing = false;
                        view.frame = view.frame >= total ? 1 : view.frame + 1;
                        break;
                    case SDL_SCANCODE_A: view.gap = view.gap > 8 ? view.gap - 2 : 8; break;
                    case SDL_SCANCODE_D: view.gap = view.gap < 400 ? view.gap + 2 : 400; break;
                    case SDL_SCANCODE_S:
                        view.defender_crouching = !view.defender_crouching;
                        break;
                    case SDL_SCANCODE_SPACE: view.playing = !view.playing; break;
                    case SDL_SCANCODE_R: {
                        const MoveReach reach =
                            measure_reach(attacker, data.characters[1 - view.attacker],
                                          static_cast<MoveId>(view.move_index),
                                          view.defender_crouching);
                        if (reach.connects) {
                            view.gap = reach.furthest;
                        }
                        break;
                    }
                    default: break;
                }
            }
        }

        if (view.playing) {
            // Six render frames per move frame: fast enough to read as motion,
            // slow enough to watch a two-frame active window go by.
            if (++view.play_counter >= 6) {
                view.play_counter = 0;
                const MoveData& md =
                    move_of(data.characters[view.attacker], static_cast<MoveId>(view.move_index));
                const int32_t total = move_total_frames(md);
                view.frame = view.frame >= total ? 1 : view.frame + 1;
            }
        }

        // Clamp after any change, so switching to a shorter move cannot leave
        // the cursor past its end.
        {
            const MoveData& md =
                move_of(data.characters[view.attacker], static_cast<MoveId>(view.move_index));
            const int32_t total = move_total_frames(md);
            if (view.frame > total) {
                view.frame = total;
            }
            if (view.frame < 1) {
                view.frame = 1;
            }
        }

        draw_frame(renderer, data, view);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
