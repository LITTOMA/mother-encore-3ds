#pragma once
#include "encore/player_initialization.hpp"
namespace encore::upstream {
struct PlayerVisualFrame {
  uint32_t resource = 0, texture = 0;
  std::string source;
  std::array<uint8_t, 32> source_sha{};
  std::array<float, 4> rect{};
};
struct PlayerVisualAnimation {
  std::string name;
  double speed = 0;
  bool loop = false;
  std::vector<PlayerVisualFrame> frames;
};
struct PlayerVisualShadow {
  uint32_t id = 0, frames_resource = 0;
  std::string script, start_member, start_default, setter, front_member, start;
  std::array<uint8_t, 32> script_sha{};
  std::vector<std::string> front;
  std::vector<PlayerVisualAnimation> animations;
};
struct PlayerVisualBat {
  uint32_t id = 0, fetcher = 0, target = 0, columns = 0, rows = 0,
           initial_frame = 0, texture = 0;
  std::string script, special_member, special_path, action_getter,
      action_member, visible_action, fetcher_getter, frame_member,
      fetcher_script, fetcher_path, texture_source;
  std::array<uint8_t, 32> script_sha{}, fetcher_sha{}, texture_sha{};
};
class PlayerVisualScriptsData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            std::string &);
  bool load_file(const char *, const PlayerInitializationData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &player_ir_sha256() const { return player_ir_; }
  const auto &shadow() const { return shadow_; }
  const auto &bat() const { return bat_; }
  uint32_t player_id() const { return player_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, player_ir_{}, player_script_sha_{};
  uint32_t player_ = 0;
  std::string scene_, player_script_;
  PlayerVisualShadow shadow_;
  PlayerVisualBat bat_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// These are live native Sprite/AnimatedSprite owners, not setter callbacks or
// source-ID stand-ins. They must expose the same ObjectDB and actual Tree node.
struct PlayerVisualNativeState {
  // native_ready means this actual Node's Ready notification cursor is reached
  // (inside && ready_notified && !ready_first), before its source ReadyScript.
  // It does not require the later ReadyNative host phase to have run.
  bool constructed = false, native_ready = false, visible = false,
       behind_parent = false;
  uint32_t frame = 0, columns = 0, rows = 0, texture = 0, frames_resource = 0;
  bool playing = false;
  std::string animation;
};
class PlayerVisualNativeOwner {
public:
  virtual ~PlayerVisualNativeOwner() = default;
  virtual const FieldGlobalRegistry *registry() const = 0;
  virtual const FieldNodeTreeRuntime *tree() const = 0;
  virtual FieldObjectId object() const = 0;
  virtual bool state(PlayerVisualNativeState &, std::string &) const = 0;
  // Requires the actual source SpriteFrames Resource, no animation receipts.
  virtual bool sprite_frames(const std::vector<PlayerVisualAnimation> *&,
                             std::string &) const = 0;
  virtual bool play(std::string_view, std::string &) = 0;
  virtual bool set_behind_parent(bool, std::string &) = 0;
  virtual bool set_visible(bool, std::string &) = 0;
  virtual bool set_frame(uint32_t, std::string &) = 0;
};
class PlayerVisualFetcherOwner {
public:
  virtual ~PlayerVisualFetcherOwner() = default;
  virtual const FieldGlobalRegistry *registry() const = 0;
  virtual const FieldNodeTreeRuntime *tree() const = 0;
  virtual FieldObjectId object() const = 0;
  virtual bool onready_complete() const = 0;
  virtual bool sprite_object(FieldObjectId &, std::string &) const = 0;
  virtual bool frame(uint32_t &, std::string &) const = 0;
};
struct PlayerVisualScriptState {
  bool constructed = false, export_applied = false, onready_complete = false;
  std::string start_anim;
  std::vector<std::string> front_anims;
  FieldObjectId special = 0;
};
class PlayerVisualScriptsRuntime {
public:
  bool initialize(const PlayerVisualScriptsData &,
                  const PlayerInitializationData &, FieldNodeTreeRuntime &,
                  FieldGlobalRegistry &, FieldGlobalConstructorRuntime &,
                  PlayerInitializationBody &, PlayerVisualNativeOwner &shadow,
                  PlayerVisualNativeOwner &bat, PlayerVisualFetcherOwner &,
                  std::string &);
  // Run at the actual script attachment, before source setget properties.
  bool construct(FieldObjectId, const FieldNodeDescriptor &, std::string &);
  // The source PackedScene calls this AFTER script attachment. It is not Ready.
  bool apply_shadow_export(FieldObjectId, std::string_view, std::string &);
  bool set_shadow_anim(FieldObjectId, std::string_view, std::string &);
  bool ready(FieldObjectId, std::string &);
  bool process(FieldObjectId, std::string &);
  const PlayerVisualScriptState *state(FieldObjectId) const;

private:
  bool node(FieldObjectId, uint32_t, std::string_view,
            const std::array<uint8_t, 32> &, std::string_view, bool inside,
            std::string &) const;
  bool native(PlayerVisualNativeOwner &, uint32_t, std::string_view, bool ready,
              PlayerVisualNativeState &, std::string &) const;
  const PlayerVisualScriptsData *data_ = nullptr;
  const PlayerInitializationData *player_data_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldGlobalConstructorRuntime *global_ = nullptr;
  PlayerInitializationBody *player_ = nullptr;
  PlayerVisualNativeOwner *shadow_ = nullptr, *bat_ = nullptr;
  PlayerVisualFetcherOwner *fetcher_ = nullptr;
  std::map<FieldObjectId, PlayerVisualScriptState> states_;
};
} // namespace encore::upstream
