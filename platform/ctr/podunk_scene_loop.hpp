#pragma once
#include "podunk_house_continuation.hpp"
#include "podunk_scene_animated_leaves.hpp"
#include "podunk_scene_native.hpp"
#include "podunk_scene_scripts.hpp"
#include "podunk_scene_timers.hpp"
#include "podunk_player_host.hpp"

namespace encore::upstream { class FieldSceneTreeTimers; }
namespace encore::ctr {
class PodunkSceneVisibility;
class PodunkSceneNpcWorld;
class PodunkPromptNative;
class PodunkButterflyAnimation;
class PodunkNpcReturnTimers;
class PodunkButterflyTimers;
class PodunkSceneClipNative;
class PodunkAudioServer;
class PodunkSceneLeafNative;
class PodunkSceneCameras;
class PodunkHouseDoorContinuation;
// Source-specific native mechanisms (audio, animation and visibility) join
// the same tree. Their concrete state owner must supply every operation;
// this interface grants no constructor or lifecycle admission itself.
class PodunkSceneNativeMechanism {
public:
  virtual ~PodunkSceneNativeMechanism() = default;
  virtual bool owns(const upstream::FieldNodeDescriptor &) const = 0;
  virtual bool owns(upstream::FieldObjectId) const = 0;
  virtual bool construct(upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         const upstream::FieldIdentity &, std::string &) = 0;
  virtual bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
                    std::string &) = 0;
  virtual bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float,
                     bool paused, bool update_pending, std::string &) = 0;
  virtual bool deferred(const upstream::FieldDeferredMessage &,
                        std::string &) = 0;
  virtual bool release(upstream::FieldObjectId, std::string &) = 0;
};
struct PodunkSceneLoopInput {
  const upstream::FieldSceneSources *sources = nullptr;
  PodunkHouseContinuation *continuation = nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
  PodunkSceneNative *native = nullptr;
  PodunkSceneVisibility *visibility = nullptr;
  PodunkSceneNpcWorld *npc_world = nullptr;
  PodunkPromptNative *prompts = nullptr;
  PodunkButterflyAnimation *butterfly_animation = nullptr;
  PodunkNpcReturnTimers *return_timers = nullptr;
  PodunkButterflyTimers *butterfly_timers = nullptr;
  PodunkSceneClipNative *clip_native = nullptr;
  PodunkAudioServer *audio_server = nullptr;
  PodunkSceneLeafNative *leaf_native = nullptr;
  PodunkSceneCameras *cameras = nullptr;
  PodunkHouseDoorContinuation *house_door_continuation = nullptr;
  PodunkPlayerHost *player = nullptr;
  PodunkPlayerPhysicsWorld *physics = nullptr;
  upstream::FieldMapSpace *map = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  upstream::FieldSceneConsumers consumers;
  upstream::FieldSceneHostOps scene_ops;
  upstream::FieldCanvasArtHost canvas;
  PodunkSceneMaterialOwner *materials = nullptr;
  PodunkSceneGrassFactory *grass = nullptr;
  std::vector<PodunkSceneNativeMechanism *> mechanisms;
  upstream::FieldObjectSignals::DeclarationQuery source_signals;
  upstream::FieldGlobalRegistry::NodeDispatch source_methods;
  std::function<bool(const upstream::FieldDeferredMessage &)> source_method_owned;
  // Source _init can address already allocated parents before the final tree
  // source index exists. Observers borrow this actual allocation, never create
  // a replacement ID or grant Ready.
  std::function<bool(upstream::FieldObjectId,
                     const upstream::FieldNodeDescriptor &,
                     const upstream::FieldIdentity &, std::string &)>
      allocation_observed;
  // Specific additional source recipes retain the same Tree/ObjectDB. Their
  // actual owner supplies constructor, source bind and every notification.
  std::function<bool(const upstream::FieldNodeDescriptor&,const upstream::FieldIdentity&)> foreign_candidate;
  std::function<bool(upstream::FieldObjectId)> foreign_owned;
  std::function<bool(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,const upstream::FieldIdentity&,std::string&)> foreign_construct;
  std::function<bool(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,upstream::FieldNodeBinding&,std::string&)> foreign_bind;
  std::function<bool(upstream::FieldObjectId,const upstream::FieldNodeBinding&,upstream::FieldTreePhase,float,bool,bool,std::string&)> foreign_phase;
  upstream::FieldGlobalRegistry::NodeDispatch foreign_deferred;
  std::function<bool(upstream::FieldObjectId,std::string&)> foreign_release;
  std::function<bool(upstream::FieldObjectId)> foreign_emits_ready;
  std::function<bool()> source_input_handled;
  // Actual continued owner reports successful source retirement. The outer
  // frame checks only after the unchanged global MessageQueue flush completes.
  std::function<bool()> source_scene_retired;
  std::string asset_root;
};
// The actual frame/factory owner for a destination. It allocates through the
// existing session ObjectDB, dispatches real native/source callbacks, and
// advances the shared tree exactly once per supplied frame. It never creates
// a second player, session, RNG, input clock or GPU frame.
class PodunkSceneLoop {
public:
  // Load the unique native world before source hosts borrow its Tree.
  // Construction later reuses exactly this preparation without loading again.
  bool prepare_native(const PodunkSceneLoopInput &, std::string &);
  bool construct(PodunkSceneLoopInput, std::string &);
  bool attach_scene(std::string &);
  // SceneTransition adds its persistent Player after the Area's Ready body.
  // Call only after that actual attachment/Player Ready boundary.
  bool activate_after_player(std::string &);
  bool physics_frame(uint64_t epoch, float delta, bool paused, std::string &);
  bool idle_frame(uint64_t epoch, float delta, bool paused,
                  bool update_pending, std::string &);
  bool input(uint32_t kind, const upstream::PlayerInputEvent &,
             bool accept_pressed, bool paused, std::string &);
  bool draw(uint64_t epoch, float delta, float global_shader_time, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view,
                          uint32_t &, std::string &) const;
  bool ready() const;
  bool source_scene_retired() const;
  // Same global native list and References; caller retains this composition.
  // A replacement target must not advance it twice in a shared idle epoch.
  upstream::FieldSceneTreeTimers *global_timers();
  const std::shared_ptr<upstream::FieldNodeTreeRuntime> &tree() const {
    return input_.tree;
  }
  PodunkSceneScripts &scripts() { return scripts_; }
  PodunkSceneTimers &timers() { return timers_; }
  PodunkSceneAnimatedLeaves &animated() { return animated_; }
  const std::vector<std::string> &missing_native() const { return missing_; }
private:
  bool native_allocated(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool dispatch(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
                upstream::FieldTreePhase, std::string &);
  bool release(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
               std::string &);
  bool prepare_leaves(std::string &);
  bool transition_jobs(float delta, bool paused, std::string &);
  bool collect_dead_source_signals(std::string &);
  bool retired_after_flush(bool idle,uint64_t epoch,float delta,bool paused,std::string &);
  PodunkSceneNativeMechanism *mechanism(
      const upstream::FieldNodeDescriptor &) const;
  PodunkSceneLoopInput input_, native_preparation_;
  bool native_prepared_ = false;
  PodunkSceneScripts scripts_;
  PodunkSceneTimers timers_;
  PodunkSceneAnimatedLeaves animated_;
  std::vector<std::string> missing_;
  std::vector<upstream::FieldObjectId> released_signals_;
  float idle_delta_ = 0, physics_delta_ = 0;
  uint64_t physics_epoch_ = 0, idle_epoch_ = 0;
  // Player callbacks share this dispatch sequence; engine phase cursors stay separate.
  uint64_t player_frame_cursor_ = 0;
  upstream::FieldObjectId source_root_object_ = 0;
  bool attempted_ = false, constructed_ = false, attached_ = false,
       leaves_prepared_ = false, source_ready_ = false, monitors_ready_ = false,
       paused_ = false, update_pending_ = false,
       physics_sampled_ = false, poisoned_ = false;
};
} // namespace encore::ctr
