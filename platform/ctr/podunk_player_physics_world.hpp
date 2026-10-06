#pragma once
#include "encore/field_object_signals.hpp"
#include "podunk_player_camera.hpp"
#include "podunk_player_host.hpp"
#include <tuple>

namespace encore::ctr {
// Actual native 2D collision objects in the existing checked world space.
// Player geometry is derived from its official native snapshot; no second
// collider, input loop, physics clock or Circle tessellation is introduced.
class PodunkPlayerPhysicsWorld final : public PodunkPlayerWorldNative {
public:
  using SourceObject =
      std::function<bool(uint32_t, upstream::FieldObjectId &, std::string &)>;
  bool prepare(const upstream::PlayerInitializationData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldGeometrySpace &,
               upstream::FieldObjectSignals &, SourceObject, std::string &);
  bool bind_camera(PodunkPlayerCamera &, std::string &);
  const upstream::FieldGlobalRegistry *registry() const override {
    return registry_;
  }
  const upstream::FieldNodeTreeRuntime *tree() const override { return tree_; }
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::PlayerInitializationData &,
                 std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             std::string &) override;
  bool disabled(upstream::FieldObjectId, bool, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool set_collision_mask(upstream::FieldObjectId, uint32_t, bool,
                          std::string &);
  // Existing world native Area owners call this at their actual construction
  // cursor. It neither supplies script Ready nor grants the scene admission.
  bool admit_static_monitor(upstream::FieldObjectId,
                            const upstream::FieldGeometryContact &,
                            std::string &);
  // Native disabled/empty shapes are valid Area state. Construct its real
  // monitor from the checked owner row without fabricating an enabled contact.
  bool admit_static_monitor(upstream::FieldObjectId, uint32_t owner_index,
                            std::string &);
  bool static_monitor_exit(upstream::FieldObjectId, std::string &);
  // The one real physics frame owner samples after step and flushes the queued
  // monitor callbacks at PhysicsServer::flush_queries, before the next frame's
  // SceneTree physics processing. No dt or independent timer is maintained.
  bool physics_step(uint64_t epoch, std::string &);
  bool flush_queries(std::string &);
  bool handles_callback(const upstream::FieldDeferredMessage &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool source_object(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool geometry_admitted(upstream::FieldObjectId owner,
                         upstream::FieldObjectId shape, std::string &) const;
  bool overlapping(upstream::FieldObjectId area, bool other_areas,
                   std::vector<upstream::FieldObjectId> &, std::string &) const;

private:
  struct Native {
    std::string klass;
    uint32_t stable = 0;
    bool entered = false, ready = false, registered = false, disabled = false;
  };
  struct Pair {
    upstream::FieldObjectId watcher = 0, other = 0;
    upstream::FieldPhysicsRid rid{};
    uint32_t other_shape = 0, self_shape = 0;
    bool area = false;
    bool operator<(const Pair &b) const {
      return std::tie(watcher, area, rid.handle, other_shape, self_shape,
                      other) < std::tie(b.watcher, b.area, b.rid.handle,
                                        b.other_shape, b.self_shape, b.other);
    }
  };
  struct Body {
    upstream::FieldPhysicsRid rid{};
    bool inside = false;
    std::set<std::pair<uint32_t, uint32_t>> shapes;
  };
  struct Monitor {
    bool monitoring = false, native_entered = false;
    std::map<upstream::FieldObjectId, Body> bodies, areas;
  };
  bool actual_player(upstream::FieldObjectId &, std::string &) const;
  bool target(const upstream::FieldGeometryContact &, upstream::FieldObjectId &,
              std::string &) const;
  bool collect(std::set<Pair> &, std::string &);
  bool inout(const Pair &, bool entering, std::string &);
  bool clear_monitor(upstream::FieldObjectId, std::string &);
  bool tree_event(upstream::FieldObjectId, upstream::FieldObjectId, bool area,
                  bool entering, std::string &);
  bool emit_shape(upstream::FieldObjectId, const Pair &, bool, std::string &);
  bool connect_tree(upstream::FieldObjectId, upstream::FieldObjectId, bool,
                    bool connecting, std::string &);
  const upstream::PlayerInitializationData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldGeometrySpace *space_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkPlayerCamera *camera_ = nullptr;
  SourceObject source_;
  std::map<upstream::FieldObjectId, Native> natives_;
  std::map<upstream::FieldObjectId, Monitor> monitors_;
  std::set<Pair> pairs_, pending_;
  uint64_t epoch_ = 0;
  bool sampled_ = false, flushing_ = false, locked_ = false, poisoned_ = false;
};
} // namespace encore::ctr
