#pragma once
#include "encore/player_ready.hpp"
#include "encore/house_return_ladder.hpp"
#include "encore/player_effects.hpp"
#include "podunk_player_visual_native.hpp"

namespace encore::ctr {
// Endpoints are concrete, same-ObjectDB native owners. Preflight must examine
// the actual target/member/method; a callback's presence is not admission.
class PodunkPlayerAnimationEndpoints {
public:
  virtual ~PodunkPlayerAnimationEndpoints() = default;
  virtual bool admit(upstream::FieldObjectId, std::string_view member,
                     bool method, std::string &) const = 0;
  virtual bool disabled(upstream::FieldObjectId, bool, std::string &) = 0;
  virtual bool audio_playing(upstream::FieldObjectId, bool, std::string &) = 0;
  virtual bool audio_stream(upstream::FieldObjectId, uint32_t source_resource,
                            std::string &) = 0;
  virtual bool animated_frame(upstream::FieldObjectId, uint32_t,
                              std::string &) = 0;
  virtual bool animated_playing(upstream::FieldObjectId, bool,
                                std::string &) = 0;
  virtual bool native_offset(upstream::FieldObjectId, upstream::Vec2,
                             std::string &) = 0;
  virtual bool shader_number(upstream::FieldObjectId actual_sprite,
                             std::string_view parameter, double,
                             std::string &) = 0;
  virtual bool shader_color(upstream::FieldObjectId actual_sprite,
                            std::string_view parameter, upstream::FieldColor,
                            std::string &) = 0;
  virtual PodunkPlayerVisualNative *visual(upstream::FieldObjectId) = 0;
  virtual bool texture(uint32_t source_resource,
                       upstream::FieldObjectId &actual_resource,
                       std::string &) = 0;
  virtual bool material(uint32_t source_resource,
                        upstream::FieldObjectId &actual_resource,
                        std::string &) = 0;
  virtual bool stream(uint32_t source_resource,
                      upstream::FieldObjectId &actual_resource,
                      std::string &) = 0;
  virtual bool signal(upstream::FieldObjectId, std::string_view,
                      std::string_view clip, std::string &) = 0;
};
struct PodunkPlayerSpriteState {
  upstream::FieldObjectId object = 0, texture = 0, material = 0;
  uint32_t texture_source = 0, material_source = 0, frame = 0, columns = 0,
           rows = 0;
  upstream::Vec2 offset{};
  bool centered = false, flip_h = false, flip_v = false;
};
// Owns ordinary Sprite native properties, and the source AnimationPlayer's
// actual track caches. Canvas values remain in the one actual SceneTree.
// Shadow/Bat retain their existing native owners; they are never duplicated.
class PodunkPlayerAnimation : public upstream::PlayerLadderAnimationNative {
public:
  const upstream::FieldGlobalRegistry *ladder_registry() const override { return registry_; }
  const upstream::FieldNodeTreeRuntime *ladder_tree() const override { return tree_; }
  bool ladder_animation(upstream::FieldObjectId, std::string_view, std::string &) const override;
  bool ladder_play(upstream::FieldObjectId id, std::string_view clip, std::string &e) override { return play(id, clip, e); }
  bool ladder_stop(upstream::FieldObjectId id, std::string &e) override { return stop(id, e); }
  bool ladder_speed(upstream::FieldObjectId, double, std::string &) override;
  bool playback_speed(upstream::FieldObjectId, double, std::string &);
  bool rebind_tree(upstream::FieldNodeTreeRuntime &, std::string &);
  bool construct(const upstream::PlayerInitializationData &,
                 const upstream::PlayerReadyData &,
                 upstream::FieldNodeTreeRuntime &,
                 upstream::FieldGlobalRegistry &,
                 upstream::FieldObjectId actual_player,
                 PodunkPlayerAnimationEndpoints &, std::string &);
  bool construct_effect(const upstream::PlayerEffectsData&,uint32_t kind,
      upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
      upstream::FieldObjectId root,upstream::FieldObjectId animation,
      PodunkPlayerAnimationEndpoints&,std::string&);
  upstream::PlayerGraphHost graph_host();
  bool ready(upstream::FieldTreePhase, const upstream::FieldNodeBinding &,
             std::string &);
  bool play(std::string_view, std::string &);
  bool play(std::string_view, float custom_speed, bool from_end, std::string &);
  bool stop(std::string &);
  bool stop(bool reset,std::string &);
  bool advance(float actual_delta, bool tree_paused, std::string &);
  bool process(upstream::FieldTreePhase, float actual_delta, bool tree_paused,
               std::string &);
  bool ready(upstream::FieldObjectId animation, upstream::FieldTreePhase,
             const upstream::FieldNodeBinding &, std::string &);
  bool play(upstream::FieldObjectId animation, std::string_view, std::string &);
  bool play(upstream::FieldObjectId animation, std::string_view,
            float custom_speed, bool from_end, std::string &);
  bool assigned(upstream::FieldObjectId animation, std::string &,
                std::string &) const;
  bool stop(upstream::FieldObjectId animation, std::string &);
  bool stop(upstream::FieldObjectId animation,bool reset,std::string &);
  bool playback_snapshot(upstream::FieldObjectId,std::string &assigned,bool &playing,float &position,float &length,std::string&)const;
  bool process(upstream::FieldObjectId animation, upstream::FieldTreePhase,
               float actual_delta, bool tree_paused, std::string &);
  std::vector<upstream::FieldObjectId> animation_objects() const;
  bool construct_native_sprite(const PodunkPlayerSpriteState&,std::string&);
  bool release_effect(upstream::FieldObjectId animation,std::string&);
  bool sprite_frame(upstream::FieldObjectId, uint32_t, std::string &);
  bool sprite_texture(upstream::FieldObjectId, uint32_t, std::string &);
  bool sprite_offset(upstream::FieldObjectId, upstream::Vec2, std::string &);
  const PodunkPlayerSpriteState *sprite(upstream::FieldObjectId) const;
  upstream::FieldObjectId animation_object() const { return animation_; }
  bool playing() const { return playing_; }
  float position() const { return position_; }

private:
  struct Value {
    uint32_t kind = 0, resource = 0;
    double number = 0;
    bool boolean = false;
    upstream::Vec2 vector{};
    upstream::FieldColor color{};
    std::string string;
  };
  struct Key {
    float time = 0, transition = 1;
    Value value;
  };
  struct Track {
    uint32_t index = 0, update = 0, interpolation = 0;
    bool method = false, wrap = false, enabled = false;
    upstream::FieldObjectId target = 0;
    std::string member, source_path;
    std::vector<Key> keys;
  };
  struct Clip {
    uint32_t resource = 0;
    float length = 0;
    bool loop = false;
    std::vector<Track> tracks;
  };
  bool check_rebind_tree(const upstream::FieldNodeTreeRuntime &, std::string &) const;
  void commit_rebind_tree(upstream::FieldNodeTreeRuntime &);
  bool live(std::string &) const;
  bool construct_one(const upstream::PlayerInitializationData &,
                     const upstream::PlayerReadyData &,
                     upstream::FieldNodeTreeRuntime &,
                     upstream::FieldGlobalRegistry &,
                     upstream::FieldObjectId player,
                     upstream::FieldObjectId animation, bool main,
                     PodunkPlayerAnimationEndpoints &, std::string &);
  bool construct_snapshot(const upstream::PlayerInitializationData*,
      const upstream::PlayerReadyData*,const upstream::PlayerEffectsData*,uint32_t,
      const upstream::FieldNodeRecipeData&,std::shared_ptr<const upstream::GlobalYamlValue>,
      upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
      upstream::FieldObjectId,upstream::FieldObjectId,bool,
      PodunkPlayerAnimationEndpoints&,std::string&);
  PodunkPlayerAnimation *for_animation(upstream::FieldObjectId);
  PodunkPlayerAnimation *sprite_owner_ = nullptr;
  bool checked_resource(uint32_t source_id, upstream::FieldObjectId actual_id,
                        bool texture, std::string &) const;
  bool parse_value(const std::shared_ptr<const upstream::GlobalYamlValue> &,
                   Value &, std::string &) const;
  bool begin(std::string &);
  bool blend(const upstream::PlayerGraphPoint &, float, float, bool, float,
             std::string &);
  bool apply(std::string &);
  bool evaluate(const Clip &, float, float, bool, bool, float, std::string &);
  bool assign(const Track &, const Value &, std::string &);
  bool sample(const Clip &, const Track &, float, Value &, bool &,
              std::string &) const;
  std::vector<size_t> events(const Clip &, const Track &, float, float) const;
  const upstream::PlayerEffectsData *effects_data_ = nullptr;
  uint32_t effect_kind_ = 0;
  const upstream::PlayerInitializationData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkPlayerAnimationEndpoints *endpoints_ = nullptr;
  upstream::FieldObjectId player_ = 0, animation_ = 0;
  std::map<std::string, Clip> clips_;
  std::map<upstream::FieldObjectId, PodunkPlayerSpriteState> sprites_;
  std::map<upstream::FieldObjectId, std::unique_ptr<PodunkPlayerAnimation>>
      children_;
  std::map<std::pair<upstream::FieldObjectId, std::string>,
           std::pair<const Track *, Value>>
      pending_;
  std::vector<std::pair<upstream::FieldObjectId, std::string>> cache_order_;
  std::string autoplay_, current_;
  float position_ = 0, speed_ = 1, custom_speed_ = 1;
  uint32_t process_mode_ = 0;
  bool playing_ = false, frame_open_ = false, ready_ = false, poisoned_ = false;
};
} // namespace encore::ctr
