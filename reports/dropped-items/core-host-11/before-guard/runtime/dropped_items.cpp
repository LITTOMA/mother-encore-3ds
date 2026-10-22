#include "encore/dropped_item_actor.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool finite(BreakablePoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e6&&std::abs(p.y)<=1e6;}
bool map_flag(const std::map<std::string,bool>&m,const std::string&key){const auto it=m.find(key);return it!=m.end()&&it->second;}
bool snapshot_flag(const std::vector<SessionFlag>&flags,const std::string&key,bool&value){
 value=false;bool found=false;for(const auto&f:flags)if(f.id==key){if(found)return false;found=true;value=f.value;}return true;
}
bool put_flag(std::vector<SessionFlag>&flags,const std::string&key,bool value){
 for(auto&f:flags)if(f.id==key){f.value=value;return true;}
 if(flags.size()>=session_save_max_entries)return false;flags.push_back({key,value});return true;
}
// Adapted Godot 3.6.2 Node2D/Tween and Penner equations, commit
// 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8. Copyright (c) Godot
// contributors, Juan Linietsky, Ariel Manzur and Robert Penner. MIT notices:
// docs/licenses/Godot-3.6.2-LICENSE.txt, docs/licenses/Penner-easing-MIT.txt.
// Equations and engine numerical invariants only; all selected curves, offsets,
// durations and property endpoints come from the checked binary resource.
constexpr double pi=3.1415926535897932384626433833;
float node_scale(float value){return std::abs(value)<float(0.00001)?float(0.00001):value;}
float node_degrees(float value){return float(value*float(pi/180.0))*float(180.0/pi);}
float equation(double transition,double ease,float t,float b,float c,float d){
 if(transition==double(DroppedTransition::Quart)&&ease==double(DroppedEase::Out))return float(-c*(std::pow(t/d-1,4)-1)+b);
 if(transition==double(DroppedTransition::Quad)&&ease==double(DroppedEase::In))return float(c*std::pow(t/d,2)+b);
 if(transition==double(DroppedTransition::Elastic)&&ease==double(DroppedEase::Out)){
  if(t==0)return b;t/=d;if(t==1)return b+c;const float p=d*0.3f,s=p/4;
  return float(c*std::pow(2,-10*t)*std::sin((t*d-s)*(2*pi)/p)+c+b);
 }
 return std::numeric_limits<float>::quiet_NaN();
}
}

BreakableContentState*DroppedItemActor::content(DroppedServices&s)const{
 if(!s.world||!owner_)return nullptr;const auto it=s.world->contents.find(owner_->path);return it==s.world->contents.end()?nullptr:&it->second;
}
bool DroppedItemActor::context(uint64_t epoch,const DroppedServices&s,std::string&e,bool retired)const{
 if(!valid_||!data_||!data_->valid()||s.data!=data_.get()||data_->generation()!=generation_||!epoch||epoch!=epoch_||s.epoch!=epoch||!owner_||!s.world||s.world!=world_||s.session!=session_||s.random!=random_||s.uid_ledger!=ledger_||!s.session||!s.random||!s.uid_ledger||!s.clock)return fail(e,"Dropped prepared data/owner/epoch/services changed");
 if(s.world->epoch!=epoch||s.world->scene!=data_->info().scene||s.requests.size()>4096||!finite(s.player)||!s.item_templates.count(owner_->template_id)||!s.dialogues.count(data_->info().success_dialogue)||!s.dialogues.count(data_->info().full_dialogue))return fail(e,"Dropped prepared scene/item/dialogue services");
 const auto ticket=s.owners.find(instance_);
 if(pose_.removed){if(!retired||ticket!=s.owners.end())return fail(e,"Dropped retired owner ticket");return true;}
 if(ticket==s.owners.end()||ticket->second!=owner_->id)return fail(e,"Dropped owner ticket replaced");
 const auto root=s.world->contents.find(owner_->path);
 const auto geometry=s.shapes.find(instance_);
 if(geometry==s.shapes.end()||geometry->second!=shapes_||!shapes_[0]||!shapes_[1]||shapes_[0]==shapes_[1])return fail(e,"Dropped prepared collider handles replaced");
 if(root==s.world->contents.end()||&root->second!=content_||!root->second.present||!root->second.has_interact||!finite(root->second.position)||root->second.shapes.size()!=1||!root->second.shapes.count(shapes_[1])||!s.world->parents.count(root->second.parent)||!s.identity_parents.count(root->second.parent))return fail(e,"Dropped prepared root/parent/interaction-shape binding");
 for(const auto&shape:root->second.shapes)if(!shape.first)return fail(e,"Dropped retired shape handle");
 bool value=false;const auto&flags=owner_->object_flag()?s.session->object_flags:s.session->flags;
 const auto&mirror=owner_->object_flag()?s.world->object_flags:s.world->story_flags;
 if(!snapshot_flag(flags,owner_->flag,value)||value!=map_flag(mirror,owner_->flag))return fail(e,"Dropped source flag views disagree");
 for(const auto&r:s.requests)if(uint32_t(r.kind)<1||uint32_t(r.kind)>7||r.epoch!=epoch)return fail(e,"Dropped unknown request kind/epoch");
 return true;
}
bool DroppedItemActor::request(DroppedServices&s,DroppedRequestKind kind,std::string target,BreakablePoint position,std::string&e){
 if(s.requests.size()>=4096)return fail(e,"Dropped request capacity");
 s.requests.push_back({kind,epoch_,instance_,s.frame,prompt_token_,owner_->id,std::move(target),position,false});return true;
}
bool DroppedItemActor::queue_free(DroppedServices&s,std::string&e){
 if(pose_.queued)return true;if(!request(s,DroppedRequestKind::FreeItem,{}, {},e))return false;pose_.queued=true;return true;
}
bool DroppedItemActor::set_flag(bool value,DroppedServices&s,std::string&e){
 auto&flags=owner_->object_flag()?s.session->object_flags:s.session->flags;
 auto&mirror=owner_->object_flag()?s.world->object_flags:s.world->story_flags;
 bool found=false;for(const auto&flag:flags)if(flag.id==owner_->flag)found=true;
 if(!found&&flags.size()>=session_save_max_entries)return fail(e,"Dropped flag capacity");
 if(!put_flag(flags,owner_->flag,value))return fail(e,"Dropped flag commit");mirror[owner_->flag]=value;return true;
}
bool DroppedItemActor::prepare(std::shared_ptr<const DroppedItemsData>data,uint32_t owner,uint64_t epoch,uint64_t instance,DroppedServices&s,std::string&e){
 if(valid_&&!pose_.removed)return fail(e,"Dropped live owner cannot run Ready twice");
 if(!data||!data->valid()||!instance||!data->owner(owner)||!s.world||!s.session||!s.random||!s.uid_ledger)return fail(e,"Dropped Ready input");
 const auto prompt=s.prompts.find(instance);
 if(prompt==s.prompts.end()||!prompt->second.prepared||!prompt->second.token)return fail(e,"Dropped Ready requires actual prompt binding");
 DroppedItemActor next;next.data_=std::move(data);next.owner_=next.data_->owner(owner);next.epoch_=epoch;next.instance_=instance;next.generation_=next.data_->generation();next.world_=s.world;next.session_=s.session;next.random_=s.random;next.ledger_=s.uid_ledger;next.prompt_token_=prompt->second.token;
 const auto geometry=s.shapes.find(instance);if(geometry==s.shapes.end())return fail(e,"Dropped Ready collider handles missing");next.shapes_=geometry->second;
 const auto slot=s.world->contents.find(next.owner_->path);if(slot==s.world->contents.end())return fail(e,"Dropped Ready root missing");next.content_=&slot->second;next.valid_=true;
 if(!next.context(epoch,s,e))return false;
 for(auto role:{DroppedScalar::CollectTravelSeconds,DroppedScalar::CollectScaleSeconds,DroppedScalar::RevertSeconds,DroppedScalar::DisappearStage1,DroppedScalar::DisappearStage2,DroppedScalar::DisappearStage3,DroppedScalar::DisappearStage4,DroppedScalar::SparkleFPS})if(!std::isfinite(float(next.data_->value(role)))||float(next.data_->value(role))<=0)return fail(e,"Dropped unsupported float clock");
 bool flag=false;const auto&flags=next.owner_->object_flag()?s.session->object_flags:s.session->flags;if(!snapshot_flag(flags,next.owner_->flag,flag))return fail(e,"Dropped Ready flag identity");
 const size_t commands=size_t(!next.owner_->can_pickup())+size_t(flag);
 if(commands>4096-s.requests.size())return fail(e,"Dropped Ready request capacity");
 auto random=*s.random;const auto draw=int64_t(random.rand_range(next.data_->value(DroppedScalar::SparkleMinimum),next.data_->value(DroppedScalar::SparkleMaximum)));
 next.pose_.sparkle=uint32_t(std::clamp<int64_t>(draw,0,int64_t(next.data_->value(DroppedScalar::SparkleFrames))-1));next.sparkle_left_=float(1.0/next.data_->value(DroppedScalar::SparkleFPS));next.pose_.animation_speed=float(next.data_->value(DroppedScalar::BlinkInitialSpeed));
 next.ready_pending_=!flag;next.pause_connected_=!flag;
 if(!next.owner_->can_pickup()&&!next.request(s,DroppedRequestKind::SetPromptEnabled,{}, {},e))return false;
 if(flag&&!next.queue_free(s,e))return false;
 next.content_->visible=false;*s.random=random;*this=std::move(next);e.clear();return true;
}
bool DroppedItemActor::ready_signal(uint64_t epoch,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e)||!ready_pending_||pose_.queued)return fail(e,"Dropped Ready continuation unavailable");
 content(s)->visible=true;ready_pending_=false;e.clear();return true;
}
bool DroppedItemActor::content_reentered(uint64_t epoch,const BreakableRequest&command,const BreakablesData&breaks,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e)||!data_->compatible(breaks,e))return false;
 if(reentered_||collecting_||disappear_stage_||!reverts_.empty())return fail(e,"Dropped unreviewed repeated/active reentry");
 const auto*container=breaks.owner(command.owner);
 if(command.kind!=BreakableRequestKind::ContentReentered||command.epoch!=epoch||command.frame!=s.frame||!container||container->content>=breaks.contents().size()||command.identity!=container->content||breaks.contents()[container->content].path!=owner_->path||content(s)->parent!=container->drop_parent||std::any_of(command.value.begin(),command.value.end(),[](double v){return v!=0;}))return fail(e,"Dropped reentry source binding/clock");
 size_t found=SIZE_MAX,count=0;
 for(size_t i=0;i<s.world->requests.size();++i){const auto&r=s.world->requests[i];if(uint32_t(r.kind)<1||uint32_t(r.kind)>7||r.epoch!=epoch)return fail(e,"Dropped unknown Breakable effect");if(r.kind==command.kind&&r.owner==command.owner){if(r.frame!=command.frame||r.identity!=command.identity||r.value!=command.value)return fail(e,"Dropped reentry effect changed");found=i;++count;}}
 if(count!=1)return fail(e,"Dropped missing/duplicate reentry effect");
 if(owner_->reset_consumed()&&!set_flag(false,s,e))return false;
 // Godot Tween::_notification(EXIT_TREE) calls stop_all, which makes the next
 // start() seek(0) immediately. No Ready/RNG callback runs for this reentry.
 legacy_stopped_=true;reentered_=true;s.world->requests.erase(s.world->requests.begin()+found);e.clear();return true;
}
bool DroppedItemActor::interact(uint64_t epoch,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e))return false;
 bool flag=false;const auto&flags=owner_->object_flag()?s.session->object_flags:s.session->flags;snapshot_flag(flags,owner_->flag,flag);
 if(!owner_->can_pickup()||flag){e.clear();return true;}
 if(collecting_||pose_.queued||reverts_.size()>=4096||s.requests.size()>4096-4)return fail(e,"Dropped interaction lifecycle/capacity");
 const auto prompt=s.prompts.find(instance_);
 if(prompt==s.prompts.end()||!prompt->second.prepared||prompt->second.token!=prompt_token_)return fail(e,"Dropped prompt replaced before interaction");
 const auto observation=prompt->second;
 auto staged_session=*s.session;auto random=*s.random;auto ledger=*s.uid_ledger;ItemHolderGrantResult result;
 if(!grant_item_holder_item(*data_,owner_->template_id,staged_session,random,ledger,s.clock,result,e))return false;
 if(!context(epoch,s,e))return false;
 // The UID clock is an external provider. Reacquire the widget after it returns;
 // it may have invalidated a map iterator while preparing the sample.
 const auto current_prompt=s.prompts.find(instance_);
 if(current_prompt==s.prompts.end()||!current_prompt->second.prepared||current_prompt->second.token!=observation.token||current_prompt->second.visible!=observation.visible||current_prompt->second.can_show!=observation.can_show)return fail(e,"Dropped prompt changed during constructor");
 if(s.requests.size()>4096-4)return fail(e,"Dropped request capacity changed during constructor");
 DroppedItemActor next=*this;auto requests=s.requests;
 const auto append=[&](DroppedRequestKind kind,std::string target={},BreakablePoint position={}){requests.push_back({kind,epoch_,instance_,s.frame,prompt_token_,owner_->id,std::move(target),position,false});};
 if(result.kind==ItemHolderGrantKind::Acquired){
  append(DroppedRequestKind::ForcePrompt);
  if(observation.visible){append(DroppedRequestKind::PressPrompt);append(DroppedRequestKind::ReparentPrompt,content(s)->parent,{content(s)->position.x+float(data_->value(DroppedScalar::PromptOffsetX)),content(s)->position.y+float(data_->value(DroppedScalar::PromptOffsetY))});}
  else append(DroppedRequestKind::FreePrompt);
  auto&staged_flags=owner_->object_flag()?staged_session.object_flags:staged_session.flags;
  if(!put_flag(staged_flags,owner_->flag,true))return fail(e,"Dropped grant flag capacity");
  next.collecting_=true;next.collect_started_=false;next.collect_position_done_=false;next.collect_scale_done_=false;next.collect_time_=0;next.collect_target_=s.player;
  append(DroppedRequestKind::Dialogue,data_->info().success_dialogue);
 }else if(result.kind==ItemHolderGrantKind::Full){next.reverts_.push_back(0);append(DroppedRequestKind::Dialogue,data_->info().full_dialogue);}
 else return fail(e,"Dropped unknown grant result");
 if(next.legacy_stopped_){for(auto&time:next.reverts_)time=0;if(!next.reverts_.empty())next.pose_.rotation_degrees=node_degrees(float(data_->value(DroppedScalar::RevertStartDegrees)));next.legacy_stopped_=false;}
 if(result.kind==ItemHolderGrantKind::Acquired){
  if(result.target==ItemHolderGrantTarget::Keys)s.session->key_items.push_back(result.item);
  else if(result.target==ItemHolderGrantTarget::Party)s.session->characters[result.character_index].inventory.push_back(result.item);
  else return fail(e,"Dropped unknown acquired target");
  // Capacity and field validity were checked on the staged snapshot.
  auto&live_flags=owner_->object_flag()?s.session->object_flags:s.session->flags;put_flag(live_flags,owner_->flag,true);
  auto&mirror=owner_->object_flag()?s.world->object_flags:s.world->story_flags;mirror[owner_->flag]=true;
 }
 *s.random=random;s.uid_ledger->swap(ledger);s.global_item=std::move(result);s.has_global_item=true;s.requests.swap(requests);*this=std::move(next);e.clear();return true;
}
bool DroppedItemActor::disappear(uint64_t epoch,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e))return false;if(disappear_stage_)return fail(e,"Dropped overlapping disappear coroutine unsupported");
 disappear_stage_=1;pose_.timer_running=true;pose_.timer_left=float(data_->value(DroppedScalar::DisappearStage1));if(s.player_paused)pose_.timer_paused=true;e.clear();return true;
}
bool DroppedItemActor::player_pause(uint64_t epoch,bool value,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e))return false;s.player_paused=value;if(pause_connected_)pose_.timer_paused=value;e.clear();return true;
}
bool DroppedItemActor::sample_blink(float delta,std::string&e){
 const auto*clip=data_->clip(DroppedClipRole::Blink);if(!clip)return fail(e,"Dropped blink clip binding lost");
 float time=blink_time_+delta;if(clip->loop&&(time>clip->duration||time<0)){time=std::fmod(time,clip->duration);if(time<0)time+=clip->duration;if(!time)time=clip->duration;}
 else if(!clip->loop)time=std::clamp(time,0.0f,clip->duration);
 const auto&keys=clip->keys;
 const auto find=[&](float value){int low=0,high=int(keys.size())-1,middle=0;while(low<=high){middle=(low+high)/2;const float k=keys[size_t(middle)].time,tolerance=std::max(1e-5f,1e-5f*std::abs(value));if(value==k||std::abs(value-k)<tolerance)return middle;if(value<k)high=middle-1;else low=middle+1;}return keys[size_t(middle)].time>value?middle-1:middle;};
 const auto range=[&](float from,float to){if(from!=clip->duration&&to==clip->duration)to=float(double(clip->duration)*1.001);int end=find(to);if(end>=0&&from==to&&keys[size_t(end)].time==from){pose_.sprite_visible=keys[size_t(end)].value;return;}if(end>=0&&keys[size_t(end)].time>=to)--end;if(end<0)return;int start=find(from);if(start<0||keys[size_t(start)].time<from)++start;for(int n=start;n<=end;++n)pose_.sprite_visible=keys[size_t(n)].value;};
 float from=time-delta,to=time;if(from>to)std::swap(from,to);
 if(clip->loop){const auto mod=[&](float value){float v=std::fmod(value,clip->duration);return v<0?float(v+clip->duration):v;};from=mod(from);to=mod(to);if(from>to){range(from,clip->duration);range(0,to);}else range(from,to);}
 else range(std::clamp(from,0.0f,clip->duration),std::clamp(to,0.0f,clip->duration));
 blink_time_=time;pose_.animation_time=time;return true;
}
bool DroppedItemActor::tick(float delta,DroppedServices&,BreakablePoint&position,bool&free,std::string&e){
 const auto value=[&](DroppedScalar role){return float(data_->value(role));};
 float remaining=delta;while(remaining){if(sparkle_left_<=0){sparkle_left_=float(1.0/data_->value(DroppedScalar::SparkleFPS));pose_.sparkle=(pose_.sparkle+1)%uint32_t(data_->value(DroppedScalar::SparkleFrames));}const float step=std::min(sparkle_left_,remaining);remaining-=step;sparkle_left_-=step;}
 for(auto&time:reverts_){time=std::min(float(time+delta),value(DroppedScalar::RevertSeconds));const float a=value(DroppedScalar::RevertStartDegrees),b=value(DroppedScalar::RevertEndDegrees);pose_.rotation_degrees=node_degrees(equation(data_->value(DroppedScalar::RevertTransition),data_->value(DroppedScalar::RevertEase),time,a,b-a,value(DroppedScalar::RevertSeconds)));}
 reverts_.erase(std::remove_if(reverts_.begin(),reverts_.end(),[&](float t){return t>=value(DroppedScalar::RevertSeconds);}),reverts_.end());
 if(pose_.timer_running&&!pose_.timer_paused){
  pose_.timer_left-=double(delta);
  if(pose_.timer_left<0){
   const auto stage=disappear_stage_;
   pose_.timer_left+=value(DroppedScalar(uint32_t(DroppedScalar::DisappearStage1)+stage-1));
   if(stage==4)free=true;
   else{++disappear_stage_;pose_.timer_left=value(DroppedScalar(uint32_t(DroppedScalar::DisappearStage1)+disappear_stage_-1));
    if(stage==1){const auto*c=data_->clip(DroppedClipRole::Blink);pose_.animation=c->id;pose_.animation_time=0;blink_time_=0;}
    else if(stage==2)pose_.animation_speed=value(DroppedScalar::BlinkSecondSpeed);
    else if(stage==3)pose_.animation_speed=value(DroppedScalar::BlinkThirdSpeed);
   }
  }
 }
 if(pose_.animation&&!sample_blink(float(delta*pose_.animation_speed),e))return false;
 if(collecting_){
  if(!collect_started_){collect_started_=true;collect_start_=position;}
  collect_time_=float(collect_time_+delta);
  const float scale_time=value(DroppedScalar::CollectScaleSeconds);
  if(!collect_scale_done_){
   const float ax=value(DroppedScalar::ScaleStartX),ay=value(DroppedScalar::ScaleStartY),bx=value(DroppedScalar::ScaleEndX),by=value(DroppedScalar::ScaleEndY);
   if(collect_time_>=scale_time){pose_.scale={node_scale(bx),node_scale(by)};collect_scale_done_=true;}
   else pose_.scale={node_scale(equation(data_->value(DroppedScalar::ScaleTransition),data_->value(DroppedScalar::ScaleEase),collect_time_,ax,bx-ax,scale_time)),node_scale(equation(data_->value(DroppedScalar::ScaleTransition),data_->value(DroppedScalar::ScaleEase),collect_time_,ay,by-ay,scale_time))};
  }
  if(!collect_position_done_&&collect_time_>=value(DroppedScalar::CollectDelaySeconds)){
   const float elapsed=float(collect_time_-value(DroppedScalar::CollectDelaySeconds)),duration=value(DroppedScalar::CollectTravelSeconds);
   if(elapsed>=duration){position=collect_target_;collect_position_done_=true;}
   else position={equation(data_->value(DroppedScalar::CollectTransition),data_->value(DroppedScalar::CollectEase),elapsed,collect_start_.x,float(collect_target_.x-collect_start_.x),duration),equation(data_->value(DroppedScalar::CollectTransition),data_->value(DroppedScalar::CollectEase),elapsed,collect_start_.y,float(collect_target_.y-collect_start_.y),duration)};
  }
  if(collect_position_done_&&collect_scale_done_)free=true;
 }
 if(!finite(position)||!finite(pose_.scale)||!std::isfinite(pose_.rotation_degrees)||!std::isfinite(pose_.timer_left))return fail(e,"Dropped unsupported numeric clock/property result");return true;
}
bool DroppedItemActor::advance_idle(uint64_t epoch,double delta,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e))return false;
 if(!std::isfinite(delta)||delta<=0||delta>1||(idle_seen_&&s.idle_step<=last_idle_))return fail(e,"Dropped idle clock/order");
 DroppedItemActor next=*this;auto position=content(s)->position;bool free=false;
 if(!next.tick(float(delta),s,position,free,e))return false;
 if(free&&!next.queue_free(s,e))return false;
 next.idle_seen_=true;next.last_idle_=s.idle_step;content(s)->position=position;*this=std::move(next);e.clear();return true;
}
bool DroppedItemActor::flush_deferred(uint64_t epoch,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e,pose_.removed))return false;if(pose_.removed){e.clear();return true;}
 if(!pose_.queued){e.clear();return true;}
 for(const auto&r:s.requests)if(r.instance==instance_)return fail(e,"Dropped pending source effects must be consumed before free");
 if(owner_->reset_consumed()&&!set_flag(false,s,e))return false;
 content(s)->present=false;s.owners.erase(instance_);pose_.removed=true;ready_pending_=false;pause_connected_=false;e.clear();return true;
}
bool DroppedItemActor::leave(uint64_t epoch,DroppedServices&s,std::string&e){
 if(!context(epoch,s,e,pose_.removed))return false;if(pose_.removed){e.clear();return true;}
 for(const auto&r:s.requests)if(r.instance==instance_)return fail(e,"Dropped source effects pending during leave");
 if(owner_->reset_consumed()&&!set_flag(false,s,e))return false;
 content(s)->present=false;s.owners.erase(instance_);pose_.removed=true;ready_pending_=false;pause_connected_=false;e.clear();return true;
}
}
