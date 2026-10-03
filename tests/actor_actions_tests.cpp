#include "encore/actor_actions.hpp"
#include "room_fixture.hpp"
// The immutable native fixture retains its original content labels. Keep this
// compatibility enum local to its test namespace, never in the engine API.
namespace actor_actions_reference { enum class ActorKind { Ninten, Lamp }; }
#include "fixtures/actor_actions_v0410.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace encore::upstream;
using actor_actions_reference::ActorKind;
namespace {
unsigned checks=0, failures=0;
void check(bool ok,const char* test,unsigned tick,const char* field) {++checks;if(!ok){if(failures<30)std::fprintf(stderr,"%s tick%u: %s\n",test,tick,field);++failures;}}
bool close(float a,float b) {return std::abs(a-b)<0.00025f;}
bool close(Vec2 a,Vec2 b) {return close(a.x,b.x)&&close(a.y,b.y);}
bool initialize_fixture_actor(ActorActionState& s,ActorKind kind,Vec2 position,Vec2 direction={0,1}) {
    if(kind!=ActorKind::Ninten&&kind!=ActorKind::Lamp)return initialize_actor(s,encore_test::room(),kRoomNoIndex,position,direction);
    return initialize_actor(s,encore_test::room(),encore_test::profile(kind==ActorKind::Ninten?"Ninten":"lamp"),position,direction);
}
bool action(ActorActionState& s,const actor_actions_reference::Action& a) {
    if(!std::strcmp(a.op,"move"))return actor_move_position(s,{a.x,a.y},a.speed);
    if(!std::strcmp(a.op,"jump"))return actor_jump(s,a.height,a.length);
    if(!std::strcmp(a.op,"shake"))return actor_shake(s,{a.x,0},a.length);
    if(!std::strcmp(a.op,"anim"))return actor_play_clip(s,encore_test::actor_clip("lamp",a.animation));
    if(!std::strcmp(a.op,"turn"))return actor_turn(s,{a.x,a.y});
    if(!std::strcmp(a.op,"turn_to"))return actor_turn_to(s,{a.x,a.y},a.length);
    if(!std::strcmp(a.op,"emote"))return actor_play_emote(s,s.content.actor_profile(s.profile_index).emote_clip);
    return false;
}
}
int main() {
    for(const auto& c:actor_actions_reference::cases) {
        ActorActionState s;check(initialize_fixture_actor(s,c.kind,c.start),c.name,0,"initialize");
        for(unsigned tick=0;tick<c.frame_count;++tick) {
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick&&std::strcmp(c.actions[i].op,"timer_shake")&&std::strcmp(c.actions[i].op,"timer_turn"))check(action(s,c.actions[i]),c.name,tick,"command");
            check(actor_physics_step(s),c.name,tick,"physics");
            check(actor_idle_animations(s),c.name,tick,"idle animation");
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick&&!std::strcmp(c.actions[i].op,"timer_shake"))check(actor_shake(s,{c.actions[i].x,0},c.actions[i].length),c.name,tick,"timer-born shake");
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick&&!std::strcmp(c.actions[i].op,"timer_turn"))check(actor_turn_to(s,{c.actions[i].x,c.actions[i].y},c.actions[i].length),c.name,tick,"timer-born turn");
            check(actor_scene_timers(s),c.name,tick,"scene timers");
            const auto& e=c.frames[tick];
            check(close(s.position,e.position),c.name,tick,"position");check(close(s.direction,e.direction),c.name,tick,"direction");
            check(close(s.velocity,e.velocity),c.name,tick,"velocity");check(close(s.sprite_position,e.sprite_position),c.name,tick,"sprite_position");
            check(close(s.sprite_offset,e.sprite_offset),c.name,tick,"sprite_offset");check(s.frame==e.frame,c.name,tick,"frame");
            check(s.emote_frame==e.emote,c.name,tick,"emote");check(s.moving==e.moving,c.name,tick,"moving");check(s.rotating==e.rotating,c.name,tick,"rotating");
            check(s.finished_movement==e.movement_signals,c.name,tick,"movement signals");check(s.finished_action==e.action_signals,c.name,tick,"action signals");
        }
    }
    ActorActionState invalid;check(!actor_physics_step(invalid),"negative",0,"uninitialized");
    check(!initialize_fixture_actor(invalid,ActorKind(99),{}),"negative",0,"kind");
    ActorActionState s;initialize_fixture_actor(s,ActorKind::Lamp,{496,390});
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check(!actor_move_position(s,{nan,0},500),"negative",0,"nonfinite target");check(!actor_move_position(s,{1,1},0),"negative",0,"zero speed");
    check(!actor_jump(s,3,0),"negative",0,"zero duration");check(!actor_jump(s,nan,.2),"negative",0,"nonfinite height");
    check(!actor_shake(s,{2,0},0),"negative",0,"looping shake");check(!actor_idle_step(s,-1),"negative",0,"negative delta");
    check(!actor_play_clip(s,kRoomNoIndex),"negative",0,"unknown animation");check(!actor_play_emote(s,s.content.actor_profile(s.profile_index).emote_clip),"negative",0,"unknown actor emote");
    check(actor_shake(s,{2,0},.5),"negative",0,"valid shake");const auto before=s;
    check(!actor_shake(s,{2,0},.5),"negative",0,"overlap shake rejected");check(close(s.sprite_offset,before.sprite_offset),"negative",0,"reject unchanged");
    ActorActionState turn;initialize_fixture_actor(turn,ActorKind::Ninten,{432,397},{-1,0});
    check(!actor_turn_to(turn,{},.08),"negative",0,"zero turn direction");
    check(!actor_turn_to(turn,{1,0},0),"negative",0,"zero turn interval");
    check(!actor_turn_to(turn,{nan,0},.08),"negative",0,"nonfinite turn target");
    check(actor_turn_to(turn,{1,0},.08),"negative",0,"valid turn");
    check(actor_turn_to(turn,{0,1},.08),"concurrent",0,"source overlapping turn accepted");
    while(std::count_if(turn.turns.begin(),turn.turns.end(),[](const ActorTurn&t){return t.active;})<4)
        check(actor_turn_to(turn,{-turn.direction.x,-turn.direction.y},.08),"concurrent",0,"bounded independent turn coroutine");
    const auto turn_before=turn;
    check(!actor_turn_to(turn,{-turn.direction.x,-turn.direction.y},.08),"negative",0,"turn capacity rejects");
    check(close(turn.direction,turn_before.direction),"negative",0,"rejected turn unchanged");
    std::printf("Actor actions: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
