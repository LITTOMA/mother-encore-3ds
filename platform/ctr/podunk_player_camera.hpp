#pragma once
#include "encore/field_object_signals.hpp"
#include "encore/grass_native.hpp"
#include "encore/player_motion.hpp"
#include "podunk_native_root.hpp"
#include "podunk_player_child_scripts.hpp"
namespace encore::ctr {
struct PodunkCameraViewport {
  upstream::Vec2 logical_size{};
  bool reference_explicit = false;
};
// These endpoints belong to the actual UI, ordered SignalBus, Input and
// World2D owners. Native Camera2D transform/state is executed below, not here.
struct PodunkPlayerCameraPorts {
  // The actual existing UI owner may implement a reviewed continuation slice.
  // Its real ObjectDB identity/source is checked; whole UiManager Ready is not
  // a prerequisite for source getters or actual signal registration.
  upstream::FieldGlobalExternalObject *ui = nullptr;
  const upstream::FieldGlobalRegistryData *ui_namespace = nullptr;
  std::function<bool(upstream::FieldObjectId, bool &, std::string &)> in_battle;
  std::function<bool(upstream::Vec2 &, std::string &)> controls;
  std::function<bool(std::string_view, upstream::PlayerInputQuery, bool &,
                     std::string &)>
      input;
  std::function<bool(upstream::FieldObjectId camera,
                     upstream::FieldObjectId player, upstream::FieldObjectId ui,
                     const upstream::FieldGameCameraData &,
                     std::function<bool()> scope_stop,
                     std::function<bool()> player_pause, std::string &)>
      connect_player;
  std::function<bool(upstream::FieldObjectId camera,
                     const upstream::FieldGameCameraData &, std::string &)>
      listeners_admitted;
  std::function<bool(upstream::FieldObjectId area,
                     upstream::FieldObjectId shape, std::string &)>
      geometry_admitted;
  // One native Viewport camera registry. Selection changes the actual previous
  // camera owner too; this must not be a local copied currentCamera field.
  std::function<bool(upstream::FieldObjectId &, std::string &)> native_current;
  std::function<bool(upstream::FieldObjectId, std::string &)> make_current;
  std::function<bool(upstream::FieldObjectId, upstream::FieldGameCameraState &,
                     std::string &)>
      current_snapshot;
  std::function<bool(upstream::FieldObjectId, std::string &)> info_plates_hide;
  std::function<bool(upstream::FieldObjectId, std::string &)>
      player_exit_camera;
};
// A native owner of the actual Player Camera2D. No new node, Sprite clock,
// animation clock or input loop is created. All tuning remains in 0061.
class PodunkPlayerCamera {
public:
  ~PodunkPlayerCamera();
  bool prepare(const upstream::PlayerChildScriptsData &,
               const upstream::PlayerInitializationData &,
               const upstream::PlayerMotionData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
               upstream::FieldGlobalConstructorRuntime &,
               upstream::PlayerInitializationBody &,
               upstream::PlayerChildScriptsRuntime &, PodunkPlayerAnimation &,
               PodunkCameraViewport, PodunkPlayerCameraPorts, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  // Borrow only the independently checked official engine Tween source proof.
  // Camera properties, targets, curves and source code remain owned by 0061.
  bool bind_tweens(const upstream::GrassNativeData &,
                   upstream::FieldObjectSignals &, std::string &);
  bool idle_tail(uint64_t epoch, float delta, bool paused, std::string &);
  bool tween_owned(upstream::FieldObjectId) const;
  bool tween_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                         std::string &) const;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 std::string &);
  upstream::FieldGameCameraHost source_host();
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, bool paused,
             std::string &);
  // Call once from the real SceneTree frame owner, before script Physics.
  // This performs actual UI/Input/body reads; missing owners reject the frame.
  bool begin_frame(uint64_t epoch, bool paused, std::string &);
  bool script_process(upstream::FieldObjectId, upstream::FieldTreePhase,
                      float delta, bool paused, std::string &);
  bool rebind_tree(upstream::FieldNodeTreeRuntime &, std::string &);
  bool owns(upstream::FieldObjectId id) const { return id && id == object_; }
  upstream::FieldObjectId object() const { return object_; }
  upstream::Vec2 display_offset() const { return display_offset_; }
  bool native_ready() const { return ready_; }
  bool native_snapshot(upstream::FieldGameCameraState &, std::string &) const;
  bool native_select(bool, std::string &);
  bool source_observation(upstream::FieldGameCameraObservation &,
                          std::string &);
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }
  const upstream::FieldNodeTreeRuntime *tree() const { return tree_; }

private:
  class TweenReference;
  struct TweenJob {
    uint64_t token = 0;
    std::shared_ptr<TweenReference> owner, waiter;
    std::vector<std::shared_ptr<TweenReference>> properties;
    std::vector<bool> property_finished;
    std::function<bool(float)> step;
    upstream::FieldObjectId wait_emitter = 0;
    bool dead = false, finished = false, resumed = false, completed = false;
  };
  bool make_tween_reference(const char *, std::string_view,
                            std::shared_ptr<TweenReference> &, std::string &);
  bool register_tween(uint32_t, uint64_t, std::function<bool(float)>,
                      std::string &);
  bool kill_tween(uint64_t, std::string &);
  bool tween_signal(uint64_t, uint32_t, bool, std::string &);
  bool coroutine_completed(uint64_t, std::string &);
  bool resume_tween_waiter(upstream::FieldObjectId, std::string &);
  bool release_tween_reference(std::shared_ptr<TweenReference> &,
                               std::string &);
  bool collect_tweens(bool all, std::string &);
  const upstream::GrassNativeData *tween_engine_ = nullptr;
  upstream::FieldObjectSignals *tween_signals_ = nullptr;
  std::map<uint64_t, TweenJob> tween_jobs_;
  std::set<upstream::FieldObjectId> tween_references_;
  uint64_t tween_epoch_ = 0;
  bool tween_poisoned_ = false, tween_stepping_ = false;
  bool live(std::string &) const;
  bool actual_ui(std::string &) const;
  bool actual(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool viewport(std::string &) const;
  bool observe(uint32_t, upstream::FieldGameCameraObservation &, std::string &);
  bool publish(uint32_t, const upstream::FieldGameCameraState &, std::string &);
  bool canvas(uint32_t, upstream::Vec2, upstream::Vec2, std::string &);
  bool update_native(std::string &);
  bool select(uint32_t, std::string &);
  bool all_children_ready(upstream::FieldObjectId) const;
  bool global_player(upstream::FieldObjectId &, std::string &) const;
  const upstream::PlayerChildScriptsData *data_ = nullptr;
  const upstream::PlayerInitializationData *player_data_ = nullptr;
  const upstream::PlayerMotionData *motion_data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldGlobalConstructorRuntime *global_ = nullptr;
  upstream::PlayerInitializationBody *body_ = nullptr;
  upstream::PlayerChildScriptsRuntime *children_ = nullptr;
  PodunkPlayerAnimation *animation_ = nullptr;
  PodunkCameraViewport viewport_{};
  PodunkPlayerCameraPorts ports_{};
  upstream::FieldObjectId object_ = 0;
  upstream::FieldGameCameraState native_{};
  upstream::Vec2 display_offset_{};
  upstream::FieldGameCameraObservation frame_{};
  uint64_t epoch_ = 0, physics_epoch_ = 0;
  bool entered_ = false, ready_ = false, paused_ = false, frame_valid_ = false,
       native_observation_ = false;
};
} // namespace encore::ctr
