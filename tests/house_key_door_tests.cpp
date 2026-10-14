#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <cstdlib>
#include <iostream>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Keys:HouseKeyInventory {
 bool held=false,reject=false,read_failure=false;unsigned reads=0;
 bool validate_key(std::string_view n,std::string&)override{return !reject&&!n.empty();}
 bool has_key(std::string_view,bool&out,std::string&)override{++reads;if(read_failure)return false;out=held;return true;}
};
}
int main(int argc,char**argv){
 check(argc==4,"House, Room and battle/font resources required");HouseData house;RoomData room;BattleData font;std::string error;
 check(house.load_file(argv[1],error),error.c_str());check(room.load_file(argv[2],error),error.c_str());check(font.load_file(argv[3],error),error.c_str());
 const auto view=house.view();uint32_t door=house_no_index;
 for(uint32_t i=0;i<view.count(HouseSection::OpenableDoors);++i)if(!view.string(view.openable_door(i).key).empty()){check(door==house_no_index,"Bounded source has exactly one key door");door=i;}
 check(door!=house_no_index,"Source key door exists");const auto data=view.openable_door(door);const auto flag=view.string(data.flag);
 check(!(data.policy&uint32_t(HouseDoorPolicy::RemoveKey)),"Original basement door retains key");
 auto fixture=[&](OpeningWorld&w,HousePresentation&p,HouseRuntime&r,SourceRandom&random,Keys&keys){
  check(w.initialize(room.view()),w.error());w.attach_random(random);check(p.begin(view,font.view(),random),p.error());check(r.initialize(view,w,p),r.error());check(r.set_player_nickname("Ana"),"Use actual nickname in PartyLead text");
  check(r.bind_key_inventory(keys),r.error());check(w.warp_same_scene({data.interact_center.x,data.interact_center.y+4},{0,-1}),"Face actual door ray");
 };
 auto finish=[&](OpeningWorld&w,HousePresentation&p,HouseRuntime&r){
  for(unsigned frame=0;frame<2000&&r.phase()==HousePhase::Dialogue;++frame){check(p.physics_frame(1./60,w.player().position),p.error());check(p.idle_frame(1./60),p.error());check(r.idle_frame(1./60,true,false),r.error());}
  check(r.phase()==HousePhase::Idle&&!w.house_paused(),"Door dialogue restores walking");
 };
 OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;SourceRandom random(47);Keys keys;
 fixture(world,presentation,runtime,random,keys);
 check(runtime.door_interaction_supported(door),"Locked door advertises reviewed investigation");
 check(runtime.idle_frame(1./60,true,false),runtime.error());check(runtime.phase()==HousePhase::Dialogue&&world.house_paused(),"No key opens source locked dialogue");
 check(!world.story_flag(flag)&&!runtime.openable_state(door).unlocked&&world.body_enabled(data.player_body_id),"Missing key leaves flag and blocker unchanged");check(runtime.take_sounds().empty(),"Missing key does not open/play door");finish(world,presentation,runtime);
 Keys rejected;rejected.reject=true;check(!runtime.bind_key_inventory(rejected),"Reject unknown key inventory policy before interaction");keys.held=true;
 const auto first=runtime.events().size();check(runtime.idle_frame(1./60,true,false),runtime.error());
 check(keys.reads==2&&!rejected.reads,"Rejected rebind preserves admitted inventory query");
 check(world.story_flag(flag)&&runtime.openable_state(door).unlocked&&runtime.openable_state(door).collision_pending,"Flag/open/unlock executes before source deferred collider");
 check(world.body_enabled(data.player_body_id),"Collider is not synchronously rewritten at key callback");
 check(runtime.events().size()==first+4&&runtime.events()[first].kind==HouseEventKind::OpenableFlagWritten&&runtime.events()[first+1].kind==HouseEventKind::OpenableOpened&&runtime.events()[first+2].kind==HouseEventKind::OpenableUnlocked&&runtime.events()[first+3].kind==HouseEventKind::DoorDialogueOpened,"Original flag, open, unlock, text ordering");
 auto sound=runtime.take_sounds();check(sound.size()==1&&sound[0].sound==data.start_sound,"One source opening sound");
 check(runtime.after_physics()&&!world.body_enabled(data.player_body_id),"Deferred unlock disables player collider");
 check(runtime.idle_frame(1./60,false,false)&&!runtime.openable_state(door).sprite_visible&&runtime.openable_state(door).nonplayer_disabled,"Action animation applies source visibility/nonplayer collider masks");
 finish(world,presentation,runtime);const auto events=runtime.events().size();const auto reads=keys.reads;
 check(runtime.idle_frame(1./60,true,false)&&runtime.phase()==HousePhase::Idle&&!world.house_paused(),"Repeated unlocked investigation remains walking");check(runtime.events().size()==events&&keys.reads==reads&&keys.held&&runtime.take_sounds().empty(),"Repeat has no key, flag, dialogue or sound side effects");
 // Both new routes use the same checked House warp execution as older rooms.
 auto step=[&](WalkInput input){check(runtime.before_physics(input),runtime.error());check(presentation.physics_frame(1./60,world.player().position),presentation.error());check(world.advance(input),world.error());check(runtime.after_physics(),runtime.error());check(world.idle_frame(1./60),world.error());check(presentation.idle_frame(1./60),presentation.error());check(runtime.idle_frame(1./60,false,false),runtime.error());};
 for(uint32_t route:{8u,9u}){const auto target=view.door(route);check(world.warp_same_scene(target.center,target.direction),"Place at source doorway");
  bool done=false;const auto before=runtime.events().size();for(unsigned frame=0;frame<300&&!done;++frame){step({});for(size_t e=before;e<runtime.events().size();++e)done|=runtime.events()[e].kind==HouseEventKind::DoorDone&&runtime.events()[e].object==route;}
  check(done,"Basement source warp completes");check(world.player().position.x==target.destination.x&&world.player().position.y==target.destination.y,"Exact source scaled basement destination");while(runtime.phase()!=HousePhase::Idle)step({});
 }
 check(world.story_flag(flag)&&keys.held,"Round trip preserves unlock and retained key");
 OpeningWorld restored;HousePresentation restored_presentation;HouseRuntime restored_runtime;SourceRandom restored_random(49);
 check(restored.initialize(room.view())&&restored.set_story_flag(flag,true,false),"Restore saved source unlock flag");restored.attach_random(restored_random);check(restored_presentation.begin(view,font.view(),restored_random),restored_presentation.error());check(restored_runtime.initialize(view,restored,restored_presentation)&&restored_runtime.bind_key_inventory(keys),restored_runtime.error());
 check(restored_runtime.openable_state(door).unlocked&&!restored_runtime.door_interaction_supported(door),"Reinstantiation honors unlock flag and hides prompt");
 OpeningWorld failed;HousePresentation failed_presentation;HouseRuntime failed_runtime;SourceRandom failed_random(51);Keys failing;failing.read_failure=true;
 fixture(failed,failed_presentation,failed_runtime,failed_random,failing);check(!failed_runtime.idle_frame(1./60,true,false),"Inventory read failure rejects interaction");check(!failed.story_flag(flag)&&!failed.house_paused()&&failed_runtime.take_sounds().empty(),"Rejected inventory read preserves flag, pause and audio");
 std::cout<<"House key door: "<<checks<<" checks; missing key, source ordering, deferred collider, repeat, restoration, failed inventory admission/read\n";
}
