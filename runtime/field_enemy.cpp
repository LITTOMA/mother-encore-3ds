#include "encore/field_enemy.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace encore::upstream {
namespace {
using V=FieldEnemyVector;
V add(V a,V b){return {a.x+b.x,a.y+b.y};}V sub(V a,V b){return {a.x-b.x,a.y-b.y};}V mul(V a,float b){return {a.x*b,a.y*b};}float length(V a){return std::sqrt(a.x*a.x+a.y*a.y);}V direction(V a,V b){const auto delta=sub(b,a);const auto n=length(delta);return n?mul(delta,1/n):V{};}V rounded(V a){return {std::round(a.x),std::round(a.y)};}bool nonzero(V a){return a.x!=0||a.y!=0;}V toward(V a,V b,float step){const auto d=sub(b,a);const auto n=length(d);return n<=step||n==0?b:add(a,mul(d,step/n));}bool finite(V v){return std::isfinite(v.x)&&std::isfinite(v.y);}bool delta(float t){return std::isfinite(t)&&t>=0&&t<=60;}
// GDScript integers are signed 64-bit. Check the half-open representable range
// before conversion: double(INT64_MAX) rounds up to 2^63 and is not castable.
bool checked_integer(double value,int64_t&out){const double limit=std::ldexp(1.0,63);if(!std::isfinite(value)||value < -limit||value>=limit)return false;out=static_cast<int64_t>(value);return true;}
}
bool FieldEnemyRuntime::fail(const char*e){error_=e;return false;}float FieldEnemyRuntime::parameter(FieldEnemyParameter p)const{return data_->parameter(p);}
size_t FieldEnemyRuntime::spawner_index(uint32_t id)const{if(!data_)return size_t(-1);for(size_t i=0;i<data_->spawners().size();++i)if(data_->spawners()[i].id==id)return i;return size_t(-1);}
FieldEnemyInstance*FieldEnemyRuntime::enemy(uint64_t id){for(auto&v:enemies_)if(v.id==id&&!v.erased)return &v;fail("Unknown/erased field enemy identity");return nullptr;}
bool FieldEnemyRuntime::context(uint64_t id,FieldEnemyContext&out){if(!host_.context(id,out,error_))return false;if(!finite(out.player_ground)||!finite(out.party_midpoint)||!finite(out.player_direction)||out.highest_party_level>1000000||out.leader_offense<0)return fail("Field enemy host context rejected");return true;}
bool FieldEnemyRuntime::present(FieldEnemyInstance&v,FieldEnemyPresentation p){return host_.present(v.id,p,v,error_);}
bool FieldEnemyRuntime::initialize(const FieldEnemyData*d,SourceRandom*r,FieldEnemyHost h,std::string&e){
 if(!d||!d->valid()||d->capabilities()!=2||!r||!h.context||!h.create||!h.wander_raycast||!h.move_and_slide||!h.roster||!h.battle||!h.present||!h.animation||!h.queue_battle||!h.damage_number||!h.reward){e="Field Enemy requires checked data/shared random and complete collision/battle/presentation hosts";return false;}
 FieldEnemyRuntime next;next.data_=d;next.random_=r;next.host_=std::move(h);next.spawners_.resize(d->spawners().size());*this=std::move(next);e.clear();return true;
}
bool FieldEnemyRuntime::ready(uint32_t id){const auto i=spawner_index(id);if(i==size_t(-1)||spawners_[i].ready||!spawners_[i].alive)return fail("Field enemy spawner Ready identity/order rejected");spawners_[i].ready=true;return true;}
bool FieldEnemyRuntime::spawner_screen_entered(uint32_t id){
 const auto i=spawner_index(id);if(i==size_t(-1)||!spawners_[i].ready||!spawners_[i].alive)return fail("Unadmitted field enemy screen event");auto&s=spawners_[i];const auto&source=data_->spawners()[i];
 // GDScript evaluates randi before every other short-circuit gate.
 const auto roll=random_->randi()%uint32_t(parameter(FieldEnemyParameter::AppearanceModulus));if(roll>source.appearance_rate||source.appearance_rate==0||s.attached)return true;FieldEnemyContext ctx;if(!context(0,ctx))return false;if(ctx.cutscene||s.cooldown>0)return true;
 const auto&p=data_->profiles()[source.profile];FieldEnemyInstance v;v.id=next_id_;if(!v.id||next_id_==UINT64_MAX)return fail("Field Enemy dynamic identity exhausted");v.spawner=uint32_t(i);v.profile=source.profile;v.position=v.start_position=v.new_position=source.position;v.hp=p.hp;v.pp=p.pp;v.returning=(source.flags&1)!=0;v.underlevel=ctx.highest_party_level>=uint32_t(p.level)+uint32_t(parameter(FieldEnemyParameter::UnderlevelGap));
 if(!host_.create(v.id,p,data_->geometry(),v.position,error_))return false;
 v.input.x=float(std::round(random_->rand_range(parameter(FieldEnemyParameter::ReadyDirectionMin),parameter(FieldEnemyParameter::ReadyDirectionMax))));v.input.y=float(std::round(random_->rand_range(parameter(FieldEnemyParameter::ReadyDirectionMin),parameter(FieldEnemyParameter::ReadyDirectionMax))));v.direction=v.input;
 if(p.walk_frequency!=0)v.wander_time=float(random_->rand_range(parameter(FieldEnemyParameter::ReadyTimerMin),p.walk_frequency));
 if((source.flags&4)&&nonzero(source.initial_direction))v.direction=v.input=source.initial_direction;v.state=FieldEnemyState::Wander;v.physics=false;enemies_.push_back(v);++next_id_;s.attached=v.id;if(source.flags&2)s.queued_free=true;return present(enemies_.back(),FieldEnemyPresentation::Idle);
}
bool FieldEnemyRuntime::spawner_tree_exited(uint32_t id){const auto i=spawner_index(id);if(i==size_t(-1)||!spawners_[i].alive)return fail("Field enemy spawner tree exit rejected");auto&s=spawners_[i];s.alive=false;s.queued_free=false;return !s.attached||die(s.attached,false);}
bool FieldEnemyRuntime::end_frame(){
 if(!data_)return fail("Field Enemy not initialized");
 for(size_t i=0;i<spawners_.size();++i)if(spawners_[i].queued_free&&!spawner_tree_exited(data_->spawners()[i].id))return false;
 // Godot queue_free leaves the object alive until the deferred deletion pass.
 // All continuations from the current animation signal have already resumed.
 for(auto&v:enemies_)if(v.queued_free&&!v.erased&&!tree_exiting(v.id,false))return false;
 enemies_.erase(std::remove_if(enemies_.begin(),enemies_.end(),[](const auto&v){return v.erased;}),enemies_.end());return true;
}
bool FieldEnemyRuntime::choose(FieldEnemyInstance&v){
 const auto&p=data_->profiles()[v.profile];
 // The source recurses only for short travel. Bound stack-equivalent work as a
 // defensive refusal, never replace a random draw or silently accept a target.
 for(size_t attempt=0;attempt<4096;++attempt){auto travel=v.position;if(random_->randi()%uint32_t(parameter(FieldEnemyParameter::WanderAxisModulus))==1)travel.x=float(random_->rand_range(v.start_position.x-p.wander_radius/2,v.start_position.x+p.wander_radius/2));else travel.y=float(random_->rand_range(v.start_position.y-p.wander_radius/2,v.start_position.y+p.wander_radius/2));travel=rounded(travel);bool hit=false;if(!host_.wander_raycast(v.id,sub(travel,v.position),hit,error_))return false;if(std::abs(travel.x-v.position.x)>parameter(FieldEnemyParameter::MinMovementLength)||std::abs(travel.y-v.position.y)>parameter(FieldEnemyParameter::MinMovementLength)){if(!hit){v.new_position=travel;v.input=direction(v.position,travel);}return true;}}
 return fail("Field Enemy recursive wander exhausted defensive work budget");
}
bool FieldEnemyRuntime::start_wander(FieldEnemyInstance&v){if(!choose(v))return false;v.state=FieldEnemyState::Wander;const auto frequency=data_->profiles()[v.profile].walk_frequency;const auto deviation=parameter(FieldEnemyParameter::WanderTimerDeviation);const auto timer=float(random_->rand_range(frequency-deviation,frequency+deviation));if(timer<=0)return fail("Unadmitted source nonpositive Wander Timer assignment");v.wander_time=timer;v.wander_timer_running=true;return true;}
bool FieldEnemyRuntime::start_chase(FieldEnemyInstance&v,const FieldEnemyContext&ctx){if((v.state!=FieldEnemyState::Wander&&v.state!=FieldEnemyState::Return)||ctx.emote_playing||ctx.paused)return true;v.state=FieldEnemyState::Chase;v.wander_timer_running=false;v.chase_time=data_->profiles()[v.profile].chase_delay;return present(v,FieldEnemyPresentation::Jump)&&present(v,v.underlevel?FieldEnemyPresentation::BlueExclamation:FieldEnemyPresentation::Exclamation);}
bool FieldEnemyRuntime::chase_stop(FieldEnemyInstance&v){const auto&p=data_->profiles()[v.profile];v.velocity=toward(v.velocity,{},p.friction);v.return_time=p.return_delay;if(v.returning){v.state=FieldEnemyState::Return;v.new_position=v.start_position;}else v.state=FieldEnemyState::Wander;return present(v,FieldEnemyPresentation::Idle);}
bool FieldEnemyRuntime::screen_entered(uint64_t id){auto*v=enemy(id);if(!v)return false;v->on_screen=true;v->contact_enabled=true;if(!start_wander(*v))return false;v->physics=true;if(!host_.roster(id,FieldEnemyRoster::Add,*v,error_))return false;v->roster_member=true;return true;}
bool FieldEnemyRuntime::screen_exited(uint64_t id){auto*v=enemy(id);if(!v)return false;v->on_screen=false;v->wander_timer_running=false;v->physics=false;if(!host_.roster(id,FieldEnemyRoster::Remove,*v,error_))return false;v->roster_member=false;return true;}
bool FieldEnemyRuntime::view_entered(uint64_t id,bool player){auto*v=enemy(id);if(!v)return false;if(player)v->seeing=true;return true;}bool FieldEnemyRuntime::view_exited(uint64_t id,bool player){auto*v=enemy(id);if(!v)return false;if(player)v->seeing=false;return true;}bool FieldEnemyRuntime::blind_entered(uint64_t id,bool player){auto*v=enemy(id);if(!v)return false;if(player)v->blind=true;return true;}bool FieldEnemyRuntime::blind_exited(uint64_t id,bool player){auto*v=enemy(id);if(!v)return false;if(player)v->blind=false;return true;}
bool FieldEnemyRuntime::wander_timeout(uint64_t id){auto*v=enemy(id);if(!v)return false;if(data_->profiles()[v->profile].walk_frequency!=0&&v->on_screen&&v->state==FieldEnemyState::Wander)return choose(*v)&&start_wander(*v);return true;}
bool FieldEnemyRuntime::physics_step(uint64_t id,float dt){
 auto*v=enemy(id);if(!v||!delta(dt))return fail("Field Enemy physics delta rejected");if(!v->physics)return true;FieldEnemyContext ctx;if(!context(id,ctx))return false;if(ctx.paused||ctx.entering_door)return !ctx.paused||present(*v,FieldEnemyPresentation::AnimationDisabled);if(!present(*v,FieldEnemyPresentation::AnimationEnabled))return false;const auto&p=data_->profiles()[v->profile];
 if(ctx.event_ray_on_player&&length(sub(v->position,v->start_position))<=p.max_distance){v->disinterest_time=0;if(v->state!=FieldEnemyState::Chase&&v->state!=FieldEnemyState::Stunned&&(v->seeing||(ctx.running&&ctx.substantial_movement))&&!v->blind&&!start_chase(*v,ctx))return false;}else if(v->state==FieldEnemyState::Chase){v->disinterest_time+=dt;if(v->disinterest_time>=parameter(FieldEnemyParameter::MinDisinterestTime)&&!chase_stop(*v))return false;}
 const auto old=v->position;const auto difference=std::max(std::ceil(std::abs(p.max_speed*dt)),1.f);const auto distant=[&](){return std::abs(v->position.x-v->new_position.x)>difference||std::abs(v->position.y-v->new_position.y)>difference;};
 switch(v->state){
  case FieldEnemyState::Wander:if(distant()){v->input=direction(v->position,v->new_position);v->velocity=toward(v->velocity,mul(v->input,p.max_speed),p.acceleration*dt);}else v->velocity={};break;
  case FieldEnemyState::Return:if(v->return_time==0){v->input=direction(v->position,v->new_position);v->velocity=toward(v->velocity,mul(v->input,p.max_speed),p.acceleration*dt);}if(distant()){v->input={};if(!start_wander(*v))return false;}break;
  case FieldEnemyState::Chase:v->input=v->underlevel?direction(ctx.party_midpoint,v->position):direction(v->position,ctx.party_midpoint);if(v->chase_time==0)v->velocity=toward(v->velocity,mul(v->input,p.max_speed),p.acceleration*dt);break;
  case FieldEnemyState::Stunned:break;
  default:return fail("Unknown field enemy state");
 }
 v->input=rounded(v->input);V moved=v->position,velocity=v->velocity;if(!host_.move_and_slide(id,v->velocity,dt,moved,velocity,error_)||!finite(moved)||!finite(velocity))return fail("Field Enemy move_and_slide host rejected");v->position=moved;v->velocity=velocity;v->knockback=toward(v->knockback,{},parameter(FieldEnemyParameter::KnockbackDeceleration)*dt);V knockback=v->knockback;if(!host_.move_and_slide(id,v->knockback,dt,moved,knockback,error_)||!finite(moved)||!finite(knockback))return fail("Field Enemy knockback move_and_slide rejected");v->position=rounded(moved);v->knockback=knockback;
 if(nonzero(v->velocity)&&(v->position.x!=old.x||v->position.y!=old.y)){v->direction=rounded(direction({},v->velocity));return present(*v,FieldEnemyPresentation::Walk);}return present(*v,FieldEnemyPresentation::Idle);
}
bool FieldEnemyRuntime::start_battle(FieldEnemyInstance&v,int32_t advantage){v.drafted=true;if(!host_.roster(v.id,v.roster_member?FieldEnemyRoster::MoveToFront:FieldEnemyRoster::AddToFront,v,error_))return false;v.roster_member=true;return host_.battle(v.id,advantage,v,error_);}
bool FieldEnemyRuntime::contact(uint64_t id,bool party_object){
 auto*v=enemy(id);if(!v)return false;FieldEnemyContext ctx;if(!context(id,ctx))return false;if(!party_object)return true;if(!v->await_unpaused_contact&&(!v->visible||ctx.cutscene||ctx.battle_queued||ctx.entering_door||!ctx.can_pause||!v->contact_enabled))return true;if(ctx.paused){v->await_unpaused_contact=true;return true;}v->await_unpaused_contact=false;
 if(v->underlevel&&ctx.running&&ctx.substantial_movement){if(v->state==FieldEnemyState::Stunned)return true;v->state=FieldEnemyState::Stunned;v->wander_timer_running=false;v->physics=false;v->velocity={};v->contact_enabled=false;v->underlevel_flash=true;const auto length=parameter(FieldEnemyParameter::UnderlevelFlashLength),interval=parameter(FieldEnemyParameter::UnderlevelFlashInterval);int64_t cycles=0;if(!checked_integer(double(length)/(double(interval)*2.0),cycles)||cycles<0||cycles>UINT32_MAX)return fail("Field Enemy flash loop numeric schema rejected");const double time=parameter(FieldEnemyParameter::UnderlevelFlashDelay)+double(cycles)*interval*2.0;if(!std::isfinite(time)||time>std::numeric_limits<float>::max())return fail("Field Enemy flash timer numeric schema rejected");v->flash_time=float(time);return present(*v,FieldEnemyPresentation::UnderlevelKnockout);}
 if(v->animation_running&&v->current_animation==FieldEnemyAnimation::Flash)return true;v->contact_enabled=false;if(!chase_stop(*v))return false;const auto advantage=ctx.player_can_see_enemy?(v->seeing?parameter(FieldEnemyParameter::AdvantageNeutral):parameter(FieldEnemyParameter::AdvantagePlayer)):parameter(FieldEnemyParameter::AdvantageEnemy);return start_battle(*v,int32_t(advantage));
}
bool FieldEnemyRuntime::hurt(uint64_t id,FieldEnemyHurt kind){
 auto*v=enemy(id);if(!v)return false;FieldEnemyContext ctx;if(!context(id,ctx))return false;if(ctx.paused||ctx.cutscene||v->drafted)return true;if(kind==FieldEnemyHurt::StunGun)return stun(id);int32_t damage=0;
 int64_t wide_damage=0;
 if(kind==FieldEnemyHurt::Explosion){if(!checked_integer(parameter(FieldEnemyParameter::ExplosionBase),wide_damage))return fail("Field Enemy explosion damage integer rejected");wide_damage+=random_->randi()%uint32_t(parameter(FieldEnemyParameter::ExplosionModulus));}
 else if(kind==FieldEnemyHurt::Bat||kind==FieldEnemyHurt::Fire){
  if(kind==FieldEnemyHurt::Bat&&!ctx.event_ray_on_player)return true;
  // The source parameter val is typed int: each float assignment truncates
  // before the following round. Its final val/4 is integer division.
  const double base=std::max(1.0,double(parameter(FieldEnemyParameter::BashPower))+ctx.leader_offense-double(data_->profiles()[v->profile].defense)/2.0);
  if(!checked_integer(base,wide_damage))return fail("Field Enemy base damage integer rejected");
  const double variance=parameter(FieldEnemyParameter::BashVariance);
  if(!checked_integer(double(wide_damage)+random_->randf()*variance-variance/2.0,wide_damage))return fail("Field Enemy variance damage integer rejected");
  if(ctx.leader_incapacitated)wide_damage/=int64_t(parameter(FieldEnemyParameter::IncapacitatedDivisor));
 }else return fail("Unknown field hurt capability");
 // The native battle/save and flying-number interfaces use signed 32-bit
 // values. Refuse an unrepresentable result before queue/HP/presentation writes.
 if(wide_damage<INT32_MIN||wide_damage>INT32_MAX)return fail("Field Enemy damage exceeds native character numeric schema");damage=static_cast<int32_t>(wide_damage);
 if(!next_waiter_||next_waiter_==UINT64_MAX)return fail("Field Enemy coroutine subscription identity exhausted");if(!host_.queue_battle(true,error_))return false;v->hp=int32_t(std::clamp(int64_t(v->hp)-damage,int64_t(0),int64_t(data_->profiles()[v->profile].max_hp)));v->knockback=mul(ctx.player_direction,parameter(FieldEnemyParameter::Knockback));if(!host_.damage_number(id,damage,error_)||!play_animation(*v,FieldEnemyAnimation::Flash))return false;v->contact_enabled=false;
 // Source yield subscribes after stop/play. Restarting a clip preserves all
 // previous subscriptions, which await the next animation_finished signal.
 v->damage_waiters.push_back(next_waiter_++);v->pending_damage=true;return present(*v,FieldEnemyPresentation::DamageFlash);
}
bool FieldEnemyRuntime::finish_damage(FieldEnemyInstance&v){if(!host_.queue_battle(false,error_))return false;FieldEnemyContext ctx;if(!context(v.id,ctx))return false;if(v.hp<=0){const auto&p=data_->profiles()[v.profile];if(!die(v.id,false)||!host_.reward(p.experience,p.cash,error_))return false;}else if(!ctx.cutscene&&!start_battle(v,int32_t(parameter(FieldEnemyParameter::AdvantageNeutral))))return false;v.knockback={};v.contact_enabled=true;return true;}
bool FieldEnemyRuntime::play_animation(FieldEnemyInstance&v,FieldEnemyAnimation kind){
 const auto*binding=data_->animation(kind);if(!binding||v.animation_generation==UINT64_MAX)return fail("Field Enemy checked animation/generation rejected");
 v.current_animation=kind;v.animation_running=true;++v.animation_generation;
 return host_.animation(v.id,*binding,v.animation_generation,v,error_);
}
bool FieldEnemyRuntime::animation_finished(uint64_t id,FieldEnemyAnimation kind,uint64_t generation){
 auto*v=enemy(id);if(!v)return false;
 if(!v->animation_running||kind==FieldEnemyAnimation::None||kind!=v->current_animation||generation!=v->animation_generation||!data_->animation(kind))return fail("Field Enemy stale/unknown animation completion rejected");
 const auto subscriptions=v->damage_waiters;
 v->animation_running=false;v->current_animation=FieldEnemyAnimation::None;
 // Snapshot subscriptions before dispatch, matching the source signal emitter;
 // an await registered during a connected handler waits for a later signal.
 // The scene-connected handler is registered before any _do_damage yield.
 // Its source RNG (wander and possibly chase) executes before Hurt resumes.
 if(kind==FieldEnemyAnimation::Stun){if(!start_wander(*v))return false;FieldEnemyContext ctx;if(!context(id,ctx)||(ctx.event_ray_on_player&&!start_chase(*v,ctx))||!present(*v,FieldEnemyPresentation::Restore))return false;}
 for(const auto receipt:subscriptions){
  if(v->erased)break;const auto at=std::find(v->damage_waiters.begin(),v->damage_waiters.end(),receipt);
  if(at==v->damage_waiters.end())return fail("Field Enemy coroutine subscription ordering rejected");
  v->damage_waiters.erase(at);v->pending_damage=!v->damage_waiters.empty();if(!finish_damage(*v))return false;
 }
 return true;
}
bool FieldEnemyRuntime::stun(uint64_t id){auto*v=enemy(id);if(!v)return false;if(!play_animation(*v,FieldEnemyAnimation::Stun))return false;v->state=FieldEnemyState::Stunned;v->wander_timer_running=false;v->velocity={};return present(*v,FieldEnemyPresentation::Stun);}
bool FieldEnemyRuntime::idle_step(float dt){
 if(!data_||!delta(dt))return fail("Field Enemy idle delta rejected");for(auto&s:spawners_)if(s.alive)s.cooldown=std::max(0.f,s.cooldown-dt);
 for(auto&v:enemies_){if(v.erased)continue;v.return_time=std::max(0.f,v.return_time-dt);v.chase_time=std::max(0.f,v.chase_time-dt);if(v.wander_timer_running){v.wander_time-=dt;if(v.wander_time<=0){v.wander_time=data_->profiles()[v.profile].walk_frequency;if(!wander_timeout(v.id))return false;}}
  if(v.await_unpaused_contact){FieldEnemyContext ctx;if(!context(v.id,ctx))return false;if(!ctx.paused&&!contact(v.id,true))return false;}
  // DamageAnimation is completed only by the host's typed source signal;
  // idle delta also drives independent native timers and SceneTreeTimer flash.
  if(v.underlevel_flash){v.flash_time=std::max(0.f,v.flash_time-dt);if(v.flash_time==0){v.underlevel_flash=false;if(!start_wander(v))return false;v.physics=true;v.contact_enabled=true;FieldEnemyContext ctx;if(!context(v.id,ctx)||(ctx.event_ray_on_player&&!start_chase(v,ctx))||!present(v,FieldEnemyPresentation::Restore))return false;}}
 }return true;
}
bool FieldEnemyRuntime::die(uint64_t id,bool in_battle){auto*v=enemy(id);if(!v)return false;FieldEnemyContext ctx;if(!context(id,ctx))return false;if(ctx.in_battle!=in_battle)return true;v->queued_free=true;
 // enemy_erased is emitted on each call, even when already queued for free.
 // Each subscribed spawner handler clears attached and restarts its timer.
 auto&s=spawners_[v->spawner];s.attached=0;if(s.alive)s.cooldown=data_->spawners()[v->spawner].cooldown;return present(*v,FieldEnemyPresentation::Erase);
}
bool FieldEnemyRuntime::tree_exiting(uint64_t id,bool changing_parents){auto*v=enemy(id);if(!v)return false;if(changing_parents)return true;if(!die(id,false))return false;
 // Actual lifetime termination invalidates every suspended source coroutine;
 // no queue_battle(false), battle or reward is synthesized for cancellation.
 v->damage_waiters.clear();v->pending_damage=false;v->animation_running=false;v->current_animation=FieldEnemyAnimation::None;v->physics=false;v->wander_timer_running=false;v->erased=true;return true;
}
bool FieldEnemyRuntime::activate(uint64_t id){auto*v=enemy(id);if(!v)return false;v->physics=true;v->drafted=false;v->visible=true;v->velocity=v->knockback={};v->contact_enabled=true;v->hp=data_->profiles()[v->profile].max_hp;v->pp=data_->profiles()[v->profile].max_pp;return present(*v,FieldEnemyPresentation::Restore);}
}
