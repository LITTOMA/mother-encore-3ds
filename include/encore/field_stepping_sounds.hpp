#pragma once
#include "encore/battle_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
struct FieldSteppingConnection {
  uint32_t role = 0;
  std::string signal, method;
};
struct FieldSteppingSound {
  std::string name, path;
};
struct FieldSteppingEffect {
  std::string name, scene, texture;
  uint32_t frames = 0;
  float fps = 0;
  bool behind = false;
};
struct FieldSteppingShape {
  uint32_t id = 0, order = 0;
  bool disabled = false;
  std::string node;
  std::vector<std::vector<Vec2>> parts;
};
struct FieldSteppingBinding {
  uint32_t id = 0, ready_ordinal = 0, layer = 0, mask = 0, flags = 0;
  bool enabled = false;
  std::string node, entering_sound, exiting_sound, enter_shadow_effect,
      exit_shadow_effect;
  std::vector<FieldSteppingShape> shapes;
};
class FieldSteppingSoundsData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const std::array<uint8_t, 20> &source_pin() const { return pin_; }
  const std::string &scene() const { return scene_; }
  const std::string &script() const { return script_; }
  const std::vector<FieldSteppingBinding> &bindings() const {
    return bindings_;
  }
  const std::vector<FieldSteppingConnection> &connections() const {
    return connections_;
  }
  const std::vector<FieldSteppingSound> &sounds() const { return sounds_; }
  const std::vector<FieldSteppingEffect> &effects() const { return effects_; }
  const FieldSteppingBinding *binding(uint32_t) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, script_;
  std::vector<FieldSteppingBinding> bindings_;
  std::vector<FieldSteppingConnection> connections_;
  std::vector<FieldSteppingSound> sounds_;
  std::vector<FieldSteppingEffect> effects_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct FieldSteppingBody {
  bool is_player = false, is_party = false;
};
struct FieldSteppingDispatch {
  uint32_t area = 0, body = 0;
  std::string sound, effect;
};
struct FieldSteppingGeometry {
  // Actual current CollisionObject shape registration, including
  // detached/reparented source polygons. Registered parts are world-space
  // engine convex pieces.
  bool attached = false, disabled = false;
  std::vector<std::vector<Vec2>> parts;
};
struct FieldSteppingSoundsHost {
  // Actual source Player runner/audio and PartyObject.Shadow AnimatedSprite
  // endpoints and existing checked assets must be admitted. No immediate audio
  // is played here: source run_sound is read by the actual running consumer.
  std::function<bool(const FieldSteppingSoundsData &, std::string &)> admit;
  std::function<bool(const FieldSteppingBinding &,
                     const std::vector<FieldSteppingConnection> &,
                     std::string &)>
      admit_ready;
  std::function<bool(uint32_t, FieldSteppingBody &, std::string &)>
      describe_body;
  // Preflight all needed endpoints before the ordered sound/shadow mutations.
  std::function<bool(const FieldSteppingDispatch &, std::string &)>
      admit_dispatch;
  std::function<bool(std::string_view, std::string &)> set_player_run_sound;
  std::function<bool(uint32_t, std::string_view, std::string &)>
      set_party_shadow;
  std::function<bool(uint32_t area, const FieldSteppingShape &,
                     FieldSteppingGeometry &, std::string &)>
      geometry;
};
struct FieldSteppingState {
  uint32_t id = 0;
  bool ready = false, alive = true, enabled = false;
};
class FieldSteppingSoundsRuntime {
public:
  bool initialize(const FieldSteppingSoundsData &, FieldSteppingSoundsHost,
                  std::string &);
  bool ready(uint32_t, std::string &);
  bool enable(uint32_t, std::string &);
  bool disable(uint32_t, std::string &);
  bool body_enter(uint32_t, uint32_t, std::string &);
  bool body_exit(uint32_t, uint32_t, std::string &);
  // For the real source physics backend; does not generate signals itself.
  // Body polygon must be the actual convex source body, not a ray point/AABB.
  bool overlaps(uint32_t, uint32_t body_layer, uint32_t body_mask,
                const std::vector<Vec2> &, bool &, std::string &);
  bool exit_tree(uint32_t, std::string &);
  const FieldSteppingState *state(uint32_t) const;
  const FieldSteppingSoundsData *content() const { return data_; }

private:
  FieldSteppingState *active(uint32_t, std::string &);
  bool dispatch(uint32_t, uint32_t, bool, std::string &);
  const FieldSteppingSoundsData *data_ = nullptr;
  FieldSteppingSoundsHost host_;
  std::vector<FieldSteppingState> states_;
  size_t ready_index_ = 0;
};
} // namespace encore::upstream
