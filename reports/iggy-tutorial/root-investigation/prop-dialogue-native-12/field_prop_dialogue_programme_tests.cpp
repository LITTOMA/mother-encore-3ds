// Actorless reusable programme execution only. No source ray/picking claim.
#include "encore/world.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool value,const char*why){++checks;if(!value){std::fprintf(stderr,"Prop dialogue %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[2];RoomData data;ResourceCatalog catalog;FieldSwingResources action;
 CHECK(data.load_file(argv[1],error)&&catalog.load_file((root+"/system/native.encresources").c_str(),error)&&action.load(catalog,root.c_str(),error));const auto room=data.view();CHECK(room.program_count()>=2);
 for(unsigned branch=0;branch<2;++branch)for(unsigned hold:{5u,180u}){
  const auto programme=room.program_count()-2+branch;const auto row=room.program(programme);CHECK(row.command_count==7);const auto shown=room.command(row.first_command+1);CHECK(shown.opcode==uint16_t(DialogueActionKind::ShowDialogue)&&shown.actor_index==kRoomNoActor&&(shown.flags&1));
  OpeningWorld world;CHECK(world.initialize(room)&&world.warp_same_scene({1300,4700},{0,1})&&world.bind_player_action(action,error));const auto position=world.player().position;const auto audio_count=world.audio_request_count();std::vector<bool>flags;for(uint32_t i=0;i<room.flag_count();++i)flags.push_back(world.story_flag_at(i));CHECK(world.begin_house_program(programme));
  auto frame=[&](WalkInput input){CHECK(world.advance(input)&&world.idle_frame(1.0/60)&&world.end_scene_frame());};
  for(unsigned i=0;i<hold;++i){frame({1,0,false,false,false});CHECK(world.stage()==OpeningStage::ScriptRunning&&world.pending_dialogue_id()==shown.target_index&&world.story_input_allowed());CHECK(world.player().position.x==position.x&&world.player().position.y==position.y&&!world.actor_bound(room.scene().player_instance_index));}
  CHECK(world.finish_story_dialogue());for(unsigned i=0;i<10&&!world.story_completed();++i)frame({});CHECK(world.story_completed()&&world.stage()==OpeningStage::Walking&&!world.house_paused()&&world.pending_dialogue_id()==UINT32_MAX&&!world.actor_cleanup_pending());
  for(uint32_t i=0;i<room.flag_count();++i)CHECK(world.story_flag_at(i)==flags[i]);CHECK(world.story_talker().kind==DialogueTalkerKind::None);CHECK(world.audio_request_count()==audio_count);
  for(unsigned i=0;i<8;++i)frame({1,0,false,false,false});CHECK(world.player().position.x>position.x&&world.player().position.y==position.y);CHECK(!world.actor_bound(room.scene().player_instance_index));
 }
 std::printf("Prop dialogue programme: %u checks; both actorless reusable messages, two manual holds, no flag/audio changes, no replacement player and restored movement. Offline candidate; live ray and UI unverified.\n",checks);
}
