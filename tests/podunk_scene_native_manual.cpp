#include "platform/ctr/podunk_scene_native.hpp"

// Manual entry only; production builds never run this function. No source
// scene, Ready owner, GPU texture or gameplay fixture is manufactured here.
bool podunk_scene_native_manual_missing_owners() {
  encore::ctr::PodunkSceneNative native;
  std::string error;
  encore::upstream::FieldObjectId object = 42;
  if (native.physics_admitted(error) || error.empty())
    return false;
  error.clear();
  if (native.source_object(1, object, error) || object != 42 || error.empty())
    return false;
  error.clear();
  if (native.begin_draw(1, 0, error) || error.empty())
    return false;
  error.clear();
  if (native.set_disabled(1, false, error) || error.empty())
    return false;
  encore::upstream::FieldNodeDescriptor unknown;
  unknown.native_class = "RigidBody2D";
  if (native.owns(unknown))
    return false;
  encore::ctr::PodunkPlayerPhysicsWorld world;
  error.clear();
  if (world.admit_static_monitor(1, uint32_t(0), error) || error.empty())
    return false;
  error.clear();
  if (world.static_monitor_exit(1, error) || error.empty())
    return false;
  return true;
}
