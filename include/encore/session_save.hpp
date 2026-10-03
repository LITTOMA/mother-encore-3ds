#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace encore::upstream {
// Native original-session format, independent of the M0 fixture and content packs.
// These are structural limits, not game rules or source-backed initial values.
constexpr uint32_t session_save_schema = 1;
constexpr size_t session_save_max_bytes = 1024 * 1024;
constexpr size_t session_save_max_string_bytes = 4096;
constexpr size_t session_save_max_entries = 4096;
constexpr size_t session_save_max_characters = 32;

struct SessionSaveCompatibility {
    uint32_t content_family = 0, content_revision = 0, rules_revision = 0;
};
enum class SessionRngPolicy : uint32_t { NotSerialized = 1 };
struct SessionFlag { std::string id; bool value = false; };
struct SessionIntegerValue { std::string id; int64_t value = 0; };
struct SessionRealValue { std::string id; double value = 0; };
struct SessionItem {
    std::string item_id;
    bool equipped = false;
    int64_t doses = 0;
    uint32_t uid = 0;
};
struct SessionStatus {
    std::string status_id;
    int64_t passive_healing_turns = 0;
};
struct SessionCharacter {
    std::string character_id, nickname;
    int64_t level = 0, experience = 0, hp = 0, pp = 0;
    std::vector<SessionIntegerValue> permanent_boosts;
    std::vector<SessionRealValue> affinity_multipliers;
    std::vector<std::string> learned_skills;
    std::vector<SessionStatus> status;
    std::vector<SessionItem> inventory;
};
struct SessionSettings {
    double text_speed = 0;
    std::string menu_flavor, button_prompts;
    bool description = false;
};
struct SessionSnapshot {
    std::string scene_id, scene_label;
    double position_x = 0, position_y = 0, direction_x = 0, direction_y = 0;
    std::string run_sound, shadow_effect;
    double playtime_seconds = 0;
    SessionSettings settings;
    std::string player_name, favorite_food;
    int64_t cash = 0, bank = 0, earned_cash = 0;
    std::vector<SessionCharacter> characters;
    std::vector<std::string> party;
    std::vector<SessionItem> key_items, storage;
    std::vector<SessionFlag> flags, object_flags, seen_dialogue_flags, encountered;
    std::vector<SessionIntegerValue> keys, rare_drops;
    std::string source_version, saved_at;
    bool is_debug = false;
    SessionRngPolicy rng_policy = SessionRngPolicy::NotSerialized;
};

// Structural validation only. The caller must validate every identity, source
// rule, supported scene/party/status and derived stat against loaded resources,
// then apply the entire candidate transactionally. No defaults are injected.
bool validate_session_snapshot(const SessionSnapshot&, std::string& error);
bool encode_session_save(const SessionSnapshot&, const SessionSaveCompatibility&,
                         std::vector<uint8_t>& output, std::string& error);
bool decode_session_save(const uint8_t*, size_t, const SessionSaveCompatibility& expected,
                         SessionSnapshot& output, std::string& error);

enum class SessionFileReadResult { Ok, Missing, Error };
// Injectable storage contract for deterministic failure tests/platform adapters.
// write_new must create exclusively, flush+close, and clean its own partial file
// on failure (or report failed cleanup). Never remove a pre-existing file.
// replace moves source over destination on success. Failure must preserve source
// bytes, but may remove destination (libctru rename has that behavior). It must
// never leave a partially copied destination. All methods are synchronous.
class SessionSaveFileOps {
public:
    virtual ~SessionSaveFileOps() = default;
    virtual SessionFileReadResult read(const std::string& path, size_t limit,
                                      std::vector<uint8_t>& output, std::string& error) = 0;
    virtual bool write_new(const std::string& path, const std::vector<uint8_t>&,
                           std::string& error) = 0;
    virtual bool replace(const std::string& from, const std::string& to,
                         std::string& error) = 0;
    virtual bool remove(const std::string& path, std::string& error) = 0;
};
// Local adapter creates no directories. .tmp/.bak.tmp must be absent. It verifies
// both temporary files before replacement, and refuses to replace an invalid old
// save. A successful overwrite retains the previous primary at path + ".bak".
// A failed final rename restores a missing primary when possible; otherwise the
// verified previous primary remains at .bak and the error identifies recovery.
// No FAT power-loss durability or concurrent-writer guarantee. Ordinary reads
// never silently substitute backups.
bool write_session_save(const char* path, const SessionSnapshot&,
                        const SessionSaveCompatibility&, std::string& error,
                        SessionSaveFileOps* operations = nullptr);
bool read_session_save(const char* path, const SessionSaveCompatibility&,
                       SessionSnapshot& output, std::string& error,
                       SessionSaveFileOps* operations = nullptr);
} // namespace encore::upstream
