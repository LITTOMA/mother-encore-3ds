#pragma once
#include "encore/field_geometry.hpp"
#include <unordered_map>
#include <unordered_set>

namespace encore::upstream {
class FieldSceneActionsData;
struct FieldGeometryBounds {
  Vec2 minimum{}, maximum{};
};
struct FieldGeometryActor {
  FieldGeometryKind kind = FieldGeometryKind::Empty;
  FieldGeometryTransform transform{{1, 0}, {0, 1}, {0, 0}};
  Vec2 extents{};
  float radius = 0;
  std::vector<Vec2> points;
  uint32_t stable_id = 0, layer = 0, mask = 0;
  bool area = false, monitorable = true;
};
struct FieldGeometryFilter {
  uint32_t layer_mask = 0, reciprocal_layer = 0, exclude_stable_id = 0;
  bool bodies = true, areas = false, require_reciprocal_mask = false,
       monitoring_only = false;
  // Native broadphase pair admission is bilateral OR. Direct-space query
  // masks retain their existing one-way behavior unless this is requested.
  bool bilateral_mask = false;
};
struct FieldGeometryContact {
  uint32_t owner = 0, shape = 0, part = 0, stable_id = 0,
           native_shape_index = 0;
};
struct FieldGeometryRayHit : FieldGeometryContact {
  Vec2 position{}, normal{};
  float fraction = 0;
};
struct FieldGeometryNodeUpdate {
  uint32_t stable_id = 0;
  // bit 1 local matrix, 2 committed deferred deletion, 4 shape disabled,
  // 8 owner layer/mask, 16 Area monitoring/monitorability. A queued deletion
  // does not use bit 2 until the actual source deferred-free boundary.
  uint32_t fields = 0;
  FieldGeometryTransform local{};
  bool deleted = false, disabled = false, monitoring = false,
       monitorable = false;
  uint32_t layer = 0, mask = 0;
};
// Pure CPU broad/narrow phase for the checked non-TileMap resource. The
// SceneHost supplies live native adapter results; there is no blanket script
// approval. A source-compatible Circle remains analytic. Unsupported live
// transforms or kinds return an error and preserve the last space/query result.
class FieldGeometrySpace {
public:
  bool configure(const FieldGeometryView &, float grid_cell_size,
                 std::string &);
  // Caller is the trusted checked adapter registry, after actual source and
  // runtime capability admission. SHA and individual NodeID must both match.
  bool bind_script(uint32_t stable_id,
                   const std::array<uint8_t, 32> &source_sha,
                   uint32_t adapter_family, uint32_t adapter_capability,
                   std::string &);
  bool apply_updates(const std::vector<FieldGeometryNodeUpdate> &,
                     std::string &);
  // Audited leaf CollisionPolygon migration only. TileMap parents are not
  // CollisionObject2D shape owners: moving there removes physical parts.
  bool detach_leaf_polygon(const FieldSceneActionsData &, uint32_t shape,
                           std::string &);
  bool attach_leaf_polygon(const FieldSceneActionsData &, uint32_t shape,
                           uint32_t parent, uint32_t native_tree_order,
                           std::string &);
  bool live_polygon_parts(uint32_t actual_owner, uint32_t shape, bool &attached,
                          bool &disabled, std::vector<std::vector<Vec2>> &,
                          std::string &) const;
  bool candidates(FieldGeometryBounds, const FieldGeometryFilter &, size_t,
                  std::vector<FieldGeometryContact> &, std::string &) const;
  bool overlap_actor(const FieldGeometryActor &, const FieldGeometryFilter &,
                     size_t, std::vector<FieldGeometryContact> &,
                     std::string &) const;
  // Monitoring Area -> actor uses the Area mask against actor layer; this
  // differs from direct-space queries against the target's collision layer.
  bool monitoring_areas(const FieldGeometryActor &, size_t,
                        std::vector<FieldGeometryContact> &,
                        std::string &) const;
  bool ray(Vec2 from, Vec2 to, const FieldGeometryFilter &, size_t,
           bool &collided, FieldGeometryRayHit &, std::string &) const;
  size_t indexed_instance_count() const { return instances_.size(); }

private:
  struct NodeState {
    FieldGeometryTransform local{}, world{};
    bool deleted = false, bound = false;
    uint32_t family = 0, capability = 0;
  };
  struct OwnerState {
    uint32_t layer = 0, mask = 0;
    bool monitoring = false, monitorable = false;
  };
  struct Instance {
    FieldGeometryContact contact{};
    uint32_t geometry = 0, shape_order = 0;
    FieldGeometryActor actor{};
    FieldGeometryBounds bounds{};
    bool disabled = false, deleted = false, resolved = false,
         ownership_override = false;
    std::vector<int64_t> cells;
  };
  bool refresh(const std::vector<uint32_t> &node_indices, std::string &);
  bool rebuild_instance(uint32_t, std::string &);
  bool admit_leaf_polygon(const FieldSceneActionsData &, uint32_t,
                          uint32_t &node, uint32_t &shape, std::string &) const;
  bool change_leaf_owner(uint32_t shape, uint32_t owner,
                         uint32_t native_tree_order, std::string &);
  void rebuild_dependencies();
  bool keys(FieldGeometryBounds, std::vector<int64_t> &, std::string &) const;
  bool filter_owner(uint32_t, const FieldGeometryFilter &) const;
  bool query_instances(FieldGeometryBounds, const FieldGeometryFilter &, size_t,
                       std::vector<uint32_t> &, std::string &) const;
  const FieldGeometryView *source_ = nullptr;
  float cell_size_ = 0;
  std::vector<NodeState> nodes_;
  std::vector<OwnerState> owners_;
  std::vector<bool> disabled_;
  std::vector<Instance> instances_;
  std::vector<std::vector<uint32_t>> node_instances_, children_,
      owner_instances_;
  std::vector<std::vector<uint32_t>> owner_shapes_;
  std::unordered_map<uint32_t, uint32_t> ids_;
  std::unordered_map<int64_t, std::vector<uint32_t>> grid_;
  std::unordered_set<uint32_t> unresolved_;
};
} // namespace encore::upstream
