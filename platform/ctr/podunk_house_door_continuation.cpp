#include "podunk_house_door_continuation.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {bool fail(std::string &e,const char *s){e=s;return false;}}
bool PodunkHouseDoorContinuation::initialize(const FieldDoorData &d,uint32_t source,
 HouseRuntime &house,FieldGlobalRegistry &registry,FieldDoorRuntime &runtime,std::string &e){
  if(data_||!d.valid()||d.door_count()!=1||!d.find(source,door_)||
     house.phase()!=HousePhase::Idle||!registry.data()||registry.poisoned()||
     registry.data()->identity().upstream_commit!=d.identity().upstream_commit)
    return fail(e,"House Door continuation requires same live source session");
  data_=&d;house_=&house;registry_=&registry;runtime_=&runtime;
  tree_=std::make_shared<FieldNodeTreeRuntime>();
  FieldNodeTreeHost h;h.object_domain=registry.kernel();
  h.allocate_object=[this](auto &id,auto &err){return registry_->allocate_object(id,err);};
  h.allocate_fast_name=[this](auto &id,auto &err){return registry_->allocate_fast_name(id,err);};
  h.object_exists=[this](auto id){return registry_->object_exists(id);};
  h.native_allocated=[this](auto id,const auto &node,const auto &identity,auto &err){
    return registry_->publish_allocated_node(tree_,id,
      [this](const auto &m,auto &error){return deferred(m,error);},err)&&construct(id,node,identity,err);
  };
  h.construct_source=[this](auto id,const auto &node,const auto &,auto &err){
    const auto i=nodes_.find(id);
    if(i==nodes_.end()||i->second.source.id!=node.id)
      return fail(err,"House Door source attachment has no actual native owner");
    err.clear();return true;
  };
  h.bind=[this](auto id,const auto &,auto &b,auto &err){return bind(id,b,err);};
  h.dispatch=[this](auto id,const auto &,auto p,auto &err){return phase(id,p,0,false,false,err);};
  h.deferred=[this](const auto &m,auto &err){return deferred(m,err);};
  h.enqueue_global=[this](auto m,auto &err){return registry_->enqueue(std::move(m),err);};
  h.flush_global=[this](auto &err){return registry_->flush_messages(err);};
  h.external_path=[this](auto id,auto path,auto &out,auto &err){return registry_->resolve_path(id,path,out,err);};
  h.input_registration=[](auto,auto,bool enabled,auto &err){
    if(enabled)return fail(err,"House Door has no native input processing");
    err.clear();return true;
  };
  h.external_pause_process=[](auto){return true;}; // Source Door PAUSE_MODE_PROCESS.
  h.release=[this](auto id,const auto &,auto &err){return release(id,err);};
  if(!tree_->initialize_door_continuation(d,source,std::move(h),e))return false;
  object_=tree_->root();e.clear();return true;
}
bool PodunkHouseDoorContinuation::owns(const FieldNodeDescriptor &n)const{
  return data_&&(n.id==door_.id||n.id==door_.shape||n.id==door_.marker||n.id==door_.audio);
}
bool PodunkHouseDoorContinuation::owns(FieldObjectId id)const{return nodes_.count(id)!=0;}
bool PodunkHouseDoorContinuation::construct(FieldObjectId id,const FieldNodeDescriptor &n,
 const FieldIdentity &identity,std::string &e){
  if(!owns(n)||nodes_.count(id)||registry_->tree_owner(id)!=tree_||
     identity.scene_id!=data_->identity().scene_id||identity.source_sha256!=data_->identity().source_sha256||
     identity.upstream_commit!=data_->identity().upstream_commit||
     (n.id==door_.id?(n.native_class!="Area2D"||n.script!=data_->script()||n.script_sha!=data_->script_sha()):!n.script.empty()))
    return fail(e,"House Door native source allocation differs");
  nodes_.emplace(id,Native{n});e.clear();return true;
}
bool PodunkHouseDoorContinuation::bind(FieldObjectId id,FieldNodeBinding &b,std::string &e){
  auto i=nodes_.find(id);auto owner=registry_->tree_owner(id);
  if(i==nodes_.end()||!owner||!owner->state(id)||!owner->state(id)->alive)
    return fail(e,"House Door native bind has no same live object");
  const auto &n=i->second.source;b={data_->identity(),n.id,n.class_index,0x454e001f,data_->capability(),n.script_sha,n.native_class};
  e.clear();return true;
}
bool PodunkHouseDoorContinuation::phase(FieldObjectId id,FieldTreePhase p,float,bool,bool,std::string &e){
  auto i=nodes_.find(id);auto owner=registry_->tree_owner(id);
  if(i==nodes_.end()||!owner||!owner->state(id))return fail(e,"House Door native phase lost actual object");
  switch(p){
  case FieldTreePhase::EnterNative:i->second.entered=true;break;
  case FieldTreePhase::ReadyScript:
    if(i->second.source.id==door_.id&&!runtime_->source_ready(door_.id)){
      if(runtime_->data()!=data_||!runtime_->ready(door_.id,e))return false;
    }break;
  case FieldTreePhase::ReadyNative:i->second.ready=true;break;
  case FieldTreePhase::ExitNative:i->second.entered=false;break;
  case FieldTreePhase::Idle:case FieldTreePhase::Physics:
  case FieldTreePhase::IdleInternal:case FieldTreePhase::PhysicsInternal:
  case FieldTreePhase::Input:case FieldTreePhase::UnhandledInput:case FieldTreePhase::UnhandledKeyInput:
    return fail(e,"House Door native continuation has no frame/input callback");
  default:break; // Native Node notifications; no source method attached to these phases.
  }
  e.clear();return true;
}
bool PodunkHouseDoorContinuation::enter(std::string &e){
  return data_&&runtime_->data()==data_&&tree_->enter(e);
}
bool PodunkHouseDoorContinuation::deferred(const FieldDeferredMessage &,std::string &e){
  return fail(e,"House Door continuation has no untyped deferred gameplay method");
}
bool PodunkHouseDoorContinuation::release(FieldObjectId id,std::string &e){
  auto i=nodes_.find(id);if(i==nodes_.end())return fail(e,"House Door release unowned object");
  nodes_.erase(i);e.clear();return true;
}
bool PodunkHouseDoorContinuation::declaration(FieldObjectId id,std::string_view name,uint32_t &arity,std::string &e)const{
  if(!owns(id))return fail(e,"House Door signal source object differs");
  if(id==object_&&data_->house_continuation())
    for(auto signal:{FieldDoorSignal::Entered,FieldDoorSignal::MovedPlayer,FieldDoorSignal::Done})
      if(name==data_->door_signal(signal)){arity=0;e.clear();return true;}
  for(auto signal:{"tree_entered","tree_exiting","tree_exited","ready","renamed"})
    if(name==signal){arity=0;e.clear();return true;}
  return fail(e,"House Door unknown source signal binding rejected");
}
bool PodunkHouseDoorContinuation::resolve_onready(const FieldDoorDescriptor &d,const FieldDoorAudio &a,std::string &e)const{
  if(!data_||d.id!=door_.id||d.marker!=door_.marker||d.audio!=door_.audio||a.id!=door_.audio)
    return fail(e,"House Door onready source descriptor differs");
  auto owner=registry_->tree_owner(object_);
  if(!owner)return fail(e,"House Door actual owning Tree missing");
  for(auto stable:{door_.marker,door_.audio}){
    auto id=owner->source_object(stable);auto i=nodes_.find(id);auto *n=owner->state(id);
    if(i==nodes_.end()||!n||!n->inside||!i->second.ready)
      return fail(e,"House Door onready native child has not completed Ready");
  }
  e.clear();return true;
}
bool PodunkHouseDoorContinuation::connect_body(uint32_t source,std::function<bool(uint64_t,std::string &)> slot,std::string &e){
  if(source!=door_.id||body_||!slot)return fail(e,"House Door body source connection duplicate/different");
  body_=std::move(slot);e.clear();return true;
}
bool PodunkHouseDoorContinuation::body_entered(uint64_t actual,std::string &e){
  return body_&&body_(actual,e);
}
bool PodunkHouseDoorContinuation::marker_world(uint32_t source,Vec2 &out,std::string &e)const{
  auto owner=registry_->tree_owner(object_);FieldTransform t;
  if(!owner||source!=door_.marker||!owner->world_transform(owner->source_object(source),t,e))return false;
  out=t[2];e.clear();return true;
}
bool PodunkHouseDoorContinuation::detach(std::string &e){
  auto owner=registry_->tree_owner(object_);auto *n=owner?owner->state(object_):nullptr;
  if(!n)return fail(e,"House Door detach lost actual object");
  if(n->parent)return owner->remove_child(n->parent,object_,e);
  if(n->inside)return owner->exit(e);
  e.clear();return true;
}
bool PodunkHouseDoorContinuation::reparent(FieldObjectId parent,std::string &e){
  return registry_->persistent_reparent(object_,parent,e);
}
bool PodunkHouseDoorContinuation::queue_free(uint32_t source,std::string &e){
  auto owner=registry_->tree_owner(object_);
  if(source!=door_.id||!owner)return fail(e,"House Door queue_free source differs");
  return owner->queue_free(object_,e);
}
FieldObjectId PodunkHouseDoorContinuation::audio_object()const{
  auto owner=registry_->tree_owner(object_);return owner?owner->source_object(door_.audio):0;
}
} // namespace encore::ctr
