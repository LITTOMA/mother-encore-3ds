#include "encore/actor_actions.hpp"
#include "encore/animation.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
constexpr float physics_delta=1.0f/60.0f;
bool finite(Vec2 v) {return std::isfinite(v.x)&&std::isfinite(v.y);}
bool valid(const ActorActionState& a) {
    return a.initialized&&a.content.valid()&&a.profile_index<a.content.actor_profile_count()
        &&finite(a.position)&&finite(a.direction)&&finite(a.velocity)&&finite(a.sprite_position)&&finite(a.sprite_offset)
        &&finite(a.blend_direction);
}
void blend(ActorActionState& a,Vec2 direction) {
    direction={std::round(direction.x),std::round(direction.y)};
    const auto profile=a.content.actor_profile(a.profile_index);
    if(profile.execution_kind!=uint16_t(ActorExecution::DirectionalProxy)&&std::abs(direction.x)==std::abs(direction.y))return;
    if(direction.x!=0||direction.y!=0)a.blend_direction=direction;
}
void finish_action(ActorActionState& a);
void start_path(ActorActionState& a,unsigned slot);
void start_turn(ActorActionState& a,unsigned slot) {
    auto& turn=a.turns[slot];
    // Consume this signal wait before emitting a nested completion. The source
    // coroutine cannot resume twice, even when its completion emits recursively.
    turn.waiting=false;turn.wait_order=0;
    const Vec2 from=a.direction,target=turn.target;
    // turn_to normalizes before yield, then compares to the live rounded value.
    // A moving actor can now have a fractional direction; do not normalize it.
    if(std::round(from.x)==target.x&&std::round(from.y)==target.y) {
        turn=ActorTurn{};finish_action(a);return;
    }
    const float angle=std::atan2(from.x*target.y-from.y*target.x,from.x*target.x+from.y*target.y);
    const float radians=a.content.rule_f32(RoomRuleKey::ActorTurnRadians);
    turn.angle=angle<0?-radians:(angle>0?radians:0);turn.remaining=turn.interval;
    a.rotating=true;a.moving=false;turn.active=true;turn.timer_order=++a.next_wait_order;
    const float cosine=std::cos(turn.angle),sine=std::sin(turn.angle);
    a.direction={std::round(cosine*from.x-sine*from.y),std::round(sine*from.x+cosine*from.y)};
    blend(a,a.direction);
}
void next_path_entry(ActorActionState& a,unsigned slot) {
    auto& path=a.move_paths[slot];
    if(path.cursor==path.count) {
        if(a.looping) {
            // Source recursion resets the shared loop flag from this path's
            // own loop argument, preserving interaction with looping shakes.
            path.cursor=0;path.wait=ActorPathWait::None;
            a.moving=false;a.rotating=false;start_path(a,slot);return;
        }
        if(a.moonwalk)a.direction={-a.direction.x,-a.direction.y};
        a.moonwalk=false;a.moving=false;a.rotating=false;
        if(path.animation_motion!=UINT16_MAX)a.animation_motion=0;
        path=ActorMovePath{};finish_action(a);return;
    }
    const auto entry=path.entries[path.cursor];
    path.wait_order=++a.next_wait_order;
    if(entry.kind==ActorPathEntryKind::Move) {
        path.wait=ActorPathWait::Movement;
        a.movement.target=path.type==ActorMoveType::Step?Vec2{a.position.x+entry.vector.x,a.position.y+entry.vector.y}:entry.vector;
        a.movement_type=path.type;a.moving=true;a.rotating=false;
        if(path.animation_motion!=UINT16_MAX)a.animation_motion=path.animation_motion;
    } else {
        path.wait=ActorPathWait::Timer;path.remaining=double(float(entry.duration));
        a.moving=false;a.rotating=false;a.talking=false;
        if(path.animation_motion!=UINT16_MAX)a.animation_motion=0;
    }
}
void start_path(ActorActionState& a,unsigned slot) {
    const auto& path=a.move_paths[slot];
    a.movement.speed=path.speed;a.moonwalk=path.moonwalk;a.looping=path.loop;
    next_path_entry(a,slot);
}
struct Waiter {uint32_t order=0;unsigned slot=0;};
std::array<Waiter,4> waiters(const ActorActionState& a,ActorPathWait wait) {
    std::array<Waiter,4> result{};
    for(unsigned i=0;i<a.move_paths.size();++i)if(a.move_paths[i].wait==wait)result[i]={a.move_paths[i].wait_order,i};
    std::sort(result.begin(),result.end(),[](Waiter x,Waiter y){return x.order<y.order;});return result;
}
void finish_action(ActorActionState& a) {
    ++a.finished_action;
    // One-shot signal connections resume in registration order. Snapshot the
    // current set across paths AND turns: a same-direction turn can emit a
    // nested finished_action, consuming later waiters before this loop resumes.
    struct ActionWaiter {uint32_t order=0;unsigned slot=0;bool turn=false;};
    std::array<ActionWaiter,8> pending{};
    for(unsigned i=0;i<a.move_paths.size();++i)if(a.move_paths[i].wait==ActorPathWait::Action)pending[i]={a.move_paths[i].wait_order,i,false};
    for(unsigned i=0;i<a.turns.size();++i)if(a.turns[i].waiting)pending[4+i]={a.turns[i].wait_order,i,true};
    std::sort(pending.begin(),pending.end(),[](ActionWaiter x,ActionWaiter y){return x.order<y.order;});
    for(const auto w:pending)if(w.order) {
        if(w.turn) {
            if(a.turns[w.slot].waiting&&a.turns[w.slot].wait_order==w.order)start_turn(a,w.slot);
        } else if(a.move_paths[w.slot].wait_order==w.order&&a.move_paths[w.slot].wait==ActorPathWait::Action)start_path(a,w.slot);
    }
}
void direction_frame(ActorActionState& a) {
    const Vec2 d=a.blend_direction;
    if(d.x==0&&d.y==0)return;
    unsigned index;
    if(d.y>0)index=d.x<0?4:(d.x>0?5:0);
    else if(d.y<0)index=d.x<0?6:(d.x>0?7:3);
    else index=d.x<0?1:2;
    const auto profile=a.content.actor_profile(a.profile_index);
    a.frame=a.content.direction_frame(profile.direction_first+index);
}
unsigned direction_index(Vec2 d) {
    if(d.x==0&&d.y==0)return 0;
    if(d.y>0)return d.x<0?4:(d.x>0?5:0);
    if(d.y<0)return d.x<0?6:(d.x>0?7:3);
    return d.x<0?1:2;
}
uint32_t directional_clip(const ActorActionState& a,uint16_t motion,unsigned direction) {
    const auto profile=a.content.actor_profile(a.profile_index);
    for(uint32_t i=0;i<profile.animation_binding_count;++i){const auto b=a.content.animation_binding(profile.animation_binding_first+i);if(b.motion_state==motion&&b.direction==direction)return b.clip_index;}
    return kRoomNoIndex;
}
bool advance_directional(ActorActionState& a,float dt) {
    const unsigned direction=direction_index(a.blend_direction);
    const auto old_index=directional_clip(a,a.playing_motion,a.playing_direction);
    if(old_index==kRoomNoIndex)return false;
    const auto old=a.content.clip(old_index);
    // CharacterSprite StateMachine travel switches on the idle phase, retaining
    // the outgoing frame for the transition. NPC Talk->Idle waits for its end.
    const bool switching=a.animation_motion!=a.playing_motion&&(a.playing_motion!=5||a.animation_motion!=0||old.length-float(a.animation_elapsed)<=dt);
    // The actor's PartyMember Idle is the source directional single-frame
    // table, distinct from the ordinary Player animation tracks.
    if(a.playing_motion==0&&a.content.actor_profile(a.profile_index).execution_kind==uint16_t(ActorExecution::DirectionalProxy)) {
        a.playing_direction=uint8_t(direction);direction_frame(a);
        if(switching){a.playing_motion=a.animation_motion;a.animation_elapsed=0;}
        return true;
    }
    const auto index=directional_clip(a,a.playing_motion,direction);
    if(index==kRoomNoIndex)return false;
    const auto clip=a.content.clip(index);
    if(direction!=a.playing_direction){a.playing_direction=uint8_t(direction);a.animation_elapsed=0;}
    else a.animation_elapsed=float(float(a.animation_elapsed)+dt);
    float time=float(a.animation_elapsed);
    if(clip.loop())time=std::fmod(time,clip.length);else time=std::min(time,clip.length);
    a.animation_elapsed=time;a.clip_index=index;
    for(uint32_t i=0;i<clip.key_count;++i){const auto key=a.content.key(clip.first_key+i);if(key.time==0||key.time<time)a.frame=key.frame;else break;}
    if(switching){a.playing_motion=a.animation_motion;a.animation_elapsed=0;}
    return true;
}
}
bool initialize_actor(ActorActionState& a,const RoomView& content,uint32_t profile_index,Vec2 position,Vec2 direction) {
    if(!content.valid()||profile_index>=content.actor_profile_count()||!finite(position)||!finite(direction))return false;
    const auto profile=content.actor_profile(profile_index);
    ActorActionState next;next.content=content;next.profile_index=profile_index;next.position=position;next.direction=direction;next.initialized=true;
    next.sprite_position=profile.sprite_position;next.sprite_offset=profile.sprite_offset;next.emote_position=profile.emote_offset;
    next.frame=profile.initial_frame;next.emote_frame=profile.emote_initial_frame;next.shadow_visible=(profile.flags&1)!=0;
    next.clip_index=profile.idle_clip;next.emote_clip_index=profile.emote_clip;
    next.movement.speed=content.rule_f32(RoomRuleKey::ActorDefaultMoveSpeed);
    if(direction.x!=0&&direction.y!=0) {
        if(std::abs(direction.x)>std::abs(direction.y))next.direction.y=0;
        else next.direction.x=0;
    }
    blend(next,next.direction);
    next.playing_direction=uint8_t(direction_index(next.blend_direction));
    next.directional_animation=profile.execution_kind==uint16_t(ActorExecution::NpcDirectionalProxy);
    if(profile.execution_kind==uint16_t(ActorExecution::DirectionalProxy)||profile.execution_kind==uint16_t(ActorExecution::NpcDirectionalProxy))direction_frame(next);
    a=next;return true;
}
bool actor_move_position(ActorActionState& a,Vec2 target,float speed) {
    const ActorPathEntry entry{ActorPathEntryKind::Move,target,0};
    return actor_move_path(a,&entry,1,speed,ActorMoveType::Position);
}
bool actor_move_path(ActorActionState& a,const ActorPathEntry* entries,size_t count,float speed,ActorMoveType type,bool moonwalk,uint16_t animation_motion,bool loop,bool queue) {
    if(!valid(a)||!entries||!count||count>16||!std::isfinite(speed)||speed<=0||speed>10000||(type!=ActorMoveType::Position&&type!=ActorMoveType::Step))return false;
    if(animation_motion!=UINT16_MAX) {
        const auto profile=a.content.actor_profile(a.profile_index);unsigned found=0;
        for(uint32_t i=0;i<profile.animation_binding_count;++i)if(a.content.animation_binding(profile.animation_binding_first+i).motion_state==animation_motion)++found;
        if(found!=profile.direction_count||!found)return false;
    }
    for(size_t i=0;i<count;++i) {
        const auto e=entries[i];
        if(!finite(e.vector)||!std::isfinite(e.duration))return false;
        if(e.kind==ActorPathEntryKind::Move) {if(e.duration!=0)return false;}
        else if(e.kind==ActorPathEntryKind::Wait) {if(e.duration<0||e.duration>60||e.vector.x!=0||e.vector.y!=0)return false;}
        else return false;
    }
    unsigned slot=0;while(slot<a.move_paths.size()&&a.move_paths[slot].count)++slot;
    if(slot==a.move_paths.size())return false;
    auto next=a;auto& path=next.move_paths[slot];path.count=uint8_t(count);path.type=type;path.speed=speed;path.moonwalk=moonwalk;path.animation_motion=animation_motion;path.loop=loop;
    std::copy(entries,entries+count,path.entries.begin());
    if(next.moving||next.rotating||queue){path.wait=ActorPathWait::Action;path.wait_order=++next.next_wait_order;}
    else start_path(next,slot);
    if(animation_motion!=UINT16_MAX)next.directional_animation=true;
    a=next;return true;
}
bool actor_teleport(ActorActionState& a,Vec2 position) {
    if(!valid(a)||!finite(position))return false;
    a.position=position;return true;
}
bool actor_move_path(ActorActionState& a,uint32_t index) {
    if(!valid(a)||index>=a.content.movement_path_count())return false;
    const auto path=a.content.movement_path(index);
    if(!path.entry_count||path.entry_count>16||(path.flags&~15u)||path.first_entry>a.content.movement_path_entry_count()||path.entry_count>a.content.movement_path_entry_count()-path.first_entry)return false;
    std::array<ActorPathEntry,16> entries{};
    for(unsigned i=0;i<path.entry_count;++i){const auto e=a.content.movement_path_entry(path.first_entry+i);if(e.kind>1)return false;entries[i]={ActorPathEntryKind(e.kind),e.vector,e.duration};}
    return actor_move_path(a,entries.data(),path.entry_count,float(path.speed),(path.flags&1)?ActorMoveType::Step:ActorMoveType::Position,(path.flags&2)!=0,path.animation_motion,(path.flags&4)!=0,(path.flags&8)!=0);
}
bool actor_set_talking(ActorActionState& a,bool talking) {
    if(!valid(a))return false;
    if(talking&&directional_clip(a,5,0)==kRoomNoIndex)return false;
    a.talking=talking;return true;
}
bool actor_turn(ActorActionState& a,Vec2 direction) {
    if(!valid(a)||!finite(direction)||a.rotating)return false;
    a.direction=direction;blend(a,direction);return true;
}
bool actor_turn_to(ActorActionState& a,Vec2 direction,double interval,bool queue) {
    if(!valid(a)||!finite(direction)||!std::isfinite(interval)||interval<=0||interval>10)return false;
    const auto normalized=[](Vec2 v) {
        const float length=std::sqrt(v.x*v.x+v.y*v.y);
        return length>0&&std::isfinite(length)?Vec2{std::round(v.x/length),std::round(v.y/length)}:Vec2{};
    };
    const Vec2 from=normalized(a.direction),target=normalized(direction);
    if((from.x==0&&from.y==0)||(target.x==0&&target.y==0))return false;
    unsigned slot=0;while(slot<a.turns.size()&&(a.turns[slot].active||a.turns[slot].waiting))++slot;
    if(slot==a.turns.size())return false;
    a.direction=from;
    // Each source turn_to owns a SceneTreeTimer coroutine, but all coroutines
    // read/write the same _direction and state. New calls do not cancel old ones.
    auto& turn=a.turns[slot];turn=ActorTurn{};turn.target=target;turn.interval=interval;
    if(queue) {turn.waiting=true;turn.wait_order=++a.next_wait_order;}
    else start_turn(a,slot);
    return true;
}
bool actor_jump(ActorActionState& a,float height,double length,unsigned repetitions) {
    if(!valid(a)||a.rotating||!std::isfinite(height)||height<0||height>1024||!std::isfinite(length)||length<=0||length>60||!repetitions||repetitions>16)return false;
    if(repetitions>1&&a.content.rule_f64(RoomRuleKey::ActorJumpRepeatDelaySeconds)<=0)return false;
    for(auto& jump:a.jumps)if(!jump.active) {blend(a,a.direction);jump={0,length,height,true};jump.remaining=uint8_t(repetitions);jump.repeated=repetitions>1;return true;}
    return false;
}
bool actor_shake(ActorActionState& a,Vec2 magnitude,double length) {
    if(!valid(a)||a.rotating||!finite(magnitude)||!std::isfinite(length)||(length<=0&&length!=-1)||length>10||a.shaking)return false;
    if(length==-1) {
        length=a.content.rule_f64(RoomRuleKey::ActorLoopShakeSeconds);
        if(length<=0)return false;
        a.looping=true;
    }
    const unsigned iterations=unsigned(length*a.content.rule_f64(RoomRuleKey::ActorShakeCyclesPerSecond)); // Exact upstream truncation.
    a.shake_base=a.sprite_offset;a.shake_magnitude=magnitude;
    a.shake_half_steps=uint16_t(iterations*2);
    if(!iterations) {finish_action(a);return true;}
    a.shaking=true;a.shake_remaining=a.content.rule_f64(RoomRuleKey::ActorShakeHalfStepSeconds);a.shake_timer_order=++a.next_wait_order;
    a.sprite_offset={a.shake_base.x+magnitude.x,a.shake_base.y+magnitude.y};
    return true;
}
bool actor_stop_loop(ActorActionState& a) {
    if(!valid(a))return false;
    a.looping=false;return true;
}
bool actor_play_clip(ActorActionState& a,uint32_t clip_index) {
    if(!valid(a)||clip_index>=a.content.clip_count())return false;
    const auto profile=a.content.actor_profile(a.profile_index);const auto clip=a.content.clip(clip_index);
    if(profile.execution_kind!=uint16_t(ActorExecution::TimelineProxy)||clip.channel!=0)return false;
    if(a.clip_index!=clip_index){a.animation_elapsed=0;a.frame=a.content.key(clip.first_key).frame;}
    a.clip_index=clip_index;
    if(clip.flags&4)finish_action(a);
    return true;
}
bool actor_play_emote(ActorActionState& a,uint32_t clip_index) {
    if(!valid(a)||clip_index>=a.content.clip_count()||a.emote_playing)return false;
    const auto clip=a.content.clip(clip_index);
    if(clip.channel!=1)return false;
    a.emote_clip_index=clip_index;a.emote_elapsed=0;a.emote_playing=true;return true;
}
bool actor_physics_step(ActorActionState& a) {
    if(!valid(a))return false;
    auto next=a;
    if(next.moving) {
        const float x=next.movement.target.x-next.position.x,y=next.movement.target.y-next.position.y;
        const double difference=std::max(std::ceil(std::abs(double(next.movement.speed)*double(physics_delta))),1.0);
        if(std::abs(x)>difference||std::abs(y)>difference) {
            const float length=std::sqrt(x*x+y*y);
            if(!std::isfinite(length)||length<=0)return false;
            if(next.movement_type==ActorMoveType::Step) {
                // Source STEP uses angle_to_point followed by -cos/-sin;
                // this intentionally retains its float trigonometric result.
                const float angle=std::atan2(-y,-x);next.direction={-std::cos(angle),-std::sin(angle)};
            }else next.direction={x/length,y/length};
            blend(next,next.moonwalk?Vec2{-next.direction.x,-next.direction.y}:next.direction);
            next.velocity={next.direction.x*next.movement.speed,next.direction.y*next.movement.speed};
            next.position.x+=next.velocity.x*physics_delta;next.position.y+=next.velocity.y*physics_delta;
        } else {
            next.position=next.movement.target;++next.finished_movement;
            const auto pending=waiters(next,ActorPathWait::Movement);
            for(const auto w:pending)if(w.order&&next.move_paths[w.slot].wait_order==w.order&&next.move_paths[w.slot].wait==ActorPathWait::Movement){++next.move_paths[w.slot].cursor;next_path_entry(next,w.slot);}
        }
    } else if(!next.rotating) {
        next.position.x=std::round(next.position.x);next.position.y=std::round(next.position.y);
        if(next.directional_animation)next.animation_motion=next.talking?5:0;
    }
    // SceneTreeTween processes in creation order. Overlapping jumps continue;
    // the newer tween writes last, while the older still emits its finish.
    for(auto& jump:next.jumps)if(jump.active&&!jump.waiting) {
        jump.elapsed+=double(physics_delta);
        const double ascent=jump.length*next.content.rule_f64(RoomRuleKey::ActorJumpAscentRatio),descent=jump.length*next.content.rule_f64(RoomRuleKey::ActorJumpDescentRatio);
        double height;
        if(jump.elapsed<ascent) {const double t=1-jump.elapsed/ascent;height=jump.height*(1-t*t*t*t);}
        else {const double t=std::min(1.0,(jump.elapsed-ascent)/descent);height=jump.height*(1-t*t);}
        next.sprite_position={0,float(next.content.actor_profile(next.profile_index).sprite_position.y-height)};
        if(jump.elapsed>=jump.length) {
            --jump.remaining;
            if(jump.repeated) {
                // Source waits after EVERY repeated jump, including the last.
                jump.waiting=true;jump.wait_remaining=next.content.rule_f64(RoomRuleKey::ActorJumpRepeatDelaySeconds);
                jump.timer_order=++next.next_wait_order;
            } else {jump.active=false;finish_action(next);}
        }
    }
    unsigned index=0;for(const auto& jump:next.jumps)if(jump.active)next.jumps[index++]=jump;
    while(index<next.jumps.size())next.jumps[index++]=ActorJump{};
    if(!finite(next.position)||!finite(next.velocity)||!finite(next.sprite_position))return false;
    a=next;return true;
}
bool actor_idle_animations(ActorActionState& a,double delta) {
    if(!valid(a)||!std::isfinite(delta)||delta<0||delta>1)return false;
    auto next=a;
    // Godot engine process deltas, animation clocks and scene timers use real_t
    // precision at this boundary even though GDScript numbers are doubles.
    const float dt=float(delta);
    const auto profile=next.content.actor_profile(next.profile_index);
    if(next.directional_animation){if(!advance_directional(next,dt))return false;}
    else if(profile.execution_kind==uint16_t(ActorExecution::DirectionalProxy)||profile.execution_kind==uint16_t(ActorExecution::NpcDirectionalProxy))direction_frame(next);
    else if(next.clip_index!=kRoomNoIndex) {
        const auto clip=next.content.clip(next.clip_index);
        if(clip.flags&2) {
            next.animation_elapsed=std::min(clip.length,float(float(next.animation_elapsed)+dt));
            for(uint32_t i=0;i<clip.key_count;++i){const auto key=next.content.key(clip.first_key+i);if(key.time<=float(next.animation_elapsed))next.frame=key.frame;}
        }else{
            FrameClip frame_clip;if(!next.content.frame_clip(next.clip_index,frame_clip))return false;
            AnimationPlayback playback{float(next.animation_elapsed),{next.frame,true,false}};
            if(!advance_clip(frame_clip,dt,clip.frame_count,playback))return false;
            next.animation_elapsed=playback.position;next.frame=playback.sample.frame;
        }
    }
    if(next.emote_playing) {
        const auto clip=next.content.clip(next.emote_clip_index);
        if(clip.flags&16) {
            const float from=float(next.emote_elapsed),to=std::min(clip.length,from+dt);
            for(uint32_t i=0;i<clip.key_count;++i){const auto key=next.content.key(clip.first_key+i);if(key.time>=from&&(key.time<to||(to==clip.length&&key.time==to)))next.emote_frame=key.frame;}
            next.emote_elapsed=to;
        }else {
            FrameClip frame_clip;if(!next.content.frame_clip(next.emote_clip_index,frame_clip))return false;
            AnimationPlayback playback{float(next.emote_elapsed),{next.emote_frame,true,false}};
            if(!advance_clip(frame_clip,dt,clip.frame_count,playback))return false;
            next.emote_elapsed=playback.position;next.emote_frame=playback.sample.frame;
        }
        if((clip.flags&8)&&next.emote_elapsed>=clip.length)next.emote_playing=false;
    }
    a=next;return true;
}
bool actor_scene_timers(ActorActionState& a,double delta) {
    if(!valid(a)||!std::isfinite(delta)||delta<0||delta>1)return false;
    auto next=a;
    const float dt=float(delta);
    struct Timer {uint32_t order=0;unsigned kind=0,slot=0;};
    std::array<Timer,13> timers{};
    for(unsigned i=0;i<next.move_paths.size();++i)if(next.move_paths[i].wait==ActorPathWait::Timer)timers[i]={next.move_paths[i].wait_order,0,i};
    for(unsigned i=0;i<next.turns.size();++i)if(next.turns[i].active)timers[4+i]={next.turns[i].timer_order,1,i};
    if(next.shaking)timers[8]={next.shake_timer_order,2,0};
    for(unsigned i=0;i<next.jumps.size();++i)if(next.jumps[i].active&&next.jumps[i].waiting)timers[9+i]={next.jumps[i].timer_order,3,i};
    std::sort(timers.begin(),timers.end(),[](Timer x,Timer y){return x.order<y.order;});
    for(const auto timer:timers)if(timer.order) {
    if(timer.kind==0) {
        auto& path=next.move_paths[timer.slot];
        if(path.wait!=ActorPathWait::Timer||path.wait_order!=timer.order)continue;
        path.remaining=float(float(path.remaining)-dt);
        if(path.remaining<0){++path.cursor;next_path_entry(next,timer.slot);}
    } else if(timer.kind==1) {
        auto& turn=next.turns[timer.slot];
        if(!turn.active||turn.timer_order!=timer.order)continue;
        turn.remaining=float(float(turn.remaining)-dt);
        if(turn.remaining<0) {
            if(next.direction.x==turn.target.x&&next.direction.y==turn.target.y) {
                next.rotating=false;next.moving=false;turn.active=false;finish_action(next);
            } else {
                const float cosine=std::cos(turn.angle),sine=std::sin(turn.angle);
                const Vec2 from=next.direction;
                next.direction={std::round(cosine*from.x-sine*from.y),std::round(sine*from.x+cosine*from.y)};
                blend(next,next.direction);
                turn.remaining=turn.interval;turn.timer_order=++next.next_wait_order;
            }
        }
    } else if(timer.kind==2) {
        if(!next.shaking||next.shake_timer_order!=timer.order)continue;
        next.shake_remaining=float(float(next.shake_remaining)-dt);
        if(next.shake_remaining<0) {
            --next.shake_half_steps;
            if(!next.shake_half_steps) {
                next.shaking=false;next.sprite_offset=next.shake_base;
                if(next.looping) {if(!actor_shake(next,next.shake_magnitude,-1))return false;}
                else finish_action(next);
            }
            else {
                const float sign=(next.shake_half_steps%2)?-1.0f:1.0f;
                next.sprite_offset={next.shake_base.x+sign*next.shake_magnitude.x,next.shake_base.y+sign*next.shake_magnitude.y};
                next.shake_remaining=next.content.rule_f64(RoomRuleKey::ActorShakeHalfStepSeconds); // No overshoot carry; a NEW timer is created.
                next.shake_timer_order=++next.next_wait_order;
            }
        }
    } else {
        auto& jump=next.jumps[timer.slot];
        if(!jump.active||!jump.waiting||jump.timer_order!=timer.order)continue;
        jump.wait_remaining=float(float(jump.wait_remaining)-dt);
        if(jump.wait_remaining<0) {
            jump.waiting=false;
            if(jump.remaining)jump.elapsed=0;
            else {jump.active=false;finish_action(next);}
        }
    }
    }
    a=next;return true;
}
bool actor_idle_step(ActorActionState& a,double delta) {
    auto next=a;
    if(!actor_idle_animations(next,delta)||!actor_scene_timers(next,delta))return false;
    a=next;return true;
}
}
