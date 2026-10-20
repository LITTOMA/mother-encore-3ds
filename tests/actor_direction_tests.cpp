// Source coroutine observations. Inputs are test cases, never game bindings.
#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool b){++checks;if(!b){std::fprintf(stderr,"Direction check %u failed\n",checks);std::exit(1);}}}
int main(int argc,char**argv){
 check(argc==2);FILE*f=std::fopen(argv[1],"rb");check(f);unsigned observations=0;
 for(unsigned c=0;c<3;++c){ActorActionState a;check(initialize_actor(a,encore_test::room(),encore_test::profile("Ninten"),{432.25f,397.25f},{0,1}));check(actor_turn(a,{1,0})&&actor_turn_to(a,{-1,0},.1));
  const auto ticks=c==0?45u:c==1?25u:22u;
  for(unsigned tick=0;tick<ticks;++tick){
   if(tick==(c==1?3u:2u)){
    check(a.rotating);const auto before=a;const float nan=std::numeric_limits<float>::quiet_NaN();check(!actor_turn(a,{nan,0})&&a.direction.x==before.direction.x&&a.direction.y==before.direction.y&&a.next_wait_order==before.next_wait_order);
    check(actor_turn(a,c==0?Vec2{0,-1}:c==1?Vec2{-1,0}:Vec2{0,0}));check(a.rotating&&a.turns[0].active&&a.turns[0].remaining==before.turns[0].remaining&&a.turns[0].angle==before.turns[0].angle);
   }
   check(actor_physics_step(a)&&actor_idle_step(a));unsigned ec,et,rot,signals;float x,y;check(std::fscanf(f,"%u\t%u\t%f\t%f\t%u\t%u",&ec,&et,&x,&y,&rot,&signals)==6);check(ec==c&&et==tick&&std::abs(a.direction.x-x)<.0003&&std::abs(a.direction.y-y)<.0003&&a.rotating==bool(rot)&&a.finished_action==signals);++observations;
  }
 }
 unsigned extra;check(std::fscanf(f,"%u",&extra)==EOF);check(std::fclose(f)==0);std::printf("Actor direction: %u checks; %u original Godot observations, immediate write preserves suspended turn and invalid input is atomic\n",checks,observations);
}
