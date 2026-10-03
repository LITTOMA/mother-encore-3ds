#include "encore/session_save.hpp"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sys/stat.h>
#include <utility>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace encore::upstream {
namespace {
constexpr size_t header_size = 28, trailer_size = 4;
constexpr char magic[8] = {'E','N','C','S','N','A','P','1'};
constexpr double exact_integer_limit = 9007199254740991.0;

bool reject(std::string& error, const char* message) { error = message; return false; }
bool compatible(const SessionSaveCompatibility& c) {
    return c.content_family && c.content_revision && c.rules_revision;
}
bool valid_utf8(const std::string& s, bool nonempty = false) {
    if ((nonempty && s.empty()) || s.size() > session_save_max_string_bytes) return false;
    for (size_t i = 0; i < s.size();) {
        const auto a = uint8_t(s[i++]);
        if (!a) return false;
        if (a < 0x80) continue;
        uint32_t cp = 0, minimum = 0; size_t count = 0;
        if (a >= 0xc2 && a <= 0xdf) { cp = a & 0x1f; minimum = 0x80; count = 1; }
        else if (a >= 0xe0 && a <= 0xef) { cp = a & 0x0f; minimum = 0x800; count = 2; }
        else if (a >= 0xf0 && a <= 0xf4) { cp = a & 7; minimum = 0x10000; count = 3; }
        else return false;
        if (count > s.size() - i) return false;
        while (count--) {
            const auto b = uint8_t(s[i++]);
            if ((b & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (b & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}
bool counter(int64_t value) { return value >= 0 && value <= UINT32_MAX; }
bool stat(int64_t value) { return value >= 0 && value <= INT32_MAX; }
template<class T, class Id> bool unique_ids(const std::vector<T>& values, Id id,
                                            size_t cap = session_save_max_entries) {
    if (values.size() > cap) return false;
    std::set<std::string> ids;
    for (const auto& v : values) if (!valid_utf8(id(v), true) || !ids.insert(id(v)).second) return false;
    return true;
}
bool flags_valid(const std::vector<SessionFlag>& values) {
    return unique_ids(values, [](const SessionFlag& v) -> const std::string& { return v.id; });
}
bool integers_valid(const std::vector<SessionIntegerValue>& values, bool signed_values) {
    if (!unique_ids(values, [](const SessionIntegerValue& v) -> const std::string& { return v.id; })) return false;
    for (const auto& v : values) {
        if (signed_values ? (v.value < INT32_MIN || v.value > INT32_MAX) : !counter(v.value)) return false;
    }
    return true;
}
bool items_valid(const std::vector<SessionItem>& items, std::set<uint32_t>& uids) {
    if (items.size() > session_save_max_entries) return false;
    for (const auto& item : items) {
        if (!valid_utf8(item.item_id, true) || !stat(item.doses) || !item.doses ||
            !uids.insert(item.uid).second) return false;
    }
    return true;
}
uint32_t checksum(const uint8_t* bytes, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
struct Writer {
    std::vector<uint8_t> bytes;
    bool ok = true;
    void raw(const uint8_t* p, size_t n) {
        if (!ok || n > session_save_max_bytes - bytes.size()) { ok = false; return; }
        bytes.insert(bytes.end(), p, p + n);
    }
    void u8(uint8_t v) { raw(&v, 1); }
    void u32(uint32_t v) { for (unsigned i = 0; i < 4; ++i) u8(uint8_t(v >> (8 * i))); }
    void u64(uint64_t v) { for (unsigned i = 0; i < 8; ++i) u8(uint8_t(v >> (8 * i))); }
    void i64(int64_t v) { u64(uint64_t(v)); }
    void real(double v) { uint64_t bits; std::memcpy(&bits, &v, sizeof(bits)); u64(bits); }
    void string(const std::string& s) { u32(uint32_t(s.size())); raw(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
    template<class T, class Emit> void list(const std::vector<T>& values, Emit emit) {
        u32(uint32_t(values.size())); for (const auto& v : values) { if (!ok) break; emit(v); }
    }
    void strings(const std::vector<std::string>& values) { list(values, [&](const std::string& v) { string(v); }); }
    void flags(const std::vector<SessionFlag>& values) { list(values, [&](const SessionFlag& v) { string(v.id); u8(v.value); }); }
    void integers(const std::vector<SessionIntegerValue>& values) { list(values, [&](const SessionIntegerValue& v) { string(v.id); i64(v.value); }); }
    void items(const std::vector<SessionItem>& values) {
        list(values, [&](const SessionItem& v) { string(v.item_id); u8(v.equipped); i64(v.doses); u32(v.uid); });
    }
};
struct Reader {
    const uint8_t* bytes; size_t size, offset = 0; bool ok = true;
    bool have(size_t n) { if (!ok || n > size - offset) { ok = false; return false; } return true; }
    uint8_t u8() { if (!have(1)) return 0; return bytes[offset++]; }
    uint32_t u32() { uint32_t v = 0; for (unsigned i = 0; i < 4; ++i) v |= uint32_t(u8()) << (8 * i); return v; }
    uint64_t u64() { uint64_t v = 0; for (unsigned i = 0; i < 8; ++i) v |= uint64_t(u8()) << (8 * i); return v; }
    int64_t i64() { const auto bits = u64(); int64_t v; std::memcpy(&v, &bits, sizeof(v)); return v; }
    double real() { const auto bits = u64(); double v; std::memcpy(&v, &bits, sizeof(v)); return v; }
    bool boolean() { const auto v = u8(); if (v > 1) ok = false; return v == 1; }
    std::string string() {
        const auto length = u32();
        if (length > session_save_max_string_bytes || !have(length)) { ok = false; return {}; }
        std::string value(reinterpret_cast<const char*>(bytes + offset), length); offset += length; return value;
    }
    template<class T, class Read> void list(std::vector<T>& values, Read read,
                                           size_t cap = session_save_max_entries) {
        const auto count = u32();
        // Every record begins with a length-prefixed identity (at least 4 bytes).
        if (!ok || count > cap || count > (size - offset) / 4) { ok = false; return; }
        values.reserve(count);
        for (uint32_t i = 0; i < count && ok; ++i) { values.emplace_back(); read(values.back()); }
    }
    void strings(std::vector<std::string>& values) { list(values, [&](std::string& v) { v = string(); }); }
    void flags(std::vector<SessionFlag>& values) { list(values, [&](SessionFlag& v) { v.id = string(); v.value = boolean(); }); }
    void integers(std::vector<SessionIntegerValue>& values) { list(values, [&](SessionIntegerValue& v) { v.id = string(); v.value = i64(); }); }
    void items(std::vector<SessionItem>& values) {
        list(values, [&](SessionItem& v) { v.item_id = string(); v.equipped = boolean(); v.doses = i64(); v.uid = u32(); });
    }
};
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559, "Save format requires IEEE-754 binary64");

void write_snapshot(Writer& w, const SessionSnapshot& s) {
    w.string(s.scene_id); w.string(s.scene_label);
    w.real(s.position_x); w.real(s.position_y); w.real(s.direction_x); w.real(s.direction_y);
    w.string(s.run_sound); w.string(s.shadow_effect); w.real(s.playtime_seconds);
    w.real(s.settings.text_speed); w.string(s.settings.menu_flavor); w.string(s.settings.button_prompts); w.u8(s.settings.description);
    w.string(s.player_name); w.string(s.favorite_food); w.i64(s.cash); w.i64(s.bank); w.i64(s.earned_cash);
    w.list(s.characters, [&](const SessionCharacter& c) {
        w.string(c.character_id); w.string(c.nickname); w.i64(c.level); w.i64(c.experience); w.i64(c.hp); w.i64(c.pp);
        w.integers(c.permanent_boosts);
        w.list(c.affinity_multipliers, [&](const SessionRealValue& v) { w.string(v.id); w.real(v.value); });
        w.strings(c.learned_skills);
        w.list(c.status, [&](const SessionStatus& v) { w.string(v.status_id); w.i64(v.passive_healing_turns); });
        w.items(c.inventory);
    });
    w.strings(s.party); w.items(s.key_items); w.items(s.storage);
    w.flags(s.flags); w.flags(s.object_flags); w.flags(s.seen_dialogue_flags); w.flags(s.encountered);
    w.integers(s.keys); w.integers(s.rare_drops);
    w.string(s.source_version); w.string(s.saved_at); w.u8(s.is_debug); w.u32(uint32_t(s.rng_policy));
}
void read_snapshot(Reader& r, SessionSnapshot& s) {
    s.scene_id = r.string(); s.scene_label = r.string();
    s.position_x = r.real(); s.position_y = r.real(); s.direction_x = r.real(); s.direction_y = r.real();
    s.run_sound = r.string(); s.shadow_effect = r.string(); s.playtime_seconds = r.real();
    s.settings.text_speed = r.real(); s.settings.menu_flavor = r.string(); s.settings.button_prompts = r.string(); s.settings.description = r.boolean();
    s.player_name = r.string(); s.favorite_food = r.string(); s.cash = r.i64(); s.bank = r.i64(); s.earned_cash = r.i64();
    r.list(s.characters, [&](SessionCharacter& c) {
        c.character_id = r.string(); c.nickname = r.string(); c.level = r.i64(); c.experience = r.i64(); c.hp = r.i64(); c.pp = r.i64();
        r.integers(c.permanent_boosts);
        r.list(c.affinity_multipliers, [&](SessionRealValue& v) { v.id = r.string(); v.value = r.real(); });
        r.strings(c.learned_skills);
        r.list(c.status, [&](SessionStatus& v) { v.status_id = r.string(); v.passive_healing_turns = r.i64(); });
        r.items(c.inventory);
    }, session_save_max_characters);
    r.strings(s.party); r.items(s.key_items); r.items(s.storage);
    r.flags(s.flags); r.flags(s.object_flags); r.flags(s.seen_dialogue_flags); r.flags(s.encountered);
    r.integers(s.keys); r.integers(s.rare_drops);
    s.source_version = r.string(); s.saved_at = r.string(); s.is_debug = r.boolean(); s.rng_policy = SessionRngPolicy(r.u32());
}

class LocalFileOps final : public SessionSaveFileOps {
public:
    SessionFileReadResult read(const std::string& path, size_t limit, std::vector<uint8_t>& output, std::string& error) override {
        errno = 0; FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            if (errno == ENOENT) { error.clear(); return SessionFileReadResult::Missing; }
            error = "Cannot open session save"; return SessionFileReadResult::Error;
        }
        if (std::fseek(f, 0, SEEK_END)) { std::fclose(f); error = "Cannot seek session save"; return SessionFileReadResult::Error; }
        const long length = std::ftell(f);
        if (length < 0 || size_t(length) > limit || std::fseek(f, 0, SEEK_SET)) {
            std::fclose(f); error = "Session save exceeds file bounds"; return SessionFileReadResult::Error;
        }
        std::vector<uint8_t> candidate(static_cast<size_t>(length));
        bool ok = std::fread(candidate.data(), 1, candidate.size(), f) == candidate.size();
        if (ok && std::fgetc(f) != EOF) ok = false;
        if (std::ferror(f)) ok = false;
        if (std::fclose(f)) ok = false;
        if (!ok) { error = "Cannot read complete session save"; return SessionFileReadResult::Error; }
        output = std::move(candidate); error.clear(); return SessionFileReadResult::Ok;
    }
    bool write_new(const std::string& path, const std::vector<uint8_t>& bytes, std::string& error) override {
#if defined(_WIN32)
        const int descriptor = _open(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
        FILE* f = descriptor < 0 ? nullptr : _fdopen(descriptor, "wb");
#else
        const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        FILE* f = descriptor < 0 ? nullptr : ::fdopen(descriptor, "wb");
#endif
        if (!f) {
            error = "Cannot exclusively create temporary session save; check directory, SD, or stale temporary file";
            if (descriptor >= 0) {
#if defined(_WIN32)
                _close(descriptor);
#else
                ::close(descriptor);
#endif
                std::string cleanup_error;
                if (!remove(path, cleanup_error)) error += "; " + cleanup_error;
            }
            return false;
        }
        bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        if (std::fflush(f)) ok = false;
        if (std::fclose(f)) ok = false;
        if (!ok) {
            error = "Session save write/flush/close failed";
            std::string cleanup_error;
            if (!remove(path, cleanup_error)) error += "; " + cleanup_error;
            return false;
        }
        error.clear(); return true;
    }
    bool replace(const std::string& from, const std::string& to, std::string& error) override {
        // libctru may delete an existing destination before a failed second
        // rename. The transaction above this adapter keeps/restores the backup.
        if (std::rename(from.c_str(), to.c_str())) return reject(error, "Cannot replace session save");
        error.clear(); return true;
    }
    bool remove(const std::string& path, std::string& error) override {
        if (std::remove(path.c_str()) && errno != ENOENT) return reject(error, "Cannot remove temporary session save");
        error.clear(); return true;
    }
};
bool valid_path(const char* path, std::string& error) {
    if (!path || !*path) return reject(error, "Empty session save path");
    const std::string value(path);
    if (value.size() > 1000 || value.back() == '/' || value.back() == '\\') return reject(error, "Invalid session save path");
    for (unsigned char c : value) if (c < 32 || c == 127) return reject(error, "Invalid session save path character");
    return true;
}
bool verify_file(SessionSaveFileOps& ops, const std::string& path, const std::vector<uint8_t>& expected, std::string& error) {
    std::vector<uint8_t> actual;
    if (ops.read(path, session_save_max_bytes, actual, error) != SessionFileReadResult::Ok) {
        if (error.empty()) error = "Temporary session save disappeared";
        return false;
    }
    if (actual != expected) return reject(error, "Temporary session save verification failed");
    return true;
}
void cleanup(SessionSaveFileOps& ops, const std::string& path, std::string& error) {
    std::string removal_error;
    if (!ops.remove(path, removal_error)) error += "; " + removal_error;
}
bool restore_after_failed_commit(SessionSaveFileOps& ops, const std::string& primary,
                                const std::vector<uint8_t>& previous, std::string& error) {
    std::vector<uint8_t> actual; std::string recovery_error;
    const auto state = ops.read(primary, session_save_max_bytes, actual, recovery_error);
    if (state == SessionFileReadResult::Ok && actual == previous) return true;
    if (state == SessionFileReadResult::Missing && ops.write_new(primary, previous, recovery_error) &&
        verify_file(ops, primary, previous, recovery_error)) {
        error += "; prior primary restored"; return true;
    }
    error += "; prior save retained at " + primary + ".bak; primary recovery required";
    if (!recovery_error.empty()) error += ": " + recovery_error;
    return false;
}
} // namespace

bool validate_session_snapshot(const SessionSnapshot& s, std::string& error) {
    if (!valid_utf8(s.scene_id, true) || !valid_utf8(s.scene_label) || !valid_utf8(s.run_sound, true) ||
        !valid_utf8(s.shadow_effect, true) || !valid_utf8(s.player_name) || !valid_utf8(s.favorite_food) ||
        !valid_utf8(s.source_version) || !valid_utf8(s.saved_at) || !valid_utf8(s.settings.menu_flavor, true) ||
        !valid_utf8(s.settings.button_prompts, true)) return reject(error, "Invalid/oversized UTF-8 session string");
    const double max_position = std::numeric_limits<float>::max();
    if (!std::isfinite(s.position_x) || !std::isfinite(s.position_y) || std::abs(s.position_x) > max_position || std::abs(s.position_y) > max_position ||
        !std::isfinite(s.direction_x) || !std::isfinite(s.direction_y) || std::abs(s.direction_x) > 1 || std::abs(s.direction_y) > 1 ||
        (s.direction_x == 0 && s.direction_y == 0)) return reject(error, "Invalid session position/direction");
    if (!std::isfinite(s.playtime_seconds) || s.playtime_seconds < 0 || s.playtime_seconds > exact_integer_limit ||
        !std::isfinite(s.settings.text_speed) || s.settings.text_speed <= 0 || s.settings.text_speed > std::numeric_limits<float>::max())
        return reject(error, "Invalid session time/settings");
    if (!counter(s.cash) || !counter(s.bank) || !counter(s.earned_cash)) return reject(error, "Invalid session money counter");
    if (s.rng_policy != SessionRngPolicy::NotSerialized) return reject(error, "Unsupported session RNG policy");
    if (s.characters.empty() || !unique_ids(s.characters, [](const SessionCharacter& c) -> const std::string& { return c.character_id; }, session_save_max_characters))
        return reject(error, "Invalid/duplicate session characters");
    if (s.party.empty() || !unique_ids(s.party, [](const std::string& id) -> const std::string& { return id; }, session_save_max_characters))
        return reject(error, "Invalid/duplicate session party");
    for (const auto& id : s.party) {
        bool found = false; for (const auto& c : s.characters) if (c.character_id == id) found = true;
        if (!found) return reject(error, "Party identity has no character snapshot");
    }
    std::set<uint32_t> uids;
    for (const auto& c : s.characters) {
        if (!valid_utf8(c.nickname) || !stat(c.level) || !c.level || !counter(c.experience) || !stat(c.hp) || !stat(c.pp))
            return reject(error, "Invalid session character stats");
        if (!integers_valid(c.permanent_boosts, true) ||
            !unique_ids(c.affinity_multipliers, [](const SessionRealValue& v) -> const std::string& { return v.id; }))
            return reject(error, "Invalid/duplicate character modifiers");
        for (const auto& affinity : c.affinity_multipliers) if (!std::isfinite(affinity.value) || std::abs(affinity.value) > std::numeric_limits<float>::max())
            return reject(error, "Invalid affinity multiplier");
        if (!unique_ids(c.learned_skills, [](const std::string& id) -> const std::string& { return id; }) ||
            !unique_ids(c.status, [](const SessionStatus& v) -> const std::string& { return v.status_id; }))
            return reject(error, "Invalid/duplicate character skills/status");
        for (const auto& status : c.status) if (!counter(status.passive_healing_turns)) return reject(error, "Invalid status turn counter");
        if (!items_valid(c.inventory, uids)) return reject(error, "Invalid character inventory or duplicate item UID");
    }
    if (!items_valid(s.key_items, uids) || !items_valid(s.storage, uids)) return reject(error, "Invalid key/storage inventory or duplicate item UID");
    if (!flags_valid(s.flags) || !flags_valid(s.object_flags) || !flags_valid(s.seen_dialogue_flags) || !flags_valid(s.encountered))
        return reject(error, "Invalid/duplicate session registry");
    if (!integers_valid(s.keys, false) || !integers_valid(s.rare_drops, false)) return reject(error, "Invalid/duplicate session counter registry");
    error.clear(); return true;
}

bool encode_session_save(const SessionSnapshot& snapshot, const SessionSaveCompatibility& c,
                         std::vector<uint8_t>& output, std::string& error) {
    if (!compatible(c)) return reject(error, "Invalid session compatibility identity");
    if (!validate_session_snapshot(snapshot, error)) return false;
    Writer w; w.raw(reinterpret_cast<const uint8_t*>(magic), sizeof(magic));
    w.u32(session_save_schema); w.u32(c.content_family); w.u32(c.content_revision); w.u32(c.rules_revision); w.u32(0);
    write_snapshot(w, snapshot);
    if (!w.ok || w.bytes.size() > session_save_max_bytes - trailer_size) return reject(error, "Session snapshot exceeds file size limit");
    const auto payload_size = uint32_t(w.bytes.size() - header_size);
    for (unsigned i = 0; i < 4; ++i) w.bytes[24 + i] = uint8_t(payload_size >> (8 * i));
    w.u32(checksum(w.bytes.data(), w.bytes.size()));
    output = std::move(w.bytes); error.clear(); return true;
}
bool decode_session_save(const uint8_t* bytes, size_t size, const SessionSaveCompatibility& expected,
                         SessionSnapshot& output, std::string& error) {
    if (!compatible(expected)) return reject(error, "Invalid expected session compatibility identity");
    if (!bytes || size < header_size + trailer_size || size > session_save_max_bytes || std::memcmp(bytes, magic, sizeof(magic)))
        return reject(error, "Invalid session save format/size");
    Reader header{bytes + 8, header_size - 8};
    const auto schema = header.u32(), family = header.u32(), content = header.u32(), rules = header.u32(), payload_size = header.u32();
    if (schema != session_save_schema || family != expected.content_family || content != expected.content_revision || rules != expected.rules_revision)
        return reject(error, "Session save requires unsupported schema/content/rules migration");
    if (payload_size != size - header_size - trailer_size) return reject(error, "Invalid session payload length");
    Reader crc_reader{bytes + size - trailer_size, trailer_size};
    if (checksum(bytes, size - trailer_size) != crc_reader.u32()) return reject(error, "Session save checksum mismatch; previous backup may be recoverable");
    Reader r{bytes + header_size, payload_size}; SessionSnapshot candidate;
    read_snapshot(r, candidate);
    if (!r.ok || r.offset != r.size) return reject(error, "Malformed/truncated session payload or unsupported trailing fields");
    if (!validate_session_snapshot(candidate, error)) return false;
    output = std::move(candidate); error.clear(); return true;
}

bool write_session_save(const char* path, const SessionSnapshot& snapshot, const SessionSaveCompatibility& compatibility,
                        std::string& error, SessionSaveFileOps* operations) {
    if (!valid_path(path, error)) return false;
    std::vector<uint8_t> bytes;
    if (!encode_session_save(snapshot, compatibility, bytes, error)) return false;
    LocalFileOps local; SessionSaveFileOps& ops = operations ? *operations : local;
    const std::string primary(path), temporary = primary + ".tmp", backup = primary + ".bak", backup_temporary = primary + ".bak.tmp";
    std::vector<uint8_t> previous;
    const auto old = ops.read(primary, session_save_max_bytes, previous, error);
    if (old == SessionFileReadResult::Error) return false;
    if (old == SessionFileReadResult::Ok) {
        SessionSnapshot checked;
        if (!decode_session_save(previous.data(), previous.size(), compatibility, checked, error)) {
            error = "Refusing to replace invalid/incompatible primary session save: " + error; return false;
        }
    }
    if (!ops.write_new(temporary, bytes, error)) return false;
    if (!verify_file(ops, temporary, bytes, error)) { cleanup(ops, temporary, error); return false; }
    if (old == SessionFileReadResult::Ok) {
        if (!ops.write_new(backup_temporary, previous, error)) { cleanup(ops, temporary, error); return false; }
        if (!verify_file(ops, backup_temporary, previous, error)) {
            cleanup(ops, backup_temporary, error); cleanup(ops, temporary, error); return false;
        }
        if (!ops.replace(backup_temporary, backup, error)) {
            cleanup(ops, backup_temporary, error); cleanup(ops, temporary, error); return false;
        }
        if (!verify_file(ops, backup, previous, error)) { cleanup(ops, temporary, error); return false; }
    }
    if (!ops.replace(temporary, primary, error)) {
        // Never move the backup to restore: even a failed restoration must leave
        // an intact previous save. Retain the new temporary if recovery fails.
        const bool recovered = old != SessionFileReadResult::Ok || restore_after_failed_commit(ops, primary, previous, error);
        if (recovered) cleanup(ops, temporary, error);
        return false;
    }
    error.clear(); return true;
}
bool read_session_save(const char* path, const SessionSaveCompatibility& compatibility,
                       SessionSnapshot& output, std::string& error, SessionSaveFileOps* operations) {
    if (!valid_path(path, error)) return false;
    LocalFileOps local; SessionSaveFileOps& ops = operations ? *operations : local;
    std::vector<uint8_t> bytes;
    const auto result = ops.read(path, session_save_max_bytes, bytes, error);
    if (result != SessionFileReadResult::Ok) {
        if (result == SessionFileReadResult::Missing) error = "Session save does not exist";
        return false;
    }
    return decode_session_save(bytes.data(), bytes.size(), compatibility, output, error);
}
} // namespace encore::upstream
