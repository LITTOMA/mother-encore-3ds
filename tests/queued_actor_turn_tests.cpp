#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
#include "../reports/queued-actor-turn/reference-final/reference.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const char* name,unsigned tick,const char* field) {
    ++checks;
    if(!ok){if(failures<60)std::fprintf(stderr,"%s tick%u: %s\n",name,tick,field);++failures;}
}
bool close(float a,float b){return std::abs(a-b)<.0003f;}
bool close(Vec2 a,Vec2 b){return close(a.x,b.x)&&close(a.y,b.y);}
bool initialize(ActorActionState& s,Vec2 position={.25f,.25f}) {
    return initialize_actor(s,encore_test::room(),encore_test::profile("Ninten"),position,{0,1});
}
bool apply(ActorActionState& s,const queued_actor_turn_reference::Action& a) {
    if(!std::strcmp(a.op,"move"))return actor_move_position(s,{a.x,a.y},a.speed);
    if(!std::strcmp(a.op,"turn"))return actor_turn(s,{a.x,a.y});
    if(!std::strcmp(a.op,"turn_to"))return actor_turn_to(s,{a.x,a.y},a.length,a.queue);
    if(!std::strcmp(a.op,"shake"))return actor_shake(s,{a.x,0},a.length);
    return false;
}
unsigned waiting(const ActorActionState& s) {
    return unsigned(std::count_if(s.turns.begin(),s.turns.end(),[](const ActorTurn& t){return t.waiting;}));
}
}
int main() {
    for(const auto& c:queued_actor_turn_reference::cases) {
        const unsigned before=failures;
        ActorActionState s;check(initialize(s,c.start),c.name,0,"initialize");
        // The oracle isolates original Actor functions, without _handle_replaced.
        s.blend_direction={};
        for(unsigned tick=0;tick<c.frame_count;++tick) {
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick)
                check(apply(s,c.actions[i]),c.name,tick,"command");
            check(actor_physics_step(s),c.name,tick,"physics");
            check(actor_idle_step(s),c.name,tick,"idle");
            const auto& e=c.frames[tick];
            check(close(s.position,e.position),c.name,tick,"position");
            check(close(s.direction,e.direction),c.name,tick,"direction");
            check(close(s.blend_direction,e.blend),c.name,tick,"blend");
            check(close(s.velocity,e.velocity),c.name,tick,"velocity");
            check(s.frame==e.frame,c.name,tick,"sprite frame");
            check(s.moving==e.moving,c.name,tick,"moving");
            check(s.rotating==e.rotating,c.name,tick,"rotating");
            check(s.finished_movement==e.movements,c.name,tick,"movement signals");
            check(s.finished_action==e.actions,c.name,tick,"action signals");
        }
        std::printf("%s: %u frames, %u failures\n",c.name,c.frame_count,failures-before);
    }
    ActorActionState s;check(initialize(s),"validation",0,"initialize");
    check(actor_turn(s,{2,1}),"validation",0,"set source direction");
    check(actor_turn_to(s,{-4,1},.08,true),"validation",0,"queued accepted");
    check(close(s.direction,{1,0})&&close(s.blend_direction,{2,1}),"validation",0,"normalize now without blending");
    check(waiting(s)==1&&!s.rotating&&!s.moving&&s.finished_action==0,"validation",0,"idle still waits for signal");
    check(close(s.turns[0].target,{-1,0}),"validation",0,"capture normalized target");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const auto before=s;
    check(!actor_turn_to(s,{nan,1},.08,true),"validation",0,"nonfinite target rejected");
    check(!actor_turn_to(s,{},.08,true),"validation",0,"zero target rejected");
    check(!actor_turn_to(s,{1,0},0,true),"validation",0,"zero interval rejected");
    check(!actor_turn_to(s,{1,0},11,true),"validation",0,"excessive interval rejected");
    check(waiting(s)==1&&s.next_wait_order==before.next_wait_order&&close(s.direction,before.direction),"validation",0,"rejection preserves state and registration");
    for(unsigned i=1;i<4;++i)check(actor_turn_to(s,{0,-1},.08,true),"validation",i,"bounded queued slot");
    const auto full=s;
    check(!actor_turn_to(s,{0,1},.08,true),"validation",0,"queue capacity rejects");
    check(!actor_turn_to(s,{0,1},.08),"validation",0,"active turn shares capacity");
    check(waiting(s)==4&&s.next_wait_order==full.next_wait_order&&close(s.direction,full.direction),"validation",0,"capacity rejection preserves actor");
    ActorActionState invalid;
    check(!actor_turn_to(invalid,{1,0},.08,true),"validation",0,"uninitialized actor rejected");
    std::printf("Queued Actor turns: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
