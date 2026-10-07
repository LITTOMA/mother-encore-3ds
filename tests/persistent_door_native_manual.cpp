// Manual source cases; not registered, built or run by this slice.
// Call with a fresh actual source fixture for each stage. Detach, old free,
// same-space replacement and Registry migration use the real source driver.
#include "../platform/ctr/podunk_scene_native.hpp"
#include "../platform/ctr/podunk_scene_scripts.hpp"
#include <cassert>
using namespace encore::upstream;
using namespace encore::ctr;

void persistent_door_native_before_detach_manual(PodunkSceneNative&native,
    const FieldDoorData&doors,uint32_t stable){
  std::string e;PodunkSceneDoorTransfer receipt;
  assert(!native.retain_detached_door(doors,stable,receipt,e));
  assert(!receipt.door());
}
void persistent_door_native_after_detach_manual(PodunkSceneNative&native,
    const FieldDoorData&doors,uint32_t stable,FieldGeometrySpace&space,
    FieldNodeTreeRuntime&old,FieldNodeTreeRuntime&destination,
    PodunkSceneDoorTransfer&retained){
  std::string e;assert(native.retain_detached_door(doors,stable,retained,e));
  assert(retained.old_tree()==&old&&!retained.destination_tree());
  assert(retained.objects().size()==4&&retained.rid().space==&space);
  assert(space.rid_alive(retained.rid()));
  assert(!old.state(retained.door())->inside&&!old.state(retained.door())->parent);
  PodunkSceneDoorTransfer duplicate;
  assert(!native.retain_detached_door(doors,stable,duplicate,e));
  assert(!native.rebind_persistent_door(retained,old,destination,retained.objects(),e));
  assert(!retained.destination_tree()&&space.rid_alive(retained.rid()));
  auto wrong=retained.objects();wrong.push_back(retained.door());
  assert(!native.rebind_persistent_door(retained,old,destination,wrong,e));
  assert(!native.observe_persistent_door(retained,destination.root(),e));
}
void persistent_door_native_after_reenter_manual(PodunkSceneNative&native,
    const PodunkSceneDoorTransfer&retained,FieldGeometrySpace&space,
    FieldObjectId actual_parent){
  std::string e;assert(native.observe_persistent_door(retained,actual_parent,e));
  assert(space.rid_alive(retained.rid()));
  assert(!native.observe_persistent_door(retained,0,e));
  assert(!native.phase(retained.door(),FieldTreePhase::ReadyNative,e));
  assert(!native.phase(retained.door(),FieldTreePhase::Deleting,e));
  FieldPersistentDoorGeometry actual;
  assert(space.persistent_door(retained.door(),actual,e));
  assert(actual.contact.actual_owner==retained.door());
  assert(actual.contact.actual_shape==retained.shape());
  assert(actual.rid.space==retained.rid().space&&actual.rid.handle==retained.rid().handle);
}
void persistent_door_native_after_source_free_manual(PodunkSceneNative&native,
    const PodunkSceneDoorTransfer&retained,FieldGeometrySpace&space){
  // Only after actual source Door.queue_free and SceneTree deletion flush.
  assert(!native.owns(retained.door())&&!native.owns(retained.shape())&&!native.owns(retained.marker()));
  assert(!space.rid_alive(retained.rid()));
}
void persistent_door_source_after_native_free_manual(PodunkSceneScripts&scripts,
    const FieldDoorData&doors,const FieldDoorRuntime&runtime,
    const PodunkSceneDoorTransfer&retained,const FieldGlobalRegistry&registry){
  // Actual fixture stops after the native delete/ObjectDB collection and
  // before the old source collector closes its retained instance.
  assert(runtime.data()==&doors&&runtime.phase()==FieldDoorPhase::Done);
  assert(retained.destination_tree()&&!registry.object_exists(retained.door()));
  std::string e;auto bad_rid=retained.rid();bad_rid.handle=0;
  assert(!scripts.lifecycle().commit_persistent_door_deleted(doors,runtime,
      runtime.active_door(),*retained.destination_tree(),registry,retained.door(),
      bad_rid,retained.objects(),e));
  assert(!scripts.lifecycle().commit_persistent_door_deleted(doors,runtime,
      0,*retained.destination_tree(),registry,retained.door(),retained.rid(),
      retained.objects(),e));
  const std::set<FieldObjectId>objects(retained.objects().begin(),retained.objects().end());
  if(registry.pending_messages_to(objects)){
    // Use an actual fixture with a queued source callback; do not drain or
    // discard it to manufacture a successful deletion lease.
    assert(!scripts.collect_deleted(e));return;
  }
  assert(scripts.collect_deleted(e));FieldSceneScriptAdmission receipt;
  assert(!scripts.admission(retained.door(),receipt));
  assert(!scripts.lifecycle().script_admission(runtime.active_door(),receipt));
}
