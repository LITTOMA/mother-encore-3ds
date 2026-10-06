#pragma once
#include "encore/field_map.hpp"
#include "encore/field_native_root.hpp"
#include "encore/field_object_signals.hpp"
#include "podunk_ui_host.hpp"
#include <citro2d.h>
#include <set>
namespace encore::ctr {
// Implemented by the actual source autoload object, not a metadata proxy.
// The object passed to register_external_child must be Registry-owned.
class PodunkExternalNodeLifecycle {
public:
  virtual ~PodunkExternalNodeLifecycle() = default;
  // Source parent assignment occurs before NOTIFICATION_PARENTED, outside Tree.
  virtual bool stage_parent(upstream::FieldObjectId, std::string &) = 0;
  virtual bool native_notification(upstream::FieldTreePhase, std::string &) = 0;
  // Enter-only for this complete branch; Ready is a separate source traversal.
  virtual bool enter(upstream::FieldObjectId parent, std::string &) = 0;
  virtual bool ready(std::string &) = 0;
  virtual bool exit(std::string &) = 0;
};
struct PodunkRootViewportState {
  bool initialized = false, inside = false, ready = false, active = false,
       world_registered = false, internal_physics = false, failed = false;
  bool ready_notified = false;
  uint32_t blocked = 0;
  bool handle_input_locally = false, audio_listener = false,
       audio_listener_2d = false;
  upstream::Vec2 size{};
  upstream::FieldTransform canvas{};
  std::vector<upstream::FieldObjectId> children;
  upstream::FieldObjectId current_scene = 0;
};
// Actual SceneTree/Viewport source objects are Registry-owned. This shared
// native state and GPU target must outlive them. It supplies no missing source
// script owner, GUI picker, 3D renderer or pause notification approximation.
class PodunkNativeRoot final : public PodunkUiExternalFactory {
public:
  bool initialize(const upstream::FieldNativeRootData &,
                  const upstream::FieldGlobalRegistryData &,
                  upstream::FieldGlobalRegistry &, C3D_RenderTarget *,
                  PodunkUiExternalFactory *other, std::string &);
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &) override;
  bool register_external_child(upstream::FieldGlobalExternalObject &,
                               PodunkExternalNodeLifecycle &, std::string &);
  using ParentObserver =
      std::function<bool(upstream::FieldObjectId parent,
                         upstream::FieldObjectId child, std::string &)>;
  bool bind_parent_observer(ParentObserver, std::string &);
  using ChildNotification = std::function<bool(
      upstream::FieldObjectId, upstream::FieldTreePhase, std::string &)>;
  bool bind_child_notifications(ChildNotification, std::string &);
  // Native class empty-root init. Original project cold startup must stage
  // all autoloads/main and use initialize_project_tree, never this shortcut.
  bool stage_child(upstream::FieldObjectId, std::string &);
  // Actual SceneTree::add_current_scene: assign native identity, then stage.
  bool stage_current_scene(upstream::FieldObjectId, std::string &);
  bool initialize_project_tree(std::string &);
  bool initialize_tree(std::string &);
  bool finalize_tree(std::string &);
  bool add_child(upstream::FieldObjectId, std::string &);
  bool remove_child(upstream::FieldObjectId, std::string &);
  bool move_child(upstream::FieldObjectId, int32_t, std::string &);
  bool node_notification(upstream::FieldObjectId, upstream::FieldTreePhase,
                         std::string &);
  upstream::FieldObjectId external_parent(upstream::FieldObjectId) const;
  bool set_current_scene(upstream::FieldObjectId, std::string &);
  bool set_canvas_transform(const upstream::FieldTransform &, std::string &);
  bool world_rect(upstream::FieldMapRect &, std::string &) const;
  bool native_group(std::string_view, std::vector<upstream::FieldObjectId> &,
                    std::string &) const;
  bool begin_draw(std::string &);
  bool clear_target(std::string &);
  bool input_registration(upstream::FieldObjectId, uint32_t, bool,
                          std::string &);
  bool input_objects(uint32_t, std::vector<upstream::FieldObjectId> &,
                     std::string &) const;
  bool bind_object_signals(upstream::FieldObjectSignals &, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &, std::string &) const;
  bool disconnect_signal(bool kernel, std::string_view, upstream::FieldObjectId,
                         std::string_view, std::string &);
  bool connect_signal(bool kernel, std::string signal, upstream::FieldObjectId,
                      std::string method, std::string &);
  bool enqueue(upstream::FieldDeferredMessage, std::string &);
  bool snapshot(bool kernel, upstream::FieldGlobalExternalState &,
                std::string &) const;
  bool deferred(bool kernel, const upstream::FieldDeferredMessage &,
                std::string &);
  const PodunkRootViewportState &viewport() const { return state_; }
  upstream::FieldObjectId kernel_object() const { return kernel_; }
  upstream::FieldObjectId viewport_object() const { return root_; }
  const std::string &failure() const { return failure_; }
  std::vector<std::string> pending_native() const;

private:
  struct External {
    upstream::FieldGlobalExternalObject *object = nullptr;
    PodunkExternalNodeLifecycle *lifecycle = nullptr;
    upstream::FieldGlobalExternalBinding binding;
  };
  const upstream::FieldNativeRootData *data_ = nullptr;
  const upstream::FieldGlobalRegistryData *source_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  struct WorldViewport {
    C3D_RenderTarget *target = nullptr;
  };
  std::map<upstream::FieldObjectId, WorldViewport> world_viewports_;
  std::map<std::string, std::vector<upstream::FieldObjectId>> native_groups_;
  C3D_RenderTarget *target_ = nullptr;
  PodunkUiExternalFactory *other_ = nullptr;
  upstream::FieldObjectId kernel_ = 0, root_ = 0;
  PodunkRootViewportState state_;
  std::map<upstream::FieldObjectId, External> external_;
  std::array<std::set<upstream::FieldObjectId>, 3> input_;
  upstream::FieldObjectSignals *signals_ = nullptr;
  ParentObserver parent_observer_;
  ChildNotification child_notification_;
  std::set<upstream::FieldObjectId> known_nodes_, entered_notified_,
      exited_notified_;
  std::string failure_;
  bool emit(bool, std::string_view, upstream::FieldObjectId, std::string &);
  bool poison(const std::string &, std::string &);
  bool actual_target(std::string &) const;
  bool begin_root_enter(std::string &);
  bool finish_root_ready(std::string &);
  bool actual_external(upstream::FieldObjectId, External *&,
                       upstream::FieldGlobalExternalState &, std::string &);
};
} // namespace encore::ctr
