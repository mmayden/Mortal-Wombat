// Parses character TOML into the structures the sim reads. ADR 0009.
//
// This lives above the sim boundary on purpose. It does file I/O, it allocates,
// it uses std::string, and it can fail — all of which are forbidden below the
// line (ARCHITECTURE.md 1). The sim receives the finished CharacterData and
// never learns a file was involved.
//
// That split is what CONVENTIONS.md 3 means by "the sim never fails": every way
// this can go wrong is discovered here, at load time, where there is somewhere
// to report it. By the time advance_frame runs, the data is known good.
#pragma once

#include <string>

#include "sim/framedata.h"

namespace mw::data {

// No exceptions (ADR 0001), so failure is a return value. Callers must handle
// every case — CONVENTIONS.md 3 forbids a `default:` that swallows.
enum class LoadResult {
    Ok,
    FileMissing,
    ParseError,
    BadSchemaVersion,
    ValidationFailed,
};

const char* load_result_name(LoadResult result);

// Loads one character file.
//
// On failure `error` describes what was wrong and which rule it broke, and
// `out` is left untouched. Every rule enforced here is listed in
// docs/framedata_schema.md under Validation; the two must agree, because the
// schema doc is the contract tools/framedata_editor/ is written against.
LoadResult load_character(const std::string& path, mw::sim::CharacterData& out, std::string& error);

// Loads both characters of a match.
//
// Fails on the first bad file rather than loading what it can: a match with one
// valid fighter is not a degraded match, it is a broken one.
LoadResult load_match(const std::string& player_one_path, const std::string& player_two_path,
                      mw::sim::MatchData& out, std::string& error);

// Maps a TOML table key to its MoveId. Returns MoveId::Count for an unknown
// key, which the loader treats as a validation failure — a typo in a move name
// must not silently produce a character missing a move.
mw::sim::MoveId move_id_from_key(const std::string& key);

// The reverse, for error messages and for the frame-data editor.
const char* move_key(mw::sim::MoveId move);

}  // namespace mw::data
