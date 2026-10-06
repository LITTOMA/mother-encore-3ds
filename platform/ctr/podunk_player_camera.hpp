#pragma once
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

private:
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
