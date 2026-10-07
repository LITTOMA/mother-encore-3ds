#pragma once
#include "encore/field_camera_arrows.hpp"
#include "encore/field_game_camera.hpp"
#include "encore/player_fetcher.hpp"
#include "encore/player_ready.hpp"
#include <set>
namespace encore::upstream {
struct PlayerChildScriptRecord {
  uint32_t role = 0, id = 0, ready = 0;
  std::string path, native_class, script;
  std::array<uint8_t, 32> script_sha{};
};
struct PlayerEmotePolicy {
  std::string object_path, animation_path, direction_method, direction_member,
      signal, method;
  std::vector<std::string> sensitive_clips;
  float padding = 0, negative_scale = 0, other_scale = 0;
};
struct PlayerTintPolicy {
  FieldColor color{};
  std::string signal, method;
  std::vector<std::string> paths;
};
struct PlayerCameraSourceConnection {
  uint32_t role=0; // 1 actual UiManager, 2 actual Player.
  std::string signal,method;
};
class PlayerChildScriptsData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            const PlayerReadyData &, std::string &);
  bool load_file(const char *, const PlayerInitializationData &,
                 const PlayerReadyData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &player_ir_sha256() const { return player_ir_; }
  const auto &records() const { return records_; }
  const PlayerChildScriptRecord *record(uint32_t role) const;
  const auto &emote() const { return emote_; }
  const auto &tint() const { return tint_; }
  const auto &camera() const { return camera_; }
  const auto &camera_connections()const{return camera_connections_;}
  const auto &arrows() const { return arrows_; }
  uint32_t camera_process_mode() const { return camera_process_mode_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, player_ir_{}, ready_ir_{};
  std::string scene_;
  std::vector<PlayerChildScriptRecord> records_;
  PlayerEmotePolicy emote_;
  PlayerTintPolicy tint_;
  std::vector<PlayerCameraSourceConnection> camera_connections_;
  uint32_t camera_process_mode_ = 0;
  FieldGameCameraData camera_;
  FieldCameraArrowsData arrows_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// All endpoints are concrete owners of these same actual Tree/ObjectDB nodes.
// connect performs the real source signal registration, never a Ready receipt.
class PlayerChildScriptNative {
public:
  virtual ~PlayerChildScriptNative() = default;
  virtual const FieldGlobalRegistry *registry() const = 0;
  virtual const FieldNodeTreeRuntime *tree() const = 0;
  virtual bool sprite(FieldObjectId, PlayerFetcherSpriteState &,
                      std::string &) const = 0;
  virtual bool texture_height(FieldObjectId, uint32_t &,
                              std::string &) const = 0;
  virtual bool connect_animation_started(
      FieldObjectId, FieldObjectId, std::string_view, std::string_view,
      std::function<bool(std::string_view, std::string &)>, std::string &) = 0;
  virtual bool emit_tint(FieldObjectId, std::string_view, const FieldColor &,
                         std::string &) = 0;
  virtual bool connect_tint(FieldObjectId, FieldObjectId, std::string_view,
                            std::string_view, std::string &) = 0;
};
// Source-only owned script state. Native clocks/frames stay with their one
// owner.
class PlayerChildScriptsRuntime {
public:
  bool prepare(const PlayerChildScriptsData &, const PlayerInitializationData &,
               const PlayerReadyData &, FieldNodeTreeRuntime &,
               FieldGlobalRegistry &, PlayerInitializationBody &,
               PlayerChildScriptNative &, std::string &);
  bool construct(FieldObjectId, const FieldNodeDescriptor &, std::string &);
  bool connect_packed_signals(std::string &);
  bool bind_camera(SourceRandom &, FieldGameCameraHost, std::string &);
  bool bind_arrows(FieldCameraArrowsHost, FieldCameraArrowsNativeOwner &,
                   std::string &);
  bool ready(FieldObjectId, FieldTreePhase, const FieldNodeBinding &,
             std::string &);
  bool animation_started(std::string_view, std::string &);
  bool set_bubble_offset(std::string &);
  bool set_tint(const FieldColor &, std::string &);
  bool connect_tint(PlayerChildScriptsRuntime &, std::string &);
  bool process(FieldObjectId, FieldTreePhase, float, bool tree_paused,
               std::string &);
  bool exit(FieldObjectId, std::string &);
  bool deleting(FieldObjectId, std::string &);
  bool rebind_tree(FieldNodeTreeRuntime &, std::string &);
  bool onready_complete(uint32_t role) const;
  FieldObjectId actual(uint32_t role) const;
  const FieldColor &current_tint() const { return tint_; }
  FieldCameraArrowsRuntime &arrows() { return arrows_; }
  FieldGameCameraRuntime &camera() { return camera_; }

private:
  bool live(FieldObjectId, std::string &) const;
  bool resolve(const PlayerChildScriptRecord &, FieldObjectId &,
               std::string &) const;
  bool tint_targets(std::string &);
  const PlayerChildScriptsData *data_ = nullptr;
  const PlayerInitializationData *player_data_ = nullptr;
  const PlayerReadyData *ready_data_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  PlayerInitializationBody *body_ = nullptr;
  PlayerChildScriptNative *native_ = nullptr;
  std::map<uint32_t, FieldObjectId> objects_;
  std::set<uint32_t> ready_roles_;
  FieldObjectId emote_object_ = 0, emote_animation_ = 0;
  std::vector<FieldObjectId> targets_;
  FieldColor tint_{};
  bool connected_ = false, camera_bound_ = false, arrows_bound_ = false,
       camera_created_ = false, arrows_created_ = false, poisoned_ = false;
  FieldCameraArrowsRuntime arrows_;
  FieldGameCameraRuntime camera_;
};
} // namespace encore::upstream
