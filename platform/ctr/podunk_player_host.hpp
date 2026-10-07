#pragma once
#include "encore/podunk_player_character.hpp"
#include "encore/podunk_player_kinematic.hpp"
#include "podunk_player_child_scripts.hpp"
#include "podunk_player_effects.hpp"
#include "podunk_player_playback.hpp"
#include "podunk_player_visual_bundle.hpp"
#include <set>

namespace encore::ctr {
// Actual physics/viewport owners for the dynamic recipe. These operations
// register native bodies and shapes with the same live world, not a source-ID
// receipt. AnimationTree, media, Sprite, Timer and script bodies are owned
// below.
class PodunkPlayerWorldNative {
public:
  virtual ~PodunkPlayerWorldNative() = default;
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual const upstream::FieldNodeTreeRuntime *tree() const = 0;
  virtual bool construct(upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         const upstream::PlayerInitializationData &,
                         std::string &) = 0;
  virtual bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
                     float delta, bool paused, std::string &) = 0;
  virtual bool disabled(upstream::FieldObjectId, bool, std::string &) = 0;
  virtual bool release(upstream::FieldObjectId, std::string &) = 0;
};
struct PodunkPlayerSources {
  std::shared_ptr<const upstream::PlayerInitializationData> initialization;
  std::shared_ptr<const upstream::PlayerReadyData> ready;
  std::shared_ptr<const upstream::PlayerMotionData> motion;
  std::shared_ptr<const upstream::PlayerVisualScriptsData> visual;
  std::shared_ptr<const upstream::PlayerGraphicsData> graphics;
  std::shared_ptr<const upstream::PlayerFetcherData> fetchers;
  std::shared_ptr<const upstream::PlayerChildScriptsData> children;
  std::shared_ptr<const upstream::PlayerEffectsData> effects;
  std::shared_ptr<const upstream::PlayerResourcesData> resources;
};
struct PodunkPlayerServices {
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldGlobalConstructorRuntime *global = nullptr;
  const upstream::FieldGlobalDataRuntime *characters = nullptr;
  const upstream::FieldCharacterLoadData *character_data = nullptr;
  const upstream::HouseStatusEffectsRuntime *statuses = nullptr;
  upstream::SourceRandom *random = nullptr;
  AudioPlayer *audio = nullptr;
  upstream::FieldMapSpace *map = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  PodunkPlayerWorldNative *world = nullptr;
  PodunkPlayerEffectOwners *effect_owners = nullptr;
  PodunkPlayerMediaHost audio_server;
  upstream::PodunkPlayerKinematicHost kinematic;
  // Actual global/Character signals, ResourceLoader's additional PackedScenes
  // and AudioManager methods; locally-owned native endpoints replace these.
  upstream::PlayerReadyHost ready;
  upstream::PlayerMotionHost motion;
  upstream::FieldGameCameraHost camera;
  upstream::FieldCameraArrowsHost arrows;
  std::function<bool(upstream::FieldObjectId, std::string_view,
                     const upstream::FieldColor &, std::string &)>
      emit_tint;
  std::function<bool(upstream::FieldObjectId, std::string_view,
                     std::string_view, std::string &)>
      emit_animation;
};
// One actual Player body in the existing currentScene tree. Root SceneHost
// forwards the factory/lifecycle callbacks into this owner. It creates no tree,
// ObjectDB, input loop, animation clock or gameplay substitute.
class PodunkPlayerHost final : public PodunkPlayerAnimationEndpoints,
                               public upstream::PlayerFetcherSpriteReader,
                               public upstream::PlayerChildScriptNative {
public:
  bool prepare(PodunkPlayerSources, PodunkPlayerServices,
               std::shared_ptr<upstream::FieldNodeTreeRuntime>, const char *,
               std::string &);
  bool instantiate(const upstream::PlayerInitializationData &,
                   std::shared_ptr<upstream::FieldNodeTreeRuntime> &,
                   upstream::FieldObjectId &, std::string &);
  // Native allocation has already been published by the shared SceneHost.
  bool construct_source(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool phase(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
             upstream::FieldTreePhase, std::string &);
  bool begin_frame(uint64_t epoch, float idle_delta, float physics_delta,
                   bool tree_paused, bool update_pending, std::string &);
  bool input(const upstream::PlayerInputEvent &, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool method_owned(const upstream::FieldDeferredMessage &) const;
  bool source_method(const upstream::FieldDeferredMessage &, std::string &);
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool draw(upstream::FieldObjectId, const upstream::FieldTransform &,
            bool pixel_snap, std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  bool owns(upstream::FieldObjectId) const;
  bool source_candidate(uint32_t) const;
  bool assembled() const { return assembled_ && !poisoned_; }
  bool ready_complete() const { return ready_.body_complete() && assembled(); }
  upstream::PlayerInitializationBody &body() { return body_; }
  PodunkPlayerAnimation &animations() { return animation_; }
  PodunkPlayerNativeMedia &media() { return media_; }
  PodunkPlayerResources &resources() { return resources_; }
  PodunkPlayerVisualBundle &visuals() { return visual_; }
  upstream::FieldNativeTimers &timers() { return timers_; }
  upstream::PlayerChildScriptsRuntime &children() { return children_; }
  upstream::PlayerMotionRuntime &motion() { return motion_; }
  PodunkPlayerEffectsHost &effects() { return effects_; }
  upstream::PodunkPlayerKinematic &kinematic() { return kinematic_; }
  const upstream::FieldGlobalRegistry *registry() const override;
  const upstream::FieldNodeTreeRuntime *tree() const override;
  bool read(upstream::FieldObjectId, upstream::PlayerFetcherSpriteState &,
            std::string &) const override;
  bool sprite(upstream::FieldObjectId, upstream::PlayerFetcherSpriteState &,
              std::string &) const override;
  bool texture_height(upstream::FieldObjectId, uint32_t &,
                      std::string &) const override;
  bool connect_animation_started(
      upstream::FieldObjectId, upstream::FieldObjectId, std::string_view,
      std::string_view, std::function<bool(std::string_view, std::string &)>,
      std::string &) override;
  bool emit_tint(upstream::FieldObjectId, std::string_view,
                 const upstream::FieldColor &, std::string &) override;
  bool connect_tint(upstream::FieldObjectId, upstream::FieldObjectId,
                    std::string_view, std::string_view, std::string &) override;
  bool admit(upstream::FieldObjectId, std::string_view, bool,
             std::string &) const override;
  bool disabled(upstream::FieldObjectId, bool, std::string &) override;
  bool audio_playing(upstream::FieldObjectId, bool, std::string &) override;
  bool audio_stream(upstream::FieldObjectId, uint32_t, std::string &) override;
  bool animated_frame(upstream::FieldObjectId, uint32_t,
                      std::string &) override;
  bool animated_playing(upstream::FieldObjectId, bool, std::string &) override;
  bool native_offset(upstream::FieldObjectId, upstream::Vec2,
                     std::string &) override;
  bool shader_number(upstream::FieldObjectId, std::string_view, double,
                     std::string &) override;
  bool shader_color(upstream::FieldObjectId, std::string_view,
                    upstream::FieldColor, std::string &) override;
  PodunkPlayerVisualNative *visual(upstream::FieldObjectId) override;
  bool texture(uint32_t, upstream::FieldObjectId &, std::string &) override;
  bool material(uint32_t, upstream::FieldObjectId &, std::string &) override;
  bool stream(uint32_t, upstream::FieldObjectId &, std::string &) override;
  bool signal(upstream::FieldObjectId, std::string_view, std::string_view,
              std::string &) override;

private:
  struct Connection {
    upstream::FieldObjectId emitter = 0, receiver = 0;
    std::string signal, method;
    std::function<bool(std::string_view, std::string &)> callback;
  };
  bool live(std::string &) const;
  bool actual(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool finish_factory(std::string &);
  bool script_phase(upstream::FieldObjectId, upstream::FieldTreePhase,
                    const upstream::FieldNodeBinding &, std::string &);
  bool timeout(upstream::FieldObjectId, std::string &);
  bool effect_timer(upstream::FieldObjectId) const;
  bool resolve_onready(const upstream::GlobalYamlValue &,
                       upstream::FieldObjectId &, std::string &);
  bool native_world(std::string_view) const;
  PodunkPlayerSources sources_;
  PodunkPlayerServices services_;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_;
  upstream::PlayerInitializationBody body_;
  PodunkPlayerResources resources_;
  PodunkPlayerVisualBundle visual_;
  PodunkPlayerNativeMedia media_;
  PodunkPlayerAnimation animation_;
  PodunkPlayerPlayback *playback_ = nullptr;
  upstream::FieldObjectId playback_id_ = 0;
  upstream::PlayerReadyRuntime ready_;
  upstream::PodunkPlayerCharacter character_;
  upstream::PlayerMotionRuntime motion_;
  upstream::PodunkPlayerKinematic kinematic_;
  upstream::FieldNativeTimerData timer_data_;
  upstream::FieldNativeTimers timers_;
  upstream::PlayerChildScriptsRuntime children_;
  PodunkPlayerArrowNative arrow_native_;
  PodunkPlayerEffectsHost effects_;
  std::map<uint32_t, std::unique_ptr<upstream::PlayerFetcherRuntime>> fetchers_;
  std::map<upstream::FieldObjectId, uint32_t> objects_;
  std::set<upstream::FieldObjectId> constructed_, native_ready_;
  std::vector<Connection> connections_;
  const upstream::PlayerInputEvent *input_event_ = nullptr;
  uint64_t epoch_ = 0;
  float idle_delta_ = 0, physics_delta_ = 0;
  bool prepared_ = false, building_ = false, assembled_ = false,
       poisoned_ = false, paused_ = false, update_pending_ = false;
};
} // namespace encore::ctr
