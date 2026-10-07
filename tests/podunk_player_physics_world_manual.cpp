#include "platform/ctr/podunk_player_physics_world.hpp"
#include <cassert>
using namespace encore::upstream;
using namespace encore::ctr;
// Explicit manual driver only. This file is compiled, never auto-executed.
void podunk_player_physics_world_negative_manual() {
  PodunkPlayerPhysicsWorld world;
  std::string error;
  assert(!world.physics_step(1, error));
  assert(!world.flush_queries(error));
  assert(!world.disabled(1, true, error));
  FieldDeferredMessage message;
  message.object = 1;
  message.member = "_body_enter_tree";
  assert(!world.handles_callback(message));
  assert(!world.deferred(message, error));
  FieldGeometrySpace space;
  assert(!space.rid_alive({&space, 1}));
  assert(!space.rid_issued({&space, 1}));
  assert(!space.remove_player_shape(1, error));
  assert(!space.set_player_shape_disabled(1, true, error));
  assert(!space.set_player_collision_mask(1, 32, true, error));
  assert(!space.retire_player_owner(1, error));
  FieldPhysicsRid rid;
  assert(!space.player_owner_rid(1, rid, error));
}
