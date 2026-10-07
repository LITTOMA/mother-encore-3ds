#pragma once
#include "encore/field_geometry.hpp"
#include "encore/field_node_tree.hpp"
#include <unordered_map>
#include <unordered_set>

namespace encore::upstream {
class FieldSceneActionsData;
class PlayerInitializationData;
class GrassNativeData;
class FieldGlobalRegistry;
class FieldDialogueVisualData;
class FieldNodeRecipeData;
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
  // Zero for unbound immutable world packs. Returned House geometry and
  // dynamic source instances retain their real native ObjectDB identities.
  FieldObjectId actual_owner = 0, actual_shape = 0;
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
class FieldNpcRuntime;
class HouseReentryData;
class HouseReturnSources;
class FieldDoorData;
struct FieldPersistentDoorGeometry {
  const FieldDoorData *data=nullptr;
  FieldNodeTreeRuntime *tree=nullptr;
  FieldGlobalRegistry *registry=nullptr;
  FieldObjectId door=0,shape=0;
  uint32_t source_id=0;
  FieldPhysicsRid rid{};
  FieldGeometryContact contact{};
  bool disabled=false;
};
class FieldGeometrySpace {
public:
  // Original persistent Door only: after actual remove_child/Exit, move its
  // existing static RID and exact Rectangle into a retained native owner.
  bool retain_detached_door(const FieldDoorData &,uint32_t,FieldNodeTreeRuntime &,
      FieldGlobalRegistry &,FieldObjectId,FieldObjectId,bool native_disabled,
      FieldPersistentDoorGeometry &,std::string &);
  bool persistent_door(FieldObjectId,FieldPersistentDoorGeometry &,std::string &)const;
  bool rebind_persistent_door(const FieldDoorData &,FieldObjectId,
      FieldNodeTreeRuntime &old_tree,FieldNodeTreeRuntime &next,
      FieldGlobalRegistry &,std::string &);
  bool update_persistent_door(const FieldDoorData &,FieldObjectId,bool disabled,
      std::string &);
  bool delete_persistent_door_shape(const FieldDoorData &,FieldObjectId,std::string &);
  bool delete_persistent_door_owner(const FieldDoorData &,FieldObjectId,std::string &);
  bool apply_npc_interaction(const FieldNpcRuntime &, uint32_t,
                             FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                             std::string &);
  bool configure(const FieldGeometryView &, float grid_cell_size,
                 std::string &);
  // Commit the checked native House geometry only after the actual old scene
  // has exited and been deleted. Only inactive, explicitly retained original
  // Door shapes may remain; Player and Door owners keep their original RIDs
  // and this space address.
  // Updates are observations of completed native property/deletion callbacks.
  // Script receipts remain individually required; this grants no Ready.
  bool replace_house_world(const HouseReentryData &,const FieldGeometryView &,
      const FieldNodeTreeRuntime &old_tree,FieldObjectId old_root,
      FieldNodeTreeRuntime &house_tree,
      const std::vector<FieldGeometryNodeUpdate> &actual_updates,std::string &);
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
  // Resolve one current contact to its actual analytic/polygon shape. The
  // contact must still name the same enabled, admitted live shape instance.
  bool live_geometry(const FieldGeometryContact &, FieldGeometryActor &,
                     FieldGeometryOwner &, FieldGeometryShape &,
                     std::string &) const;
  // Observe a real dynamic player shape, including its disabled property.
  // This never enables a shape or admits it to overlap queries.
  bool player_shape_snapshot(const FieldGeometryContact&,FieldGeometryActor&,
      FieldGeometryOwner&,FieldGeometryShape&,bool &disabled,std::string&)const;
  bool register_player_shape(const PlayerInitializationData &,
                             FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                             FieldObjectId player, FieldObjectId shape,
                             std::string &);
  bool reserve_player_owner(const PlayerInitializationData &,
                            FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                            FieldObjectId owner, std::string &);
  bool rebind_player_owner(const PlayerInitializationData &,
                           FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                           FieldObjectId owner, std::string &);
  bool reserve_grass_owner(const GrassNativeData &, FieldNodeTreeRuntime &,
                           FieldGlobalRegistry &, FieldObjectId, std::string &);
  bool register_grass_shape(const GrassNativeData &, FieldNodeTreeRuntime &,
                            FieldGlobalRegistry &, FieldObjectId area,
                            FieldObjectId shape, std::string &);
  bool reserve_dialogue_camera_owner(const FieldDialogueVisualData &,
      const FieldNodeRecipeData &,FieldNodeTreeRuntime &,FieldGlobalRegistry &,
      FieldObjectId,std::string &);
  bool register_dialogue_camera_shape(const FieldDialogueVisualData &,
      const FieldNodeRecipeData &,FieldNodeTreeRuntime &,FieldGlobalRegistry &,
      FieldObjectId area,FieldObjectId shape,std::string &);
  bool physics_rid(const FieldGeometryContact &, FieldPhysicsRid &,
                   std::string &) const;
  bool player_owner_rid(FieldObjectId owner, FieldPhysicsRid &,
                        std::string &) const;
  // Observe the already allocated native House CollisionObject RID, including
  // disabled/empty shapes. This does not admit a contact or allocate a RID.
  bool house_owner_rid(FieldObjectId, const HouseReturnSources &,
                       FieldPhysicsRid &, std::string &) const;
  bool rid_alive(FieldPhysicsRid) const;
  bool rid_issued(FieldPhysicsRid rid) const {
    return rid.space == this && rid.handle && rid.handle <= next_rid_;
  }
  bool retire_player_owner(FieldObjectId owner, std::string &);
  bool remove_player_shape(FieldObjectId shape, std::string &);
  bool set_player_shape_disabled(FieldObjectId shape, bool, std::string &);
  bool set_player_collision_mask(FieldObjectId owner, uint32_t bit, bool,
                                 std::string &);
  bool player_shapes(FieldObjectId owner, std::vector<FieldGeometryContact> &,
                     std::string &) const;
  const FieldGeometryView *source() const { return source_; }
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
  size_t indexed_instance_count() const {
    return instances_.size() + dynamic_.size();
  }

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
  struct DynamicInstance {
    const PlayerInitializationData *data = nullptr;
    FieldNodeTreeRuntime *tree = nullptr;
    FieldGlobalRegistry *registry = nullptr;
    FieldObjectId player = 0;
    FieldGeometryContact contact{};
    FieldGeometryActor actor{};
    FieldGeometryOwner owner{};
    FieldGeometryShape shape{};
    bool disabled = false;
    const GrassNativeData *grass = nullptr;
    const FieldDialogueVisualData *dialogue = nullptr;
    const FieldNodeRecipeData *recipe = nullptr;
    const FieldDoorData *door = nullptr;
  };
  bool dynamic_actor(const DynamicInstance &, FieldGeometryActor &,
                     std::string &) const;
  bool dynamic_filter(const DynamicInstance &,
                      const FieldGeometryFilter &) const;
  bool query_dynamic(FieldGeometryBounds, const FieldGeometryFilter &, size_t,
                     std::vector<size_t> &, std::string &) const;
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
  bool native_instance(const Instance &,std::string &)const;
  const FieldGeometryView *source_ = nullptr;
  FieldNodeTreeRuntime *native_tree_ = nullptr;
  float cell_size_ = 0;
  std::vector<NodeState> nodes_;
  std::vector<OwnerState> owners_;
  std::vector<bool> disabled_;
  std::map<uint32_t, Vec2> npc_rectangle_extents_;
  std::vector<Instance> instances_;
  std::vector<std::vector<uint32_t>> node_instances_, children_,
      owner_instances_;
  std::vector<std::vector<uint32_t>> owner_shapes_;
  std::unordered_map<uint32_t, uint32_t> ids_;
  std::unordered_map<int64_t, std::vector<uint32_t>> grid_;
  std::unordered_set<uint32_t> unresolved_;
  std::vector<DynamicInstance> dynamic_;
  struct DynamicOwner {
    const PlayerInitializationData *data = nullptr;
    FieldNodeTreeRuntime *tree = nullptr;
    FieldGlobalRegistry *registry = nullptr;
    FieldObjectId object = 0;
    uint64_t rid = 0;
    const GrassNativeData *grass = nullptr;
    const FieldDialogueVisualData *dialogue = nullptr;
    const FieldNodeRecipeData *recipe = nullptr;
    const FieldDoorData *door = nullptr;
    const FieldGeometryView *door_geometry = nullptr;
    uint32_t door_id=0;
    FieldObjectId door_shape=0;
    FieldNodeDescriptor door_descriptor{},shape_descriptor{};
  };
  bool persistent_door_node(const DynamicOwner &,FieldObjectId,std::string &,
                            bool detached_shape_delete=false)const;
  std::map<FieldObjectId, DynamicOwner> dynamic_owners_;
  mutable std::map<uint32_t, uint64_t> static_rids_;
  mutable uint64_t next_rid_ = 0;
};
} // namespace encore::upstream
