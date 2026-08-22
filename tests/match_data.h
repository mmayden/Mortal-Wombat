// The shipped character data, for every test tier that needs to advance the
// simulation.
//
// Deliberately the real data/characters/*.toml rather than a hand-built fixture
// with round numbers. Frame data IS the behaviour of a fighting game: a test
// against invented timings proves the code works on data that will never ship,
// and would stay green through a change that broke every actual match.
//
// It also means the replay recordings encode the real frame data, so editing a
// character file fails the replay tier — which is exactly the signal wanted
// (AGENTS.md rule 8).
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#include "sim/framedata.h"

#include "data/framedata_loader.h"

namespace mw::test {

// Loaded once and reused. Loading is pure with respect to the sim: the result
// is immutable for the process, so sharing it cannot let one test perturb
// another.
inline const mw::sim::MatchData& shipped_match_data() {
    static const mw::sim::MatchData data = [] {
        const std::string dir = std::string(MW_DATA_DIR) + "/characters/";
        mw::sim::MatchData loaded{};
        std::string error;
        const mw::data::LoadResult result =
            mw::data::load_match(dir + "frenchy.toml", dir + "wisdom.toml", loaded, error);

        if (result != mw::data::LoadResult::Ok) {
            // Aborting rather than returning empty data. A test tier that
            // silently ran against zeroed frame data would pass a great many
            // assertions while proving nothing at all.
            std::fprintf(stderr, "FATAL: could not load shipped character data (%s): %s\n",
                         mw::data::load_result_name(result), error.c_str());
            std::abort();
        }
        return loaded;
    }();
    return data;
}

}  // namespace mw::test
