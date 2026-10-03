#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::upstream {
namespace {
constexpr float delta=1.0f/60.0f;
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y); }
bool zero(Vec2 v) { return v.x==0&&v.y==0; }
Vec2 scale(Vec2 v,float f) { return {v.x*f,v.y*f}; }
float round_godot(float value) { return std::round(value); }
bool valid(const WalkState& s,const RoomView& content) {
    const float x=std::abs(s.direction.x),y=std::abs(s.direction.y);
    const bool integer=(x==0||x==1)&&(y==0||y==1)&&!zero(s.direction);
    const bool normalized=x==y&&std::abs(x-std::sqrt(.5f))<1e-6f;
    return finite(s.position)&&finite(s.velocity)&&finite(s.direction)&&(integer||normalized)
        &&(s.speed==content.rule_f32(RoomRuleKey::WalkSpeed)||s.speed==content.rule_f32(RoomRuleKey::RunSpeed))&&uint8_t(s.animation)<=uint8_t(MotionAnimation::Crouch)
        &&s.moved_signals<std::numeric_limits<uint32_t>::max()-1;
}
}
bool controls_vector(Vec2 input,int32_t threshold,Vec2& result) {
    if(!finite(input)||threshold<0)return false;
    Vec2 output{};
    const auto component=[threshold](float n) {
        if(std::abs(n)<=threshold)return 0.0f;
        const float rounded=round_godot(n);
        return rounded==0?0.0f:(rounded>0?1.0f:-1.0f);
    };
    output.x=component(input.x);output.y=component(input.y);result=output;return true;
}
bool FreeMotionSolver::slide(Vec2 position,Vec2 velocity,SlideResult& result) const {
    const Vec2 target{position.x+velocity.x*delta,position.y+velocity.y*delta};
    if(!finite(target)||!finite(velocity))return false;
    result={target,velocity};return true;
}
bool advance_walk(WalkState& state,WalkInput input,const MotionSolver& solver,const RoomView& content) {
    if(!content.valid()||!valid(state,content)||input.x<-1||input.x>1||input.y<-1||input.y>1)return false;
    WalkState s=state;
    const bool pressed=input.toggle&&!s.previous_toggle,released=!input.toggle&&s.previous_toggle;
    s.previous_toggle=input.toggle;
    // Player._move_state gates the entire movement function while paused or
    // entering a door. Input edges still expire in Godot's input service.
    if(input.paused||input.entering_door){state=s;return true;}
    const Vec2 vector{float(input.x),float(input.y)};
    if(!zero(vector)||s.tap_run) {
        // _move precedes the speed selection in _movement: preserve the old
        // speed for the first frame of switching walking/running.
        s.velocity=scale(s.tap_run?s.direction:vector,s.speed);
        if(!zero(vector)){s.direction=vector;++s.moved_signals;}
        if(input.toggle||s.tap_run) {
            if(pressed&&!s.crouch&&!s.running)s.crouch=true;
            if(pressed&&s.tap_run){s.tap_run=false;s.running=false;}
            if(s.substantial)s.running=true;
            s.animation=MotionAnimation::Run;s.speed=content.rule_f32(RoomRuleKey::RunSpeed);
        } else {
            s.speed=content.rule_f32(RoomRuleKey::WalkSpeed);s.crouch=false;s.animation=MotionAnimation::Walk;
            if(released&&!s.tap_run)s.running=false;
        }
    } else {
        s.velocity={};s.walking=false;
        if(released&&s.crouch){
            s.crouch=false;s.speed=content.rule_f32(RoomRuleKey::RunSpeed);s.tap_run=true;s.velocity=scale(s.direction,s.speed);
        }
    }
    const Vec2 old=s.position;
    SlideResult moved;
    const Vec2 velocity=scale(scale(s.velocity,delta),s.speed/content.rule_f32(RoomRuleKey::MovementDivisor));
    if(!solver.slide(s.position,velocity,moved)||!finite(moved.position)||!finite(moved.velocity))return false;
    s.position=moved.position;s.velocity=moved.velocity;
    // Original _movement also invokes move_and_slide for zero knockback.
    // Keep that query: physics recovery can occur even with zero velocity.
    SlideResult recovered;
    if(!solver.slide(s.position,{},recovered)||!finite(recovered.position)||!finite(recovered.velocity))return false;
    s.position=recovered.position;
    s.substantial=std::max(round_godot(std::abs(old.x-s.position.x)),round_godot(std::abs(old.y-s.position.y)))>0;
    if(s.substantial){s.walking=true;s.crouch=false;}
    else {
        s.animation=MotionAnimation::Idle;s.walking=false;s.tap_run=false;s.running=false;
        if(pressed&&!s.crouch){++s.moved_signals;s.crouch=true;}
        else if(pressed&&s.crouch)s.crouch=false;
        if(s.crouch)s.animation=MotionAnimation::Crouch;
    }
    s.position={round_godot(s.position.x),round_godot(s.position.y)};
    if(!valid(s,content))return false;
    state=s;return true;
}
}
