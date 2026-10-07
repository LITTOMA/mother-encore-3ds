#include "encore/field_geometry_space.hpp"
#include "encore/house_return_sources.hpp"
#include "manual_require.hpp"

using namespace encore::upstream;
// Explicit manual driver supplies a fully constructed, entered House in its
// real replacement Space. No fixture supplies ObjectIDs, RIDs or script Ready.
void house_geometry_rid_manual(FieldGeometrySpace &space,
    const HouseReturnSources &sources, FieldNodeTreeRuntime &tree,
    uint32_t owner_index, uint32_t shape_index) {
  MANUAL_REQUIRE(owner_index < sources.geometry().owner_count());
  MANUAL_REQUIRE(shape_index < sources.geometry().shape_count());
  const auto owner = sources.geometry().owner(owner_index);
  const auto shape = sources.geometry().shape(shape_index);
  MANUAL_REQUIRE(shape.owner == owner_index);
  const auto owner_source = sources.geometry().node(owner.node).stable_id;
  const auto shape_source = sources.geometry().node(shape.node).stable_id;
  const auto actual_owner = tree.source_object(owner_source);
  const auto actual_shape = tree.source_object(shape_source);
  MANUAL_REQUIRE(actual_owner && actual_shape);
  FieldPhysicsRid rid;
  std::string error;
  MANUAL_REQUIRE(space.house_owner_rid(actual_owner, sources, rid, error));
  MANUAL_REQUIRE(space.rid_alive(rid));
  FieldPhysicsRid unchanged = rid;
  MANUAL_REQUIRE(!space.house_owner_rid(actual_shape, sources, unchanged, error));
  MANUAL_REQUIRE(unchanged.space == rid.space && unchanged.handle == rid.handle);
  HouseReturnSources absent;
  MANUAL_REQUIRE(!space.house_owner_rid(actual_owner, absent, unchanged, error));
  // First observe the driver's real shape deletion, then commit it to Space.
  MANUAL_REQUIRE(tree.queue_free(actual_shape, error));
  MANUAL_REQUIRE(tree.flush_deferred(error));
  MANUAL_REQUIRE(!tree.state(actual_shape));
  FieldGeometryNodeUpdate shape_deleted;
  shape_deleted.stable_id = shape_source;
  shape_deleted.fields = 2;
  shape_deleted.deleted = true;
  MANUAL_REQUIRE(space.apply_updates({shape_deleted}, error));
  MANUAL_REQUIRE(space.rid_alive(rid));
  // An invalid transaction must not retire the still-live owner.
  FieldGeometryNodeUpdate unknown;
  unknown.stable_id = 0;
  unknown.fields = 2;
  unknown.deleted = true;
  MANUAL_REQUIRE(!space.apply_updates({unknown}, error));
  MANUAL_REQUIRE(space.rid_alive(rid));
  MANUAL_REQUIRE(tree.queue_free(actual_owner, error));
  MANUAL_REQUIRE(tree.flush_deferred(error));
  MANUAL_REQUIRE(!tree.state(actual_owner));
  FieldGeometryNodeUpdate owner_deleted;
  owner_deleted.stable_id = owner_source;
  owner_deleted.fields = 2;
  owner_deleted.deleted = true;
  MANUAL_REQUIRE(space.apply_updates({owner_deleted}, error));
  MANUAL_REQUIRE(!space.rid_alive(rid));
  MANUAL_REQUIRE(space.rid_issued(rid));
  MANUAL_REQUIRE(!space.house_owner_rid(actual_owner, sources, unchanged, error));
}
