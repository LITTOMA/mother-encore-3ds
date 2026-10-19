#include "encore/actor_actions.hpp"
#include "encore/room_data.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"NPC sprite %u: %s\n",checks,why);std::exit(1);}}
}
int main(int argc,char**argv){
 check(argc==3,"original Room and native reference required");std::string error;RoomData data;
 check(data.load_file(argv[1],error),error.c_str());auto room=data.view();
 ActorActionState actors[8];for(unsigned i=0;i<8;++i)check(initialize_actor(actors[i],room,i>=4&&i<=6?5:(i==7?4:1),{0,0},{0,1}),"initialize source NPC sprite profile");
 std::ifstream ref(argv[2]);check(ref.good(),"native source fixture opens");
 for(unsigned tick=0;tick<120;++tick)for(unsigned i=0;i<8;++i){
  unsigned frame,id,want;float bx,by,dx,dy;std::string state,raw_talking;
  check(bool(ref>>frame>>id>>want>>state>>bx>>by>>dx>>dy>>raw_talking)&&frame==tick&&id==i,"native rows are ordered and complete");
  const bool talking=raw_talking=="true";check(talking||raw_talking=="false","native talking bool");
  const bool mute=i==2&&tick>=12&&tick<35;
  auto& a=actors[i];check(npc_sprite_physics(a,{bx,by},talking&&!mute,i==7),"source NPC discrete blend/travel");
  check(actor_idle_animations(a),"source idle animation clock");
  if(a.frame!=want||a.playing_motion!=(state=="Talk"?5:(state=="Walk"?4:0))){std::fprintf(stderr,"frame %u case %u: got %u/%u expected %u/%s\n",tick,i,a.frame,a.playing_motion,want,state.c_str());check(false,"native AnimationTree outcome differs");}
  ++checks;
 }
 std::string extra;check(!(ref>>extra)&&ref.eof(),"no extra native rows");
 auto actor=actors[0];check(!npc_sprite_blend(actor,{0,0})&&!npc_sprite_blend(actor,{NAN,1}),"bad NPC direction rejected");
 ActorActionState player;check(initialize_actor(player,room,0,{0,0},{0,1}),"source Player profile");check(!npc_sprite_blend(player,{1,0}),"unreplaced NPC execution cannot borrow player");
 check(actor.frame==actors[0].frame&&actor.blend_direction.x==actors[0].blend_direction.x&&actor.blend_direction.y==actors[0].blend_direction.y,"negative path preserves prior state");
 std::printf("NPC source sprite: %u checks; 960 native AnimationTree frame/state rows match; original 4/8 direction, Talk key delay/end, mute, looking and Walk idle; no map/hardware claim\n",checks);
}
