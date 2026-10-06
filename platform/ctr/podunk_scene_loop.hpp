#pragma once
#include "podunk_house_continuation.hpp"
#include "podunk_scene_animated_leaves.hpp"
#include "podunk_scene_native.hpp"
#include "podunk_scene_scripts.hpp"
#include "podunk_scene_timers.hpp"
#include "podunk_player_host.hpp"

namespace encore::ctr {
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
  std::string asset_root;
};
// The actual frame/factory owner for a destination. It allocates through the
// existing session ObjectDB, dispatches real native/source callbacks, and
// advances the shared tree exactly once per supplied frame. It never creates
// a second player, session, RNG, input clock or GPU frame.
class PodunkSceneLoop {
public:
  bool construct(PodunkSceneLoopInput, std::string &);
  bool attach_and_ready(std::string &);
  bool physics_frame(uint64_t epoch, float delta, bool paused, std::string &);
  bool idle_frame(uint64_t epoch, float delta, bool paused,
                  bool update_pending, std::string &);
  bool input(uint32_t kind, const upstream::PlayerInputEvent &,
             bool accept_pressed, bool paused, std::string &);
  bool draw(uint64_t epoch, float delta, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view,
                          uint32_t &, std::string &) const;
  bool ready() const;
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
  PodunkSceneNativeMechanism *mechanism(
      const upstream::FieldNodeDescriptor &) const;
  PodunkSceneLoopInput input_;
  PodunkSceneScripts scripts_;
  PodunkSceneTimers timers_;
  PodunkSceneAnimatedLeaves animated_;
  std::vector<std::string> missing_;
  std::vector<upstream::FieldObjectId> released_signals_;
  float idle_delta_ = 0, physics_delta_ = 0;
  uint64_t physics_epoch_ = 0, idle_epoch_ = 0;
  bool attempted_ = false, constructed_ = false, attached_ = false,
       leaves_prepared_ = false, source_ready_ = false, paused_ = false, update_pending_ = false,
       physics_sampled_ = false, poisoned_ = false;
};
} // namespace encore::ctr
