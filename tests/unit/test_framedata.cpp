// Frame data loading and validation. ADR 0009.
//
// Two jobs here, and the second matters more than it looks.
//
// The first is that the loader rejects bad data. Every rule in
// docs/framedata_schema.md is a mistake somebody will make while authoring
// content, and each one that gets through presents at play time as a move that
// mysteriously does nothing.
//
// The second is that the SHIPPED character files still say what DESIGN.md 4.5
// says. Frame data is the entire feel of a fighting game, so a silent edit to a
// startup value is a design change disguised as a data commit. Pinning the
// numbers to the design doc means changing them requires changing both, which
// is exactly the friction that should exist.
#include <cstdio>
#include <fstream>
#include <string>

#include <doctest/doctest.h>

#include "sim/framedata.h"

#include "data/framedata_loader.h"

using namespace mw::data;
using namespace mw::sim;

namespace {

std::string character_path(const char* id) {
    return std::string(MW_DATA_DIR) + "/characters/" + id + ".toml";
}

CharacterData load_or_fail(const char* id) {
    CharacterData character{};
    std::string error;
    const LoadResult result = load_character(character_path(id), character, error);
    REQUIRE_MESSAGE(result == LoadResult::Ok, load_result_name(result), ": ", error);
    return character;
}

// Writes a temporary TOML file, loads it, and returns the outcome. Used to
// prove each validation rule actually rejects what it claims to.
LoadResult load_text(const std::string& body, std::string& error) {
    const std::string path = std::string(MW_DATA_DIR) + "/characters/__probe.toml";
    {
        std::ofstream file(path, std::ios::binary);
        REQUIRE(file.is_open());
        file << body;
    }

    CharacterData character{};
    const LoadResult result = load_character(path, character, error);
    std::remove(path.c_str());
    return result;
}

// A minimal file that passes every rule. Each rule test corrupts exactly one
// thing in it, so a failure names the rule rather than the file.
std::string valid_probe(const std::string& moves_override = "") {
    const std::string moves = moves_override.empty() ? R"(
[moves.low_punch]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
[[moves.low_punch.hitboxes]]
frames = [5, 6]
x = 16
y = -110
w = 30
h = 20
)"
                                                     : moves_override;

    return std::string(R"(
schema_version = 1
[character]
id = "__probe"
display_name = "Probe"
[character.physics]
walk_forward_speed = 78643
walk_backward_speed = 65536
jump_duration = 44
jump_apex = 90
starting_health = 100
[character.boxes]
standing_hurtbox  = { x = -16, y = -140, w = 32, h = 140 }
crouching_hurtbox = { x = -18, y = -80, w = 36, h = 80 }
pushbox           = { x = -14, y = -130, w = 28, h = 130 }
)") + moves;
}

// The probe file defines only low_punch, so it always trips the missing-move
// rule. Tests that target a different rule assert on the message instead of the
// status, which is why the loader's errors name their rule.
bool mentions(const std::string& error, const char* fragment) {
    return error.find(fragment) != std::string::npos;
}

}  // namespace

TEST_CASE("Both shipped characters load") {
    for (const char* id : {"frenchy", "wisdom"}) {
        CAPTURE(id);
        const CharacterData character = load_or_fail(id);
        CHECK(std::string(character.id) == id);
    }
}

TEST_CASE("Shipped frame data matches DESIGN.md 4.5") {
    // If this fails, either the data drifted or the design changed. Either way
    // the two must be reconciled in one commit -- and if it is a real balance
    // change, the affected replays get re-recorded in that same commit
    // (AGENTS.md rule 8).
    struct Expected {
        MoveId move;
        int32_t startup;
        int32_t active;
        int32_t recovery;
        int32_t damage;
        int32_t hitstun;
        int32_t blockstun;
    };

    // DESIGN.md 4.5, transcribed. Crouching variants "inherit" their timings,
    // so they repeat the standing numbers.
    const Expected table[] = {
        {MoveId::StandLowPunch, 4, 2, 8, 3, 12, 8},  {MoveId::StandHighPunch, 7, 3, 16, 8, 18, 12},
        {MoveId::StandLowKick, 5, 3, 10, 4, 13, 9},  {MoveId::StandHighKick, 9, 4, 20, 9, 20, 14},
        {MoveId::CrouchLowPunch, 4, 2, 8, 3, 12, 8}, {MoveId::CrouchHighPunch, 7, 3, 16, 8, 18, 12},
        {MoveId::CrouchLowKick, 5, 3, 10, 4, 13, 9}, {MoveId::CrouchHighKick, 9, 4, 20, 9, 20, 14},
        {MoveId::Special, 12, 4, 24, 6, 18, 12},
    };

    for (const char* id : {"frenchy", "wisdom"}) {
        const CharacterData character = load_or_fail(id);
        for (const Expected& expected : table) {
            CAPTURE(id);
            CAPTURE(move_key(expected.move));
            const MoveData& move = move_of(character, expected.move);
            CHECK(move.startup == expected.startup);
            CHECK(move.active == expected.active);
            CHECK(move.recovery == expected.recovery);
            CHECK(move.damage == expected.damage);
            CHECK(move.hitstun == expected.hitstun);
            CHECK(move.blockstun == expected.blockstun);
        }

        SUBCASE("the jump attack keeps its specified startup, damage and stun") {
            // Its "active" is not pinned: DESIGN.md 4.5 gives it as "until
            // landing", which the schema cannot express as an integer. The sim
            // will end the active window on ground contact.
            const MoveData& jump = move_of(character, MoveId::JumpAttack);
            CHECK(jump.startup == 6);
            CHECK(jump.recovery == 4);
            CHECK(jump.damage == 7);
            CHECK(jump.hitstun == 16);
            CHECK(jump.blockstun == 11);
        }
    }
}

TEST_CASE("Shipped physics match DESIGN.md 4.4") {
    for (const char* id : {"frenchy", "wisdom"}) {
        CAPTURE(id);
        const CharacterData character = load_or_fail(id);

        CHECK(character.walk_forward_speed == Fixed::from_ratio(12, 10));
        CHECK(character.walk_backward_speed == Fixed::from_ratio(10, 10));
        CHECK(character.jump_duration == 44);
        CHECK(character.jump_apex == 90);
        CHECK(character.starting_health == 100);
    }
}

TEST_CASE("Frenchy and Wisdom are mechanically identical in v1") {
    // DESIGN.md 4.5 gives one frame-data table "per character" and 4.6 cuts
    // per-character variation, so any difference here is drift rather than
    // design -- most likely someone editing one file and not the other before
    // the frame-data editor exists.
    const CharacterData a = load_or_fail("frenchy");
    const CharacterData b = load_or_fail("wisdom");

    CHECK(a.walk_forward_speed == b.walk_forward_speed);
    CHECK(a.jump_duration == b.jump_duration);
    CHECK(a.starting_health == b.starting_health);

    for (int32_t i = 0; i < MOVE_COUNT; ++i) {
        CAPTURE(move_key(static_cast<MoveId>(i)));
        const MoveData& left = a.moves[i];
        const MoveData& right = b.moves[i];
        CHECK(left.startup == right.startup);
        CHECK(left.active == right.active);
        CHECK(left.recovery == right.recovery);
        CHECK(left.damage == right.damage);
        CHECK(left.hitstun == right.hitstun);
        CHECK(left.blockstun == right.blockstun);
        CHECK(left.hitbox_count == right.hitbox_count);
    }
}

TEST_CASE("Every move has a hitbox inside its active window") {
    // Rule 6, checked against the shipped files rather than a probe. A hitbox
    // live outside the active window reads at play time as a move that hits
    // during its own recovery -- almost undiagnosable from the game alone.
    for (const char* id : {"frenchy", "wisdom"}) {
        const CharacterData character = load_or_fail(id);
        for (int32_t i = 0; i < MOVE_COUNT; ++i) {
            CAPTURE(id);
            CAPTURE(move_key(static_cast<MoveId>(i)));
            const MoveData& move = character.moves[i];

            REQUIRE(move.hitbox_count >= 1);
            for (int32_t h = 0; h < move.hitbox_count; ++h) {
                const HitboxSpan& span = move.hitboxes[h];
                CHECK(span.first_frame >= move_first_active_frame(move));
                CHECK(span.last_frame <= move_last_active_frame(move));
                CHECK(span.first_frame <= span.last_frame);
                CHECK(span.box.w > 0);
                CHECK(span.box.h > 0);
            }
        }
    }
}

TEST_CASE("move_is_active_on agrees with startup and active") {
    MoveData move{};
    move.startup = 7;
    move.active = 3;
    move.recovery = 16;

    CHECK(move_total_frames(move) == 26);
    CHECK(move_first_active_frame(move) == 8);
    CHECK(move_last_active_frame(move) == 10);

    CHECK_FALSE(move_is_active_on(move, 7));   // last startup frame
    CHECK(move_is_active_on(move, 8));         // first active
    CHECK(move_is_active_on(move, 10));        // last active
    CHECK_FALSE(move_is_active_on(move, 11));  // first recovery frame
}

TEST_CASE("A missing file is reported, not ignored") {
    CharacterData character{};
    std::string error;
    const LoadResult result =
        load_character(character_path("__no_such_character"), character, error);

    CHECK(result == LoadResult::FileMissing);
    CHECK_FALSE(error.empty());
}

TEST_CASE("Validation rejects a float") {
    // The single most important rule in the loader. TOML distinguishes 1 from
    // 1.0 at the type level, so this is exact rather than heuristic -- and a
    // float that reached the sim would be a desync visible only in a real match
    // between two machines (ADR 0002).
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.low_punch]
startup = 4.0
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
[[moves.low_punch.hitboxes]]
frames = [5, 6]
x = 16
y = -110
w = 30
h = 20
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "float"));
    CHECK(mentions(error, "desync"));
}

TEST_CASE("Validation rejects a wrong schema version") {
    std::string error;
    std::string body = valid_probe();
    body.replace(body.find("schema_version = 1"), 18, "schema_version = 99");

    const LoadResult result = load_text(body, error);
    CHECK(result == LoadResult::BadSchemaVersion);
    CHECK(mentions(error, "99"));
}

TEST_CASE("Validation rejects an id that does not match the filename") {
    std::string error;
    std::string body = valid_probe();
    body.replace(body.find("id = \"__probe\""), 14, "id = \"someone_else\"");

    const LoadResult result = load_text(body, error);
    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "rule 2"));
}

TEST_CASE("Validation rejects an unknown move name") {
    // A typo would otherwise leave the character silently missing a move.
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.uppercut]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
[[moves.uppercut.hitboxes]]
frames = [5, 6]
x = 16
y = -110
w = 30
h = 20
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "unknown move"));
    CHECK(mentions(error, "rule 4"));
}

TEST_CASE("Validation rejects a move with no hitbox") {
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.low_punch]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "rule 5"));
}

TEST_CASE("Validation rejects a hitbox outside the active window") {
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.low_punch]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
[[moves.low_punch.hitboxes]]
frames = [9, 11]
x = 16
y = -110
w = 30
h = 20
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "rule 6"));
    CHECK(mentions(error, "active window"));
}

TEST_CASE("Validation rejects a zero-extent box") {
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.low_punch]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
[[moves.low_punch.hitboxes]]
frames = [5, 6]
x = 16
y = -110
w = 0
h = 20
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "rule 7"));
}

TEST_CASE("Validation rejects a non-empty cancel_into") {
    // DESIGN.md 4.6 cuts cancels from v1. The field exists in the format ready
    // for post-v1; this stops anyone quietly starting to use it early.
    std::string error;
    const LoadResult result = load_text(valid_probe(R"(
[moves.low_punch]
startup = 4
active = 2
recovery = 8
damage = 3
hitstun = 12
blockstun = 8
cancel_into = ["special"]
[[moves.low_punch.hitboxes]]
frames = [5, 6]
x = 16
y = -110
w = 30
h = 20
)"),
                                        error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "rule 8"));
}

TEST_CASE("Validation reports a missing move") {
    // The probe defines only low_punch, so a file that is otherwise perfectly
    // valid still fails -- which is the intended behaviour. A character missing
    // a move the sim can reference is not a partial character, it is a broken
    // one.
    std::string error;
    const LoadResult result = load_text(valid_probe(), error);

    CHECK(result == LoadResult::ValidationFailed);
    CHECK(mentions(error, "missing move"));
    CHECK(mentions(error, "rule 4"));
}

TEST_CASE("Move keys round-trip") {
    for (int32_t i = 0; i < MOVE_COUNT; ++i) {
        const MoveId move = static_cast<MoveId>(i);
        CHECK(move_id_from_key(move_key(move)) == move);
    }
    CHECK(move_id_from_key("not_a_move") == MoveId::Count);
}

TEST_CASE("load_match refuses a half-loaded match") {
    MatchData match{};
    std::string error;

    const LoadResult result =
        load_match(character_path("frenchy"), character_path("__no_such_character"), match, error);

    CHECK(result == LoadResult::FileMissing);
}

TEST_CASE("load_match loads the shipped pair") {
    MatchData match{};
    std::string error;
    const LoadResult result =
        load_match(character_path("frenchy"), character_path("wisdom"), match, error);

    REQUIRE_MESSAGE(result == LoadResult::Ok, error);
    CHECK(std::string(match.characters[0].id) == "frenchy");
    CHECK(std::string(match.characters[1].id) == "wisdom");
}
