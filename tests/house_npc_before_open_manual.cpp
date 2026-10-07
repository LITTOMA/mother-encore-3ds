// Manual only; not registered or run. The live case replaces the one actual
// host.open_program call at an authorized npc.gd BeforeOpen source invocation.
// It must not be called after that invocation already opened its DialogueBox.
#include "../platform/ctr/house_return_npc_runtime.hpp"
namespace encore::manual {
// Supply real native busy state (e.g. an actual open input traversal, an
// actual dialogue callback, or a queued message to dialogue ownership history).
// Do not replace those owners/counters with a synthetic bool or receipt.
bool house_npc_before_open_busy_manual(const ctr::HouseReturnNpcRuntime&source,
 ctr::HouseReturnDialogueNativeOwner&native,ctr::HouseReturnDialogue&driver,
 const upstream::HouseSourceNpcProgramme&actual,uint32_t generation,std::string&e){
 if(!source.before_open(actual,generation,e))return false;
 const auto before=driver.context();std::string error;
 if(native.admit_npc_before_open(source,actual,generation,error)||error.empty()){
  e="NPC manual BeforeOpen ignored an actual busy native frame";return false;
 }
 error.clear();
 if(driver.request_npc_programme(source,actual,error)||error.empty()||
    driver.context().dialogue!=before.dialogue||driver.context().generation!=before.generation){
  e="NPC manual actual busy native state minted a request scope";return false;
 }
 e.clear();return true;
}
bool house_npc_before_open_inactive_manual(const ctr::HouseReturnNpcRuntime&source,
 ctr::HouseReturnDialogueNativeOwner&native,ctr::HouseReturnDialogue&driver,
 const upstream::HouseSourceNpcProgramme&copied,uint32_t generation,std::string&e){
 std::string error;
 if(source.before_open(copied,generation,error)||error.empty()){
  e="NPC manual copied receipt incorrectly created a live BeforeOpen prefix";return false;
 }
 error.clear();
 if(native.admit_npc_before_open(source,copied,generation,error)||error.empty()){
  e="NPC manual absent source scope was admitted by native owner";return false;
 }
 error.clear();
 const auto before=driver.context();
 if(driver.request_npc_programme(source,copied,error)||error.empty()||
    driver.context().dialogue!=before.dialogue||driver.context().generation!=before.generation){
  e="NPC manual inactive request changed the actual driver";return false;
 }
 e.clear();return true;
}
bool house_npc_before_open_live_manual(const ctr::HouseReturnNpcRuntime&source,
 const ctr::HouseReturnNpcRuntime&foreign,ctr::HouseReturnDialogueNativeOwner&native,
 ctr::HouseReturnDialogue&driver,const upstream::HouseSourceNpcProgramme&actual,
 uint32_t generation,std::string&e){
 if(!native.admit_npc_before_open(source,actual,generation,e))return false;
 const auto before=driver.context();
 auto reject=[&](const upstream::HouseSourceNpcProgramme&bad,uint32_t gen,const char*name){
  std::string error;
  if(native.admit_npc_before_open(source,bad,gen,error)||error.empty()||
     driver.context().dialogue!=before.dialogue||driver.context().generation!=before.generation){
   e=std::string("NPC manual BeforeOpen rejection/preservation failed: ")+name;return false;
  }return true;
 };
 auto bad=actual;bad.object=0;if(!reject(bad,generation,"foreign actual object"))return false;
 bad=actual;bad.source_id^=1;if(!reject(bad,generation,"wrong stable source ID"))return false;
 bad=actual;bad.original_npc=upstream::kRoomNoIndex;if(!reject(bad,generation,"wrong original actor"))return false;
 bad=actual;bad.programme=upstream::kRoomNoIndex;if(!reject(bad,generation,"wrong immutable programme"))return false;
 bad=actual;bad.registry=nullptr;if(!reject(bad,generation,"foreign Registry"))return false;
 bad=actual;bad.tree=nullptr;if(!reject(bad,generation,"foreign actual Tree"))return false;
 bad=actual;bad.source=nullptr;if(!reject(bad,generation,"foreign source runtime"))return false;
 bad=actual;bad.thoughts=true;if(!reject(bad,generation,"telepathy relabelled ordinary"))return false;
 if(!reject(actual,generation^1u,"stale world generation"))return false;
 std::string error;
 if(&foreign==&source||native.admit_npc_before_open(foreign,actual,generation,error)||error.empty()){
  e="NPC manual foreign concrete source owner did not reject";return false;
 }
 // Strict source-frame closure remains unavailable after the real source
 // talker prefix. This verifies that transfer/inventory cannot borrow it.
 error.clear();
 if(driver.source_frame_closed(*driver.world(),error)||error.empty()){
  e="NPC manual source prefix incorrectly granted a strict closed frame";return false;
 }
 if(!driver.request_npc_programme(source,actual,e))return false;
 upstream::HouseProgrammeState state;
 auto next=generation+1;if(!next)++next;
 if(!driver.observe_house_programme(state,e)||state.phase!=upstream::HouseProgrammePhase::WaitingReady||
    state.native_closed||state.programme!=actual.programme||state.original_npc!=actual.original_npc||
    state.request_generation!=next||!driver.context().dialogue||driver.context().talker!=actual.object){
  e="NPC manual source prefix did not retain its actual factory/Ready lease";return false;
 }
 // A second request while WaitingReady must retain the first actual lease.
 const auto lease=driver.context();error.clear();
 if(driver.request_npc_programme(source,actual,error)||error.empty()||
    driver.context().dialogue!=lease.dialogue||driver.context().generation!=lease.generation){
  e="NPC manual duplicate request replaced its actual Ready lease";return false;
 }
 e.clear();return true;
}
}
