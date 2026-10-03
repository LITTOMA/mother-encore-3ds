#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
#include "fixtures/doll_actor_actions_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const char* test,unsigned tick,const char* field) {++checks;if(!ok){if(failures<60)std::fprintf(stderr,"%s tick%u: %s\n",test,tick,field);++failures;}}
bool close(float a,float b) {return std::abs(a-b)<.0003f;}
bool close(Vec2 a,Vec2 b){return close(a.x,b.x)&&close(a.y,b.y);}
uint32_t profile(const char* actor){const auto r=encore_test::room();for(uint32_t i=0;i<r.actor_instance_count();++i){const auto ins=r.actor_instance(i);auto name=r.string(ins.display_name_string);if((!std::strcmp(actor,"Doll")&&name=="npcdoll")||(!std::strcmp(actor,"Mimmie")&&name=="npc2")||name==actor)return ins.profile_index;}return kRoomNoIndex;}
uint32_t float_clip(const ActorActionState&s){const auto p=s.content.actor_profile(s.profile_index);for(uint32_t i=0;i<s.content.clip_count();++i){const auto c=s.content.clip(i);if(c.channel==0&&c.frame_count==4&&c.key_count==6)return i;}return p.idle_clip;}
bool action(ActorActionState&s,const doll_actor_actions_reference::Action&a){
 if(!std::strcmp(a.op,"path"))return actor_move_path(s,a.entries,a.count,a.speed,a.step?ActorMoveType::Step:ActorMoveType::Position,a.moonwalk,*a.animation?4:UINT16_MAX);
 if(!std::strcmp(a.op,"turn"))return actor_turn(s,{a.x,a.y});
 if(!std::strcmp(a.op,"turn_to"))return actor_turn_to(s,{a.x,a.y},a.length);
 if(!std::strcmp(a.op,"teleport"))return actor_teleport(s,{a.x,a.y});
 if(!std::strcmp(a.op,"anim"))return actor_play_clip(s,float_clip(s));
 if(!std::strcmp(a.op,"shake"))return actor_shake(s,{a.x,0},a.length);
 if(!std::strcmp(a.op,"talk"))return actor_set_talking(s,a.enabled);
 if(!std::strcmp(a.op,"emote"))return actor_play_emote(s,s.content.actor_profile(s.profile_index).emote_clip);
 return false;
}
}
int main(){
 for(const auto& c:doll_actor_actions_reference::cases){
  ActorActionState s;check(initialize_actor(s,encore_test::room(),profile(c.actor),c.start,{0,1}),c.name,0,"initialize");
  // The isolated upstream wrapper has not run _handle_replaced, so its
  // CharacterSprite blend starts at zero while the Actor direction is down.
  s.blend_direction={};
  for(unsigned tick=0;tick<c.frame_count;++tick){
   for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick)check(action(s,c.actions[i]),c.name,tick,"command");
   check(actor_physics_step(s),c.name,tick,"physics");check(actor_idle_step(s),c.name,tick,"idle");
   const auto&e=c.frames[tick];
   check(close(s.position,e.position),c.name,tick,"position");check(close(s.direction,e.direction),c.name,tick,"direction");check(close(s.blend_direction,e.blend),c.name,tick,"blend");check(close(s.velocity,e.velocity),c.name,tick,"velocity");
   if(s.frame!=e.frame&&failures<60)std::fprintf(stderr,"frame actual%u expected%u clock%.9g motion%u playing%u\n",s.frame,e.frame,s.animation_elapsed,s.animation_motion,s.playing_motion);
   check(s.frame==e.frame,c.name,tick,"frame");check(s.emote_frame==e.emote,c.name,tick,"emote");check(s.moving==e.moving,c.name,tick,"moving");check(s.rotating==e.rotating,c.name,tick,"rotating");check(s.moonwalk==e.moonwalk,c.name,tick,"moonwalk");check(s.finished_movement==e.movements,c.name,tick,"movement signals");check(s.finished_action==e.actions,c.name,tick,"action signals");
  }
 }
 ActorActionState s;check(initialize_actor(s,encore_test::room(),profile("Ninten"),{0,0},{0,1}),"negative",0,"initialize");const auto before=s;
 const ActorPathEntry move{ActorPathEntryKind::Move,{1,2},0};const ActorPathEntry wait{ActorPathEntryKind::Wait,{},.1};
 check(!actor_move_path(s,nullptr,1,64,ActorMoveType::Step),"negative",0,"null path");check(!actor_move_path(s,&move,0,64,ActorMoveType::Step),"negative",0,"empty path");
 check(!actor_move_path(s,&move,17,64,ActorMoveType::Step),"negative",0,"oversize path");check(!actor_move_path(s,&move,1,64,ActorMoveType(9)),"negative",0,"unknown type");
 auto bad=move;bad.kind=ActorPathEntryKind(9);check(!actor_move_path(s,&bad,1,64,ActorMoveType::Step),"negative",0,"unknown entry");bad=wait;bad.duration=-1;check(!actor_move_path(s,&bad,1,64,ActorMoveType::Step),"negative",0,"negative wait");bad.duration=std::numeric_limits<double>::infinity();check(!actor_move_path(s,&bad,1,64,ActorMoveType::Step),"negative",0,"nonfinite wait");bad=move;bad.duration=1;check(!actor_move_path(s,&bad,1,64,ActorMoveType::Step),"negative",0,"move has duration");
 check(!actor_move_path(s,&move,1,64,ActorMoveType::Step,false,77),"negative",0,"unknown animation");check(!actor_move_path(s,kRoomNoIndex),"negative",0,"unknown data path");
 check(!actor_teleport(s,{std::numeric_limits<float>::quiet_NaN(),0}),"negative",0,"nonfinite teleport");check(close(s.position,before.position)&&s.next_wait_order==before.next_wait_order,"negative",0,"rejections preserve state");
 for(unsigned i=0;i<4;++i)check(actor_move_path(s,&wait,1,64,ActorMoveType::Position),"negative",i,"bounded concurrent waits");
 const auto full=s;check(!actor_move_path(s,&wait,1,64,ActorMoveType::Position),"negative",0,"capacity rejection");check(s.next_wait_order==full.next_wait_order,"negative",0,"capacity unchanged");
 ActorActionState talk;check(initialize_actor(talk,encore_test::room(),profile("Mimmie"),{0,0},{0,1}),"wait talking",0,"initialize");
 check(actor_set_talking(talk,true),"wait talking",0,"begin talking");check(actor_move_path(talk,&wait,1,64,ActorMoveType::Position),"wait talking",0,"begin wait");check(!talk.talking,"wait talking",0,"source wait clears talking");
 std::printf("Doll actor actions: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
