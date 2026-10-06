#pragma once
#include "encore/player_initialization.hpp"
namespace encore::upstream {
enum class PlayerReadyBinding : size_t {
  PartyMember,
  Steps,
  TapRun,
  Crouch,
  Direction,
  LastStep,
  RunSound,
  Costume,
  PartySignal,
  UpdateMethod,
  StatusSignal,
  RefreshMethod,
  SweatEffect,
  ShadowAnimation,
  SweatPath,
  SpritePath,
  SpecialPath,
  CameraPath,
  TreePath,
  AnimationPath,
  NormalFormat,
  SpecialFormat,
  SnowFormat,
  FallbackTexture,
  SnowCostume,
  FootstepsFormat,
  FootstepsVoice,
  BlendFormat,
  Climbing,
  Count
};
struct PlayerGraphPoint {
  Vec2 position{};
  std::string clip;
  uint32_t node_id = 0, clip_id = 0;
  float length = 0;
  bool loop = false;
};
struct PlayerGraphState {
  std::string name, parameter, scale_parameter;
  Vec2 position{}, initial{};
  float scale = 1;
  std::vector<PlayerGraphPoint> points;
};
struct PlayerGraphEdge {
  std::string source, target;
  uint32_t mode = 0, priority = 0;
  bool auto_advance = false, disabled = false;
};
class PlayerReadyData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            std::string &);
  bool load_file(const char *, const PlayerInitializationData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const std::string &binding(PlayerReadyBinding b) const {
    return bindings_[size_t(b)];
  }
  const auto &states() const { return states_; }
  const auto &edges() const { return edges_; }
  const auto &blend_parameters() const { return blends_; }
  const auto &start() const { return start_; }
  const auto &fainted_prefix() const { return fainted_prefix_; }
  const auto &incapacitated_rules() const { return incap_rules_; }
  Vec2 direction() const { return direction_; }
  uint32_t party_space_size() const { return party_size_; }
  uint32_t height_divisor() const { return height_divisor_; }
  uint32_t height_offset() const { return height_offset_; }
  uint32_t playback_resource() const { return playback_; }
  uint32_t process_mode() const { return process_mode_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, initialization_ir_{};
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::array<std::string, size_t(PlayerReadyBinding::Count)> bindings_{};
  std::vector<PlayerGraphState> states_;
  std::vector<PlayerGraphEdge> edges_;
  std::vector<std::string> blends_;
  std::string start_, fainted_prefix_;
  std::vector<std::pair<std::string, std::string>> incap_rules_;
  Vec2 direction_{};
  uint32_t party_size_ = 0, height_divisor_ = 0, height_offset_ = 0,
           playback_ = 0, process_mode_ = 0;
};
// An actual AnimationPlayer owner must evaluate the original clip/track data
// from the same 0056 resource. This is not AnimationPlayer.play() or a second
// clock: the Tree's one internal process delta drives this graph.
struct PlayerGraphHost {
  std::function<bool(std::string &)> begin_frame, apply_frame;
  std::function<bool(const PlayerGraphPoint &, float time, float step,
                     bool seek, float weight, std::string &)>
      blend_clip;
};
class PlayerAnimationGraph {
public:
  bool initialize(const PlayerReadyData &, PlayerGraphHost, std::string &);
  bool set_active(bool, std::string &);
  bool set_blend(std::string_view parameter, Vec2, std::string &);
  bool set_scale(std::string_view parameter, float, std::string &);
  bool travel(std::string_view state, std::string &);
  bool start(std::string_view state, std::string &);
  bool stop(std::string &);
  bool process(float actual_delta, std::string &);
  const PlayerReadyData *data() const { return data_; }
  bool active() const { return active_; }
  bool playing() const { return playing_; }
  const auto &current() const { return current_; }
  const auto &travel_path() const { return path_; }
  float position() const { return position_; }
  float length() const { return length_; }

private:
  bool travel_route(std::string_view);
  bool evaluate(const std::string &, float, bool, float, float &,
                std::string &);
  bool process_graph(float, bool, std::string &);
  const PlayerGraphState *state(std::string_view) const;
  const PlayerReadyData *data_ = nullptr;
  PlayerGraphHost host_;
  bool active_ = false, started_ = false, playing_ = false, stop_ = false,
       request_travel_ = false, poisoned_ = false;
  std::string request_, current_;
  float position_ = 0, length_ = 0;
  std::vector<std::string> path_;
  std::map<std::string, Vec2> blends_;
  std::map<std::string, float> scales_;
  std::map<std::string, int> closest_;
  std::map<uint32_t, float> times_;
};
// These endpoints operate real native/script owners. Missing endpoints reject
// before Ready body mutation; no class-name or callback-presence Ready grant.
struct PlayerReadyHost {
  std::function<bool(const GlobalYamlValue &, FieldObjectId &, std::string &)>
      resolve_resource;
  std::function<bool(FieldObjectId, std::string_view, FieldObjectId,
                     std::string_view, bool &, std::string &)>
      connected;
  std::function<bool(FieldObjectId, std::string_view, FieldObjectId,
                     std::string_view, std::string &)>
      connect;
  std::function<bool(FieldObjectId, std::string_view, bool &, std::string &)>
      character_effect;
  std::function<bool(FieldObjectId, std::string &, std::string &)>
      character_sprite;
  std::function<bool(FieldObjectId, bool &, std::string &)>
      character_incapacitated;
  std::function<bool(FieldObjectId, bool, std::string &)> animation_active;
  std::function<bool(FieldObjectId, bool, std::string &)> sprite_playing;
  std::function<bool(FieldObjectId, uint32_t, std::string &)> sprite_frame;
  std::function<bool(std::string_view, bool &, std::string &)> resource_exists;
  std::function<bool(FieldObjectId, std::string &, std::string &)> texture_path;
  std::function<bool(FieldObjectId, std::string_view, std::string &)>
      load_texture;
  std::function<bool(FieldObjectId, uint32_t &, std::string &)> texture_height;
  std::function<bool(FieldObjectId, float, std::string &)> sprite_offset_y;
  std::function<bool(FieldObjectId, std::string_view, std::string &)>
      shadow_animation;
  std::function<bool(std::string_view, std::string_view, std::string &)>
      add_sfx;
};
class PlayerReadyRuntime {
public:
  bool initialize(const PlayerReadyData &, PlayerInitializationBody &,
                  FieldNodeTreeRuntime &, FieldGlobalConstructorRuntime &,
                  const FieldGlobalDataRuntime &, PlayerAnimationGraph &,
                  PlayerReadyHost, std::string &);
  // Run only at actual script Ready notification; the enclosing Tree is
  // responsible for native Ready, children and final ready signal.
  bool ready(FieldTreePhase, const FieldNodeBinding &, std::string &);
  bool update_party_member(std::string &);
  bool refresh_status(std::string &);
  bool blend_position(Vec2, std::string &);
  bool set_anim_state(std::string_view, std::string &);
  bool body_complete() const { return complete_ && !poisoned_; }

private:
  bool live(std::string &) const;
  bool onready(std::string_view, std::string &);
  bool node(PlayerReadyBinding, FieldObjectId &, std::string &) const;
  bool spritesheet(std::string &);
  bool assign(PlayerReadyBinding, uint32_t, bool, Vec2, FieldObjectId,
              std::string &);
  const PlayerReadyData *data_ = nullptr;
  PlayerInitializationBody *body_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalConstructorRuntime *global_ = nullptr;
  const FieldGlobalDataRuntime *characters_ = nullptr;
  PlayerAnimationGraph *animation_ = nullptr;
  PlayerReadyHost host_;
  bool started_ = false, complete_ = false, poisoned_ = false;
};
} // namespace encore::upstream
