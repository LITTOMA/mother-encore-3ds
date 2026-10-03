#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
#include "fixtures/pillow_actor_actions_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const char* test,unsigned tick,const char* field) {
    ++checks;
    if(!ok){if(failures<80)std::fprintf(stderr,"%s tick%u: %s\n",test,tick,field);++failures;}
}
bool close(float a,float b){return std::abs(a-b)<.0003f;}
bool close(Vec2 a,Vec2 b){return close(a.x,b.x)&&close(a.y,b.y);}
uint32_t profile(const char* actor){return encore_test::profile(!std::strcmp(actor,"Minnie")?"npc3":"pillow");}
bool action(ActorActionState&s,const pillow_actor_actions_reference::Action&a){
    if(!std::strcmp(a.op,"path"))return actor_move_path(s,a.entries,a.count,a.speed,ActorMoveType::Position,false,*a.animation?4:UINT16_MAX,a.loop,a.queue);
    if(!std::strcmp(a.op,"turn"))return actor_turn(s,{a.x,a.y});
    if(!std::strcmp(a.op,"turn_to"))return actor_turn_to(s,{a.x,a.y},a.length);
    if(!std::strcmp(a.op,"teleport"))return actor_teleport(s,{a.x,a.y});
    if(!std::strcmp(a.op,"anim"))return actor_play_clip(s,s.content.actor_profile(s.profile_index).idle_clip);
    if(!std::strcmp(a.op,"shake"))return actor_shake(s,{a.x,0},a.length);
    if(!std::strcmp(a.op,"jump"))return actor_jump(s,a.height,a.length,a.times);
    if(!std::strcmp(a.op,"stop"))return actor_stop_loop(s);
    return false;
}
void vector_check(Vec2 actual,Vec2 expected,const char* name,unsigned tick,const char* field){
    if(!close(actual,expected)&&failures<80)
        std::fprintf(stderr,"actual(%.9g,%.9g) expected(%.9g,%.9g) ",actual.x,actual.y,expected.x,expected.y);
    check(close(actual,expected),name,tick,field);
}
}
int main(){
    for(const auto& c:pillow_actor_actions_reference::cases){
        const auto before=failures;
        ActorActionState s;
        check(initialize_actor(s,encore_test::room(),profile(c.actor),c.start,{0,1}),c.name,0,"initialize");
        // The isolated unchanged-source wrapper has not run _handle_replaced.
        s.blend_direction={};
        for(unsigned tick=0;tick<c.frame_count;++tick){
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick)check(action(s,c.actions[i]),c.name,tick,"command");
            check(actor_physics_step(s),c.name,tick,"physics");
            check(actor_idle_step(s),c.name,tick,"idle");
            const auto&e=c.frames[tick];
            vector_check(s.position,e.position,c.name,tick,"position");
            vector_check(s.direction,e.direction,c.name,tick,"direction");
            vector_check(s.blend_direction,e.blend,c.name,tick,"blend");
            vector_check(s.velocity,e.velocity,c.name,tick,"velocity");
            vector_check(s.sprite_position,e.sprite_position,c.name,tick,"sprite position");
            vector_check(s.sprite_offset,e.sprite_offset,c.name,tick,"sprite offset");
            check(s.frame==e.frame,c.name,tick,"frame");
            check(s.moving==e.moving,c.name,tick,"moving");
            check(s.rotating==e.rotating,c.name,tick,"rotating");
            check(s.looping==e.looping,c.name,tick,"shared loop flag");
            check(s.finished_movement==e.movements,c.name,tick,"movement signals");
            check(s.finished_action==e.actions,c.name,tick,"action signals");
        }
        std::printf("%s: %u frames, %u failures\n",c.name,c.frame_count,failures-before);
    }
    ActorActionState s;
    check(initialize_actor(s,encore_test::room(),profile("Minnie"),{0,0},{0,1}),"negative",0,"initialize");
    const auto before=s;
    check(!actor_jump(s,8,.2,0),"negative",0,"zero repeat rejected");
    check(!actor_jump(s,8,.2,17),"negative",0,"oversized repeat rejected");
    check(!actor_shake(s,{1,0},-2),"negative",0,"unknown negative shake rejected");
    check(!actor_shake(s,{1,0},std::numeric_limits<double>::infinity()),"negative",0,"nonfinite shake rejected");
    check(!s.looping&&!s.shaking&&s.next_wait_order==before.next_wait_order&&s.finished_action==before.finished_action,"negative",0,"rejection leaves state unchanged");
    ActorActionState missing;
    check(!actor_stop_loop(missing),"negative",0,"uninitialized stop rejected");
    check(actor_stop_loop(s),"negative",0,"idle stop accepted");
    std::printf("Pillow actor actions: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
