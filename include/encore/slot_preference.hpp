#pragma once
#include "encore/session_save.hpp"

namespace encore::upstream {
// Native selection metadata, independent of game snapshots and content packs.
// Zero means no selected slot (the source globalData.save_file initial state),
// not a fallback game or an existing save. max_slots comes from loaded data.
constexpr uint32_t slot_preference_schema = 1;
constexpr size_t slot_preference_bytes = 20;
constexpr uint32_t slot_preference_none = 0;
enum class SlotPreferenceReadResult { Ok, Missing, Error };

bool encode_slot_preference(uint32_t max_slots, uint32_t slot,
                            std::vector<uint8_t>& output, std::string& error);
bool decode_slot_preference(const uint8_t*, size_t, uint32_t max_slots,
                            uint32_t& output, std::string& error);
// Missing and Error leave output untouched. Missing clears error. Reads never
// substitute a backup or inspect game save files; the caller handles default 0.
SlotPreferenceReadResult read_slot_preference(const char* path, uint32_t max_slots,
                                             uint32_t& output, std::string& error,
                                             SessionSaveFileOps* operations = nullptr);
// Accepts 1..max_slots, or explicit 0 to reset selection. Creates no directories.
// Refuses an invalid existing preference. Verified old metadata is kept at .bak
// before commit, and restored when possible if rename removes its destination.
// Stale .tmp/.bak.tmp files are never overwritten. No power-loss durability or
// concurrent-writer guarantee; game files are never opened or modified here.
bool write_slot_preference(const char* path, uint32_t max_slots, uint32_t slot,
                           std::string& error, SessionSaveFileOps* operations = nullptr);
} // namespace encore::upstream
