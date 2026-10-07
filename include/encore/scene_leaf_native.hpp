#pragma once
#include "encore/field_birds.hpp"
#include "encore/field_dropped.hpp"
#include "encore/field_game_camera.hpp"
#include "encore/field_melody_background.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/field_payphone.hpp"
#include "encore/field_player_transitions.hpp"
namespace encore::upstream {
enum class SceneLeafKind : uint32_t {
  CameraAnimation = 1,
  JumpAnimation,
  BirdAnimation,
  DroppedAnimation,
  PayphoneAnimation,
  EmptyAnimation,
  MelodyAnimation,
  DroppedTween,
  GameCamera,
  IntroCamera,
  EditorReferenceRect,
  MelodyRect
};
struct SceneLeafClip {
  std::string name;
  float length = 0;
  bool loop = false;
};
struct SceneLeafCamera {
  Vec2 offset{}, zoom{};
  uint32_t anchor = 0, process = 0;
  bool rotating = false, current = false;
  std::array<int32_t, 4> limits{};
};
struct SceneLeafNativeRecord {
  uint32_t id = 0, owner = 0, parent = 0, target = 0, method = 0;
  SceneLeafKind kind{};
  float speed = 0;
  std::string path, native_class, active_clip;
  std::vector<SceneLeafClip> clips;
  SceneLeafCamera camera{};
  std::array<float, 4> rect{};
  bool editor_only = false;
  const SceneLeafClip *clip(std::string_view name) const;
};
struct SceneLeafNativeSources {
  const FieldNodeTreeData &tree;
  const FieldGameCameraData &camera;
  const FieldPlayerTransitionsData &transitions;
  const FieldBirdData &birds;
  const FieldDroppedData &dropped;
  const FieldPayphoneData &payphone;
  const FieldMelodyBackgroundData &melody;
};
class SceneLeafNativeData {
public:
  bool load(const uint8_t *, size_t, const SceneLeafNativeSources &,
            std::string &);
  bool load_file(const char *, const SceneLeafNativeSources &, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const std::vector<SceneLeafNativeRecord> &records() const { return records_; }
  const SceneLeafNativeRecord *record(uint32_t) const;
  const SceneLeafNativeRecord *owner(SceneLeafKind, uint32_t) const;
  const std::string &internal_group() const { return symbols_[0]; }
  const std::string &started_signal() const { return symbols_[1]; }
  const std::string &finished_signal() const { return symbols_[2]; }
  const std::string &viewport_size_signal() const { return symbols_[3]; }
  const std::string &camera_scroll_method() const { return symbols_[4]; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<std::string, 5> symbols_;
  std::vector<SceneLeafNativeRecord> records_;
};
} // namespace encore::upstream
