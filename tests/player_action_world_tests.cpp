#include "encore/world.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Player world %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void seal(std::vector<uint8_t>&b){put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];ResourceCatalog catalog;CHECK(catalog.load_file((root+"/system/native.encresources").c_str(),error));FieldSwingResources resources,empty;CHECK(resources.load(catalog,root.c_str(),error));RoomData room;CHECK(room.load_file((root+"/world/house/opening.encroom").c_str(),error));
 OpeningWorld world;CHECK(world.initialize(room.view()));CHECK(!world.bind_player_action(empty,error)&&!world.player_action().ready());
 {ResourceCatalog legacy_catalog;FieldSwingResources legacy;CHECK(legacy_catalog.load_file((root+"/system/legacy.encresources").c_str(),error));CHECK(legacy.load(legacy_catalog,root.c_str(),error)&&legacy.entry().capability()==1);CHECK(!world.bind_player_action(legacy,error)&&!world.player_action().ready()&&world.healthy());}
 CHECK(world.pause_for_house());CHECK(world.warp_same_scene({143,702},{0,1}));CHECK(world.unpause_from_house());CHECK(world.bind_player_action(resources,error));CHECK(!world.bind_player_action(resources,error)&&world.player_action().ready());
 const auto*identity=world.persistent_player();CHECK(world.advance({1,0,false,false,false}));CHECK(world.idle_frame(1.0/60)&&world.end_scene_frame());
 CHECK(world.advance({1,0,false,false,false}));CHECK(world.idle_frame(1.0/60)&&world.end_scene_frame());const auto before=world.player();CHECK(before.velocity.x!=0);
 bool accepted=true;CHECK(world.player_action_input(resources.action().strings()[2],true,false,false,false,accepted,error)&&!accepted);CHECK(world.take_player_action_events().empty());
 CHECK(world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);CHECK(!world.player().running&&!world.player().walking&&!world.player().crouch&&!world.player().tap_run);CHECK(same(world.player().velocity,before.velocity));CHECK(world.player_action().attacking());
 auto events=world.take_player_action_events();CHECK(events.size()==1&&events[0]==PlayerActionEvent::Start);
 bool pending=false;unsigned ticks=0;for(;ticks<60&&world.player_action().attacking();++ticks){CHECK(world.advance({1,0,true,false,false}));CHECK(same(world.player().position,before.position)&&same(world.player().velocity,before.velocity));CHECK(world.idle_frame(1.0/60));if(world.player_action().state_id()==resources.entry().idle()&&ticks>4){CHECK(world.player_action().attacking());pending=true;}CHECK(world.end_scene_frame());}
 CHECK(ticks<60&&pending&&!world.player_action().attacking());CHECK(world.persistent_player()==identity);CHECK(world.advance({1,0,false,false,false}));CHECK(!same(world.player().position,before.position));CHECK(world.idle_frame(1.0/60)&&world.end_scene_frame());
 CHECK(world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);
 for(unsigned i=0;i<12;++i)CHECK(world.advance({})&&world.idle_frame(1.0/60)&&world.end_scene_frame());CHECK(world.player_action().state_id()==resources.entry().attack());
 const auto velocity=world.player().velocity;const auto time=world.player_action().position();CHECK(world.pause_for_house());CHECK(world.house_paused()&&world.player_action().paused()&&!world.player_action().attacking()&&same(world.player().velocity,velocity));CHECK(world.idle_frame(1.0/60)&&world.end_scene_frame());CHECK(world.player_action().position()>time);
 CHECK(world.unpause_from_house());const auto resumed=world.player_action().position();CHECK(world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);CHECK(world.player_action().position()==resumed&&world.player_action().starts()==3);
 events=world.take_player_action_events();CHECK(events.size()==4&&events[0]==PlayerActionEvent::Start&&events[1]==PlayerActionEvent::Pause&&events[2]==PlayerActionEvent::Resume&&events[3]==PlayerActionEvent::Start);
 CHECK(world.pause_for_house());OpeningWorld destination;CHECK(destination.initialize(room.view()));CHECK(destination.pause_for_house());CHECK(destination.bind_player_action(resources,error));
 ScenePlayerTransfer stale;CHECK(destination.prepare_scene_player(world,{145,702},{0,0},stale,error));CHECK(world.player_action_input("unrelated",false,true,false,false,accepted,error)&&!accepted);CHECK(!destination.commit_scene_player(world,std::move(stale),error));CHECK(world.healthy()&&destination.healthy()&&world.persistent_player()==identity);
 stale=ScenePlayerTransfer{}; // A rejected token still pins its two owners.
 ScenePlayerTransfer transfer;const auto starts=world.player_action().starts(),frame=world.player_action().pose().frame;const auto clock=world.player_action().position();CHECK(destination.prepare_scene_player(world,{145,702},{0,0},transfer,error));CHECK(destination.commit_scene_player(world,std::move(transfer),error));CHECK(destination.persistent_player()==identity&&!world.healthy()&&destination.house_paused());CHECK(destination.player_action().starts()==starts&&destination.player_action().pose().frame==frame&&destination.player_action().position()==clock);
 events=destination.take_player_action_events();CHECK(events.size()==1&&events[0]==PlayerActionEvent::Pause);CHECK(!world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error));CHECK(world.take_player_action_events().empty());CHECK(destination.unpause_from_house());CHECK(destination.idle_frame(1.0/60)&&destination.end_scene_frame());
 CHECK(destination.animation().frame==destination.persistent_player()->animation().frame&&destination.animation().frame==destination.player_action().main_frame());
 // A correctly parsed room with a changed reachable frame must still fail
 // cross-binding atomically. Unreachable Idle Right keys are not frame data
 // that this tree can execute; both source proofs explicitly retain that fact.
 const auto view=room.view();const auto profile=view.actor_profile(view.actor_instance(view.scene().player_instance_index).profile_index);std::vector<uint8_t>original;CHECK(encore::read_file((root+"/world/house/opening.encroom").c_str(),original,kMaxRoomBytes,error));
 uint32_t moving=kRoomNoIndex,unreachable=kRoomNoIndex;for(uint32_t i=0;i<profile.animation_binding_count;++i){const auto binding=view.animation_binding(profile.animation_binding_first+i);const auto clip=view.clip(binding.clip_index);if(binding.motion_state==1&&binding.direction==0)moving=clip.first_key;for(uint32_t j=0;j<clip.key_count;++j)if(view.key(clip.first_key+j).time>clip.length)unreachable=clip.first_key+j;}
 CHECK(moving!=kRoomNoIndex&&unreachable!=kRoomNoIndex);
 {auto bytes=original;const auto at=view.section_offset(RoomSection::Key)+moving*8+4;bytes[at]^=1;seal(bytes);RoomData changed;CHECK(changed.load(bytes.data(),bytes.size(),error));OpeningWorld rejected;CHECK(rejected.initialize(changed.view()));CHECK(!rejected.bind_player_action(resources,error)&&!rejected.player_action().ready()&&rejected.healthy());}
 {auto bytes=original;const auto at=view.section_offset(RoomSection::Key)+unreachable*8+4;bytes[at]^=1;seal(bytes);RoomData changed;CHECK(changed.load(bytes.data(),bytes.size(),error));OpeningWorld compatible;CHECK(compatible.initialize(changed.view()));CHECK(compatible.bind_player_action(resources,error));}
 // Player ATTACK must not gate the independent source Area callbacks.
 {OpeningWorld area;CHECK(area.initialize(view));CHECK(area.bind_player_action(resources,error));CHECK(area.warp_same_scene({430,397},{1,0}));CHECK(area.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);CHECK(area.advance({})&&area.idle_frame(1.0/60));CHECK(area.cutscene_active()&&area.has_cutscene_actors()&&area.player_action().paused()&&!area.player_action().attacking());CHECK(area.end_scene_frame());unsigned frames=0;for(;frames<1200&&area.stage()!=OpeningStage::BattleRequested;++frames)CHECK(area.advance({})&&area.idle_frame(1.0/60)&&area.end_scene_frame());CHECK(frames<1200&&area.healthy()&&area.battle_request().requested&&area.player_action().paused()&&area.audio_request_count()==5);}
 {OpeningWorld bounded;CHECK(bounded.initialize(view)&&bounded.bind_player_action(resources,error));for(unsigned i=0;i<64;++i)CHECK(bounded.pause_for_house());CHECK(!bounded.pause_for_house()&&!bounded.healthy());}
 std::printf("Persistent player action World: %u checks; live motion/B input, velocity retention, deferred release, pause/repeat, ordered audio events and revision-checked scene transfer; no GPU/audio/emulator/hardware claim\n",checks);
}
