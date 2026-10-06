#include "encore/field_scene_actions.hpp"
#include <algorithm>
#include <utility>
namespace encore::upstream {
const FieldSceneActionState*FieldSceneActionsRuntime::state(uint32_t id)const{for(const auto&s:states_)if(s.id==id)return&s;return nullptr;}
FieldSceneActionState*FieldSceneActionsRuntime::active(uint32_t id,std::string&e){for(auto&s:states_)if(s.id==id){if(s.alive&&s.ready)return&s;e="Scene action source lifecycle inactive";return nullptr;}e="Scene action source identity unknown";return nullptr;}
bool FieldSceneActionsRuntime::initialize(const FieldSceneActionsData&d,FieldSceneActionsHost h,std::string&e){
 if(!d.valid()||!h.admit_ready||!h.resolve||!h.describe||!h.body_is_player||!h.admit_reparent||!h.remove_child||!h.add_child||!h.read_collision||!h.set_collision||!h.enqueue||!h.admit_event||!h.has_method||!h.call_method||!h.read_flag||!h.write_existing_flag){e="Scene actions checked data/typed host incomplete";return false;}
 std::vector<FieldSceneActionState>s;for(const auto&b:d.bindings()){FieldSceneActionState v;v.id=b.id;s.push_back(v);}data_=&d;host_=std::move(h);states_=std::move(s);pending_.clear();ready_index_=0;last_token_=0;e.clear();return true;
}
bool FieldSceneActionsRuntime::ready(uint32_t id,std::string&e){
 if(!data_||ready_index_>=states_.size()||states_[ready_index_].id!=id||!states_[ready_index_].alive||states_[ready_index_].ready){e="Scene actions source Ready identity/order";return false;}
 auto candidate=states_[ready_index_];const auto&b=data_->bindings()[ready_index_];if(!host_.admit_ready(b,*data_,e))return false;
 if(b.kind==1){
  if(!host_.resolve(id,b.parent_path,b.parent_id,candidate.parent,e))return false;
  for(const auto&o:b.objects){uint64_t object=0;if(!host_.resolve(id,o.path,o.source_id,object,e))return false;candidate.objects.push_back(object);}
 }else if(!host_.resolve(id,b.object_path,b.object_id,candidate.object,e))return false;
 candidate.ready=true;states_[ready_index_]=std::move(candidate);++ready_index_;return true;
}
bool FieldSceneActionsRuntime::body_enter(uint32_t id,uint32_t body,std::string&e){
 if(!active(id,e))return false;
 if(!body){e="Scene action body identity absent";return false;}bool player=false;if(!host_.body_is_player(body,player,e))return false;if(!player)return true;const auto*b=data_->binding(id);
 if(b->kind==1)return reparent(id,e);
 if(!b->check_flag.empty()){bool exists=false,value=false;if(!host_.read_flag(b->check_flag,exists,value,e))return false;if(!exists||value!=b->check_state)return true;}
 return activate_event(id,e);
}
bool FieldSceneActionsRuntime::queue(FieldSceneDeferred op,std::string&e){
 uint64_t token=0;if(!host_.enqueue(op,token,e))return false;if(!token||token<=last_token_){e="Scene action global deferred token/order rejected";return false;}op.token=token;last_token_=token;pending_.push_back(op);return true;
}
bool FieldSceneActionsRuntime::reparent(uint32_t id,std::string&e){
 auto*s=active(id,e);if(!s)return false;const auto*b=data_->binding(id);if(b->kind!=1){e="Scene action is not Reparenter";return false;}
 if(pending_.size()+s->objects.size()*3>4096){e="Scene actions bounded deferred capacity exhausted";return false;}
 if(!host_.admit_reparent(*b,*s,e))return false;
 for(size_t i=0;i<s->objects.size();++i){const auto item=s->objects[i];if(!item||!s->parent)continue;
  FieldSceneActionNodeState current,parent;if(!host_.describe(item,current,e)||!host_.describe(s->parent,parent,e))return false;
  if(!current.live||!parent.live){e="Scene action cached object was freed";return false;}
  if(!current.parent||current.parent==s->parent)continue;
  const auto*ref=data_->reference(b->objects[i].source_id);const auto*parent_ref=data_->reference(b->parent_id);
  if(!ref||!parent_ref||current.kind!=ref->kind||parent.kind!=parent_ref->kind){e="Scene action cached source class mismatch";return false;}
  if(!host_.remove_child(current.parent,item,e))return false;
  FieldSceneDeferred add;add.target=s->parent;add.item=item;add.target_kind=parent_ref->kind;add.item_kind=ref->kind;if(!queue(add,e))return false;
  if(b->copy){for(uint32_t role=1;role<=2;++role){uint32_t value=0;if(!host_.read_collision(s->parent,role,value,e))return false;FieldSceneDeferred op;op.target=item;op.target_kind=ref->kind;op.kind=role==1?FieldSceneDeferredKind::SetMask:FieldSceneDeferredKind::SetLayer;op.value=value;op.known_absent=!ref->collision_properties;if(!queue(op,e))return false;}}
 }
 return true;
}
bool FieldSceneActionsRuntime::activate_event(uint32_t id,std::string&e){
 auto*s=active(id,e);if(!s)return false;const auto*b=data_->binding(id);if(b->kind!=2){e="Scene action is not EventActivator";return false;}
 if(!host_.admit_event(*b,*s,e))return false;
 if(s->object&&!b->method.empty()){FieldSceneActionNodeState object;if(!host_.describe(s->object,object,e))return false;if(!object.live){e="EventActivator cached object was freed";return false;}bool present=false;if(!host_.has_method(s->object,b->action,b->method,present,e))return false;if(present&&!host_.call_method(s->object,b->action,e))return false;}
 return set_flag(id,e);
}
bool FieldSceneActionsRuntime::set_flag(uint32_t id,std::string&e){
 if(!active(id,e))return false;
 const auto*b=data_->binding(id);if(b->kind!=2){e="Source set_flag target is not EventActivator";return false;}
 if(!b->set_flag.empty()){bool exists=false,value=false;if(!host_.read_flag(b->set_flag,exists,value,e))return false;if(exists&&!host_.write_existing_flag(b->set_flag,b->set_state,e))return false;}return true;
}
bool FieldSceneActionsRuntime::flush_deferred(uint64_t token,std::string&e){
 // Root global queue may retain entries after the source Area exits; target
 // ObjectIDs and their native lifetime, not the Area, own deferred effects.
 if(pending_.empty()||pending_.front().token!=token){e="Scene action deferred FIFO token rejected";return false;}const auto op=pending_.front();FieldSceneActionNodeState target;if(!host_.describe(op.target,target,e))return false;
 if(target.live){
  if(target.kind!=op.target_kind||(op.known_absent&&target.kind!=2)){e="Scene action deferred target class mismatch";return false;}
  if(op.kind==FieldSceneDeferredKind::AddChild){if(!host_.add_child(op.target,op.item,e))return false;}
  else if(!op.known_absent){const auto role=op.kind==FieldSceneDeferredKind::SetMask?1u:2u;if(!host_.set_collision(op.target,role,op.value,e))return false;}
 }
 pending_.erase(pending_.begin());return true;
}
bool FieldSceneActionsRuntime::exit_tree(uint32_t id,std::string&e){auto*s=active(id,e);if(!s)return false;s->alive=false;return true;}
}
