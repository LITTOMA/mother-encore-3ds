// Manual source-owner cases only. Not registered, built or executed by this
// slice. A caller supplies a real staged Target and its actual source owners;
// no fixture is allowed to fabricate callback depth or an NPC Ready receipt.
#include "../platform/ctr/house_return_ui_context.hpp"

namespace encore::ctr::manual {
bool house_staged_unentered_finish(HouseReturnDialogue&driver,
    const upstream::FieldNodeTreeRuntime&tree,std::string&e){
  const auto*root=tree.state(tree.root());
  if(!driver.staged()||!root||root->inside){
    e="manual finish refusal requires the real assigned, unentered House root";return false;
  }
  std::string rejected;
  if(driver.finish_ready(rejected)||!driver.staged()){
    e="manual finish admitted an unentered Root or destroyed the staged binding";return false;
  }
  e.clear();return true;
}
bool house_staged_closed_refusals(HouseReturnDialogue&driver,
    HouseReturnNpcRuntime&npc,HouseReturnNpcRuntime&foreign,
    upstream::FieldObjectId actual_npc,std::string&e){
  if(!driver.staged()||!driver.world()||!npc.source_frame_closed()){
    e="manual case requires a real staged owner between source callbacks";return false;
  }
  const auto*world=driver.world();const auto generation=world->source_generation();
  HouseReturnDialogueReceipt inventory;std::string rejected;
  if(driver.source_frame_closed(*world,rejected)||
      driver.inventory_frame(inventory,rejected)||
      driver.request(0,upstream::kRoomNoIndex,rejected)||
      driver.staged_npc_context(npc,actual_npc,rejected)||
      driver.staged_npc_context(foreign,actual_npc,rejected)||
      npc.source_ready_live(actual_npc,rejected)||!driver.staged()||
      world->source_generation()!=generation){
    e="manual staged case admitted ordinary closed/request or a fabricated Ready scope";return false;
  }
  e.clear();return true;
}
// Invoke only from the actual npc.gd Ready/context port. Same-owner succeeds;
// an arbitrary object, another NPC owner and another Player tree must reject.
bool house_staged_live_ready(HouseReturnDialogue&driver,
    HouseReturnNpcRuntime&npc,HouseReturnNpcRuntime&foreign,
    upstream::FieldObjectId actual_npc,upstream::FieldObjectId actual_player,
    const upstream::FieldNodeTreeRuntime&retired,
    const upstream::FieldNodeTreeRuntime&other,std::string&e){
  std::string rejected;
  if(!driver.staged()||!npc.source_ready_live(actual_npc,e)||
      !driver.staged_npc_context(npc,actual_npc,e)||
      !driver.staged_player(npc,actual_npc,actual_player,retired,e))return false;
  if(driver.staged_npc_context(foreign,actual_npc,rejected)||
      driver.staged_npc_context(npc,0,rejected)||
      driver.staged_player(npc,actual_npc,actual_player,other,rejected)||
      driver.source_frame_closed(*driver.world(),rejected)){
    e="manual live Ready scope admitted foreign owner/object/tree or closed transfer";return false;
  }
  e.clear();return true;
}
}
