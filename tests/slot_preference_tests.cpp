#include "encore/slot_preference.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

using namespace encore::upstream;
namespace {
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::cerr << __LINE__ << ": " #x "\n"; } } while (false)
// Structural test capacity; this is not a runtime game-content default.
constexpr uint32_t capacity = 10;
std::vector<uint8_t> encoded(uint32_t slot) {
    std::vector<uint8_t> bytes; std::string error;
    CHECK(encode_slot_preference(capacity, slot, bytes, error)); CHECK(error.empty()); return bytes;
}
uint32_t crc32(const uint8_t* bytes, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
}
void fix_crc(std::vector<uint8_t>& bytes) { put32(bytes, 16, crc32(bytes.data(), 16)); }
void rejects(const std::vector<uint8_t>& bytes, uint32_t max_slots = capacity) {
    uint32_t output = 1234; std::string error;
    CHECK(!decode_slot_preference(bytes.data(), bytes.size(), max_slots, output, error));
    CHECK(!error.empty()); CHECK(output == 1234);
}
void codec_tests() {
    // CRC values independently generated with Python zlib.crc32.
    const std::vector<uint8_t> zero{0x45,0x4e,0x43,0x53,0x4c,0x4f,0x54,0x31,1,0,0,0,0,0,0,0,0x41,0x6f,0x4a,0x23};
    const std::vector<uint8_t> ten{0x45,0x4e,0x43,0x53,0x4c,0x4f,0x54,0x31,1,0,0,0,10,0,0,0,0x25,0x8f,0xf7,0x4c};
    CHECK(encoded(0) == zero); CHECK(encoded(10) == ten);
    std::string error;
    for (uint32_t slot = 0; slot <= capacity; ++slot) {
        const auto bytes = encoded(slot); uint32_t result = 1234;
        CHECK(bytes.size() == slot_preference_bytes);
        CHECK(decode_slot_preference(bytes.data(), bytes.size(), capacity, result, error));
        CHECK(result == slot); CHECK(error.empty());
    }
    for (size_t size = 0; size < ten.size(); ++size) rejects({ten.begin(), ten.begin() + size});
    for (size_t i = 0; i < ten.size(); ++i) {
        auto bytes = ten; bytes[i] ^= 0x80; rejects(bytes);
    }
    auto bytes = ten; bytes.push_back(0); rejects(bytes);
    bytes = ten; put32(bytes, 8, 2); fix_crc(bytes); rejects(bytes);
    bytes = ten; put32(bytes, 12, capacity + 1); fix_crc(bytes); rejects(bytes);
    bytes = ten; put32(bytes, 12, UINT32_MAX); fix_crc(bytes); rejects(bytes);
    rejects(ten, 0); rejects(ten, capacity - 1);
    uint32_t output = 42;
    CHECK(!decode_slot_preference(nullptr, slot_preference_bytes, capacity, output, error)); CHECK(output == 42);
    for (const auto& values : {std::pair<uint32_t,uint32_t>{0,0}, {capacity,capacity + 1}, {capacity,UINT32_MAX}}) {
        bytes = {1,2,3}; CHECK(!encode_slot_preference(values.first, values.second, bytes, error));
        CHECK((bytes == std::vector<uint8_t>{1,2,3})); CHECK(!error.empty());
    }
    CHECK(encode_slot_preference(UINT32_MAX, UINT32_MAX, bytes, error));
    CHECK(decode_slot_preference(bytes.data(), bytes.size(), UINT32_MAX, output, error)); CHECK(output == UINT32_MAX);
}
class MemoryOps final : public SessionSaveFileOps {
public:
    std::map<std::string,std::vector<uint8_t>> files;
    int calls = 0, fail_call = 0;
    std::string corrupt_read, destructive_failure, remove_failure;
    bool fail_restore = false, commit_failed = false;
    bool operation(std::string& error) {
        if (++calls == fail_call) { error = "Injected storage failure"; return false; }
        error.clear(); return true;
    }
    SessionFileReadResult read(const std::string& path, size_t limit,
                              std::vector<uint8_t>& output, std::string& error) override {
        CHECK(limit == slot_preference_bytes);
        if (!operation(error)) return SessionFileReadResult::Error;
        const auto found = files.find(path);
        if (found == files.end()) return SessionFileReadResult::Missing;
        if (found->second.size() > limit) { error = "Oversized input"; return SessionFileReadResult::Error; }
        output = found->second; if (path == corrupt_read && !output.empty()) output[0] ^= 1;
        return SessionFileReadResult::Ok;
    }
    bool write_new(const std::string& path, const std::vector<uint8_t>& bytes, std::string& error) override {
        if (!operation(error)) return false;
        if (fail_restore && commit_failed && path == "preference") { error = "Injected recovery failure"; return false; }
        if (files.count(path)) { error = "Already exists"; return false; }
        files[path] = bytes; return true;
    }
    bool replace(const std::string& from, const std::string& to, std::string& error) override {
        if (!operation(error)) return false;
        if (to == destructive_failure) {
            files.erase(to); commit_failed = true; error = "Injected delete-then-rename failure"; return false;
        }
        if (!files.count(from)) { error = "Missing source"; return false; }
        files[to] = files.at(from); files.erase(from); return true;
    }
    bool remove(const std::string& path, std::string& error) override {
        if (!operation(error) || path == remove_failure) { error = "Injected cleanup failure"; return false; }
        files.erase(path); return true;
    }
};
void storage_tests() {
    const auto old = encoded(1), next = encoded(capacity); std::string error;
    MemoryOps success; success.files["preference"] = old;
    CHECK(write_slot_preference("preference", capacity, capacity, error, &success)); CHECK(error.empty());
    CHECK(success.files["preference"] == next); CHECK(success.files["preference.bak"] == old);
    CHECK(!success.files.count("preference.tmp") && !success.files.count("preference.bak.tmp"));
    const int overwrite_calls = success.calls;
    // Every read/write/verification/rename in a successful overwrite can fail.
    for (int failure = 1; failure <= overwrite_calls; ++failure) {
        MemoryOps ops; ops.files["preference"] = old; ops.files["preference.bak"] = old; ops.fail_call = failure;
        CHECK(!write_slot_preference("preference", capacity, capacity, error, &ops)); CHECK(!error.empty());
        CHECK(ops.files["preference"] == old); CHECK(ops.files["preference.bak"] == old);
        CHECK(!ops.files.count("preference.tmp") && !ops.files.count("preference.bak.tmp"));
    }
    for (const std::string path : {"preference.tmp", "preference.bak.tmp", "preference.bak"}) {
        MemoryOps ops; ops.files["preference"] = old; ops.corrupt_read = path;
        CHECK(!write_slot_preference("preference", capacity, capacity, error, &ops)); CHECK(ops.files["preference"] == old);
    }
    for (const std::string path : {"preference.tmp", "preference.bak.tmp"}) {
        MemoryOps ops; ops.files["preference"] = old; ops.files[path] = {4,5,6};
        CHECK(!write_slot_preference("preference", capacity, capacity, error, &ops)); CHECK(ops.files["preference"] == old);
        CHECK((ops.files[path] == std::vector<uint8_t>{4,5,6}));
    }
    MemoryOps destructive; destructive.files["preference"] = old; destructive.destructive_failure = "preference";
    CHECK(!write_slot_preference("preference", capacity, capacity, error, &destructive));
    CHECK(destructive.files["preference"] == old); CHECK(destructive.files["preference.bak"] == old);
    CHECK(error.find("prior preference restored") != std::string::npos); CHECK(!destructive.files.count("preference.tmp"));
    MemoryOps recovery; recovery.files["preference"] = old; recovery.destructive_failure = "preference"; recovery.fail_restore = true;
    CHECK(!write_slot_preference("preference", capacity, capacity, error, &recovery));
    CHECK(!recovery.files.count("preference")); CHECK(recovery.files["preference.bak"] == old);
    CHECK(recovery.files["preference.tmp"] == next); CHECK(error.find("primary recovery required") != std::string::npos);
    MemoryOps backup; backup.files["preference"] = old; backup.files["preference.bak"] = next; backup.destructive_failure = "preference.bak";
    CHECK(!write_slot_preference("preference", capacity, capacity, error, &backup)); CHECK(backup.files["preference"] == old);
    MemoryOps cleanup; cleanup.files["preference"] = old; cleanup.corrupt_read = "preference.tmp"; cleanup.remove_failure = "preference.tmp";
    CHECK(!write_slot_preference("preference", capacity, capacity, error, &cleanup)); CHECK(cleanup.files["preference"] == old);
    CHECK(error.find("cleanup failure") != std::string::npos);
    MemoryOps invalid; invalid.files["preference"] = {1,2,3}; invalid.files["preference.bak"] = old;
    CHECK(!write_slot_preference("preference", capacity, 0, error, &invalid)); CHECK(invalid.calls == 1);
    CHECK((invalid.files["preference"] == std::vector<uint8_t>{1,2,3})); CHECK(invalid.files["preference.bak"] == old);
    MemoryOps empty;
    CHECK(write_slot_preference("preference", capacity, 0, error, &empty)); CHECK(empty.files["preference"] == encoded(0));
    CHECK(!empty.files.count("preference.bak"));
    const int create_calls = empty.calls;
    for (int failure = 1; failure <= create_calls; ++failure) {
        MemoryOps ops; ops.fail_call = failure;
        CHECK(!write_slot_preference("preference", capacity, capacity, error, &ops));
        CHECK(!ops.files.count("preference") && !ops.files.count("preference.tmp"));
    }
    uint32_t output = 1234; MemoryOps missing; missing.files["preference.bak"] = old;
    error = "stale";
    CHECK(read_slot_preference("preference", capacity, output, error, &missing) == SlotPreferenceReadResult::Missing);
    CHECK(output == 1234); CHECK(error.empty()); CHECK(missing.files.size() == 1);
    CHECK(read_slot_preference("preference", capacity, output, error, &invalid) == SlotPreferenceReadResult::Error);
    CHECK(output == 1234); CHECK(!error.empty());
    CHECK(read_slot_preference("preference", capacity, output, error, &success) == SlotPreferenceReadResult::Ok);
    CHECK(output == capacity); CHECK(error.empty());
    for (const char* path : {static_cast<const char*>(nullptr), "", "bad\npath", "directory/", "directory\\"}) {
        CHECK(!write_slot_preference(path, capacity, 1, error, &missing));
        CHECK(read_slot_preference(path, capacity, output, error, &missing) == SlotPreferenceReadResult::Error);
        CHECK(output == capacity);
    }
    const int prior_calls = missing.calls;
    CHECK(read_slot_preference("preference", 0, output, error, &missing) == SlotPreferenceReadResult::Error);
    CHECK(!write_slot_preference("preference", 0, 0, error, &missing)); CHECK(missing.calls == prior_calls);
}
std::vector<uint8_t> raw(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
void local_tests() {
    const char* base = std::getenv("TMPDIR"); if (!base) base = "/tmp";
    std::filesystem::path directory; std::error_code ec;
    for (unsigned i = 0; i < 10000; ++i) {
        const auto candidate = std::filesystem::path(base) / ("encore-slot-preference-tests-" + std::to_string(i));
        if (std::filesystem::create_directory(candidate, ec)) { directory = candidate; break; }
        ec.clear();
    }
    CHECK(!directory.empty()); if (directory.empty()) return;
    const auto path = (directory / "selection.pref").string(); std::string error; uint32_t output = 1234;
    CHECK(read_slot_preference(path.c_str(), capacity, output, error) == SlotPreferenceReadResult::Missing);
    CHECK(output == 1234); CHECK(error.empty());
    CHECK(write_slot_preference(path.c_str(), capacity, capacity, error)); CHECK(raw(path) == encoded(capacity));
    CHECK(read_slot_preference(path.c_str(), capacity, output, error) == SlotPreferenceReadResult::Ok); CHECK(output == capacity);
    CHECK(write_slot_preference(path.c_str(), capacity, 0, error)); CHECK(raw(path + ".bak") == encoded(capacity));
    CHECK(read_slot_preference(path.c_str(), capacity, output, error) == SlotPreferenceReadResult::Ok); CHECK(output == 0);
    { std::ofstream f(path + ".tmp", std::ios::binary); f << "existing"; }
    CHECK(!write_slot_preference(path.c_str(), capacity, 2, error)); CHECK(raw(path) == encoded(0));
    CHECK((raw(path + ".tmp") == std::vector<uint8_t>{'e','x','i','s','t','i','n','g'}));
    { std::ofstream f(path, std::ios::binary); f << std::string(1000, 'x'); }
    output = 1234;
    CHECK(read_slot_preference(path.c_str(), capacity, output, error) == SlotPreferenceReadResult::Error);
    CHECK(output == 1234); CHECK(!error.empty());
    const auto absent = (directory / "absent" / "selection.pref").string();
    CHECK(!write_slot_preference(absent.c_str(), capacity, 1, error)); CHECK(!std::filesystem::exists(directory / "absent"));
    std::filesystem::remove_all(directory, ec); CHECK(!ec);
}
} // namespace
int main() {
    codec_tests(); storage_tests(); local_tests();
    std::cout << "slot preference: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
