// Complete source getshovel candidate, using a post-tutorial pose fixture.
// No Map/graphics/root-contact/pickup/emulator or natural-opening claim.
#include "encore/field_objects.hpp"
#include "encore/world.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Shovel programme %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
bool close(Vec2 a,Vec2 b){return std::abs(a.x-b.x)<=1.01f&&std::abs(a.y-b.y)<=1.01f;}
}
int main(int argc,char**argv){
 CHECK(argc==3);RoomData room;FieldObjectsData metadata;CHECK(room.load_file(argv[1],error)&&metadata.load_file(argv[2],error));const auto view=room.view();CHECK(view.program_count()==8);
 uint32_t programme=kRoomNoIndex;for(uint32_t i=0;i<view.program_count();++i)if(view.string(view.program(i).source_path_string)=="Podunk/cutscenes/gravedigger_getshovel")programme=i;
 CHECK(programme!=kRoomNoIndex);const auto p=view.program(programme);CHECK(p.command_count==68&&p.phrase_count==10);
 unsigned yields=0,erase=0,restore=0,grants=0,del=0;uint32_t ninten=view.scene().player_instance_index,iggy=kRoomNoIndex;
 for(uint32_t i=0;i<p.command_count;++i){auto c=view.command(p.first_command+i);yields+=c.opcode==25;grants+=c.opcode==48;del+=c.opcode==49;if(c.opcode==47){++erase;iggy=c.actor_index;CHECK(c.phrase==9);}if(c.opcode==20){++restore;CHECK(c.actor_index==ninten);}}
 CHECK(yields==4&&erase==1&&restore==1&&grants==0&&del==1&&iggy!=kRoomNoIndex);
 Vec2 previous{};
 for(unsigned repetition=0;repetition<2;++repetition){
  OpeningWorld world;FieldObjects objects;SourceRandom random(9);CHECK(world.initialize(view)&&world.warp_same_scene({1432,4719},{0,-1})&&world.set_initial_actor_pose(iggy,{1432,4720},{-1,0}));
  CHECK(objects.prepare(metadata,view,error)&&objects.enter_ready(1,false,random,error)&&objects.idle_frame(error)&&world.bind_story_objects(objects,error));
  CHECK(world.begin_house_program(programme));CHECK(!world.story_flag("got_shovel")&&objects.state(1)->alive); // source initial actor idle boundary is real
  uint32_t last=kRoomNoIndex;unsigned texts=0,phase2_ticks=0;bool queued=false,removed=false,saw_wait=false;const unsigned hold=repetition?18:5;
  auto frame=[&]{CHECK(world.advance({})&&world.idle_frame(1.0/60));CHECK(objects.advance_idle(1.0/60,error)&&objects.idle_frame(error));
   if(objects.state(1)->queued&&objects.state(1)->alive){CHECK(world.phrase()==3&&world.story_flag("got_shovel")&&objects.state(1)->visible);queued=true;}
   CHECK(world.end_scene_frame()&&objects.flush_queued(error));
   if(queued){CHECK(!objects.state(1)->alive);removed=true;}
  };
  for(unsigned tick=0;tick<2400&&!world.story_completed();++tick){
   frame();const auto id=world.pending_dialogue_id();
   if(world.phrase()==2){++phase2_ticks;if(phase2_ticks<=20)CHECK(close(world.actor(ninten).position,{1432,4719}));saw_wait=true;}
   if(id!=kRoomNoIndex){
    CHECK(id>=3200&&id<=3204&&world.story_flag("got_shovel"));
    if(id!=last){++texts;last=id;for(unsigned i=0;i<hold;++i){frame();CHECK(world.pending_dialogue_id()==id);}}
    if(id<3202)CHECK(objects.state(1)->alive&&!objects.state(1)->queued);else CHECK(!objects.state(1)->alive);
    CHECK(world.finish_story_dialogue());
   }
  }
  CHECK(texts==5&&saw_wait&&phase2_ticks>=179&&queued&&removed&&world.story_completed()&&world.stage()==OpeningStage::Walking);
  CHECK(world.story_flag("got_shovel")&&world.story_flag("returned_shovel")&&!world.instance_visible(iggy)&&!world.actor_bound(iggy));
  CHECK(world.actor_cleanup_pending()&&world.actor_bound(ninten));CHECK(world.idle_frame(0)&&world.actor_cleanup_pending());frame();CHECK(!world.actor_cleanup_pending()&&!world.actor_bound(ninten));
  CHECK(close(world.player().position,{1408,4720}));CHECK(world.player().direction.x==-1&&world.player().direction.y==0);
  if(repetition)CHECK(world.player().position.x==previous.x&&world.player().position.y==previous.y);previous=world.player().position;
  const auto revision=world.story_revision();CHECK(!world.begin_house_program(programme)&&world.healthy()&&world.story_revision()==revision); // dead object preflight prevents replay mutations
  std::printf("Shovel repetition%u: texts%u phase2ticks%u final%.3f,%.3f Iggy-hidden; object removed\n",repetition,texts,phase2_ticks,previous.x,previous.y);
 }
 std::printf("Full shovel candidate: %u checks;10labels/68commands, five texts, half-second actor wait, deferred object deletion, live actor erasure before returned flag, only Ninten restored; no frontend claim\n",checks);
}
