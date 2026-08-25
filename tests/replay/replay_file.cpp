#include "replay/replay_file.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ds::test {
namespace {

constexpr int32_t REPLAY_FORMAT_VERSION = 1;

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parse_u64(const std::string& token, uint64_t& out) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    const int base = token.rfind("0x", 0) == 0 ? 16 : 10;
    const unsigned long long value = std::strtoull(token.c_str(), &end, base);
    if (end == token.c_str() || *end != '\0') {
        return false;
    }
    out = static_cast<uint64_t>(value);
    return true;
}

bool parse_i32(const std::string& token, int32_t& out) {
    uint64_t value = 0;
    if (!parse_u64(token, value) || value > 0x7FFFFFFFull) {
        return false;
    }
    out = static_cast<int32_t>(value);
    return true;
}

}  // namespace

const char* replay_io_status_name(ReplayIoStatus status) {
    switch (status) {
        case ReplayIoStatus::Ok:
            return "ok";
        case ReplayIoStatus::FileMissing:
            return "file missing";
        case ReplayIoStatus::BadVersion:
            return "unsupported format version";
        case ReplayIoStatus::Malformed:
            return "malformed";
        case ReplayIoStatus::WriteFailed:
            return "write failed";
    }
    return "unknown";
}

ReplayIoStatus load_replay(const std::string& path, Replay& out, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "could not open " + path;
        return ReplayIoStatus::FileMissing;
    }

    out = Replay{};
    bool saw_version = false;
    int32_t line_number = 0;
    std::string line;

    while (std::getline(file, line)) {
        ++line_number;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream stream(trimmed);
        std::string keyword;
        stream >> keyword;

        auto fail = [&](const char* what) {
            error =
                path + ":" + std::to_string(line_number) + ": " + what + " -- '" + trimmed + "'";
        };

        if (keyword == "version") {
            int32_t version = 0;
            std::string token;
            stream >> token;
            if (!parse_i32(token, version)) {
                fail("version must be an integer");
                return ReplayIoStatus::Malformed;
            }
            if (version != REPLAY_FORMAT_VERSION) {
                error = path + ": format version " + std::to_string(version) + ", expected " +
                        std::to_string(REPLAY_FORMAT_VERSION);
                return ReplayIoStatus::BadVersion;
            }
            saw_version = true;
        } else if (keyword == "name") {
            std::getline(stream, out.name);
            out.name = trim(out.name);
        } else if (keyword == "description") {
            std::getline(stream, out.description);
            out.description = trim(out.description);
        } else if (keyword == "seed") {
            std::string token;
            stream >> token;
            if (!parse_u64(token, out.seed)) {
                fail("seed must be an integer");
                return ReplayIoStatus::Malformed;
            }
        } else if (keyword == "frames") {
            std::string token;
            stream >> token;
            if (!parse_i32(token, out.frame_count) || out.frame_count <= 0) {
                fail("frames must be a positive integer");
                return ReplayIoStatus::Malformed;
            }
        } else if (keyword == "input") {
            std::string frame_token;
            std::string p1_token;
            std::string p2_token;
            stream >> frame_token >> p1_token >> p2_token;

            InputChange change{};
            uint64_t p1 = 0;
            uint64_t p2 = 0;
            if (!parse_i32(frame_token, change.frame) || !parse_u64(p1_token, p1) ||
                !parse_u64(p2_token, p2) || p1 > 0xFFFFull || p2 > 0xFFFFull) {
                fail("expected: input <frame> <p1_hex> <p2_hex>");
                return ReplayIoStatus::Malformed;
            }
            change.players[0] = ds::sim::InputFrame{static_cast<uint16_t>(p1)};
            change.players[1] = ds::sim::InputFrame{static_cast<uint16_t>(p2)};

            // Changes must be ordered and unique so that input_at_frame can
            // scan forward once. Unordered lines would resolve differently
            // depending on how the reader happened to search.
            if (!out.changes.empty() && change.frame <= out.changes.back().frame) {
                fail("input frames must strictly increase");
                return ReplayIoStatus::Malformed;
            }
            out.changes.push_back(change);
        } else if (keyword == "checkpoint") {
            std::string frame_token;
            std::string hash_token;
            stream >> frame_token >> hash_token;

            Checkpoint checkpoint{};
            if (!parse_i32(frame_token, checkpoint.frame) ||
                !parse_u64(hash_token, checkpoint.hash)) {
                fail("expected: checkpoint <frame> <hash_hex>");
                return ReplayIoStatus::Malformed;
            }
            out.checkpoints.push_back(checkpoint);
        } else if (keyword == "final") {
            std::string token;
            stream >> token;
            if (!parse_u64(token, out.final_visible_hash)) {
                fail("expected: final <hash_hex>");
                return ReplayIoStatus::Malformed;
            }
        } else {
            fail("unknown keyword");
            return ReplayIoStatus::Malformed;
        }
    }

    if (!saw_version) {
        error = path + ": missing 'version' line";
        return ReplayIoStatus::Malformed;
    }
    if (out.frame_count <= 0) {
        error = path + ": missing or non-positive 'frames' line";
        return ReplayIoStatus::Malformed;
    }
    // A recording with no expected hash would pass no matter what the sim did,
    // which is the one outcome worse than failing.
    if (out.final_visible_hash == 0 && out.checkpoints.empty()) {
        error = path + ": no 'final' hash and no checkpoints -- this replay asserts nothing";
        return ReplayIoStatus::Malformed;
    }

    return ReplayIoStatus::Ok;
}

ReplayIoStatus save_replay(const std::string& path, const Replay& replay) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ReplayIoStatus::WriteFailed;
    }

    // '\n' with a binary stream, so the file is byte-identical on Windows and
    // on Linux. A recording that differs only by line ending would show up as
    // a spurious diff every time it was regenerated on the other platform.
    auto write_line = [&file](const std::string& text) { file << text << '\n'; };

    char buffer[128];

    write_line("# Replay recording -- tests/replay/replay_file.h");
    write_line("#");
    write_line("# Generated by ds_replay_record. Do not hand-edit the hashes.");
    write_line("# When a deliberate balance or behavior change invalidates this file,");
    write_line("# re-record it IN THE SAME COMMIT as the change (AGENTS.md rule 8).");
    write_line("# A separate 'fix tests' commit destroys the signal that makes");
    write_line("# replays worth having.");
    write_line("");
    std::snprintf(buffer, sizeof(buffer), "version %d", REPLAY_FORMAT_VERSION);
    write_line(buffer);
    write_line("name " + replay.name);
    write_line("description " + replay.description);
    std::snprintf(buffer, sizeof(buffer), "seed %llu",
                  static_cast<unsigned long long>(replay.seed));
    write_line(buffer);
    std::snprintf(buffer, sizeof(buffer), "frames %d", replay.frame_count);
    write_line(buffer);

    write_line("");
    write_line("# input <frame> <p1_buttons> <p2_buttons> -- holds until the next line");
    for (const InputChange& change : replay.changes) {
        std::snprintf(buffer, sizeof(buffer), "input %d 0x%04X 0x%04X", change.frame,
                      change.players[0].buttons, change.players[1].buttons);
        write_line(buffer);
    }

    write_line("");
    write_line("# Full state hash at intervals, so a divergence reports its first frame.");
    for (const Checkpoint& checkpoint : replay.checkpoints) {
        std::snprintf(buffer, sizeof(buffer), "checkpoint %d 0x%016llX", checkpoint.frame,
                      static_cast<unsigned long long>(checkpoint.hash));
        write_line(buffer);
    }

    write_line("");
    write_line("# Visible-state hash after the final frame. Excludes RNG position");
    write_line("# so that a render-side effect drawing a number cannot fail this.");
    std::snprintf(buffer, sizeof(buffer), "final 0x%016llX",
                  static_cast<unsigned long long>(replay.final_visible_hash));
    write_line(buffer);

    return file.good() ? ReplayIoStatus::Ok : ReplayIoStatus::WriteFailed;
}

ds::sim::InputPair input_at_frame(const Replay& replay, int32_t frame) {
    ds::sim::InputPair result{{ds::sim::InputFrame{0u}, ds::sim::InputFrame{0u}}};

    // Changes are ordered and strictly increasing, enforced at load time.
    for (const InputChange& change : replay.changes) {
        if (change.frame > frame) {
            break;
        }
        result.players[0] = change.players[0];
        result.players[1] = change.players[1];
    }
    return result;
}

}  // namespace ds::test
