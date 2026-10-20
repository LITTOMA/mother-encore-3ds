#include "encore/dungeon_switch_state.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace encore::upstream {namespace {bool reject(std::string&e,const char*s){e=s;return false;}}
uint32_t DungeonSwitchRuntime::find_switch(uint32_t id)const{for(uint32_t i=0;i<switches_.size();++i)if(data_.switch_node(i).id==id)return i;return kFieldNone;}
uint32_t DungeonSwitchRuntime::find_object(uint32_t id)const{for(uint32_t i=0;i<animations_.size();++i)if(data_.object(i).id==id)return i;return kFieldNone;}
DungeonSwitchHandle DungeonSwitchRuntime::handle(uint32_t id)const{return ready()&&(find_switch(id)!=kFieldNone||find_object(id)!=kFieldNone)?DungeonSwitchHandle{id,generation_}:DungeonSwitchHandle{};}
bool DungeonSwitchRuntime::validate(DungeonSwitchHandle h,bool object,uint32_t&i,std::string&e)const{if(!ready()||h.generation!=generation_||!h.id)return reject(e,"Dungeon switch callback has no current owner");i=object?find_object(h.id):find_switch(h.id);return i!=kFieldNone||reject(e,"Dungeon switch callback identity/kind rejected");}
bool DungeonSwitchRuntime::object_on(uint32_t i)const{auto id=data_.object(i).id;bool on=data_.object(i).bits&1;for(uint32_t j=0;j<data_.edge_count();++j){auto edge=data_.edge(j);if(edge.target==id)on=on!=switches_[find_switch(edge.source)].on;}return on;}
bool DungeonSwitchRuntime::switch_state(DungeonSwitchHandle h,DungeonSwitchState&v,std::string&e)const{uint32_t i;if(!validate(h,false,i,e))return false;v=switches_[i];e.clear();return true;}
bool DungeonSwitchRuntime::object_state(DungeonSwitchHandle h,DungeonControlledState&v,std::string&e)const{uint32_t i;if(!validate(h,true,i,e))return false;auto row=data_.object(i);v={object_on(i),animations_[i],row.order,0};for(uint32_t j=0;j<data_.edge_count();++j)v.registrations+=data_.edge(j).target==h.id;e.clear();return true;}
bool DungeonSwitchRuntime::emit(DungeonSwitchEventKind kind,uint32_t id,DungeonSwitchToken token,bool value,bool silent,float x,float y,float duration,DungeonSwitchAnimation animation,std::string&e){
 if(events_.size()>=2048)return reject(e,"Dungeon switch event capacity exceeded");auto i=find_switch(id);events_.push_back({kind,{id,generation_},token,value,silent,x,y,duration,animation,i==kFieldNone?DungeonSwitchState{}:switches_[i]});return true;
}
bool DungeonSwitchRuntime::emit(DungeonSwitchEventKind kind,uint32_t id,std::string&e){return emit(kind,id,{},false,false,0,0,0,DungeonSwitchAnimation::None,e);}
bool DungeonSwitchRuntime::timer(uint32_t i,bool wall,std::string&e){
 if(timers_.size()>=256||next_token_==std::numeric_limits<uint64_t>::max())return reject(e,"Dungeon switch timer capacity exceeded");auto token=++next_token_;timers_.push_back({token,i,wall});auto policy=data_.policy();return emit(wall?DungeonSwitchEventKind::WallTimer:DungeonSwitchEventKind::ReadyTimer,wall?data_.object(i).id:data_.switch_node(i).id,{token,generation_},false,false,0,0,wall?policy.post_delay:policy.ready_delay,DungeonSwitchAnimation::None,e);
}
bool DungeonSwitchRuntime::flag(uint32_t i,bool value,std::string&e){
 switches_[i].on=value;auto id=data_.switch_node(i).id;
 if(!emit(DungeonSwitchEventKind::FlagChanged,id,{},value,false,0,0,0,DungeonSwitchAnimation::None,e))return false;
 return !(data_.switch_node(i).bits&2)||emit(DungeonSwitchEventKind::FlagsUpdated,id,{},value,false,0,0,0,DungeonSwitchAnimation::None,e);
}
bool DungeonSwitchRuntime::request_object(uint32_t i,uint64_t operation,bool silent,std::string&e){
 if(waiters_.size()>=512)return reject(e,"Dungeon switch animation waiter capacity exceeded");bool on=object_on(i);auto animation=silent?(on?DungeonSwitchAnimation::StayOn:DungeonSwitchAnimation::StayOff):(on?DungeonSwitchAnimation::On:DungeonSwitchAnimation::Off);
 // This admitted choreography has no Off animation for wall objects.
 if(!silent&&!on&&data_.object(i).kind==DungeonControlledKind::Wall)return reject(e,"Dungeon wall off transition has no admitted animation");
 animations_[i]=animation;waiters_.push_back({i,operation,on,silent});return emit(DungeonSwitchEventKind::ObjectAnimation,data_.object(i).id,{},on,silent,0,0,0,animation,e);
}
bool DungeonSwitchRuntime::prepare(DungeonSwitchesView view,uint32_t scene,std::string_view region,const std::vector<DungeonSwitchFlag>&flags,std::string&e){
 if(!view.valid()||view.scene_id()!=scene||view.text(view.policy().region)!=region||flags.size()!=view.switch_count()||generation_==std::numeric_limits<uint64_t>::max())return reject(e,"Dungeon switch scene/flag admission rejected");
 // Multiple signed update tiers require additional camera/action barriers.
 // Their schema is checked, but the current source-method owner rejects them.
 for(uint32_t i=0;i<view.object_count();++i)if(view.object(i).order)return reject(e,"Dungeon switch multiple update tiers not admitted");
 DungeonSwitchRuntime next;next.data_=view;next.generation_=generation_+1;next.switches_.resize(view.switch_count());next.animations_.resize(view.object_count());std::set<uint32_t>seen;
 for(auto f:flags){auto i=next.find_switch(f.id);if(i==kFieldNone||!seen.insert(f.id).second)return reject(e,"Dungeon switch saved identity coverage rejected");next.switches_[i].on=f.value;}
 for(uint32_t i=0;i<view.switch_count();++i){auto&v=next.switches_[i];v.disabled=v.on&&(view.switch_node(i).bits&16);v.animation=v.on?DungeonSwitchAnimation::TurnOn:DungeonSwitchAnimation::TurnOff;
  if(!next.timer(i,false,e)||!next.emit(DungeonSwitchEventKind::SwitchAnimation,view.switch_node(i).id,{},v.on,false,0,0,0,v.animation,e))return false;
 }
 for(uint32_t i=0;i<view.object_count();++i)if(!next.request_object(i,0,true,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::finish_operation(size_t i,std::string&e){auto id=data_.switch_node(operations_[i].source).id;if(data_.switch_node(operations_[i].source).bits&32){if(!emit(DungeonSwitchEventKind::Unpause,id,e)||!emit(DungeonSwitchEventKind::Cutscene,id,{},false,false,0,0,0,DungeonSwitchAnimation::None,e))return false;}operations_.erase(operations_.begin()+i);return true;}
bool DungeonSwitchRuntime::settle(std::string&e){for(size_t i=0;i<operations_.size();){auto&op=operations_[i];if(op.phase==Phase::Waiting&&op.waiting.empty()){if(data_.switch_node(op.source).bits&64){op.phase=Phase::Idle;++i;}else if(!finish_operation(i,e))return false;}else ++i;}return true;}
bool DungeonSwitchRuntime::begin_control(uint32_t i,std::string&e){
 if(operations_.size()>=128||next_token_==std::numeric_limits<uint64_t>::max())return reject(e,"Dungeon switch operation capacity exceeded");auto row=data_.switch_node(i);Operation op;op.token=++next_token_;op.source=i;
 if(row.bits&32){if(!emit(DungeonSwitchEventKind::Pause,row.id,e)||!emit(DungeonSwitchEventKind::Cutscene,row.id,{},true,false,0,0,0,DungeonSwitchAnimation::None,e))return false;}
 float x=0,y=0;uint32_t count=0;
 for(uint32_t j=0;j<row.edge_count;++j){auto target=data_.edge(row.first_edge+j).target;auto object=find_object(target);if(!request_object(object,op.token,false,e))return false;op.waiting.push_back(target);auto item=data_.object(object);if((row.bits&64)&&(item.bits&2)){x+=item.x+item.offset_x;y+=item.y+item.offset_y;++count;}}
 if(count){op.phase=Phase::FocusCamera;if(!emit(DungeonSwitchEventKind::CameraMove,row.id,{op.token,generation_},false,false,x/count,y/count,row.camera_delay,DungeonSwitchAnimation::None,e))return false;}
 operations_.push_back(std::move(op));return settle(e);
}
bool DungeonSwitchRuntime::operate(DungeonSwitchHandle h,DungeonSwitchMethod method,bool contact,bool paused,bool can_switch,std::string&e){
 uint32_t i;if(!validate(h,false,i,e))return false;
 if(method!=DungeonSwitchMethod::Interact&&method!=DungeonSwitchMethod::Hit&&method!=DungeonSwitchMethod::PlayerEntered)return reject(e,"Dungeon switch method rejected");
 if((method==DungeonSwitchMethod::PlayerEntered&&!contact)||switches_[i].disabled||paused||!switches_[i].ready){e.clear();return true;}
 DungeonSwitchRuntime next=*this;
 if(!can_switch){if(!next.emit(DungeonSwitchEventKind::DeniedAudio,h.id,e))return false;}
 else {
  if(!next.emit(DungeonSwitchEventKind::SwitchAudio,h.id,e)||!next.emit(DungeonSwitchEventKind::SwitchHit,h.id,e))return false;
  if(h.id==data_.policy().main_switch){if(!next.emit(DungeonSwitchEventKind::MusicFade,h.id,{},false,false,0,0,data_.policy().music_fade,DungeonSwitchAnimation::None,e)||!next.emit(DungeonSwitchEventKind::Pause,h.id,e)||!next.emit(DungeonSwitchEventKind::Cutscene,h.id,{},true,false,0,0,0,DungeonSwitchAnimation::None,e))return false;}
  if(!next.flag(i,!switches_[i].on,e))return false;auto&v=next.switches_[i];v.animation=v.on?DungeonSwitchAnimation::TurnOn:DungeonSwitchAnimation::TurnOff;
  if(!next.emit(DungeonSwitchEventKind::SwitchAnimation,h.id,{},v.on,false,0,0,0,v.animation,e))return false;if(v.on&&(data_.switch_node(i).bits&16))v.disabled=true;
  if(!next.begin_control(i,e))return false;
 }
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::timer_finished(DungeonSwitchToken token,std::string&e){
 if(!ready()||token.generation!=generation_||!token.value)return reject(e,"Dungeon switch timer has no current owner");auto at=std::find_if(timers_.begin(),timers_.end(),[&](const Timer&t){return t.token==token.value;});if(at==timers_.end())return reject(e,"Dungeon switch timer identity rejected");auto index=size_t(at-timers_.begin());auto item=*at;DungeonSwitchRuntime next=*this;next.timers_.erase(next.timers_.begin()+index);
 if(!item.wall)next.switches_[item.index].ready=true;
 else {auto id=data_.object(item.index).id;if(!next.emit(DungeonSwitchEventKind::Cutscene,id,{},false,false,0,0,0,DungeonSwitchAnimation::None,e)||!next.emit(DungeonSwitchEventKind::Unpause,id,e)||!next.emit(DungeonSwitchEventKind::MusicResume,id,e))return false;}
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::object_animation_finished(DungeonSwitchHandle h,std::string&e){
 uint32_t i;if(!validate(h,true,i,e))return false;DungeonSwitchRuntime next=*this;std::vector<Waiter>done;
 for(auto w:next.waiters_)if(w.object==i)done.push_back(w);
 next.waiters_.erase(std::remove_if(next.waiters_.begin(),next.waiters_.end(),[&](const Waiter&w){return w.object==i;}),next.waiters_.end());
 for(auto w:done){
  if(!next.emit(DungeonSwitchEventKind::ObjectStateChanged,h.id,{},w.on,w.silent,0,0,0,animations_[i],e))return false;
  if(h.id==data_.policy().wall&&w.on&&!w.silent&&!next.timer(i,true,e))return false;
  if(w.operation){auto op=std::find_if(next.operations_.begin(),next.operations_.end(),[&](const Operation&v){return v.token==w.operation;});if(op==next.operations_.end())return reject(e,"Dungeon switch animation lost its continuation");auto pending=std::find(op->waiting.begin(),op->waiting.end(),h.id);if(pending==op->waiting.end())return reject(e,"Dungeon switch duplicate animation continuation");op->waiting.erase(pending);}
  if(!next.settle(e))return false;
 }
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::camera_finished(DungeonSwitchToken token,std::string&e){
 if(!ready()||token.generation!=generation_||!token.value)return reject(e,"Dungeon switch camera has no current owner");auto at=std::find_if(operations_.begin(),operations_.end(),[&](const Operation&v){return v.token==token.value;});if(at==operations_.end()||(at->phase!=Phase::FocusCamera&&at->phase!=Phase::ReturnCamera))return reject(e,"Dungeon switch camera phase rejected");size_t i=at-operations_.begin();DungeonSwitchRuntime next=*this;
 if(at->phase==Phase::ReturnCamera){if(!next.finish_operation(i,e))return false;}
 else {next.operations_[i].phase=Phase::Waiting;if(!next.settle(e))return false;}
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::idle_frame(std::string&e){
 if(!ready())return reject(e,"Dungeon switch idle has no scene owner");DungeonSwitchRuntime next=*this;
 for(auto&op:next.operations_)if(op.phase==Phase::Idle){op.phase=Phase::ReturnCamera;auto row=data_.switch_node(op.source);if(!next.emit(DungeonSwitchEventKind::CameraReturn,row.id,{op.token,generation_},false,false,0,0,row.camera_delay,DungeonSwitchAnimation::None,e))return false;}
 *this=std::move(next);e.clear();return true;
}
bool DungeonSwitchRuntime::leave_area(bool changed,std::string&e){
 if(!ready())return reject(e,"Dungeon switch leave has no scene owner");DungeonSwitchRuntime next=*this;
 for(uint32_t i=0;i<switches_.size();++i)if((data_.switch_node(i).bits&4)||(changed&&(data_.switch_node(i).bits&8)))if(!next.flag(i,false,e))return false;
 *this=std::move(next);e.clear();return true;
}
}
