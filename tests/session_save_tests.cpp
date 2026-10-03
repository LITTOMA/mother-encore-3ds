#include "encore/session_save.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>
#if defined(__unix__)
#include <csignal>
#include <sys/resource.h>
#endif

using namespace encore::upstream;
namespace {
int failures = 0, checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::cerr << __LINE__ << ": " #x "\n"; } } while (false)
const SessionSaveCompatibility compatibility{11, 22, 33};
// Synthetic values test serialization, not game defaults or an upstream trace.
SessionSnapshot sample() {
    SessionSnapshot s;
    s.scene_id = "scene/test"; s.scene_label = "Synthetic scene";
    s.position_x = -17.375; s.position_y = 681.25; s.direction_x = -1; s.direction_y = 1;
    s.run_sound = "step_test"; s.shadow_effect = "shadow_test"; s.playtime_seconds = 3661.75;
    s.settings = {0.028, "flavor_test", "prompts_test", true};
    s.player_name = "\xe6\xb5\x8b\xe8\xaf\x95"; s.favorite_food = "\xf0\x9f\x8d\x9c";
    s.cash = 7; s.bank = 219; s.earned_cash = 11;
    SessionCharacter a;
    a.character_id = "hero_a"; a.nickname = "Nickname marker";
    a.level = 3; a.experience = 100; a.hp = 39; a.pp = 7;
    a.permanent_boosts = {{"stat_a", 4}, {"stat_b", -2}};
    a.affinity_multipliers = {{"affinity_a", 0.25}, {"affinity_b", -1.5}};
    a.learned_skills = {"skill_a", "skill_b"}; a.status = {{"status_a", 8}};
    a.inventory = {{"item_a", true, 3, 0}, {"item_a", false, 1, UINT32_MAX}};
    auto b = a; b.character_id = "hero_b"; b.inventory.clear();
    s.characters = {a, b}; s.party = {"hero_b", "hero_a"};
    s.key_items = {{"key_item", false, 1, 9}}; s.storage = {{"stored_item", false, 5, 10}};
    s.flags = {{"flag_one", false}, {"flag_two", true}};
    s.object_flags = {{"object/one", true}}; s.seen_dialogue_flags = {{"dialogue/one", true}};
    s.encountered = {{"enemy_a", true}}; s.keys = {{"region_a", 3}}; s.rare_drops = {{"enemy_a", 2}};
    s.source_version = "source-test"; s.saved_at = "2026-10-02T00:00:00Z"; s.is_debug = true;
    return s;
}
std::vector<uint8_t> encode(const SessionSnapshot& snapshot) {
    std::vector<uint8_t> bytes; std::string error;
    CHECK(encode_session_save(snapshot, compatibility, bytes, error)); CHECK(error.empty()); return bytes;
}
uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
}
void fix_crc(std::vector<uint8_t>& bytes) { put32(bytes, bytes.size() - 4, crc32(bytes.data(), bytes.size() - 4)); }
size_t locate(const std::vector<uint8_t>& bytes, const std::string& token) {
    const auto at = std::search(bytes.begin(), bytes.end(), token.begin(), token.end());
    CHECK(at != bytes.end()); return size_t(at - bytes.begin());
}
void rejected_bytes(const std::vector<uint8_t>& bytes) {
    auto target = sample(); target.bank = 1234; const auto before = encode(target); std::string error;
    CHECK(!decode_session_save(bytes.data(), bytes.size(), compatibility, target, error));
    CHECK(!error.empty()); CHECK(encode(target) == before);
}
void rejected_snapshot(const std::function<void(SessionSnapshot&)>& change) {
    auto s = sample(); change(s); std::vector<uint8_t> bytes{9, 8, 7}; std::string error;
    CHECK(!encode_session_save(s, compatibility, bytes, error));
    CHECK(!error.empty()); CHECK((bytes == std::vector<uint8_t>{9, 8, 7}));
}
void codec_tests() {
    const auto source = sample(); const auto bytes = encode(source); std::string error;
    CHECK(bytes.size() < session_save_max_bytes); CHECK(bytes[8] == session_save_schema);
    SessionSnapshot restored; CHECK(decode_session_save(bytes.data(), bytes.size(), compatibility, restored, error));
    CHECK(error.empty()); CHECK(encode(restored) == bytes);
    CHECK(restored.party[0] == "hero_b" && restored.characters[0].inventory[0].uid == 0);
    CHECK(restored.characters[0].inventory[1].uid == UINT32_MAX);
    CHECK(restored.characters[0].permanent_boosts[1].value == -2);
    CHECK(restored.rng_policy == SessionRngPolicy::NotSerialized);
    CHECK(!restored.flags[0].value); // No implicit saved-flag mutation.
    for (size_t n = 0; n < bytes.size(); ++n) rejected_bytes(std::vector<uint8_t>(bytes.begin(), bytes.begin() + n));
    // Rebuild envelope and CRC so every payload truncation reaches the parser.
    for (size_t n = 32; n < bytes.size(); ++n) {
        std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + n - 4);
        truncated.resize(n); put32(truncated, 24, uint32_t(n - 32)); fix_crc(truncated); rejected_bytes(truncated);
    }
    for (size_t i = 0; i < bytes.size(); ++i) {
        auto damaged = bytes; damaged[i] ^= 0x80; rejected_bytes(damaged);
    }
    auto damaged = bytes; damaged.push_back(0); rejected_bytes(damaged);
    damaged = bytes; damaged.insert(damaged.end() - 4, 0); put32(damaged, 24, uint32_t(damaged.size() - 32)); fix_crc(damaged); rejected_bytes(damaged);
    for (size_t offset : {size_t(8), size_t(12), size_t(16), size_t(20)}) {
        damaged = bytes; put32(damaged, offset, 999); fix_crc(damaged); rejected_bytes(damaged);
    }
    damaged = bytes; put32(damaged, 28, UINT32_MAX); fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes; put32(damaged, locate(damaged, "hero_a") - 8, UINT32_MAX); fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes; damaged[locate(damaged, "flag_one") + 8] = 2; fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes; put32(damaged, damaged.size() - 8, 2); fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes; damaged[locate(damaged, "scene/test")] = 0xc0; fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes;
    const auto hp_offset = locate(damaged, "Nickname marker") + std::string("Nickname marker").size() + 16;
    std::fill_n(damaged.begin() + hp_offset, 8, 0xff); fix_crc(damaged); rejected_bytes(damaged);
    damaged = bytes; const auto second_skill = locate(damaged, "skill_b"); damaged[second_skill + 6] = 'a'; fix_crc(damaged); rejected_bytes(damaged);
    SessionSnapshot unchanged = source; auto before = encode(unchanged);
    CHECK(!decode_session_save(nullptr, bytes.size(), compatibility, unchanged, error)); CHECK(encode(unchanged) == before);
    CHECK(!decode_session_save(bytes.data(), session_save_max_bytes + 1, compatibility, unchanged, error));
    auto c = compatibility; c.rules_revision = 0;
    CHECK(!decode_session_save(bytes.data(), bytes.size(), c, unchanged, error));
    std::vector<uint8_t> out{5}; CHECK(!encode_session_save(source, c, out, error)); CHECK(out == std::vector<uint8_t>{5});

    rejected_snapshot([](auto& s) { s.scene_id.clear(); });
    rejected_snapshot([](auto& s) { s.scene_id = std::string("abc\0def", 7); });
    for (const std::string& invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xe2\x82")})
        rejected_snapshot([&](auto& s) { s.player_name = invalid; });
    rejected_snapshot([](auto& s) { s.scene_label.assign(session_save_max_string_bytes + 1, 'x'); });
    rejected_snapshot([](auto& s) { s.position_x = std::numeric_limits<double>::infinity(); });
    rejected_snapshot([](auto& s) { s.direction_y = std::numeric_limits<double>::quiet_NaN(); });
    rejected_snapshot([](auto& s) { s.direction_x = 0; s.direction_y = 0; });
    rejected_snapshot([](auto& s) { s.direction_x = 2; });
    rejected_snapshot([](auto& s) { s.playtime_seconds = -1; });
    rejected_snapshot([](auto& s) { s.settings.text_speed = 0; });
    rejected_snapshot([](auto& s) { s.settings.button_prompts.clear(); });
    rejected_snapshot([](auto& s) { s.cash = -1; });
    rejected_snapshot([](auto& s) { s.bank = int64_t(UINT32_MAX) + 1; });
    rejected_snapshot([](auto& s) { s.rng_policy = SessionRngPolicy(0); });
    rejected_snapshot([](auto& s) { s.characters.clear(); });
    rejected_snapshot([](auto& s) { s.characters[1].character_id = s.characters[0].character_id; });
    rejected_snapshot([](auto& s) { s.party.clear(); });
    rejected_snapshot([](auto& s) { s.party[1] = s.party[0]; });
    rejected_snapshot([](auto& s) { s.party[0] = "unknown"; });
    rejected_snapshot([](auto& s) { s.characters[0].level = 0; });
    rejected_snapshot([](auto& s) { s.characters[0].hp = -1; });
    rejected_snapshot([](auto& s) { s.characters[0].experience = -1; });
    rejected_snapshot([](auto& s) { s.characters[0].permanent_boosts[1].id = s.characters[0].permanent_boosts[0].id; });
    rejected_snapshot([](auto& s) { s.characters[0].affinity_multipliers[0].value = std::numeric_limits<double>::infinity(); });
    rejected_snapshot([](auto& s) { s.characters[0].learned_skills.push_back(s.characters[0].learned_skills[0]); });
    rejected_snapshot([](auto& s) { s.characters[0].status[0].passive_healing_turns = -1; });
    rejected_snapshot([](auto& s) { s.characters[0].inventory[0].doses = 0; });
    rejected_snapshot([](auto& s) { s.key_items[0].uid = s.characters[0].inventory[0].uid; });
    rejected_snapshot([](auto& s) { s.flags.push_back(s.flags[0]); });
    rejected_snapshot([](auto& s) { s.object_flags[0].id.clear(); });
    rejected_snapshot([](auto& s) { s.rare_drops[0].value = -1; });
    rejected_snapshot([](auto& s) { s.flags.resize(session_save_max_entries + 1); });
    rejected_snapshot([](auto& s) {
        s.flags.clear();
        for (int i = 0; i < 300; ++i) s.flags.push_back({std::to_string(i) + std::string(4000, 'x'), false});
    });
    auto edge = sample(); edge.cash = edge.bank = edge.earned_cash = UINT32_MAX;
    edge.characters[0].experience = UINT32_MAX; edge.characters[0].hp = edge.characters[0].pp = INT32_MAX;
    edge.characters[0].permanent_boosts[0].value = INT32_MIN;
    edge.player_name.assign(session_save_max_string_bytes, 'x');
    const auto edge_bytes = encode(edge); CHECK(decode_session_save(edge_bytes.data(), edge_bytes.size(), compatibility, restored, error));
    CHECK(encode(restored) == edge_bytes);
    // CRC-valid deterministic malformed inputs exercise nested parser paths.
    uint32_t pattern = 0x7342a192u;
    for (unsigned i = 0; i < 512; ++i) {
        auto mutated = bytes;
        for (int j = 0; j < 3; ++j) {
            pattern = pattern * 1664525u + 1013904223u;
            const size_t offset = 28 + pattern % (mutated.size() - 32);
            mutated[offset] ^= uint8_t(1u << ((pattern >> 24) & 7));
        }
        fix_crc(mutated);
        auto candidate = source; candidate.bank = 999; const auto prior = encode(candidate);
        if (decode_session_save(mutated.data(), mutated.size(), compatibility, candidate, error)) {
            CHECK(validate_session_snapshot(candidate, error)); CHECK(encode(candidate) == mutated);
        } else { CHECK(!error.empty()); CHECK(encode(candidate) == prior); }
    }
}

class MemoryFileOps final : public SessionSaveFileOps {
public:
    std::map<std::string, std::vector<uint8_t>> files;
    int calls = 0, fail_call = 0;
    std::string corrupt_read, fail_remove, destructive_replace_failure;
    bool fail_restoration = false, commit_failed = false;
    bool operation(std::string& error) {
        if (++calls == fail_call) { error = "Injected storage failure"; return false; }
        error.clear(); return true;
    }
    SessionFileReadResult read(const std::string& path, size_t limit, std::vector<uint8_t>& out, std::string& error) override {
        if (!operation(error)) return SessionFileReadResult::Error;
        const auto i = files.find(path); if (i == files.end()) return SessionFileReadResult::Missing;
        if (i->second.size() > limit) { error = "oversized"; return SessionFileReadResult::Error; }
        out = i->second; if (corrupt_read == path && !out.empty()) out[0] ^= 1; return SessionFileReadResult::Ok;
    }
    bool write_new(const std::string& path, const std::vector<uint8_t>& bytes, std::string& error) override {
        if (!operation(error)) return false;
        if (fail_restoration && commit_failed && path == "slot") { error = "Injected primary restoration failure"; return false; }
        if (files.count(path)) { error = "already exists"; return false; }
        files[path] = bytes; return true;
    }
    bool replace(const std::string& from, const std::string& to, std::string& error) override {
        if (!operation(error)) return false;
        if (to == destructive_replace_failure) {
            files.erase(to); commit_failed = true; error = "Injected delete-then-rename failure"; return false;
        }
        if (!files.count(from)) { error = "missing source"; return false; }
        files[to] = files.at(from); files.erase(from); return true;
    }
    bool remove(const std::string& path, std::string& error) override {
        if (!operation(error) || path == fail_remove) { error = "cleanup failed"; return false; }
        files.erase(path); return true;
    }
};
void file_failure_tests() {
    auto first = sample(); auto next = first; next.bank = 1000; next.flags[0].value = true;
    const auto old = encode(first), newer = encode(next); std::string error;
    MemoryFileOps success; success.files["slot"] = old;
    CHECK(write_session_save("slot", next, compatibility, error, &success)); CHECK(error.empty());
    CHECK(success.files["slot"] == newer); CHECK(success.files["slot.bak"] == old);
    CHECK(!success.files.count("slot.tmp") && !success.files.count("slot.bak.tmp"));
    const int total_calls = success.calls;
    for (int failure = 1; failure <= total_calls; ++failure) {
        MemoryFileOps ops; ops.files["slot"] = old; ops.files["slot.bak"] = old; ops.fail_call = failure;
        CHECK(!write_session_save("slot", next, compatibility, error, &ops)); CHECK(!error.empty());
        CHECK(ops.files["slot"] == old); CHECK(ops.files["slot.bak"] == old);
        CHECK(!ops.files.count("slot.tmp") && !ops.files.count("slot.bak.tmp"));
    }
    for (const auto& path : {"slot.tmp", "slot.bak.tmp"}) {
        MemoryFileOps ops; ops.files["slot"] = old; ops.corrupt_read = path;
        CHECK(!write_session_save("slot", next, compatibility, error, &ops)); CHECK(ops.files["slot"] == old);
    }
    MemoryFileOps non_atomic; non_atomic.files["slot"] = old; non_atomic.destructive_replace_failure = "slot";
    CHECK(!write_session_save("slot", next, compatibility, error, &non_atomic));
    CHECK(non_atomic.files["slot"] == old); CHECK(non_atomic.files["slot.bak"] == old);
    CHECK(error.find("prior primary restored") != std::string::npos); CHECK(!non_atomic.files.count("slot.tmp"));
    MemoryFileOps recovery_failure; recovery_failure.files["slot"] = old;
    recovery_failure.destructive_replace_failure = "slot"; recovery_failure.fail_restoration = true;
    CHECK(!write_session_save("slot", next, compatibility, error, &recovery_failure));
    CHECK(!recovery_failure.files.count("slot")); CHECK(recovery_failure.files["slot.bak"] == old);
    CHECK(recovery_failure.files["slot.tmp"] == newer); CHECK(error.find("primary recovery required") != std::string::npos);
    MemoryFileOps backup_failure; backup_failure.files["slot"] = old; backup_failure.files["slot.bak"] = newer;
    backup_failure.destructive_replace_failure = "slot.bak";
    CHECK(!write_session_save("slot", next, compatibility, error, &backup_failure));
    CHECK(backup_failure.files["slot"] == old); CHECK(!backup_failure.files.count("slot.tmp"));
    for (const auto& path : {"slot.tmp", "slot.bak.tmp"}) {
        MemoryFileOps ops; ops.files["slot"] = old; ops.files[path] = {4, 5, 6};
        CHECK(!write_session_save("slot", next, compatibility, error, &ops)); CHECK(ops.files["slot"] == old);
        CHECK((ops.files[path] == std::vector<uint8_t>{4, 5, 6}));
    }
    MemoryFileOps dirty; dirty.files["slot"] = old; dirty.corrupt_read = "slot.tmp"; dirty.fail_remove = "slot.tmp";
    CHECK(!write_session_save("slot", next, compatibility, error, &dirty)); CHECK(dirty.files["slot"] == old);
    CHECK(error.find("cleanup failed") != std::string::npos);
    MemoryFileOps invalid; invalid.files["slot"] = {1, 2, 3}; invalid.files["slot.bak"] = old;
    CHECK(!write_session_save("slot", next, compatibility, error, &invalid)); CHECK(invalid.calls == 1);
    CHECK((invalid.files["slot"] == std::vector<uint8_t>{1, 2, 3})); CHECK(invalid.files["slot.bak"] == old);
    MemoryFileOps empty;
    CHECK(write_session_save("slot", first, compatibility, error, &empty)); CHECK(!empty.files.count("slot.bak"));
    const int create_calls = empty.calls;
    for (int failure = 1; failure <= create_calls; ++failure) {
        MemoryFileOps ops; ops.fail_call = failure;
        CHECK(!write_session_save("slot", first, compatibility, error, &ops)); CHECK(!ops.files.count("slot"));
    }
    SessionSnapshot target = next; const auto unchanged = encode(target);
    MemoryFileOps missing; CHECK(!read_session_save("slot", compatibility, target, error, &missing)); CHECK(encode(target) == unchanged);
    CHECK(!read_session_save("slot", compatibility, target, error, &invalid)); CHECK(encode(target) == unchanged);
    CHECK(read_session_save("slot.bak", compatibility, target, error, &invalid)); CHECK(encode(target) == old);
    CHECK(!write_session_save(nullptr, first, compatibility, error, &empty));
    CHECK(!read_session_save("", compatibility, target, error, &empty));
    CHECK(!write_session_save("bad\npath", first, compatibility, error, &empty));
}

std::vector<uint8_t> read_raw(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void local_file_tests() {
    const char* base = std::getenv("TMPDIR"); if (!base) base = "/tmp";
    std::filesystem::path directory;
    std::error_code ec;
    for (unsigned i = 0; i < 10000; ++i) {
        const auto candidate = std::filesystem::path(base) / ("encore-session-save-tests-" + std::to_string(i));
        if (std::filesystem::create_directory(candidate, ec)) { directory = candidate; break; }
        ec.clear();
    }
    CHECK(!directory.empty()); if (directory.empty()) return;
    const auto path = (directory / "slot.save").string(); const auto first = sample(); auto next = first; next.bank = 77;
    std::string error; SessionSnapshot restored;
    CHECK(write_session_save(path.c_str(), first, compatibility, error));
    CHECK(read_session_save(path.c_str(), compatibility, restored, error)); CHECK(encode(restored) == encode(first));
    CHECK(write_session_save(path.c_str(), next, compatibility, error));
    CHECK(read_raw(path) == encode(next)); CHECK(read_raw(path + ".bak") == encode(first));
    auto third = next; third.bank = 88;
    CHECK(write_session_save(path.c_str(), third, compatibility, error)); // Replaces existing backup too.
    CHECK(read_raw(path + ".bak") == encode(next));
    const auto old_primary = read_raw(path), old_backup = read_raw(path + ".bak");
#if defined(__unix__)
    // Force the real stdio adapter to fail while flushing a partially written
    // temporary file. Restore the process limit before printing test results.
    struct rlimit original_limit{};
    const bool got_limit = getrlimit(RLIMIT_FSIZE, &original_limit) == 0;
    CHECK(got_limit);
    if (got_limit && original_limit.rlim_cur >= 128) {
        const auto previous_handler = std::signal(SIGXFSZ, SIG_IGN);
        auto short_limit = original_limit; short_limit.rlim_cur = 128;
        const bool changed_limit = setrlimit(RLIMIT_FSIZE, &short_limit) == 0;
        const bool write_result = changed_limit && write_session_save(path.c_str(), first, compatibility, error);
        const bool restored_limit = setrlimit(RLIMIT_FSIZE, &original_limit) == 0;
        std::signal(SIGXFSZ, previous_handler);
        CHECK(changed_limit); CHECK(restored_limit); CHECK(!write_result);
        CHECK(error.find("write/flush/close failed") != std::string::npos);
        CHECK(read_raw(path) == old_primary); CHECK(read_raw(path + ".bak") == old_backup);
        CHECK(!std::filesystem::exists(path + ".tmp"));
    }
#endif
    { std::ofstream stale(path + ".tmp", std::ios::binary); stale << "stale"; }
    CHECK(!write_session_save(path.c_str(), first, compatibility, error));
    CHECK(read_raw(path) == old_primary); CHECK(read_raw(path + ".bak") == old_backup);
    CHECK((read_raw(path + ".tmp") == std::vector<uint8_t>{'s','t','a','l','e'}));
    std::filesystem::remove(path + ".tmp", ec);
    std::filesystem::create_directory(path + ".bak.tmp", ec);
    CHECK(!write_session_save(path.c_str(), first, compatibility, error)); CHECK(read_raw(path) == old_primary);
    CHECK(!std::filesystem::exists(path + ".tmp"));
    CHECK(!write_session_save((directory / "missing" / "save").string().c_str(), first, compatibility, error));
    const auto prior = encode(restored);
    CHECK(!read_session_save((directory / "missing").string().c_str(), compatibility, restored, error)); CHECK(encode(restored) == prior);
    std::filesystem::remove_all(directory, ec); CHECK(!ec);
}
} // namespace
int main() {
    codec_tests(); file_failure_tests(); local_file_tests();
    std::cout << "Session save: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
