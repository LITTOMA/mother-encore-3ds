// Manual cases using independently prepared actual source fixtures. This file
// is deliberately not registered, compiled or run by the retirement slice.
#include "../platform/ctr/podunk_scene_scripts.hpp"
#include <algorithm>
#include <cassert>
using namespace encore::upstream;
using namespace encore::ctr;

void old_scene_retirement_before_source_free_manual(PodunkSceneScripts&scripts,
    const FieldDoorData&doors,FieldDoorRuntime&door,FieldObjectId root,
    FieldObjectId held,const FieldNodeTreeRuntime&next,FieldSceneRetirement&receipt,
    uint32_t actual_nonlandmark){
  std::string e;FieldSceneRetirement arbitrary,wrong;
  assert(!scripts.commit_old_scene_retirement(arbitrary,next,next.root(),e));
  assert(!scripts.stage_old_scene_retirement(held,doors,door,held,wrong,e));
  assert(!wrong.old_root());
  assert(!scripts.lifecycle().commit_deleted(actual_nonlandmark,e));
  assert(scripts.stage_old_scene_retirement(root,doors,door,held,receipt,e));
  assert(receipt.old_root()==root&&!receipt.committed());
  assert(std::find(receipt.objects().begin(),receipt.objects().end(),held)==receipt.objects().end());
  assert(!scripts.commit_old_scene_retirement(receipt,next,next.root(),e));
  assert(!receipt.committed()&&!scripts.lifecycle().scene_retired());
  assert(!scripts.stage_old_scene_retirement(root,doors,door,held,wrong,e));
}
void old_scene_retirement_after_actual_free_before_assignment_manual(
    PodunkSceneScripts&scripts,const FieldNodeTreeRuntime&next,
    const FieldGlobalRegistry&registry,FieldSceneRetirement&receipt){
  assert(!receipt.old_tree()->state(receipt.old_root()));
  assert(!registry.object_exists(receipt.old_root()));
  assert(registry.current_scene()!=next.root());
  std::string e;
  assert(scripts.collect_deleted(e)); // Captured nonlandmarks remain pending.
  assert(!scripts.commit_old_scene_retirement(receipt,next,next.root(),e));
  assert(!receipt.committed()&&!scripts.lifecycle().scene_retired());
}
void old_scene_retirement_actual_callback_negative_manual(PodunkSceneScripts&scripts,
    const FieldNodeTreeRuntime&next,const FieldGlobalRegistry&registry,
    const FieldObjectSignals&signals,FieldSceneRetirement&receipt){
  const std::set<FieldObjectId>objects(receipt.objects().begin(),receipt.objects().end());
  // Caller supplies a real pending queue entry or actual synchronous emission.
  assert(registry.pending_messages_to(objects)||signals.active_dispatch_to(objects));
  std::string e;assert(!scripts.commit_old_scene_retirement(receipt,next,next.root(),e));
  assert(!receipt.committed()&&!scripts.lifecycle().scene_retired());
}
void old_scene_retirement_after_actual_assignment_manual(PodunkSceneScripts&scripts,
    const FieldNodeTreeRuntime&next,const FieldGlobalRegistry&registry,
    FieldSceneRetirement&receipt,FieldObjectId held){
  assert(registry.current_scene()==next.root());
  assert(registry.object_exists(held));
  std::string e;FieldSceneScriptAdmission admission;
  assert(scripts.commit_old_scene_retirement(receipt,next,next.root(),e));
  assert(receipt.committed()&&scripts.lifecycle().scene_retired());
  assert(scripts.admission(held,admission));
  for(auto stable:receipt.scripts())assert(!scripts.lifecycle().script_admission(stable,admission));
  assert(!scripts.commit_old_scene_retirement(receipt,next,next.root(),e));
  assert(!scripts.lifecycle().ready_next(e));
}
