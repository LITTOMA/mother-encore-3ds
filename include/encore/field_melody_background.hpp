#pragma once
#include "encore/battle_data.hpp"
#include <array>
#include <functional>
#include <map>
namespace encore::upstream {
struct FieldMelodyAsset {
  std::string source, path;
  uint32_t tile_width = 0, tile_height = 0, width = 0, height = 0, bytes = 0,
           crc = 0;
};
struct FieldMelodyKey {
  float time = 0;
  BattleValue color{};
};
struct FieldMelodyFade {
  float duration = 0;
  BattleValue from{}, to{};
  bool explicit_from = false;
};
struct FieldMelodyBinding {
  uint32_t id = 0, ready_ordinal = 0, bg_id = 0, bg_ready = 0, animation_id = 0,
           animation_ready = 0;
  std::string node;
  Vec2 position{};
  BattleValue initial_modulate{}, initial_self{};
};
struct FieldMelodyPolicy {
  Vec2 source_viewport{}, native_viewport{};
  BattleValue rect{};
  float ready_alpha = 0;
  std::array<float, 7> vertical{};
  std::string clip;
  float length = 0;
  std::vector<FieldMelodyKey> keys;
  FieldMelodyFade fade_in, fade_out;
};
class FieldMelodyBackgroundData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const std::array<uint8_t, 20> &source_pin() const { return pin_; }
  const std::string &scene() const { return scene_; }
  const std::string &script() const { return script_; }
  const std::string &shader() const { return shader_; }
  const FieldMelodyPolicy &policy() const { return policy_; }
  const FieldMelodyAsset &asset() const { return asset_; }
  const std::vector<FieldMelodyBinding> &bindings() const { return bindings_; }
  const FieldMelodyBinding *binding(uint32_t) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, script_, shader_;
  FieldMelodyPolicy policy_;
  FieldMelodyAsset asset_;
  std::vector<FieldMelodyBinding> bindings_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
using FieldMelodyObject = uint64_t;
struct FieldMelodyActorEntry {
  std::string key;
  FieldMelodyObject object = 0;
};
struct FieldMelodyActor {
  bool alive = false, has_position = false, has_active = false;
  FieldMelodyObject parent = 0;
  Vec2 position{};
};
struct FieldMelodyMoved {
  FieldMelodyObject object = 0, parent = 0;
};
enum class FieldMelodyColorRole : uint32_t { Modulate = 1, SelfModulate = 2 };
enum class FieldMelodyTweenSignal : uint32_t {
  PropertyFinished = 1,
  StepFinished = 2,
  Finished = 3
};
struct FieldMelodyTweenSpec {
  uint32_t owner = 0, bg = 0, role = 0;
  FieldMelodyFade fade;
};
struct FieldMelodyBackgroundHost {
  std::function<bool(const FieldMelodyBinding &, const FieldMelodyPolicy &,
                     const FieldMelodyAsset &, std::string &)>
      admit_ready;
  // Pure preflight proves actual YSort/Camera/actor/active-setter/tree and
  // source signal closures, including aliases, dynamic parent and real tween
  // capacity.
  std::function<bool(const FieldMelodyBinding &, bool appear, std::string &)>
      admit_call;
  std::function<bool(Vec2 &, std::string &)> camera_screen_center;
  std::function<bool(uint32_t, FieldMelodyObject &, std::string &)> root_object;
  std::function<bool(uint32_t, Vec2, std::string &)> set_global_position;
  std::function<bool(uint32_t, Vec2 &, std::string &)> local_position;
  // Actual UI dictionary insertion order. Never sort keys or deduplicate
  // actors.
  std::function<bool(std::vector<FieldMelodyActorEntry> &, std::string &)>
      dialogue_actors;
  std::function<bool(FieldMelodyObject &, std::string &)> talker;
  std::function<bool(std::string_view, std::string &)> print_actor_key;
  std::function<bool(FieldMelodyObject, FieldMelodyActor &, std::string &)>
      describe_actor;
  std::function<bool(FieldMelodyObject, Vec2, std::string &)>
      set_actor_position;
  std::function<bool(FieldMelodyObject, bool, std::string &)> set_actor_active;
  // Source remove_child/add_child may emit known parent mismatch diagnostics
  // and have no effect; report these as known source success, not fake reparent
  // or generic ignored failure. Unknown/incomplete typed operation returns
  // false.
  std::function<bool(FieldMelodyObject parent, FieldMelodyObject child,
                     std::string &)>
      remove_child, add_child;
  std::function<bool(uint32_t, bool, std::string &)> set_visible;
  std::function<bool(uint32_t, FieldMelodyColorRole, BattleValue,
                     std::string &)>
      write_color;
  std::function<bool(uint32_t, std::string_view, std::string &)>
      animation_started;
  std::function<bool(uint32_t, std::string &)> animation_stopped;
  // Actual scene-global bound-node Tween insertion token. Root drives this
  // consumer per token, in the real post-AnimationPlayer idle Tween pass.
  // Tokens created during that pass are excluded until its next source pass.
  std::function<bool(const FieldMelodyTweenSpec &, uint64_t &, std::string &)>
      create_tween;
  std::function<bool(uint64_t, FieldMelodyTweenSignal, std::string &)>
      tween_signal;
};
struct FieldMelodyState {
  uint32_t id = 0;
  FieldMelodyObject object = 0;
  bool ready = false, alive = true, attached = true, visible = false,
       playing = false, assigned = false;
  float animation_time = 0;
  BattleValue modulate{}, self_modulate{};
  std::vector<FieldMelodyMoved> moved;
};
struct FieldMelodyTween {
  uint64_t token = 0, last_frame = 0;
  uint32_t owner = 0, role = 0;
  bool started = false;
  float elapsed = 0;
  BattleValue from{};
  FieldMelodyFade fade;
};
class FieldMelodyBackgroundRuntime {
public:
  bool initialize(const FieldMelodyBackgroundData &, FieldMelodyBackgroundHost,
                  std::string &);
  bool ready(uint32_t, std::string &);
  bool appear(uint32_t, std::string &);
  bool disappear(uint32_t, std::string &);
  bool animation_idle(uint32_t, float source_delta, bool tree_can_process,
                      std::string &);
  bool tween_step(uint64_t, float source_delta, uint64_t source_frame,
                  bool bound_can_process, std::string &);
  bool exit_tree(uint32_t, std::string &);
  bool enter_tree(uint32_t, std::string &);
  bool free_instance(uint32_t, std::string &);
  const FieldMelodyState *state(uint32_t) const;
  const std::vector<FieldMelodyTween> &tweens() const { return tweens_; }
  const FieldMelodyBackgroundData *content() const { return data_; }

private:
  FieldMelodyState *active(uint32_t, std::string &);
  bool move_actor(FieldMelodyState &, FieldMelodyObject, bool require_position,
                  std::string &);
  bool add_tween(uint32_t, uint32_t, std::string &);
  bool restore(FieldMelodyState &, std::string &);
  const FieldMelodyBackgroundData *data_ = nullptr;
  FieldMelodyBackgroundHost host_;
  std::vector<FieldMelodyState> states_;
  std::vector<FieldMelodyTween> tweens_;
  size_t ready_index_ = 0;
  uint64_t last_token_ = 0;
};
} // namespace encore::upstream
