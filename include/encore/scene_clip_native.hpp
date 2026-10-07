#pragma once
#include "encore/field_dead_bush.hpp"
#include "encore/field_emotes.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/field_openable_door.hpp"
#include "encore/field_present.hpp"
namespace encore::upstream {
enum class SceneClipOwner : uint32_t { Openable = 1, Present, Emote, Bush };
struct SceneClipNativeRecord {
  uint32_t id = 0, owner = 0, parent = 0, timer = 0, frame_target = 0;
  SceneClipOwner kind{};
  std::string path, timer_method;
};
// Native playback metadata supplements existing typed clips, never duplicates
// their tracks or owns another playback clock.
class SceneClipNativeData {
public:
  bool load(const uint8_t *, size_t, const FieldNodeTreeData &,
            const FieldOpenableDoorData &, const FieldPresentData &,
            const FieldEmoteData &, const FieldBushData &, std::string &);
  bool load_file(const char *, const FieldNodeTreeData &,
                 const FieldOpenableDoorData &, const FieldPresentData &,
                 const FieldEmoteData &, const FieldBushData &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::vector<SceneClipNativeRecord> &records() const { return records_; }
  const SceneClipNativeRecord *record(uint32_t) const;
  const SceneClipNativeRecord *owner(SceneClipOwner, uint32_t) const;
  const std::string &internal_group() const { return symbols_[0]; }
  const std::string &started_signal() const { return symbols_[1]; }
  const std::string &finished_signal() const { return symbols_[2]; }
  const std::string &timeout_signal() const { return symbols_[3]; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<std::string, 4> symbols_;
  std::vector<SceneClipNativeRecord> records_;
};
} // namespace encore::upstream
