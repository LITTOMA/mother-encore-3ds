#include "encore/dungeon_mechanism_state.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {namespace {bool bad(std::string&e,const char*s){e=s;return false;}}
size_t DungeonMechanismState::find(uint32_t id)const{const auto&rows=data_.bindings();for(size_t i=0;i<rows.size();++i)if(rows[i].owner==id)return i;return rows.size();}
bool DungeonMechanismState::emit(DungeonMechanismEvent event,std::string&e){if(events_.size()>=4096)return bad(e,"Mechanism event capacity exceeded");event.epoch=epoch_;events_.push_back(event);return true;}
bool DungeonMechanismState::apply_requests(std::string&e){
 const auto requests=graph_.events();graph_.clear_events();
 for(const auto&request:requests){const auto index=find(request.target.id);if(index>=players_.size()||request.target.generation!=graph_.generation())return bad(e,"Mechanism request owner rejected");
  DungeonMechanismEvent event;event.kind=DungeonMechanismEventKind::Request;event.owner=request.target.id;event.request=request;
  const bool timer=request.kind==DungeonSwitchEventKind::ReadyTimer||request.kind==DungeonSwitchEventKind::WallTimer;
  const bool camera=request.kind==DungeonSwitchEventKind::CameraMove||request.kind==DungeonSwitchEventKind::CameraReturn;
  if(timer||camera){
   if(callbacks_.size()>=512||next_ticket_==std::numeric_limits<uint64_t>::max())return bad(e,"Mechanism callback capacity exceeded");
   event.callback_ticket=++next_ticket_;callbacks_.push_back({event.callback_ticket,request.token,camera});
  }
  if(!emit(event,e))return false;
  switch(request.kind){
   case DungeonSwitchEventKind::SwitchAnimation:case DungeonSwitchEventKind::ObjectAnimation:{
    const auto role=uint32_t(request.animation);const auto&binding=data_.bindings()[index];const auto*profile=data_.profile(binding.profile);
    if(role>=binding.roles.size()||!binding.roles[role]||!profile)return bad(e,"Mechanism requested role/profile rejected");
    if((request.kind==DungeonSwitchEventKind::SwitchAnimation)!=(binding.kind==DungeonMechanismOwner::Switch))return bad(e,"Mechanism animation owner kind rejected");
    if(profile->family==DungeonMechanismFamily::Wall){if(!players_[index].wall.play(binding.roles[role],e))return false;}
    else if(!players_[index].properties.play(binding.roles[role],e))return false;
    break;
   }
   case DungeonSwitchEventKind::Pause:paused_=true;break;
   case DungeonSwitchEventKind::Unpause:paused_=false;break;
   case DungeonSwitchEventKind::Cutscene:cutscene_=request.value;break;
   case DungeonSwitchEventKind::ReadyTimer:case DungeonSwitchEventKind::WallTimer:
   case DungeonSwitchEventKind::CameraMove:case DungeonSwitchEventKind::CameraReturn:
   case DungeonSwitchEventKind::SwitchAudio:case DungeonSwitchEventKind::SwitchHit:
   case DungeonSwitchEventKind::DeniedAudio:case DungeonSwitchEventKind::FlagChanged:
   case DungeonSwitchEventKind::FlagsUpdated:case DungeonSwitchEventKind::MusicFade:
   case DungeonSwitchEventKind::MusicResume:case DungeonSwitchEventKind::ObjectStateChanged:break;
   default:return bad(e,"Mechanism unknown request kind");
  }
 }
 e.clear();return true;
}
bool DungeonMechanismState::prepare(const DungeonMechanismBindingsData&data,uint64_t epoch,const std::vector<DungeonSwitchFlag>&flags,std::string&e){
 if(valid()||!data.valid()||!epoch)return bad(e,"Mechanism prepare requires fresh owner, checked bindings and scene epoch");
 DungeonMechanismState v;v.data_=data;v.epoch_=epoch;v.players_.resize(data.bindings().size());
 for(size_t i=0;i<v.players_.size();++i){const auto*p=data.profile(data.bindings()[i].profile);if(!p)return bad(e,"Mechanism profile unavailable");
  if(p->family==DungeonMechanismFamily::Wall){if(!v.players_[i].wall.initialize(p->wall,epoch,e))return false;}
  else if(!v.players_[i].properties.initialize(p->properties,e))return false;
 }
 const auto g=data.graph();if(!v.graph_.prepare(g,g.scene_id(),g.text(g.policy().region),flags,e)||!v.apply_requests(e))return false;
 *this=std::move(v);e.clear();return true;
}
bool DungeonMechanismState::operate(uint32_t id,DungeonSwitchMethod method,bool contact,bool external_paused,bool can_switch,std::string&e){
 if(!valid())return bad(e,"Mechanism input has no owner");
 auto next=*this;
 if(!next.graph_.operate(next.graph_.handle(id),method,contact,external_paused||paused_,can_switch,e)||!next.apply_requests(e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::finish(size_t index,uint32_t clip,std::string&e){
 const auto&binding=data_.bindings()[index];DungeonMechanismEvent event;event.kind=DungeonMechanismEventKind::AnimationFinished;event.owner=binding.owner;event.clip=clip;if(!emit(event,e))return false;
 if(binding.kind==DungeonMechanismOwner::Controlled&&(!graph_.object_animation_finished(graph_.handle(binding.owner),e)||!apply_requests(e)))return false;
 return true;
}
bool DungeonMechanismState::wall_events(size_t index,std::string&e){
 const auto&binding=data_.bindings()[index];for(const auto&request:players_[index].wall.take_events()){
  if(request.owner!=binding.owner||request.generation!=epoch_)return bad(e,"Mechanism wall command owner rejected");
  if(request.kind==DungeonWallEventKind::Finished){if(!finish(index,request.clip,e))return false;continue;}
  DungeonMechanismEvent event;event.owner=binding.owner;event.clip=request.clip;
  if(request.kind==DungeonWallEventKind::AudioPlaying){event.kind=DungeonMechanismEventKind::WallAudio;event.playing=request.playing;}
  else if(request.kind==DungeonWallEventKind::CameraShake){event.kind=DungeonMechanismEventKind::WallCamera;event.duration=request.duration;}
  else return bad(e,"Mechanism unknown wall command");
  if(!emit(event,e))return false;
 }
 return true;
}
bool DungeonMechanismState::advance_owned(double delta,std::string&e){
 // Completion can change another owner's assigned animation. Apply it before
 // advancing the next source-ordered owner, matching native callback order.
 for(size_t i=0;i<players_.size();++i){const auto*p=data_.profile(data_.bindings()[i].profile);if(!p)return bad(e,"Mechanism profile identity lost");
  if(p->family==DungeonMechanismFamily::Wall){if(!players_[i].wall.process(delta,e)||!wall_events(i,e))return false;}
  else if(players_[i].properties.playing()){
   if(!players_[i].properties.advance(delta,e))return false;
   for(auto clip:players_[i].properties.take_completed())if(!finish(i,clip,e))return false;
  }
 }
 return true;
}
bool DungeonMechanismState::advance(double delta,std::string&e){
 if(!valid()||!std::isfinite(delta)||delta<0||delta>60)return bad(e,"Mechanism animation frame rejected");
 auto next=*this;
 if(!next.advance_owned(delta,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::flush_deferred(std::string&e){
 if(!valid())return bad(e,"Mechanism deferred queue has no owner");
 auto next=*this;
 for(size_t i=0;i<next.players_.size();++i)if(next.players_[i].wall.valid()&&(!next.players_[i].wall.flush_deferred(e)||!next.wall_events(i,e)))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::idle_frame(std::string&e){
 if(!valid())return bad(e,"Mechanism idle boundary has no owner");
 auto next=*this;
 if(!next.graph_.idle_frame(e)||!next.apply_requests(e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::timer_finished(DungeonMechanismCallback callback,std::string&e){
 return complete_callback(callback,false,e);
}
bool DungeonMechanismState::camera_finished(DungeonMechanismCallback callback,std::string&e){
 return complete_callback(callback,true,e);
}
bool DungeonMechanismState::complete_callback(DungeonMechanismCallback callback,bool camera,std::string&e){
 if(!valid()||callback.epoch!=epoch_||!callback.ticket)return bad(e,"Mechanism callback scene epoch/ticket rejected");
 const auto at=std::find_if(callbacks_.begin(),callbacks_.end(),[&](const Callback&v){return v.ticket==callback.ticket&&v.camera==camera;});
 if(at==callbacks_.end())return bad(e,"Mechanism callback identity/kind rejected");
 const auto index=size_t(at-callbacks_.begin());const auto token=at->token;
 auto next=*this;
 next.callbacks_.erase(next.callbacks_.begin()+index);
 if(!(camera?next.graph_.camera_finished(token,e):next.graph_.timer_finished(token,e))||!next.apply_requests(e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::leave_area(bool changed,std::string&e){
 if(!valid())return bad(e,"Mechanism area boundary has no owner");
 auto next=*this;
 if(!next.graph_.leave_area(changed,e)||!next.apply_requests(e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismState::inspect(uint32_t id,DungeonMechanismPose&result,std::string&e)const{
 if(!valid())return bad(e,"Mechanism observation has no owner");
 const auto i=find(id);if(i>=players_.size())return bad(e,"Mechanism observation identity rejected");
 const auto&binding=data_.bindings()[i];const auto*p=data_.profile(binding.profile);if(!p)return bad(e,"Mechanism observation profile rejected");DungeonMechanismPose value;value.owner=id;
 if(p->family==DungeonMechanismFamily::Wall){const auto&player=players_[i].wall;value.clip=player.assigned_clip();value.pose=player.pose();value.position=player.position();value.playing=player.playing();}
 else {const auto&player=players_[i].properties;value.clip=player.assigned_clip();value.pose=player.pose();value.position=player.position();value.playing=player.playing();}
 if(binding.kind==DungeonMechanismOwner::Switch){DungeonSwitchState state;if(!graph_.switch_state(graph_.handle(id),state,e))return false;value.on=state.on;}
 else {DungeonControlledState state;if(!graph_.object_state(graph_.handle(id),state,e))return false;value.on=state.on;}
 result=value;e.clear();return true;
}
}
