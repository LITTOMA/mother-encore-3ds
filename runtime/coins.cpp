/* Static-convex subset adapted from Godot Engine 3.6.2-stable:
 * servers/physics_2d/{collision_solver_2d_sat,shape_2d_sw,space_2d_sw}.cpp
 * scene/2d/physics_body_2d.cpp; commit 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8.
 * Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
 * Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "encore/coins.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e6&&std::abs(p.y)<=1e6;}
bool services(const CoinsData&d,uint64_t epoch,uint64_t instance,const CoinServices&s,bool prospective=false){return d.valid()&&epoch&&s.epoch==epoch&&instance&&(prospective?!s.owners.count(instance):bool(s.owners.count(instance)))&&s.session&&s.random&&s.hud_ready&&s.sounds.count(d.info().drop_audio)&&s.sounds.count(d.info().collect_audio)&&s.requests.size()<=4096;}
bool post(CoinServices&s,uint64_t instance,std::initializer_list<CoinRequestKind>kinds,std::string&e){
 if(kinds.size()>4096-s.requests.size())return fail(e,"Coin command capacity");auto requests=s.requests;for(auto k:kinds)requests.push_back({k,s.epoch,instance,s.frame});s.requests.swap(requests);return true;
}
}
bool CoinActor::context(uint64_t epoch,const CoinServices&s,std::string&e)const{
 if(!valid_||!data_||data_->generation()!=generation_||epoch!=epoch_||!services(*data_,epoch,instance_,s)||s.session!=session_||s.random!=random_||state_.removed)return fail(e,"Coin prepared owner/epoch/services changed");return true;
}
bool CoinActor::play(uint32_t id,std::string&e){const auto*c=data_->clip(id);if(!c)return fail(e,"Coin clip role lost");if(state_.animation!=id||state_.animation_time==c->duration)state_.animation_time=0;state_.animation=id;state_.playing=true;return true;}
bool CoinActor::prepare(std::shared_ptr<const CoinsData>d,uint64_t epoch,uint64_t instance,Vec2 position,CoinServices&s,std::string&e){
 if(valid_&&!state_.removed)return fail(e,"Coin active actor cannot run Ready twice");if(!s.random)return fail(e,"Coin prepare random service");auto random=*s.random;CoinActor next;if(!next.prepare_ready(std::move(d),epoch,instance,position,s,random,false,e))return false;*this=std::move(next);*s.random=random;e.clear();return true;
}
bool CoinActor::prepare_ready(std::shared_ptr<const CoinsData>d,uint64_t epoch,uint64_t instance,Vec2 position,CoinServices&s,SourceRandom&stream,bool prospective,std::string&e){
 if(!d||!services(*d,epoch,instance,s,prospective)||!finite(position))return fail(e,"Coin prepare data/epoch/position/services");
 CoinActor v;v.data_=std::move(d);v.epoch_=epoch;v.instance_=instance;v.session_=s.session;v.random_=s.random;v.generation_=v.data_->generation();v.last_physics_=s.physics_step;const auto&p=v.data_->policy();const auto&i=v.data_->info();auto random=stream;
 v.state_.position=position;v.state_.offset=p.sprite_offset;v.state_.frame=i.initial_frame;v.state_.glow=v.data_->initial_modifiers()[1];
 const auto sparkle=int64_t(random.rand_range(p.value(CoinScalar::SparkleMinimum),p.value(CoinScalar::SparkleMaximum)));v.state_.sparkle=uint32_t(std::clamp<int64_t>(sparkle,0,i.sparkle_count-1));v.sparkle_left_=float(1.0/double(p.sparkle_fps));
 v.state_.speed=random.rand_range(p.value(CoinScalar::MinimumSpeed),p.value(CoinScalar::MaximumSpeed));v.state_.direction=v.data_->directions()[random.randi()%i.direction_count];v.jump_height_=random.rand_range(p.value(CoinScalar::MinimumHeight),p.value(CoinScalar::MaximumHeight));v.state_.air_time=v.jump_height_*p.value(CoinScalar::AirMultiplier);
 v.tween_duration_=float(v.state_.air_time/2);if(!std::isfinite(v.tween_duration_)||v.tween_duration_<=0)return fail(e,"Coin unsupported jump duration");v.tween_start_=p.sprite_offset.y;v.tween_target_=-float(v.jump_height_);v.pending_pickup_=true;v.pickup_value_=true;v.valid_=true;if(!v.play(i.flipping,e))return false;
 *this=std::move(v);stream=random;e.clear();return true;
}
bool CoinActor::sample(float time,bool seek,float delta,std::string&e){
 const auto*c=data_->clip(state_.animation);if(!c)return fail(e,"Coin animation binding");
 for(uint32_t target=1;target<=2;++target){
  size_t first=0;while(first<c->keys.size()&&uint32_t(c->keys[first].target)<target)++first;size_t count=0;while(first+count<c->keys.size()&&uint32_t(c->keys[first+count].target)==target)++count;if(!count)continue;const auto*keys=c->keys.data()+first;
  const auto find=[&](float value){int low=0,high=int(count)-1,middle=0;while(low<=high){middle=(low+high)/2;const float key_time=keys[size_t(middle)].time,tolerance=std::max(1e-5f,1e-5f*std::abs(value));if(value==key_time||std::abs(value-key_time)<tolerance)return middle;if(value<key_time)high=middle-1;else low=middle+1;}return keys[size_t(middle)].time>value?middle-1:middle;};
  if(target==1&&!seek){
   const auto range=[&](float from,float to){if(from!=c->duration&&to==c->duration)to=float(double(c->duration)*1.001);int end=find(to);if(end>=0&&from==to&&keys[size_t(end)].time==from){state_.frame=uint32_t(keys[size_t(end)].value);return;}if(end>=0&&keys[size_t(end)].time>=to)--end;if(end<0)return;int start=find(from);if(start<0||keys[size_t(start)].time<from)++start;for(int n=start;n<=end;++n)state_.frame=uint32_t(keys[size_t(n)].value);};
   float from=time-delta,to=time;if(from>to)std::swap(from,to);
   if(c->loop){const auto mod=[&](float value){float v=std::fmod(value,c->duration);return v<0?float(v+c->duration):v;};from=mod(from);to=mod(to);if(from>to){range(from,c->duration);range(0,to);continue;}}
   else{from=std::clamp(from,0.0f,c->duration);to=std::clamp(to,0.0f,c->duration);}range(from,to);continue;
  }
  int at=find(time);
  if(target==1){if(count==1)state_.frame=uint32_t(keys[0].value);else if(at>=0)state_.frame=uint32_t(keys[size_t(at)].value);else if(c->loop)state_.frame=uint32_t(keys[count-1].value);continue;}
  if(count==1){state_.glow=keys[0].value;continue;}int next=0;float coefficient=0;
  if(at<0){if(!c->loop)continue;at=int(count-1);const float end=std::max(0.0f,c->duration-keys[count-1].time),span=end+keys[0].time;coefficient=std::abs(span)<1e-5f?0:(end+time)/span;}
  else if(size_t(at+1)<count){next=at+1;const float span=keys[size_t(next)].time-keys[size_t(at)].time;coefficient=std::abs(span)<1e-5f?0:(time-keys[size_t(at)].time)/span;}
  else if(c->loop){const float span=(c->duration-keys[size_t(at)].time)+keys[0].time;coefficient=std::abs(span)<1e-5f?0:(time-keys[size_t(at)].time)/span;}else next=at;
  // Variant::interpolate casts REAL keys to real_t before arithmetic.
  if(at==next)state_.glow=keys[size_t(at)].value;else{const float a=float(keys[size_t(at)].value),b=float(keys[size_t(next)].value);state_.glow=double(a+(b-a)*coefficient);}
 }
 state_.animation_time=time;return true;
}
bool CoinActor::tick(float delta,SourceRandom&random,bool&drop,std::string&e){
 const auto&p=data_->policy();const auto&i=data_->info();
 if(pending_seek_&&last_idle_>landed_idle_){pending_seek_=false;const float seek=float(random.rand_range(0,double(data_->clip(i.grounded)->duration)));const auto*c=data_->clip(state_.animation);float time=seek;if(c->loop){time=std::fmod(time,c->duration);if(!time&&seek)time=c->duration;}else time=std::clamp(time,0.0f,c->duration);if(!sample(time,true,0,e))return false;}
 if(i.sparkle_playing){float remaining=delta;while(remaining){if(sparkle_left_<=0){sparkle_left_=float(1.0/double(p.sparkle_fps));if(state_.sparkle+1>=i.sparkle_count){if(i.sparkle_loop)state_.sparkle=0;}else ++state_.sparkle;}const float step=std::min(sparkle_left_,remaining);remaining-=step;sparkle_left_-=step;}}
 if(state_.playing){const auto*c=data_->clip(state_.animation);const float sum=state_.animation_time+delta;float next=sum;if(c->loop){next=std::fmod(sum,c->duration);if(!next&&sum)next=c->duration;}else next=std::min(sum,c->duration);const float effective_delta=c->loop?delta:next-state_.animation_time;if(!sample(next,effective_delta==0,effective_delta,e))return false;if(!c->loop&&next==c->duration){state_.playing=false;if(state_.animation==i.collect)state_.queued=true;}}
 if(jump_step_<2){float remaining=delta;while(jump_step_<2){tween_time_=float(tween_time_+remaining);if(tween_time_<tween_duration_){const float change=tween_target_-tween_start_;state_.offset.y=change*tween_time_/tween_duration_+tween_start_;break;}state_.offset.y=tween_target_;remaining=tween_time_-tween_duration_;tween_time_=0;++jump_step_;if(jump_step_==1){tween_start_=state_.offset.y;tween_target_=0;}else{if(!play(i.grounded,e))return false;drop=true;pending_pickup_=true;pickup_value_=false;state_.grounded=true;pending_seek_=true;landed_idle_=last_idle_;break;}if(remaining==0)break;}}
 return true;
}
bool CoinActor::advance_idle(uint64_t epoch,double delta,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||!std::isfinite(delta)||delta<0||delta>1||(idle_started_&&s.frame<=last_idle_)||state_.queued)return fail(e,"Coin idle clock/epoch");CoinActor v=*this;auto random=*s.random;v.last_idle_=s.frame;v.idle_started_=true;bool drop=false;if(!v.tick(float(delta),random,drop,e))return false;if(drop&&!post(s,instance_,{CoinRequestKind::DropAudio},e))return false;*this=std::move(v);*s.random=random;e.clear();return true;
}
bool CoinActor::advance_physics(uint64_t epoch,const CircleMotionSolver&solver,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||s.physics_step<=last_physics_||!solver.matches(data_->policy().body_radius,{},data_->policy().margin)||state_.queued)return fail(e,"Coin physics clock/shape/epoch");CoinActor v=*this;v.last_physics_=s.physics_step;if(v.state_.physics){const auto&p=data_->policy();if(v.state_.speed>0&&!v.state_.grounded){const float speed=float(v.state_.speed);const Vec2 velocity{v.state_.direction.x*speed,v.state_.direction.y*speed};if(v.state_.body_disabled)v.state_.position={v.state_.position.x+velocity.x*p.delta,v.state_.position.y+velocity.y*p.delta};else{SlideResult result;if(!solver.advance(v.state_.position,velocity,p.delta,result))return fail(e,"Coin static motion rejected");v.state_.position=result.position;}v.state_.speed-=p.value(CoinScalar::Deceleration)*double(p.delta);}else{v.state_.position={std::round(v.state_.position.x),std::round(v.state_.position.y)};v.state_.physics=false;}if(!finite(v.state_.position))return fail(e,"Coin motion domain");}*this=std::move(v);e.clear();return true;
}
bool CoinActor::collect(CoinServices&s,std::string&e){
 if(state_.queued)return fail(e,"Coin callback after deferred deletion");if(state_.playing&&state_.animation==data_->info().collect){e.clear();return true;}
 const auto amount=data_->info().cash;if(s.session->cash<0||s.session->cash>std::numeric_limits<int64_t>::max()-amount)return fail(e,"Coin cash overflow/domain");CoinActor v=*this;if(!v.play(data_->info().collect,e))return false;v.pending_pickup_=true;v.pickup_value_=true;v.pending_body_=true;if(!post(s,instance_,{CoinRequestKind::CashHud,CoinRequestKind::CollectAudio},e))return false;s.session->cash+=amount;*this=std::move(v);e.clear();return true;
}
bool CoinActor::body_entered(uint64_t epoch,uint64_t body,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||!body)return fail(e,"Coin body contact context");auto found=s.bodies.find(body);if(found==s.bodies.end()||(found->second!=CoinBodyKind::Other&&found->second!=CoinBodyKind::PartyObject))return fail(e,"Coin unprepared body identity/type");if(found->second==CoinBodyKind::Other){e.clear();return true;}return collect(s,e);
}
bool CoinActor::area_entered(uint64_t epoch,uint64_t area,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||!area)return fail(e,"Coin area contact context");auto found=s.areas.find(area);if(found==s.areas.end()||found->second>=1u<<20)return fail(e,"Coin unprepared area identity/layer");if(!(found->second&(1u<<data_->info().pickup_area_bit))){e.clear();return true;}return collect(s,e);
}
bool CoinActor::flush_deferred(uint64_t epoch,CoinServices&s,std::string&e){
 if(!context(epoch,s,e))return false;if(pending_pickup_){state_.pickup_disabled=pickup_value_;pending_pickup_=false;}if(pending_body_){state_.body_disabled=true;pending_body_=false;}if(state_.queued){state_.removed=true;s.owners.erase(instance_);}e.clear();return true;
}
namespace {
bool parent_ready(const CoinParent&p,uint64_t epoch){return p.epoch==epoch&&p.token&&p.position.x==0&&p.position.y==0&&p.rotation==0&&p.scale.x==1&&p.scale.y==1;}
}
bool CoinFactory::context(uint64_t epoch,const CoinServices&s,std::string&e)const{
 if(!valid_||!data_||data_->generation()!=generation_||epoch!=epoch_||s.epoch!=epoch||s.session!=session_||s.random!=random_||!s.session||!s.random||!s.hud_ready||!s.factories.count(instance_)||!s.sounds.count(data_->info().drop_audio)||!s.sounds.count(data_->info().collect_audio)||s.requests.size()>4096||s.owners.size()>4096||!s.next_owner||s.next_owner==UINT64_MAX||s.next_owner<=s.last_assigned_owner||s.owners.count(s.next_owner)||instances_.count(s.next_owner))return fail(e,"Coin factory source/epoch/services/allocation");
 for(const auto&entry:instances_)if(entry.second.instance()!=entry.first||entry.second.data_.get()!=data_.get()||(!entry.second.snapshot().removed&&!entry.second.context(epoch,s,e))||(entry.second.snapshot().removed&&s.owners.count(entry.first)))return fail(e,"Coin factory child binding changed");
 const auto parent=s.parents.find(data_->assets()[5]);if(parent==s.parents.end()||parent->second.token!=parent_||!parent_ready(parent->second,epoch))return fail(e,"Coin factory prepared parent changed");return true;
}
bool CoinFactory::prepare(std::shared_ptr<const CoinsData>d,uint64_t epoch,uint64_t instance,Vec2 position,CoinServices&s,std::string&e){
 if(!d||!d->valid()||!instances_.empty()||!epoch||s.epoch!=epoch||!instance||!s.factories.count(instance)||!finite(position))return fail(e,"Coin factory prepare source/instance/position");const auto parent=s.parents.find(d->assets()[5]);if(parent==s.parents.end()||!parent_ready(parent->second,epoch))return fail(e,"Coin factory parent must be ready with admitted identity transform");
 CoinFactory next;next.data_=std::move(d);next.epoch_=epoch;next.instance_=instance;next.position_=position;next.session_=s.session;next.random_=s.random;next.generation_=next.data_->generation();next.parent_=parent->second.token;next.remaining_=next.data_->info().default_coins;next.last_physics_=s.physics_step;next.valid_=true;if(!next.context(epoch,s,e))return false;*this=std::move(next);e.clear();return true;
}
bool CoinFactory::set_position(uint64_t epoch,Vec2 position,CoinServices&s,std::string&e){if(!context(epoch,s,e)||!finite(position))return fail(e,"Coin factory position/context");position_=position;e.clear();return true;}
bool CoinFactory::spawn(uint64_t epoch,uint32_t amount,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||!amount||amount>1024)return fail(e,"Coin factory unadmitted zero/negative/unbounded burst");remaining_=amount;spawning_=true;e.clear();return true;
}
bool CoinFactory::advance_physics(uint64_t epoch,CoinServices&s,std::string&e){
 if(!context(epoch,s,e)||s.physics_step<=last_physics_)return fail(e,"Coin factory physics clock/context");
 if(!spawning_){last_physics_=s.physics_step;e.clear();return true;}
 double timer=timer_;uint32_t remaining=remaining_;bool spawning=spawning_;auto random=*s.random;std::map<uint64_t,CoinActor>created;std::set<uint64_t>owners;const bool birth=timer>=data_->policy().value(CoinScalar::SpawnInterval);
 if(birth){if(s.owners.size()>=4096||instances_.size()>=4096)return fail(e,"Coin factory instance capacity");CoinActor next;if(!next.prepare_ready(data_,epoch_,s.next_owner,position_,s,random,true,e))return false;created.emplace(s.next_owner,std::move(next));owners.insert(s.next_owner);--remaining;timer=0;if(!remaining)spawning=false;}
 timer+=double(data_->policy().delta);if(!std::isfinite(timer))return fail(e,"Coin factory timer overflow");
 // All new nodes were allocated during preparation. Map/set node transfer
 // preserves existing actor addresses and cannot allocate after commit starts.
 if(birth){instances_.merge(created);s.owners.merge(owners);s.last_assigned_owner=s.next_owner;++s.next_owner;*s.random=random;}timer_=timer;remaining_=remaining;spawning_=spawning;last_physics_=s.physics_step;e.clear();return true;
}
bool CoinFactory::retire_removed(uint64_t epoch,uint64_t instance,CoinServices&s,std::string&e){
 if(!context(epoch,s,e))return false;auto found=instances_.find(instance);if(found==instances_.end()||!found->second.snapshot().removed||s.owners.count(instance))return fail(e,"Coin factory child not retired by deferred deletion");instances_.erase(found);e.clear();return true;
}
bool CoinFactory::leave(uint64_t epoch,CoinServices&s,std::string&e){
 if(!context(epoch,s,e))return false;for(const auto&r:s.requests)if(instances_.count(r.instance))return fail(e,"Coin factory undelivered child commands");for(const auto&entry:instances_)s.owners.erase(entry.first);s.factories.erase(instance_);instances_.clear();valid_=false;e.clear();return true;
}

}
