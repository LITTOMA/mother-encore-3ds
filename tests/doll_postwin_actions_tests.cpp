#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
#include "fixtures/doll_postwin_actions_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const char* name,unsigned tick,const char* field){++checks;if(!ok){if(failures<60)std::fprintf(stderr,"%s tick%u: %s\n",name,tick,field);++failures;}}
bool close(float a,float b){return std::abs(a-b)<.0003f;}
bool close(Vec2 a,Vec2 b){return close(a.x,b.x)&&close(a.y,b.y);}
uint32_t profile(const char* actor){const auto r=encore_test::room();for(uint32_t i=0;i<r.actor_instance_count();++i){const auto a=r.actor_instance(i);const auto name=r.string(a.display_name_string);if((!std::strcmp(actor,"Doll")&&name=="npcdoll")||(!std::strcmp(actor,"Mimmie")&&name=="npc2")||name==actor)return a.profile_index;}return kRoomNoIndex;}
bool apply(ActorActionState&s,const doll_postwin_actions_reference::Action&a){
 if(!std::strcmp(a.op,"path"))return actor_move_path(s,a.entries,a.count,a.speed,a.step?ActorMoveType::Step:ActorMoveType::Position,a.moonwalk,*a.animation?4:UINT16_MAX);
 if(!std::strcmp(a.op,"turn"))return actor_turn(s,{a.x,a.y});
 if(!std::strcmp(a.op,"turn_to"))return actor_turn_to(s,{a.x,a.y},a.length);
 if(!std::strcmp(a.op,"jump"))return actor_jump(s,a.height,a.length);
 if(!std::strcmp(a.op,"anim")&&!std::strcmp(a.animation,"Idle"))return actor_play_clip(s,s.content.actor_profile(s.profile_index).idle_clip);
 return false;
}
}
int main(){
 for(const auto&c:doll_postwin_actions_reference::cases){
  const unsigned before_failures=failures;
  ActorActionState s;check(initialize_actor(s,encore_test::room(),profile(c.actor),c.start,{0,1}),c.name,0,"initialize");
  // The isolated native wrapper does not run _handle_replaced; source blend
  // therefore begins at zero, matching the prior original-Actor oracle.
  s.blend_direction={};
  for(unsigned tick=0;tick<c.frame_count;++tick){
   for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick)check(apply(s,c.actions[i]),c.name,tick,"source action accepted");
   check(actor_physics_step(s),c.name,tick,"physics");check(actor_idle_step(s),c.name,tick,"idle");const auto&e=c.frames[tick];
   check(close(s.position,e.position),c.name,tick,"position");check(close(s.direction,e.direction),c.name,tick,"direction");check(close(s.blend_direction,e.blend),c.name,tick,"blend");check(close(s.velocity,e.velocity),c.name,tick,"velocity");
   check(s.frame==e.frame,c.name,tick,"sprite frame");check(s.emote_frame==e.emote,c.name,tick,"emote frame");check(s.moving==e.moving,c.name,tick,"moving");check(s.rotating==e.rotating,c.name,tick,"rotating");check(s.moonwalk==e.moonwalk,c.name,tick,"moonwalk");check(s.finished_movement==e.movements,c.name,tick,"movement signals");check(s.finished_action==e.actions,c.name,tick,"action signals");
  }
  std::printf("%s: %u samples, %u failures\n",c.name,c.frame_count,failures-before_failures);
 }
 std::printf("Doll post-win native actions: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
