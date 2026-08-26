// What the training readout says.
//
// It exists because a playtester could not tell six attack buttons apart and
// said so. Half the fix was the renderer drawing an identical limb for every
// attack; this is the other half -- putting the move's name and its frame count
// on screen, because startup/active/recovery is the entire vocabulary of the
// genre and none of it was visible.
//
// A readout that prints the WRONG thing is worse than none, because a player
// believes it. That is the same failure as a debug overlay agreeing with itself
// and disagreeing with the simulation, which is how the jump attack survived.
// So these tests check the words against the frame data they claim to describe.

#include <string>

#include <doctest/doctest.h>

#include "render/readout_text.h"

#include "match_data.h"

using namespace ds::sim;
using ds::render::readout_held_buttons;
using ds::render::readout_lines;
using ds::render::ReadoutLines;

namespace {

Fighter fighter_attacking(MoveId move, int32_t move_frame) {
    Fighter fighter{};
    reset_fighter(fighter, 0);
    fighter.state = FighterState::Attack;
    fighter.move_id = static_cast<int32_t>(move);
    fighter.move_frame = move_frame;
    return fighter;
}

InputFrame held(std::initializer_list<Button> buttons) {
    InputFrame frame{0u};
    for (Button button : buttons) {
        frame = input_with(frame, button);
    }
    return frame;
}

bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}

}  // namespace

TEST_CASE("Every move has a name, and no two share one") {
    // A default case would let a new MoveId print as "?" forever without
    // failing anything. The switch is exhaustive; this proves the entries are
    // distinct as well as present.
    std::string seen[MOVE_COUNT];
    for (int32_t m = 0; m < MOVE_COUNT; ++m) {
        const std::string name = ds::render::readout_move_name(static_cast<MoveId>(m));
        INFO("move index " << m);
        CHECK(name != "?");
        CHECK_FALSE(name.empty());
        for (int32_t j = 0; j < m; ++j) {
            CHECK(name != seen[j]);
        }
        seen[m] = name;
    }
}

TEST_CASE("Every fighter state has a name") {
    for (int32_t s = 0; s <= static_cast<int32_t>(FighterState::Lose); ++s) {
        INFO("state index " << s);
        CHECK(std::string(ds::render::readout_state_name(static_cast<FighterState>(s))) != "?");
    }
}

// The reported problem, as an assertion: a player must be able to see WHICH
// attack they threw.
TEST_CASE("The readout names the move being performed") {
    const CharacterData& character = ds::test::shipped_match_data().characters[0];

    const ReadoutLines light =
        readout_lines(character, fighter_attacking(MoveId::StandLightPunch, 1), InputFrame{0u});
    const ReadoutLines heavy =
        readout_lines(character, fighter_attacking(MoveId::StandHeavyKick, 1), InputFrame{0u});

    CHECK(contains(light.detail, "LP"));
    CHECK(contains(heavy.detail, "HK"));
    CHECK(light.detail != heavy.detail);
}

TEST_CASE("The phase tracks the move's real frame data") {
    const CharacterData& character = ds::test::shipped_match_data().characters[0];

    for (int32_t m = 0; m < MOVE_COUNT; ++m) {
        const MoveId move = static_cast<MoveId>(m);
        const MoveData& data = move_of(character, move);
        INFO("move index " << m);

        // The frame before the hitbox goes live is startup; the first live
        // frame is active; the frame after the last is recovery. Reading those
        // off the picture is how a player learns what "minus on block" means.
        const ReadoutLines startup =
            readout_lines(character, fighter_attacking(move, data.startup), InputFrame{0u});
        const ReadoutLines active =
            readout_lines(character, fighter_attacking(move, data.startup + 1), InputFrame{0u});
        const ReadoutLines recovery = readout_lines(
            character, fighter_attacking(move, data.startup + data.active + 1), InputFrame{0u});

        if (data.startup > 0) {
            CHECK(startup.phase == ReadoutLines::Phase::Startup);
            CHECK(contains(startup.detail, "STARTUP"));
        }
        CHECK(active.phase == ReadoutLines::Phase::Active);
        CHECK(contains(active.detail, "ACTIVE"));
        if (data.recovery > 0) {
            CHECK(recovery.phase == ReadoutLines::Phase::Recovery);
            CHECK(contains(recovery.detail, "RECOVERY"));
        }
    }
}

TEST_CASE("The numbers shown are the move's own frame data") {
    const CharacterData& character = ds::test::shipped_match_data().characters[0];
    const MoveData& data = move_of(character, MoveId::StandHeavyKick);

    const ReadoutLines lines =
        readout_lines(character, fighter_attacking(MoveId::StandHeavyKick, 1), InputFrame{0u});

    // Quoted from the data, never recomputed: a readout with its own arithmetic
    // is a second source of truth waiting to disagree with the first.
    CHECK(contains(lines.numbers, std::to_string(data.startup).c_str()));
    CHECK(contains(lines.numbers, std::to_string(data.recovery).c_str()));
    CHECK(contains(lines.numbers, std::to_string(data.damage).c_str()));
}

TEST_CASE("Stun is shown counting down, because that is why you cannot move") {
    const CharacterData& character = ds::test::shipped_match_data().characters[0];

    Fighter hurt{};
    reset_fighter(hurt, 0);
    hurt.state = FighterState::Hitstun;
    hurt.hitstun_remaining = 9;
    CHECK(contains(readout_lines(character, hurt, InputFrame{0u}).detail, "HITSTUN 9"));

    Fighter blocking{};
    reset_fighter(blocking, 0);
    blocking.state = FighterState::Blockstun;
    blocking.blockstun_remaining = 4;
    CHECK(contains(readout_lines(character, blocking, InputFrame{0u}).detail, "BLOCKSTUN 4"));
}

TEST_CASE("Guarding is shown, since there is no block button to look for") {
    const CharacterData& character = ds::test::shipped_match_data().characters[0];

    Fighter guarding{};
    reset_fighter(guarding, 0);
    guarding.state = FighterState::WalkBackward;
    guarding.guarding = 1;

    const ReadoutLines lines = readout_lines(character, guarding, held({Button::Left}));
    CHECK(contains(lines.detail, "GUARDING"));
    CHECK(contains(lines.state, "WALK BACK"));
}

TEST_CASE("Held buttons are listed, and nothing held reads as nothing") {
    CHECK(readout_held_buttons(InputFrame{0u}) == "-");

    const std::string both = readout_held_buttons(held({Button::Left, Button::HeavyKick}));
    CHECK(contains(both, "L"));
    CHECK(contains(both, "HK"));

    // Every button must be reportable. A missing entry here is a control the
    // input display silently cannot see, which is precisely what --input-test
    // exists to rule out.
    constexpr Button ALL[] = {Button::Up,         Button::Down,       Button::Left,
                              Button::Right,      Button::LightPunch, Button::MediumPunch,
                              Button::HeavyPunch, Button::LightKick,  Button::MediumKick,
                              Button::HeavyKick};
    for (Button button : ALL) {
        CHECK(readout_held_buttons(held({button})) != "-");
    }
}
