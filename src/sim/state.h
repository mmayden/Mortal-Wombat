// The complete match state. ADR 0005.
//
// One flat POD struct, no pointers, no heap, fixed-size arrays. Rollback saves
// and restores this up to 8x per frame; a memcpy of a small trivially-copyable
// struct is effectively free, and a serialization pass is not.
//
// The static_asserts at the bottom of this file are load-bearing. They fail the
// build the moment someone adds a std::string, a pointer, or a virtual
// function — each of which silently breaks save/restore rather than producing
// an error at the point of use.
//
// THE RULE: if it affects the outcome of a match, it is in here and it is
// deterministic. If it does not, it is in the render layer and it never
// touches the sim.
#pragma once

#include <cstdint>
#include <type_traits>

#include "sim/constants.h"
#include "sim/fixed.h"
#include "sim/rng.h"

namespace mw::sim {

// DESIGN.md 4.2. This set is closed — adding a state requires an ADR.
enum class FighterState : int32_t {
    RoundStart = 0,
    Idle,
    WalkForward,
    WalkBackward,
    Crouch,
    JumpStartup,
    Airborne,
    Landing,
    Attack,
    Blocking,
    Hitstun,
    Blockstun,
    Knockdown,
    Wakeup,
    Win,
    Lose,
};

// Fighters always face each other; facing flips when they cross up.
// DESIGN.md 4.3.
enum class Facing : int32_t {
    Right = 1,
    Left = -1,
};

struct Fighter {
    // Position of the ground point, between the feet. Y equals GROUND_Y when
    // standing; smaller values are higher, matching screen space.
    Fixed x;
    Fixed y;
    Fixed velocity_x;
    Fixed velocity_y;

    FighterState state;
    Facing facing;

    int32_t health;

    // How many frames the fighter remains in its current state. Counts down.
    // Zero means the state is either free (Idle) or ends this frame.
    int32_t state_frames_remaining;

    // Frames elapsed within the current move, 1-based, so it lines up with the
    // frame ranges written in data/characters/*.toml.
    int32_t move_frame;

    // Which move is executing. Meaningful only in FighterState::Attack.
    int32_t move_id;

    int32_t hitstun_remaining;
    int32_t blockstun_remaining;

    // Set on the frame a hit lands and cleared the next frame. The render and
    // audio layers observe this rather than being called back into, because a
    // side effect inside the sim fires again on every rollback re-simulation
    // (ARCHITECTURE.md 1).
    int32_t hit_confirm_frame;

    // Whether the current move has already connected. A move's hitbox is live
    // for several frames, so without this a single punch would deal its damage
    // once per active frame -- an 8-damage HP would take 24 health over its
    // three active frames.
    //
    // One hit per move is the current rule. Multi-hit moves are undecided
    // rather than excluded (ADR 0018). Cleared when a new move starts, not
    // when the move ends, because the fighter can be interrupted out of a
    // move at any point.
    int32_t hit_already_landed;
};

struct Projectile {
    Fixed x;
    Fixed y;
    Fixed velocity_x;
    Fixed velocity_y;

    // Index into GameState::fighters, never a pointer. Pointers do not survive
    // a memcpy restore and are not stable across machines.
    int32_t owner_index;

    int32_t damage;
    int32_t frames_remaining;
    int32_t active;  // 0 or 1; int32_t rather than bool to keep the layout
                     // free of padding, which the state hash walks over.
};

// DESIGN.md 2: a round ends on KO or on timer expiry.
enum class RoundPhase : int32_t {
    Starting = 0,
    Fighting,
    Ended,
    MatchEnded,
};

struct GameState {
    Fighter fighters[2];
    Projectile projectiles[MAX_PROJECTILES];

    int32_t frame;
    int32_t round_timer;
    int32_t round_number;
    int32_t rounds_won[2];
    RoundPhase round_phase;

    // Frames until the current round phase transition completes. Keeps round
    // flow in the state rather than in a timer owned by the caller.
    int32_t phase_frames_remaining;

    // Explicit padding, not a spare field. RngState is 8-byte aligned, so
    // without this the compiler inserts four anonymous bytes here — and the
    // state hash walks the struct's bytes, so those bytes would feed
    // uninitialized garbage into the hash. Two machines would then disagree
    // and the desync test would fail on padding rather than on behavior.
    //
    // Must stay zero. If you need a field, take it from here and delete the
    // padding, keeping the int32_t count in this block even.
    int32_t reserved;

    RngState rng;
};

// Rollback saves and restores this with memcpy. These assertions are the only
// thing standing between that and a silent corruption.
static_assert(std::is_trivially_copyable_v<GameState>,
              "GameState must be memcpy-able: rollback save_state/load_state depends on it. "
              "A std::string, a pointer with ownership, or a virtual function breaks this.");
static_assert(std::is_standard_layout_v<GameState>,
              "GameState must have standard layout so the state hash can walk its bytes.");

// Trivially copyable is not enough on its own.
//
// This assertion exists because its absence let a bug through: Fixed had a
// user-provided default constructor, which left GameState trivially COPYABLE
// but not trivially DEFAULT-CONSTRUCTIBLE. memset on such a type is still
// well-defined, but GCC rejects it under -Wclass-memaccess, so the build was
// green on MSVC and broken on Linux -- discovered by CI rather than here.
//
// is_trivial_v covers both halves, which is what "flat POD" in ADR 0005
// actually means. Keeping it makes the property a compile error on every
// compiler instead of a warning on one of them.
static_assert(std::is_trivial_v<GameState>,
              "GameState must be trivial, not merely trivially copyable: a member with a "
              "user-provided default constructor makes memset on it a diagnostic under GCC, "
              "and makes 'flat POD' (ADR 0005) untrue.");
static_assert(std::is_trivial_v<Fighter>, "Fighter must be trivial -- see GameState above");
static_assert(std::is_trivial_v<Projectile>, "Projectile must be trivial -- see GameState above");
static_assert(sizeof(GameState) < 4096,
              "GameState is saved up to 8x per frame under rollback. Growing past 4KB means "
              "the entity model needs a rethink, not a bigger budget.");

// The desync test hashes these structs byte by byte across three platforms.
// Any compiler-inserted padding would be uninitialized, so these assertions
// prove there is none: each struct's size must be exactly the sum of its
// members. If one of these fails after you added a field, add or remove
// explicit padding to restore it — do not raise the number.
static_assert(sizeof(Fighter) == 14 * sizeof(int32_t), "Fighter has implicit padding");
static_assert(sizeof(Projectile) == 8 * sizeof(int32_t), "Projectile has implicit padding");
static_assert(sizeof(RngState) == 2 * sizeof(uint64_t), "RngState has implicit padding");
static_assert(sizeof(GameState) == 2 * sizeof(Fighter) + MAX_PROJECTILES * sizeof(Projectile) +
                                       8 * sizeof(int32_t) + sizeof(RngState),
              "GameState has implicit padding, which would feed uninitialized bytes into the "
              "state hash and produce a false desync across platforms");

// Resets a fighter to its round-start position and state. Called at the start
// of every round, not just the match, so health and position return to their
// opening values while rounds_won does not.
void reset_fighter(Fighter& fighter, int32_t player_index);

// Initializes a match. The seed is the only external input: everything else
// about how the match unfolds comes from the input stream. That pairing —
// seed plus inputs — is exactly what a replay file records.
void init_state(GameState& state, uint64_t seed);

// Resets for the next round, preserving rounds_won and the RNG stream position.
void begin_round(GameState& state);

}  // namespace mw::sim
