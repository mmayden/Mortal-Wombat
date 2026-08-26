// Frame data as the simulation sees it. ADR 0009.
//
// These are the structures `advance_frame` reads to know how long a move takes
// and where its boxes are. They are immutable during a match and live OUTSIDE
// GameState (ARCHITECTURE.md 3): nothing here is rolled back, because nothing
// here changes.
//
// Parsing lives in src/data/framedata_loader.h, above the sim boundary, where
// I/O and std::string are allowed. This header is sim-side, so it is POD only:
// fixed arrays, no pointers, no allocation. The split is what lets the sim read
// frame data without ever touching a file.
//
// The authoring format is docs/framedata_schema.md, which is versioned and
// binding on both the loader and tools/framedata_editor/.
#pragma once

#include <cstdint>

#include "sim/fixed.h"

namespace ds::sim {

// Fifteen moves: six standing normals, six crouching variants, a jump attack,
// one special, and a throw. DESIGN.md 4.1 and ADR 0021.
//
// The count is not asserted anywhere in prose -- it falls out of the button set,
// which is what dissolved the old "twelve or ten moves?" discrepancy rather than
// answering it. Six buttons times two stances, plus the two that are neither.
//
// Order is load-bearing in one place: tests and tools iterate 0..Count and the
// TOML keys map by name, so inserting a move in the middle is a data migration,
// not a rename. It is also the priority order for a simultaneous press
// (sim.cpp), where lighter wins because it is the one a player can react out
// of.
enum class MoveId : int32_t {
    StandLightPunch = 0,
    StandMediumPunch,
    StandHeavyPunch,
    StandLightKick,
    StandMediumKick,
    StandHeavyKick,
    CrouchLightPunch,
    CrouchMediumPunch,
    CrouchHeavyPunch,
    CrouchLightKick,
    CrouchMediumKick,
    CrouchHeavyKick,
    JumpAttack,
    Special,

    // Appended, not inserted. Tools and tests iterate 0..Count and the TOML
    // keys map by name, so putting a move in the middle would renumber every
    // one after it -- a data migration wearing the clothes of a rename.
    Throw,

    Count,
};

inline constexpr int32_t MOVE_COUNT = static_cast<int32_t>(MoveId::Count);

// An axis-aligned box in the fighter's local space. Origin is the ground point
// between the feet, +x is forward relative to facing, -y is up.
//
// The loader mirrors x for a left-facing fighter, so authored data is always
// written facing right (docs/framedata_schema.md).
struct Box {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

constexpr bool box_is_empty(const Box& box) {
    return box.w <= 0 || box.h <= 0;
}

// A hitbox and the frames of the move on which it is live.
//
// Frames are 1-based and inclusive, counted from the move's first frame, which
// is how docs/framedata_schema.md asks authors to write them. Keeping the same
// convention in memory means the number in the TOML is the number in the
// debugger.
struct HitboxSpan {
    int32_t first_frame;
    int32_t last_frame;
    Box box;
};

// Capacity per move. Four is enough while moves are single-hit, which is the
// current behaviour rather than a settled rule -- multi-hit moves are
// undecided (ADR 0018). The loader rejects a move that exceeds this rather
// than silently dropping boxes.
inline constexpr int32_t MAX_HITBOXES_PER_MOVE = 4;

// What a move does to a standing opponent when it connects. ADR 0026.
//
// Hard knockdowns are the reward for landing something harder to land, which is
// what gives "go for the knockdown" a gradient instead of a switch. The
// assignment follows the convention almost every 2D fighter shares: sweeps and
// the special knock down hard, everything else on the ground does not, and any
// clean hit on an AIRBORNE fighter is a soft knockdown regardless of the move
// (handled in sim.cpp, since it is a property of the defender's state rather
// than of the attack).
// How a move must be blocked. ADR 0028.
//
// The three-way split every 2D fighter uses, with the names the genre uses for
// them. "High" is deliberately absent: it is the most confused word in the
// vocabulary -- in most games a high attack is still blockable crouching, so
// the functional categories are these three and calling one of them "high"
// would invite exactly the wrong guess.
enum class AttackHeight : int32_t {
    Mid = 0,   // Blockable standing or crouching. The default, and most moves.
    Low,       // Must be blocked CROUCHING. Sweeps and crouching kicks.
    Overhead,  // Must be blocked STANDING. Jump attacks.
};

enum class KnockdownKind : int32_t {
    None = 0,
    Soft,  // The defender chooses when to rise.
    Hard,  // Fixed timing; the attacker gets the cleaner setup.
};

struct MoveData {
    int32_t startup;
    int32_t active;
    int32_t recovery;

    int32_t damage;
    int32_t hitstun;
    int32_t blockstun;

    // Replaces the character's default hurtbox for this move's duration.
    // Empty (w or h <= 0) means the default applies.
    AttackHeight height;
    KnockdownKind knockdown;

    Box hurtbox_override;

    HitboxSpan hitboxes[MAX_HITBOXES_PER_MOVE];
    int32_t hitbox_count;
};

// Total duration is derived, never stored. Storing it would let it disagree
// with its parts the first time someone edited one and not the other
// (docs/framedata_schema.md).
constexpr int32_t move_total_frames(const MoveData& move) {
    return move.startup + move.active + move.recovery;
}

// The frame range on which the move can connect, 1-based inclusive.
constexpr int32_t move_first_active_frame(const MoveData& move) {
    return move.startup + 1;
}
constexpr int32_t move_last_active_frame(const MoveData& move) {
    return move.startup + move.active;
}

constexpr bool move_is_active_on(const MoveData& move, int32_t move_frame) {
    return move_frame >= move_first_active_frame(move) &&
           move_frame <= move_last_active_frame(move);
}

// Identifiers are fixed char arrays rather than std::string because this struct
// is read by the sim, and ADR 0001 forbids std:: containers below the boundary.
inline constexpr int32_t MAX_NAME_LENGTH = 32;

struct CharacterData {
    char id[MAX_NAME_LENGTH];
    char display_name[MAX_NAME_LENGTH];

    Fixed walk_forward_speed;
    Fixed walk_backward_speed;
    int32_t jump_duration;
    int32_t jump_apex;
    int32_t starting_health;

    Box standing_hurtbox;
    Box crouching_hurtbox;
    Box pushbox;

    MoveData moves[MOVE_COUNT];
};

// Both fighters' data for one match. Immutable for the match's lifetime.
//
// Passed to advance_frame alongside GameState. It is deliberately NOT part of
// GameState: rollback copies GameState up to 8x per frame, and copying data
// that cannot change would be pure waste.
struct MatchData {
    CharacterData characters[2];
};

constexpr const MoveData& move_of(const CharacterData& character, MoveId move) {
    return character.moves[static_cast<int32_t>(move)];
}

// Frame data is read by the sim, so it lives under the same constraints as
// GameState even though it is never rolled back.
static_assert(sizeof(Box) == 4 * sizeof(int32_t), "Box has implicit padding");
static_assert(sizeof(HitboxSpan) == 2 * sizeof(int32_t) + sizeof(Box),
              "HitboxSpan has implicit padding");

}  // namespace ds::sim
