#pragma once
#include "field_dialogue_ui_renderer.hpp"
#include "field_dialogue_visual_renderer.hpp"
#include "podunk_audio_server.hpp"
#include "podunk_dialogue_host.hpp"
#include "podunk_named_sfx.hpp"
#include "podunk_player_physics_world.hpp"
#include "podunk_scene_cameras.hpp"

namespace encore::ctr {
struct PodunkDialogueNativeInput {
  const upstream::FieldNodeRecipeData *recipe = nullptr;
  const upstream::FieldDialogueUiData *ui = nullptr;
  const upstream::FieldDialogueVisualData *visual = nullptr;
  const upstream::FieldDialogueAudioData *audio = nullptr;
  const upstream::FieldNativeTimerData *timer_data = nullptr;
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  upstream::FieldNativeTimers *timers = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  PodunkPlayerPhysicsWorld *physics = nullptr;
  upstream::FieldGlobalConstructorRuntime *global = nullptr;
  PodunkNativeRoot *root = nullptr;
  PodunkSceneCameras *cameras = nullptr;
  PodunkPlayerCamera *player_camera = nullptr;
  PodunkAudioServer *audio_server = nullptr;
  PodunkNamedSfx *sounds = nullptr;
  PodunkDialogueHost *dialogue = nullptr;
  upstream::HousePresentation *printer = nullptr;
  const HouseRenderer *house_renderer = nullptr;
  const BattleRenderer *font = nullptr;
  const SourceFontRenderer *source_font = nullptr;
  std::string asset_root;
  // Read the actual notification/input already being dispatched by its owner.
  // This does not initialize a second input or game clock.
  std::function<bool(upstream::FieldObjectId, PodunkDialogueFrame &,
                     std::string &)>
      frame;
  std::function<bool(upstream::Vec2 &, std::string &)> controls;
  std::function<bool(std::vector<upstream::Vec2> &,
                     std::vector<upstream::Vec2> &, std::string &)>
      control_directions;
  std::function<bool(bool &, std::string &)> update_pending;
};
// The checked DialogueBox subtree only. UI/Visual/Audio/Timer cores own their
// actual state and clocks; this supplies native registration, shared signals,
// cursor Tween receivers and GPU submission for those same live objects.
class PodunkDialogueSceneNative {
public:
  bool prepare(PodunkDialogueNativeInput, std::string &);
  bool apply(PodunkDialogueServices &, std::string &);
  bool admit(const upstream::FieldNodeRecipeData &, std::string &) const;
  bool factory_services(upstream::FieldObjectId,
                        const std::map<uint32_t, upstream::FieldObjectId> &,
                        PodunkDialogueFactoryServices &, std::string &);
  bool native_notification(upstream::FieldObjectId,
                           const upstream::FieldNodeDescriptor &,
                           upstream::FieldTreePhase, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool frame(upstream::FieldObjectId, PodunkDialogueFrame &,
             std::string &) const;
  // Once at the actual idle Tween traversal, after the existing Tree process.
  bool idle_tail(uint64_t epoch, float delta, bool paused, std::string &);
  bool draw(uint64_t frame_epoch, float width, float height, float origin_x,
            float origin_y, uint32_t checked_text_color, std::string &);
  bool animation_playing(upstream::FieldObjectId, bool &, std::string &) const;
  bool name_rect_changed(upstream::FieldObjectId, std::string &);

private:
  struct NativeBody {
    upstream::FieldObjectId root = 0;
    const upstream::FieldNodeRecipeRecord *source = nullptr;
    bool canvas_registered = false, control_registered = false, entered = false,
         ready = false, camera_registered = false, deleting = false,
         geometry_reserved = false, shape_registered = false,
         monitor_registered = false;
    std::map<std::string, std::pair<uint32_t, std::function<bool()>>> slots;
    std::function<bool()> mix;
    PodunkAudioVoiceCallback audio_callback{};
  };
  struct Factory {
    std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
    std::map<uint32_t, upstream::FieldObjectId> ids;
    std::unique_ptr<FieldDialogueVisualRenderer> renderer;
    uint64_t draw_epoch = 0;
  };
  struct Tween {
    upstream::FieldObjectId object = 0;
    upstream::Vec2 initial{}, target{};
    float duration = 0, elapsed = 0;
    bool alive = true;
  };
  struct IdleWait {
    upstream::FieldObjectId object = 0;
    std::function<bool()> callback;
  };
  struct SignalWait {
    upstream::FieldObjectId emitter = 0, receiver = 0;
    std::string signal, method;
  };
  PodunkDialogueNativeInput in_;
  std::map<upstream::FieldObjectId, NativeBody> bodies_;
  std::map<upstream::FieldObjectId, Factory> factories_;
  std::map<uint64_t, Tween> tweens_;
  std::vector<IdleWait> waits_;
  std::map<std::pair<upstream::FieldObjectId, uint64_t>, SignalWait>
      signal_waits_;
  uint64_t next_ = 1, idle_epoch_ = 0;
  NativeBody *body(upstream::FieldObjectId, std::string &);
  const NativeBody *body(upstream::FieldObjectId, std::string &) const;
  Factory *factory(upstream::FieldObjectId);
  const Factory *factory(upstream::FieldObjectId) const;
  bool source(upstream::FieldObjectId, uint32_t, upstream::FieldObjectId &,
              std::string &) const;
  bool emit(upstream::FieldObjectId, std::string_view,
            const upstream::FieldDeferredValue &, std::string &);
  bool connect(upstream::FieldObjectId receiver,
               upstream::FieldObjectId emitter, std::string_view,
               std::function<bool()>, std::string &, uint32_t flags = 0,
               std::string *method = nullptr);
  bool global_position(upstream::FieldObjectId, upstream::Vec2, std::string &);
  bool step_tween(Tween &, float, std::string &);
  bool camera_observe(upstream::FieldObjectId, uint32_t,
                      upstream::FieldGameCameraObservation &, std::string &);
  bool arrows_observe(upstream::FieldObjectId, uint32_t,
                      upstream::FieldArrowObservation &, std::string &);
};
} // namespace encore::ctr
