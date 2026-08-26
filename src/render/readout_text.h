// What the training readout says, separated from how it is drawn.
//
// Header-only and SDL-free on purpose. The drawing needs a renderer and cannot
// be tested; the words can be, and the words are the part that has to be right.
// A readout that prints the wrong move name is worse than no readout, because a
// player will believe it -- the same failure as a debug overlay that agrees
// with itself and disagrees with the simulation.
//
// Every switch here is exhaustive with no default case, so adding a MoveId or a
// FighterState fails the build rather than silently printing "?". That rule is
// here because the opposite cost three separate defects in one day: the
// frame-data viewer showed new buttons as UNKNOWN, its test did the same, and
// the renderer drew them at the wrong height.
#pragma once

#include <string>

#include "sim/framedata.h"
#include "sim/input.h"
#include "sim/state.h"

namespace ds::render {

struct ReadoutLines {
    std::string state;    // What the fighter is doing.
    std::string detail;   // The move and its frame, or why it cannot act. May be empty.
    std::string numbers;  // Frame data for the current move. May be empty.
    std::string inputs;   // Buttons held this frame.

    // Which third of the current move the fighter is in, for colouring. None
    // when not attacking.
    enum class Phase { None, Startup, Active, Recovery };
    Phase phase = Phase::None;
};

inline const char* readout_move_name(ds::sim::MoveId move) {
    switch (move) {
        case ds::sim::MoveId::StandLightPunch:
            return "LP";
        case ds::sim::MoveId::StandMediumPunch:
            return "MP";
        case ds::sim::MoveId::StandHeavyPunch:
            return "HP";
        case ds::sim::MoveId::StandLightKick:
            return "LK";
        case ds::sim::MoveId::StandMediumKick:
            return "MK";
        case ds::sim::MoveId::StandHeavyKick:
            return "HK";
        case ds::sim::MoveId::CrouchLightPunch:
            return "CR.LP";
        case ds::sim::MoveId::CrouchMediumPunch:
            return "CR.MP";
        case ds::sim::MoveId::CrouchHeavyPunch:
            return "CR.HP";
        case ds::sim::MoveId::CrouchLightKick:
            return "CR.LK";
        case ds::sim::MoveId::CrouchMediumKick:
            return "CR.MK";
        case ds::sim::MoveId::CrouchHeavyKick:
            return "CR.HK";
        case ds::sim::MoveId::JumpAttack:
            return "JUMP ATK";
        case ds::sim::MoveId::Special:
            return "SPECIAL";
        case ds::sim::MoveId::Throw:
            return "THROW";
        case ds::sim::MoveId::Count:
            break;
    }
    return "?";
}

inline const char* readout_state_name(ds::sim::FighterState state) {
    switch (state) {
        case ds::sim::FighterState::RoundStart:
            return "ROUND START";
        case ds::sim::FighterState::Idle:
            return "IDLE";
        case ds::sim::FighterState::WalkForward:
            return "WALK FWD";
        case ds::sim::FighterState::WalkBackward:
            return "WALK BACK";
        case ds::sim::FighterState::Crouch:
            return "CROUCH";
        case ds::sim::FighterState::JumpStartup:
            return "JUMP START";
        case ds::sim::FighterState::Airborne:
            return "AIRBORNE";
        case ds::sim::FighterState::Landing:
            return "LANDING";
        case ds::sim::FighterState::Attack:
            return "ATTACK";
        case ds::sim::FighterState::Blocking:
            return "BLOCKING";
        case ds::sim::FighterState::Hitstun:
            return "HITSTUN";
        case ds::sim::FighterState::Blockstun:
            return "BLOCKSTUN";
        case ds::sim::FighterState::Knockdown:
            return "KNOCKDOWN";
        case ds::sim::FighterState::Wakeup:
            return "WAKEUP";
        case ds::sim::FighterState::Win:
            return "WIN";
        case ds::sim::FighterState::Lose:
            return "LOSE";
    }
    return "?";
}

// Directions first, so a hold-back block reads as a direction rather than as a
// missing button -- there is no block button to look for (ADR 0021).
inline std::string readout_held_buttons(ds::sim::InputFrame input) {
    struct Label {
        ds::sim::Button button;
        const char* name;
    };
    constexpr Label LABELS[] = {
        {ds::sim::Button::Up, "U"},          {ds::sim::Button::Down, "D"},
        {ds::sim::Button::Left, "L"},        {ds::sim::Button::Right, "R"},
        {ds::sim::Button::LightPunch, "LP"}, {ds::sim::Button::MediumPunch, "MP"},
        {ds::sim::Button::HeavyPunch, "HP"}, {ds::sim::Button::LightKick, "LK"},
        {ds::sim::Button::MediumKick, "MK"}, {ds::sim::Button::HeavyKick, "HK"},
    };

    std::string out;
    for (const Label& label : LABELS) {
        if (ds::sim::input_held(input, label.button)) {
            if (!out.empty()) {
                out += " ";
            }
            out += label.name;
        }
    }
    return out.empty() ? "-" : out;
}

inline ReadoutLines readout_lines(const ds::sim::CharacterData& character,
                                  const ds::sim::Fighter& fighter, ds::sim::InputFrame input) {
    using namespace ds::sim;

    ReadoutLines lines;
    lines.state = readout_state_name(fighter.state);
    lines.inputs = readout_held_buttons(input);

    if (fighter.state == FighterState::Attack && fighter.move_id >= 0 &&
        fighter.move_id < MOVE_COUNT) {
        const MoveId move = static_cast<MoveId>(fighter.move_id);
        const MoveData& data = move_of(character, move);
        const int32_t frame = fighter.move_frame;

        if (frame <= data.startup) {
            lines.phase = ReadoutLines::Phase::Startup;
        } else if (frame <= data.startup + data.active) {
            lines.phase = ReadoutLines::Phase::Active;
        } else {
            lines.phase = ReadoutLines::Phase::Recovery;
        }

        const char* phase_name = lines.phase == ReadoutLines::Phase::Startup  ? "STARTUP"
                                 : lines.phase == ReadoutLines::Phase::Active ? "ACTIVE"
                                                                              : "RECOVERY";

        lines.detail = std::string(readout_move_name(move)) + "  " + std::to_string(frame) + "/" +
                       std::to_string(move_total_frames(data)) + "  " + phase_name;
        lines.numbers = std::to_string(data.startup) + "-" + std::to_string(data.active) + "-" +
                        std::to_string(data.recovery) + "  DMG " + std::to_string(data.damage);
        return lines;
    }

    // Stun counting down is the single most confusing moment for a newcomer --
    // "why can't I move" has a number attached to it and nobody could see it.
    if (fighter.hitstun_remaining > 0) {
        lines.detail = "HITSTUN " + std::to_string(fighter.hitstun_remaining);
    } else if (fighter.blockstun_remaining > 0) {
        lines.detail = "BLOCKSTUN " + std::to_string(fighter.blockstun_remaining);
    } else if (fighter.guarding != 0) {
        lines.detail = "GUARDING (BACK)";
    }
    return lines;
}

}  // namespace ds::render
