#include "encore/dungeon_key_state.hpp"
#include <limits>
#include <set>
namespace encore::upstream {namespace {
bool reject(std::string&e,const char*why){e=why;return false;}
}
uint32_t DungeonKeyRuntime::find(uint32_t id)const{for(uint32_t i=0;i<states_.size();++i)if(data_.node(i).id==id)return i;return kFieldNone;}
DungeonKeyHandle DungeonKeyRuntime::handle(uint32_t id)const{return ready()&&find(id)!=kFieldNone?DungeonKeyHandle{id,generation_}:DungeonKeyHandle{};}
bool DungeonKeyRuntime::validate(DungeonKeyHandle h,uint32_t&index,std::string&e)const{
 if(!ready()||!h.id||h.generation!=generation_)return reject(e,"Dungeon key callback has no current owner");
 index=find(h.id);if(index==kFieldNone)return reject(e,"Dungeon key callback identity rejected");return true;
}
bool DungeonKeyRuntime::live(DungeonKeyHandle h,DungeonKeyKind kind,uint32_t&index,std::string&e)const{
 return validate(h,index,e)&&(states_[index].alive&&data_.node(index).kind==kind?true:reject(e,"Dungeon key callback node lifetime/kind rejected"));
}
bool DungeonKeyRuntime::state(DungeonKeyHandle h,DungeonKeyState&out,std::string&e)const{uint32_t i;if(!validate(h,i,e))return false;out=states_[i];e.clear();return true;}
bool DungeonKeyRuntime::flag(DungeonKeyHandle h,bool&out,std::string&e)const{uint32_t i;if(!validate(h,i,e))return false;out=flags_[i];e.clear();return true;}
bool DungeonKeyRuntime::emit(DungeonKeyEventKind kind,uint32_t i,std::string&e){
 if(events_.size()>=1024)return reject(e,"Dungeon key unconsumed event capacity exceeded");
 events_.push_back({kind,{data_.node(i).id,generation_},counter_,flags_[i],states_[i]});return true;
}
bool DungeonKeyRuntime::write_flag(uint32_t i,std::string&e){
 flags_[i]=true;if(!emit(DungeonKeyEventKind::FlagChanged,i,e))return false;
 return !(data_.node(i).flags&1)||emit(DungeonKeyEventKind::FlagsUpdated,i,e);
}
bool DungeonKeyRuntime::queue_delete(uint32_t i,std::string&e){
 if(states_[i].queued)return true;states_[i].queued=true;return emit(DungeonKeyEventKind::DeleteQueued,i,e);
}
bool DungeonKeyRuntime::open(uint32_t i,std::string&e){
 states_[i].collision_disabled=true;if(!emit(DungeonKeyEventKind::CollisionDisabled,i,e))return false;
 states_[i].prompt_enabled=false;return emit(DungeonKeyEventKind::PromptDisabled,i,e);
}
bool DungeonKeyRuntime::prepare(DungeonKeysView view,uint32_t scene,std::string_view region,int32_t count,const std::vector<DungeonKeyFlagValue>&values,std::string&e){
 if(!view.valid()||view.scene_id()!=scene||view.text(view.policy().region)!=region||count<view.policy().minimum||count>view.policy().maximum||values.size()!=view.node_count()||generation_==std::numeric_limits<uint64_t>::max())return reject(e,"Dungeon key scene/counter/flag admission rejected");
 DungeonKeyRuntime next;next.data_=view;next.counter_=count;next.generation_=generation_+1;
 next.states_.resize(view.node_count());next.flags_.resize(view.node_count());next.disable_pending_.resize(view.node_count());std::set<uint32_t>seen;
 for(const auto&value:values){const auto i=next.find(value.id);if(i==kFieldNone||!seen.insert(value.id).second)return reject(e,"Dungeon key saved identity coverage rejected");next.flags_[i]=value.value;}
 for(uint32_t i=0;i<view.node_count();++i){
  auto&state=next.states_[i];state.alive=true;const auto kind=view.node(i).kind;state.prompt_enabled=kind==DungeonKeyKind::Lock;
  if(!next.flags_[i])continue;
  if(kind==DungeonKeyKind::Pickup){if(!next.queue_delete(i,e))return false;}
  else {state.opened=true;state.requested_animation=DungeonLockAnimation::Opened;if(!next.emit(DungeonKeyEventKind::LockAnimationRequested,i,e)||!next.open(i,e))return false;}
 }
 *this=std::move(next);e.clear();return true;
}
bool DungeonKeyRuntime::pickup_body_entered(DungeonKeyHandle h,bool player,bool paused,bool playing,std::string&e){
 uint32_t i;if(!live(h,DungeonKeyKind::Pickup,i,e))return false;
 if(!player||paused||playing){e.clear();return true;}
 DungeonKeyRuntime next=*this;const auto amount=int64_t(counter_)+data_.node(i).delta;
 if(amount<data_.policy().minimum||amount>data_.policy().maximum)return reject(e,"Dungeon key pickup exceeds admitted counter scope");
 next.counter_=int32_t(amount);
 if(!next.emit(DungeonKeyEventKind::CounterChanged,i,e)||!next.write_flag(i,e)||!next.emit(DungeonKeyEventKind::IndicatorRequested,i,e)||!next.emit(DungeonKeyEventKind::PickupAudioRequested,i,e))return false;
 next.states_[i].visible=false;if(!next.emit(DungeonKeyEventKind::Hidden,i,e))return false;
 next.disable_pending_[i]=true;if(!next.emit(DungeonKeyEventKind::CollisionDisableDeferred,i,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonKeyRuntime::pickup_finished(DungeonKeyHandle h,std::string&e){
 uint32_t i;if(!live(h,DungeonKeyKind::Pickup,i,e))return false;DungeonKeyRuntime next=*this;
 if(!next.queue_delete(i,e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyRuntime::lock_interact(DungeonKeyHandle h,std::string&e){
 uint32_t i;if(!live(h,DungeonKeyKind::Lock,i,e))return false;
 if(states_[i].opened){e.clear();return true;}
 DungeonKeyRuntime next=*this;const auto amount=int64_t(counter_)+data_.node(i).delta;
 if(amount<data_.policy().minimum){if(!next.emit(DungeonKeyEventKind::LockedDialogue,i,e))return false;}
 else {
  if(amount>data_.policy().maximum)return reject(e,"Dungeon key lock exceeds admitted counter scope");
  next.counter_=int32_t(amount);if(!next.emit(DungeonKeyEventKind::CounterChanged,i,e))return false;
  next.states_[i].requested_animation=DungeonLockAnimation::Opening;if(!next.emit(DungeonKeyEventKind::LockAnimationRequested,i,e))return false;
  next.states_[i].opened=true;if(!next.write_flag(i,e)||!next.emit(DungeonKeyEventKind::IndicatorRequested,i,e))return false;
 }
 *this=std::move(next);e.clear();return true;
}
bool DungeonKeyRuntime::lock_open_method(DungeonKeyHandle h,std::string&e){
 uint32_t i;if(!live(h,DungeonKeyKind::Lock,i,e))return false;DungeonKeyRuntime next=*this;
 if(!next.open(i,e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyRuntime::end_scene_frame(std::string&e){
 if(!ready())return reject(e,"Dungeon key frame has no source owner");DungeonKeyRuntime next=*this;
 // Native MessageQueue precedes SceneTree's deferred deletion queue.
 for(uint32_t i=0;i<states_.size();++i)if(next.states_[i].alive&&next.disable_pending_[i]){next.disable_pending_[i]=false;next.states_[i].collision_disabled=true;if(!next.emit(DungeonKeyEventKind::CollisionDisabled,i,e))return false;}
 for(uint32_t i=0;i<states_.size();++i)if(next.states_[i].alive&&next.states_[i].queued){next.states_[i].alive=false;if(!next.emit(DungeonKeyEventKind::Deleted,i,e))return false;}
 *this=std::move(next);e.clear();return true;
}
}
