#include "data/framedata_loader.h"

#include <cstring>
#include <fstream>
#include <optional>
#include <string>

#include <toml++/toml.hpp>

namespace ds::data {
namespace {

using ds::sim::Box;
using ds::sim::CharacterData;
using ds::sim::HitboxSpan;
using ds::sim::MoveData;
using ds::sim::MoveId;

// docs/framedata_schema.md, Version history. Bumping the schema means bumping
// this and updating tools/framedata_editor/ in the same commit (AGENTS.md
// rule 5) -- a schema change that lands in only one consumer produces files the
// other cannot read, with no build error to catch it.
constexpr int64_t SUPPORTED_SCHEMA_VERSION = 4;

// Table key for each MoveId, in enum order. The loader and the editor both
// address moves by these strings, so they are part of the schema contract.
constexpr const char* MOVE_KEYS[ds::sim::MOVE_COUNT] = {
    "light_punch",        "medium_punch",      "heavy_punch",        "light_kick",
    "medium_kick",        "heavy_kick",        "crouch_light_punch", "crouch_medium_punch",
    "crouch_heavy_punch", "crouch_light_kick", "crouch_medium_kick", "crouch_heavy_kick",
    "jump_attack",        "special",
};

// The table above must stay the same length as the enum. Without this, adding a
// MoveId and forgetting a key reads past the end of the array at load time --
// which on a good day is a garbage move name in an error message and on a bad
// one is a crash in a tool nobody was debugging.
static_assert(sizeof(MOVE_KEYS) / sizeof(MOVE_KEYS[0]) == ds::sim::MOVE_COUNT,
              "MOVE_KEYS must have exactly one entry per MoveId, in enum order");

// Looks up a nested key, returning null if any level is missing or is not a
// table.
//
// Plain node pointers rather than toml::node_view throughout this file: GCC's
// -Wconversion rejects constructing a node_view from a table as an ambiguous
// conversion, and the pointer form is clearer about "this may not exist"
// anyway.
const toml::node* find(const toml::table& table, std::initializer_list<const char*> path) {
    const toml::table* current = &table;
    const toml::node* found = nullptr;

    for (const char* key : path) {
        if (current == nullptr) {
            return nullptr;
        }
        found = current->get(key);
        if (found == nullptr) {
            return nullptr;
        }
        current = found->as_table();
    }
    return found;
}

// Reads an integer, refusing a float.
//
// This is the single most important validation in the file. A float in the data
// is a float in the sim, and ADR 0002 makes that a desync -- one that would
// appear only in a real match between two machines. TOML distinguishes 1 from
// 1.0 at the type level, so rejecting is exact rather than heuristic.
bool read_int(const toml::node* node, const char* what, int64_t& out, std::string& error) {
    if (node == nullptr) {
        error = std::string("missing required field: ") + what;
        return false;
    }
    if (node->is_floating_point()) {
        error = std::string(what) +
                " is a float. Every value in this format is an integer -- durations are "
                "frames at 60Hz, speeds are 1/65536ths (docs/framedata_schema.md, Units). "
                "A float here becomes a float in the sim, which ADR 0002 makes a desync.";
        return false;
    }
    const std::optional<int64_t> value = node->value<int64_t>();
    if (!value) {
        error = std::string(what) + " is not an integer";
        return false;
    }
    out = *value;
    return true;
}

bool read_i32(const toml::node* node, const char* what, int32_t& out, std::string& error) {
    int64_t wide = 0;
    if (!read_int(node, what, wide, error)) {
        return false;
    }
    if (wide < INT32_MIN || wide > INT32_MAX) {
        error = std::string(what) + " does not fit in an int32";
        return false;
    }
    out = static_cast<int32_t>(wide);
    return true;
}

bool read_box(const toml::node* node, const char* what, Box& out, std::string& error) {
    const toml::table* table = (node == nullptr) ? nullptr : node->as_table();
    if (table == nullptr) {
        error = std::string(what) + " must be a table with x, y, w, h";
        return false;
    }
    const std::string prefix(what);
    return read_i32(table->get("x"), (prefix + ".x").c_str(), out.x, error) &&
           read_i32(table->get("y"), (prefix + ".y").c_str(), out.y, error) &&
           read_i32(table->get("w"), (prefix + ".w").c_str(), out.w, error) &&
           read_i32(table->get("h"), (prefix + ".h").c_str(), out.h, error);
}

// Rule 7: every box has positive extent. A zero-width hitbox can never connect
// and a zero-width hurtbox can never be hit, so both are authoring mistakes
// that would otherwise present as "this move does nothing" at play time.
bool check_box_extent(const Box& box, const char* what, std::string& error) {
    if (box.w <= 0 || box.h <= 0) {
        error = std::string(what) + " must have w > 0 and h > 0 (rule 7)";
        return false;
    }
    return true;
}

// The filename without directories or extension. Hand-rolled for the same
// reason as above -- std::filesystem is not available to this target.
std::string path_stem(const std::string& path) {
    size_t start = path.find_last_of("/\\");
    start = (start == std::string::npos) ? 0 : start + 1;

    const size_t dot = path.find_last_of('.');
    const size_t end = (dot == std::string::npos || dot < start) ? path.size() : dot;

    return path.substr(start, end - start);
}

bool copy_name(const std::string& value, char (&out)[ds::sim::MAX_NAME_LENGTH], const char* what,
               std::string& error) {
    if (value.size() + 1 > static_cast<size_t>(ds::sim::MAX_NAME_LENGTH)) {
        error = std::string(what) + " is longer than " +
                std::to_string(ds::sim::MAX_NAME_LENGTH - 1) + " characters";
        return false;
    }
    std::memset(out, 0, sizeof(out));
    std::memcpy(out, value.c_str(), value.size());
    return true;
}

bool load_move(const toml::table& table, const char* key, MoveData& out, std::string& error) {
    out = MoveData{};

    const std::string prefix = std::string("moves.") + key;

    if (!read_i32(table.get("startup"), (prefix + ".startup").c_str(), out.startup, error) ||
        !read_i32(table.get("active"), (prefix + ".active").c_str(), out.active, error) ||
        !read_i32(table.get("recovery"), (prefix + ".recovery").c_str(), out.recovery, error) ||
        !read_i32(table.get("damage"), (prefix + ".damage").c_str(), out.damage, error) ||
        !read_i32(table.get("hitstun"), (prefix + ".hitstun").c_str(), out.hitstun, error) ||
        !read_i32(table.get("blockstun"), (prefix + ".blockstun").c_str(), out.blockstun, error)) {
        return false;
    }

    // Rule 3, in its specific form for durations.
    if (out.startup < 1 || out.active < 1 || out.recovery < 0) {
        error = prefix + ": startup and active must be >= 1 and recovery >= 0 (rule 3)";
        return false;
    }
    if (out.damage < 0 || out.hitstun < 0 || out.blockstun < 0) {
        error = prefix + ": damage, hitstun and blockstun must be >= 0 (rule 3)";
        return false;
    }

    // Attack height. Optional and defaulting to mid, because most moves are
    // mid and spelling it on all fourteen would be noise.
    out.height = ds::sim::AttackHeight::Mid;
    if (const toml::node* node = table.get("height")) {
        const std::optional<std::string> word = node->value<std::string>();
        if (!word.has_value()) {
            error = prefix + ".height: must be a string -- mid, low or overhead";
            return false;
        }
        if (*word == "mid") {
            out.height = ds::sim::AttackHeight::Mid;
        } else if (*word == "low") {
            out.height = ds::sim::AttackHeight::Low;
        } else if (*word == "overhead") {
            out.height = ds::sim::AttackHeight::Overhead;
        } else {
            error =
                prefix + ".height: unknown value '" + *word + "' -- must be mid, low or overhead";
            return false;
        }
    }

    // Knockdown. Optional and defaulting to none, because most moves do not
    // knock down and requiring the line on all fourteen would be noise that
    // people stop reading.
    //
    // Spelled rather than numbered: "hard" says what it means at the point of
    // use, where a 2 would send the reader to a header to find out. The words
    // are the schema (docs/framedata_schema.md).
    out.knockdown = ds::sim::KnockdownKind::None;
    if (const toml::node* node = table.get("knockdown")) {
        const std::optional<std::string> word = node->value<std::string>();
        if (!word.has_value()) {
            error = prefix + ".knockdown: must be a string -- none, soft or hard";
            return false;
        }
        if (*word == "none") {
            out.knockdown = ds::sim::KnockdownKind::None;
        } else if (*word == "soft") {
            out.knockdown = ds::sim::KnockdownKind::Soft;
        } else if (*word == "hard") {
            out.knockdown = ds::sim::KnockdownKind::Hard;
        } else {
            error =
                prefix + ".knockdown: unknown value '" + *word + "' -- must be none, soft or hard";
            return false;
        }
    }

    // Rule 8. Cancels are UNDECIDED, not cut -- the old cut list that excluded
    // them is void (ADR 0018). The field exists in the format so it is ready
    // when a combo system is designed, and is rejected until then so nobody
    // starts depending on behaviour that has not been agreed.
    const toml::node* cancel_node = table.get("cancel_into");
    if (const toml::array* cancels = (cancel_node == nullptr) ? nullptr : cancel_node->as_array()) {
        if (!cancels->empty()) {
            error = prefix +
                    ": cancel_into must be empty -- the combo system is undecided, and this "
                    "field is reserved until it is (rule 8)";
            return false;
        }
    }

    if (table.contains("hurtbox_override")) {
        if (!read_box(table.get("hurtbox_override"), (prefix + ".hurtbox_override").c_str(),
                      out.hurtbox_override, error) ||
            !check_box_extent(out.hurtbox_override, (prefix + ".hurtbox_override").c_str(),
                              error)) {
            return false;
        }
    }

    const toml::node* hitboxes_node = table.get("hitboxes");
    const toml::array* hitboxes = (hitboxes_node == nullptr) ? nullptr : hitboxes_node->as_array();
    if (hitboxes == nullptr || hitboxes->empty()) {
        error = prefix + ": needs at least one hitbox (rule 5)";
        return false;
    }
    if (hitboxes->size() > static_cast<size_t>(ds::sim::MAX_HITBOXES_PER_MOVE)) {
        error =
            prefix + ": more than " + std::to_string(ds::sim::MAX_HITBOXES_PER_MOVE) + " hitboxes";
        return false;
    }

    int32_t index = 0;
    for (const toml::node& element : *hitboxes) {
        const toml::table* hitbox = element.as_table();
        if (hitbox == nullptr) {
            error = prefix + ".hitboxes[" + std::to_string(index) + "] is not a table";
            return false;
        }

        const std::string where = prefix + ".hitboxes[" + std::to_string(index) + "]";

        HitboxSpan span{};
        const toml::node* frames_node = hitbox->get("frames");
        const toml::array* frames = (frames_node == nullptr) ? nullptr : frames_node->as_array();
        if (frames == nullptr || frames->size() != 2) {
            error = where + ".frames must be a two-element array [first, last]";
            return false;
        }
        if (!read_i32(frames->get(0), (where + ".frames[0]").c_str(), span.first_frame, error) ||
            !read_i32(frames->get(1), (where + ".frames[1]").c_str(), span.last_frame, error)) {
            return false;
        }
        if (!read_i32(hitbox->get("x"), (where + ".x").c_str(), span.box.x, error) ||
            !read_i32(hitbox->get("y"), (where + ".y").c_str(), span.box.y, error) ||
            !read_i32(hitbox->get("w"), (where + ".w").c_str(), span.box.w, error) ||
            !read_i32(hitbox->get("h"), (where + ".h").c_str(), span.box.h, error)) {
            return false;
        }
        if (!check_box_extent(span.box, where.c_str(), error)) {
            return false;
        }

        // Rule 6. A hitbox outside the active window is live on a frame the
        // move is not attacking -- which reads at play time as a move that hits
        // during its own recovery, and is nearly impossible to diagnose from
        // the game alone.
        if (span.first_frame > span.last_frame) {
            error = where + ".frames is inverted (first > last)";
            return false;
        }
        if (span.first_frame < ds::sim::move_first_active_frame(out) ||
            span.last_frame > ds::sim::move_last_active_frame(out)) {
            error = where + ".frames [" + std::to_string(span.first_frame) + ", " +
                    std::to_string(span.last_frame) + "] falls outside the active window [" +
                    std::to_string(ds::sim::move_first_active_frame(out)) + ", " +
                    std::to_string(ds::sim::move_last_active_frame(out)) + "] implied by startup " +
                    std::to_string(out.startup) + " and active " + std::to_string(out.active) +
                    " (rule 6)";
            return false;
        }

        out.hitboxes[index] = span;
        ++index;
    }
    out.hitbox_count = index;
    return true;
}

}  // namespace

const char* load_result_name(LoadResult result) {
    switch (result) {
        case LoadResult::Ok:
            return "ok";
        case LoadResult::FileMissing:
            return "file missing";
        case LoadResult::ParseError:
            return "TOML parse error";
        case LoadResult::BadSchemaVersion:
            return "unsupported schema version";
        case LoadResult::ValidationFailed:
            return "validation failed";
    }
    return "unknown";
}

const char* move_key(MoveId move) {
    const int32_t index = static_cast<int32_t>(move);
    if (index < 0 || index >= ds::sim::MOVE_COUNT) {
        return "<invalid>";
    }
    return MOVE_KEYS[index];
}

MoveId move_id_from_key(const std::string& key) {
    for (int32_t i = 0; i < ds::sim::MOVE_COUNT; ++i) {
        if (key == MOVE_KEYS[i]) {
            return static_cast<MoveId>(i);
        }
    }
    return MoveId::Count;
}

LoadResult load_character(const std::string& path, CharacterData& out, std::string& error) {
    error.clear();

    // Deliberately not std::filesystem. On MSVC it pulls in <chrono>, which
    // uses exception handling, and this library links into the game binary --
    // which ADR 0001 builds with exceptions disabled. Opening the file is also
    // a more honest existence check than stat: it answers "can I read this",
    // which is the question actually being asked.
    {
        const std::ifstream probe(path, std::ios::binary);
        if (!probe.is_open()) {
            error = "cannot open: " + path;
            return LoadResult::FileMissing;
        }
    }

    // TOML_EXCEPTIONS=0, so the result carries the error rather than throwing.
    const toml::parse_result parsed = toml::parse_file(path);
    if (!parsed) {
        const toml::parse_error& failure = parsed.error();
        error = path + ":" + std::to_string(failure.source().begin.line) + ":" +
                std::to_string(failure.source().begin.column) + ": " +
                std::string(failure.description());
        return LoadResult::ParseError;
    }
    const toml::table& root = parsed.table();

    // Rule 1.
    int64_t schema_version = 0;
    if (!read_int(root.get("schema_version"), "schema_version", schema_version, error)) {
        return LoadResult::ValidationFailed;
    }
    if (schema_version != SUPPORTED_SCHEMA_VERSION) {
        error = path + ": schema_version " + std::to_string(schema_version) +
                ", this build "
                "supports " +
                std::to_string(SUPPORTED_SCHEMA_VERSION);
        return LoadResult::BadSchemaVersion;
    }

    CharacterData character{};

    const toml::node* id_node = find(root, {"character", "id"});
    const std::optional<std::string> id =
        (id_node == nullptr) ? std::nullopt : id_node->value<std::string>();
    if (!id) {
        error = path + ": character.id is missing";
        return LoadResult::ValidationFailed;
    }

    // Rule 2. Filename and id disagreeing means one of them is a typo, and the
    // sim addresses characters by id while a human addresses them by filename.
    const std::string stem = path_stem(path);
    if (*id != stem) {
        error = path + ": character.id '" + *id + "' does not match the filename stem '" + stem +
                "' (rule 2)";
        return LoadResult::ValidationFailed;
    }
    if (!copy_name(*id, character.id, "character.id", error)) {
        return LoadResult::ValidationFailed;
    }

    const toml::node* display_node = find(root, {"character", "display_name"});
    const std::optional<std::string> display =
        (display_node == nullptr) ? std::nullopt : display_node->value<std::string>();
    if (!copy_name(display.value_or(*id), character.display_name, "character.display_name",
                   error)) {
        return LoadResult::ValidationFailed;
    }

    int32_t walk_forward = 0;
    int32_t walk_backward = 0;
    if (!read_i32(find(root, {"character", "physics", "walk_forward_speed"}),
                  "character.physics.walk_forward_speed", walk_forward, error) ||
        !read_i32(find(root, {"character", "physics", "walk_backward_speed"}),
                  "character.physics.walk_backward_speed", walk_backward, error) ||
        !read_i32(find(root, {"character", "physics", "jump_duration"}),
                  "character.physics.jump_duration", character.jump_duration, error) ||
        !read_i32(find(root, {"character", "physics", "jump_apex"}), "character.physics.jump_apex",
                  character.jump_apex, error) ||
        !read_i32(find(root, {"character", "physics", "starting_health"}),
                  "character.physics.starting_health", character.starting_health, error)) {
        return LoadResult::ValidationFailed;
    }

    // Speeds are authored as raw i32.16, which is why the schema documents them
    // in 1/65536ths rather than as decimals.
    character.walk_forward_speed = ds::sim::Fixed(walk_forward);
    character.walk_backward_speed = ds::sim::Fixed(walk_backward);

    if (character.jump_duration < 1 || character.jump_apex < 1 || character.starting_health < 1) {
        error = path + ": jump_duration, jump_apex and starting_health must be positive (rule 3)";
        return LoadResult::ValidationFailed;
    }

    if (!read_box(find(root, {"character", "boxes", "standing_hurtbox"}),
                  "character.boxes.standing_hurtbox", character.standing_hurtbox, error) ||
        !read_box(find(root, {"character", "boxes", "crouching_hurtbox"}),
                  "character.boxes.crouching_hurtbox", character.crouching_hurtbox, error) ||
        !read_box(find(root, {"character", "boxes", "pushbox"}), "character.boxes.pushbox",
                  character.pushbox, error)) {
        return LoadResult::ValidationFailed;
    }
    if (!check_box_extent(character.standing_hurtbox, "character.boxes.standing_hurtbox", error) ||
        !check_box_extent(character.crouching_hurtbox, "character.boxes.crouching_hurtbox",
                          error) ||
        !check_box_extent(character.pushbox, "character.boxes.pushbox", error)) {
        return LoadResult::ValidationFailed;
    }

    const toml::node* moves_node = root.get("moves");
    const toml::table* moves = (moves_node == nullptr) ? nullptr : moves_node->as_table();
    if (moves == nullptr) {
        error = path + ": no [moves] table";
        return LoadResult::ValidationFailed;
    }

    // Rule 4, in both directions. An unknown key is a typo that would otherwise
    // leave a move silently absent, and a missing key is a move the sim can
    // reference but the data does not define.
    bool seen[ds::sim::MOVE_COUNT] = {};
    for (const auto& [key, value] : *moves) {
        const std::string key_string(key.str());
        const MoveId id_for_key = move_id_from_key(key_string);
        if (id_for_key == MoveId::Count) {
            error = path + ": unknown move '" + key_string +
                    "'. It must match a MoveId in src/sim/framedata.h (rule 4)";
            return LoadResult::ValidationFailed;
        }
        const toml::table* move_table = value.as_table();
        if (move_table == nullptr) {
            error = path + ": moves." + key_string + " is not a table";
            return LoadResult::ValidationFailed;
        }
        if (!load_move(*move_table, key_string.c_str(),
                       character.moves[static_cast<int32_t>(id_for_key)], error)) {
            error = path + ": " + error;
            return LoadResult::ValidationFailed;
        }
        seen[static_cast<int32_t>(id_for_key)] = true;
    }

    for (int32_t i = 0; i < ds::sim::MOVE_COUNT; ++i) {
        if (!seen[i]) {
            error = path + ": missing move '" + MOVE_KEYS[i] + "' (rule 4)";
            return LoadResult::ValidationFailed;
        }
    }

    out = character;
    return LoadResult::Ok;
}

LoadResult load_match(const std::string& player_one_path, const std::string& player_two_path,
                      ds::sim::MatchData& out, std::string& error) {
    ds::sim::MatchData match{};

    const LoadResult first = load_character(player_one_path, match.characters[0], error);
    if (first != LoadResult::Ok) {
        return first;
    }
    const LoadResult second = load_character(player_two_path, match.characters[1], error);
    if (second != LoadResult::Ok) {
        return second;
    }

    out = match;
    return LoadResult::Ok;
}

}  // namespace ds::data
