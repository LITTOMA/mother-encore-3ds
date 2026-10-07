#include "house_return_npc_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.source_sha256==b.source_sha256&&
      a.upstream_commit==b.upstream_commit;
}
bool inverse(const FieldTransform&t,Vec2 p,Vec2&out){
  const float det=t[0].x*t[1].y-t[1].x*t[0].y;
  if(!std::isfinite(det)||!det)return false;
  p={p.x-t[2].x,p.y-t[2].y};
  out={(t[1].y*p.x-t[1].x*p.y)/det,(-t[0].y*p.x+t[0].x*p.y)/det};
  return std::isfinite(out.x)&&std::isfinite(out.y);
}
struct SourceCall {
  uint32_t&source;bool&thoughts;size_t&depth;
  uint32_t previous;bool previous_thoughts;
  SourceCall(uint32_t&s,bool&t,size_t&d,uint32_t id,bool th)
      :source(s),thoughts(t),depth(d),previous(s),previous_thoughts(t){s=id;t=th;++d;}
  ~SourceCall(){source=previous;thoughts=previous_thoughts;--depth;}
};
}
const FieldNpcDescriptor*HouseReturnNpcRuntime::descriptor(uint32_t id)const{
  if(!in_.data)return nullptr;
  for(const auto&d:in_.data->npcs())if(d.id==id)return &d;
  return nullptr;
}
bool HouseReturnNpcRuntime::owns(const FieldNodeDescriptor&d)const{
  const auto*n=descriptor(d.id);
  const auto*r=in_.sources?in_.sources->tree().record(d.id):nullptr;
  return n&&r&&r->path==d.path&&n->node==d.path&&r->script==d.script&&
      r->script_sha==d.script_sha&&r->native_class==d.native_class;
}
bool HouseReturnNpcRuntime::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnNpcRuntime::prepare(HouseReturnNpcInput in,std::string&e){
  if(prepared_||in_.sources||!in.sources||!in.sources->valid()||!in.data||!in.data->valid()||
      !in.world_data||!in.world_data->valid()||!in.doors||!in.tree||!in.registry||!in.signals||
      !in.space||!in.kinematic||!in.geometry||!in.rays||!in.global||!in.characters||
      !in.random||!in.uid_ledger||!in.house||!in.text.valid()||!in.session||
      !in.dialogue||!in.dialogue_native||!in.player||
      !in.ports.context||!in.ports.sprite||!in.ports.timer||!in.ports.connect_source||
      !in.ports.close_commands||
      !in.ports.telepathy||!in.ports.admit_callback||!in.ports.party_player||!in.ports.persistent)
    return reject(e,"House NPC requires all concrete source/native borrowers before construction");
  const auto identity=in.sources->tree().identity();
  if(in.data!=&in.sources->npcs()||in.world_data!=&in.sources->npc_world()||
      !same(identity,in.world_data->identity())||in.data->scene_id()!=identity.scene_id||
      in.data->source_pin()!=identity.upstream_commit||
      !in.registry->kernel()||(in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel())||
      in.characters->runtime().registry()!=in.registry||in.signals->registry()!=in.registry||
      !in.global->data()||in.global->data()->identity().upstream_commit!=identity.upstream_commit||
      !in.registry->object_exists(in.global->owner())||
      in.rays->sources()!=in.sources||in.rays->data()!=in.world_data||
      in.rays->npc_data()!=in.data||in.rays->tree()!=in.tree||in.rays->registry()!=in.registry)
    return reject(e,"House NPC source/tree/native rays/global domains differ");
  for(const auto&d:in.data->npcs()){
    const auto*r=in.sources->tree().record(d.id);const auto*l=in.world_data->npc(d.id);
    bool source_hash=false;
    if(r)for(const auto&s:in.data->sources())
      if(s.path==r->script&&s.sha256==r->script_sha)source_hash=true;
    if(!r||!l||!in.world_data->body(d.id)||!in.world_data->ray(l->ray)||
        r->path!=d.node||r->ready!=d.ready_ordinal||!source_hash||
        !in.kinematic->owns(*r))
      return reject(e,"House NPC descriptor is not its actual full-tree source/native body");
  }
  in_=std::move(in);
  if(!runtime_.initialize(in_.data,in_.random,host(),e)){poisoned_=true;return false;}
  prepared_=true;e.clear();return true;
}
bool HouseReturnNpcRuntime::actual(FieldObjectId id,uint32_t&stable,std::string&e)const{
  const auto i=instances_.find(id);
  const auto*s=in_.tree?in_.tree->state(id):nullptr;
  const auto*d=in_.tree?in_.tree->descriptor(id):nullptr;FieldIdentity identity;
  if(!prepared_||poisoned_||i==instances_.end()||!s||!s->alive||!d||
      i->second.source!=d->id||!owns(*d)||!in_.registry->object_exists(id)||
      in_.registry->tree_owner(id).get()!=in_.tree||in_.tree->source_object(d->id)!=id||
      !in_.tree->object_identity(id,identity)||!same(identity,in_.world_data->identity()))
    return reject(e,"House NPC receiver no longer owns its actual source/tree/ObjectDB identity");
  stable=d->id;e.clear();return true;
}
bool HouseReturnNpcRuntime::source(uint32_t id,FieldObjectId&out,std::string&e)const{
  out=in_.tree?in_.tree->source_object(id):0;uint32_t stable=0;
  return out&&actual(out,stable,e)&&stable==id;
}
bool HouseReturnNpcRuntime::construct(FieldObjectId id,const FieldNodeDescriptor&d,
    const FieldIdentity&identity,std::string&e){
  if(!prepared_||poisoned_||!owns(d)||instances_.count(id)||
      !same(identity,in_.world_data->identity())||!in_.registry->object_exists(id)||
      !in_.kinematic->owns(id))
    return reject(e,"House NPC source construction requires its already allocated native body");
  if(!in_.kinematic->bind_source(id,runtime_,d.id,e)){poisoned_=true;return false;}
  instances_.emplace(id,Instance{d.id,false,false});e.clear();return true;
}
bool HouseReturnNpcRuntime::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
  uint32_t stable=0;
  if(!actual(id,stable,e))return false;
  auto&i=instances_.at(id);const auto*r=in_.sources->tree().record(stable);
  if(i.bound||!r||b.stable_id!=stable||b.class_index!=r->class_index||
      b.native_class!=r->native_class||b.script_sha!=r->script_sha||
      !same(b.identity,in_.world_data->identity()))
    return reject(e,"House NPC source binding differs from its checked native/script descriptor");
  i.bound=true;e.clear();return true;
}
bool HouseReturnNpcRuntime::result(bool ok,std::string&e){
  if(!ok){if(e.empty())e=runtime_.error();poisoned_=true;return false;}
  e.clear();return true;
}
bool HouseReturnNpcRuntime::invoke(FieldObjectId id,bool thoughts,
    const std::function<bool()>&call,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  if(source_call_&&source_call_!=stable)
    return reject(e,"House NPC recursive callback crossed source receivers");
  SourceCall guard(source_call_,thoughts_call_,callback_depth_,stable,thoughts);
  return result(call(),e);
}
bool HouseReturnNpcRuntime::borrows(std::string&e)const{
  PodunkMickHouseNativeState s;
  if(!in_.session->house_native_state(*in_.house,s,e))return false;
  if(s.registry!=in_.registry||s.random!=in_.random||s.uid_ledger!=in_.uid_ledger||
      s.global_data!=&in_.characters->runtime()||in_.dialogue_native->house()!=in_.house||
      in_.dialogue->world()!=&in_.house->world||
      in_.house->world.house_programme_owner()!=in_.dialogue||
      in_.house->house.programme_owner()!=in_.dialogue)
    return reject(e,"House NPC UI/programme borrowed different actual session/entropy/House owners");
  e.clear();return true;
}
bool HouseReturnNpcRuntime::dialogue_row(uint32_t id,const FieldNpcDialogue&row)const{
  const auto*d=descriptor(id);if(!d)return false;
  for(const auto&r:d->dialogues)if(r.source==row.source&&r.program==row.program&&
      r.flag==row.flag&&r.group==row.group&&r.ordinal==row.ordinal&&
      r.thoughts==row.thoughts&&r.last==row.last)return true;
  return false;
}
bool HouseReturnNpcRuntime::programme(uint32_t id,std::string_view path,uint32_t&npc,
    uint32_t&program,std::string&e)const{
  const auto*d=descriptor(id);if(!d||path.empty())return reject(e,"House NPC programme lacks a source row");
  bool row=false;for(const auto&r:d->dialogues)if(r.program==path&&r.thoughts==thoughts_call_)row=true;
  if(!row)return reject(e,"House NPC requested a programme outside its actual selected dialogue source");
  npc=program=kRoomNoIndex;
  for(const auto&a:in_.sources->reentry().actors())if(a.node==d->node){
    if(npc!=kRoomNoIndex)return reject(e,"House NPC has duplicate original actor consumers");
    npc=a.house_index;
    if(npc>=in_.text.count(HouseSection::Npcs)||in_.text.npc(npc).body_id!=a.body||
        in_.text.npc(npc).room_actor_index!=a.room_index)
      return reject(e,"House NPC original House/Room actor binding differs");
  }
  const auto room=in_.house->world.content();
  for(uint32_t i=0;i<room.program_count();++i)if(room.string(room.program(i).source_path_string)==path){
    if(program!=kRoomNoIndex)return reject(e,"House NPC programme source path is ambiguous");
    program=i;
  }
  if(npc==kRoomNoIndex||program==kRoomNoIndex)
    return reject(e,"House NPC programme has no actual House actor/Room consumer");
  // The retained concrete Room lifecycle currently admits ordinary dialogue.
  // A thoughts resource cannot be relabelled ordinary to get past that check.
  if(thoughts_call_)return reject(e,"House NPC thoughts requires a concrete House native telepathy programme owner");
  e.clear();return true;
}
bool HouseReturnNpcRuntime::context(uint32_t stable,FieldNpcContext&out,std::string&e){
  FieldObjectId body=0;if(!source(stable,body,e)||!borrows(e))return false;
  std::shared_ptr<const GlobalLoadObjectArray>party;FieldObjectId talker=0;
  if(!in_.global->array(FieldGlobalMemberRole::PartyObjects,party,e)||!party||
      party->values.empty()||party->values.front()!=in_.player||
      !in_.global->object(FieldGlobalMemberRole::Talker,talker,e)||
      !in_.ports.context(body,out,e))return false;
  auto player_tree=in_.registry->tree_owner(in_.player);FieldTransform transform;
  if(!player_tree||!player_tree->world_transform(in_.player,transform,e))return false;
  const auto*s=in_.tree->state(body);if(!s)return reject(e,"House NPC lost actual Canvas parent");
  // npc.gd screen callbacks read uiManager.is_in_cutscene(), supplied by the
  // actual UI owner. Global.InCutscene is a separate field and must not replace it.
  out.player=transform[2];out.current_talker=talker==body;
  out.ancestor_visible=!s->canvas_parent||in_.tree->visible_in_tree(s->canvas_parent);
  out.debug_build=in_.ports.actual_debug_build;e.clear();return true;
}
bool HouseReturnNpcRuntime::programme_input(FieldObjectId object,uint32_t program,bool thoughts,
    HouseSourceNpcProgramme&out,std::string&e)const{
  uint32_t stable=0,npc=0,mapped_program=0;
  if(!callback_depth_||!actual(object,stable,e)||source_call_!=stable||thoughts_call_!=thoughts||
      program>=in_.house->world.content().program_count())
    return reject(e,"House NPC programme receipt is outside its live actual source invocation");
  const auto room=in_.house->world.content();
  if(!programme(stable,room.string(room.program(program).source_path_string),npc,mapped_program,e)||
      mapped_program!=program)return false;
  HouseSourceNpcProgramme r;
  r.source=&runtime_;r.tree=in_.tree;r.tree_data=&in_.sources->tree();r.registry=in_.registry;
  r.reentry=&in_.sources->reentry();r.doors=in_.doors;r.house=in_.text;
  r.object=object;r.source_id=stable;r.original_npc=npc;r.programme=program;r.thoughts=thoughts;
  out=r;e.clear();return true;
}
FieldNpcHost HouseReturnNpcRuntime::host(){
  FieldNpcHost h;
  h.context=[this](uint32_t id,auto&out,auto&e){return context(id,out,e);};
  h.ready_context=[this](uint32_t id,FieldNpcReadyContext&out,std::string&e){
    FieldObjectId body=0;if(!source(id,body,e))return false;
    const auto*s=in_.tree->state(body);
    if(!s||!s->inside||!s->bound||!s->ready_notified)
      return reject(e,"House NPC source Ready/visibility requires its entered bound Canvas receiver");
    out.ancestor_visible=!s->canvas_parent||in_.tree->visible_in_tree(s->canvas_parent);
    out.debug_build=in_.ports.actual_debug_build;e.clear();return true;
  };
  h.flag=[this](const std::string&key,bool&value,std::string&e){
    bool present=false;if(!in_.characters->flags().read(false,key,present,value,e))return false;
    value=present&&value;return true;
  };
  h.seen=[this](uint32_t id,const FieldNpcDialogue&row,bool&value,std::string&e){
    if(!dialogue_row(id,row))return reject(e,"House NPC seen key is not an actual source dialogue row");
    return in_.characters->flags().seen(row.source,value,e);
  };
  h.mark_seen=[this](uint32_t id,const FieldNpcDialogue&row,std::string&e){
    if(!dialogue_row(id,row))return reject(e,"House NPC mark-seen key is not an actual source dialogue row");
    return in_.characters->flags().mark_seen(row.source,e);
  };
  h.admit_program=[this](const std::string&path,std::string&e){
    uint32_t npc=0,program=0;FieldObjectId body=0;
    if(!source_call_||!source(source_call_,body,e)||!borrows(e)||
        !programme(source_call_,path,npc,program,e))return false;
    const auto state=in_.random->state(),draws=in_.random->raw_draw_count();
    const auto ledger=*in_.uid_ledger;const auto flags=in_.characters->flags().revision();
    HouseSourceNpcProgramme request;
    const bool admitted=programme_input(body,program,thoughts_call_,request,e)&&
        in_.house->house.admit_source_npc_programme(request,e)&&
        in_.dialogue_native->admit_npc_programme(*this,request,e);
    if(state!=in_.random->state()||draws!=in_.random->raw_draw_count()||
        ledger!=*in_.uid_ledger||flags!=in_.characters->flags().revision())
      return reject(e,"House NPC read-only programme admission changed actual RNG/UID/flags");
    return admitted;
  };
  h.open_program=[this](uint32_t id,const std::string&path,bool thoughts,
      const FieldNpcDescriptor&d,std::string&e){
    uint32_t npc=0,program=0;FieldObjectId body=0;
    if(id!=source_call_||d.id!=id||thoughts!=thoughts_call_||!source(id,body,e)||
        !borrows(e)||!programme(id,path,npc,program,e))return false;
    HouseSourceNpcProgramme request;
    if(!programme_input(body,program,thoughts,request,e)||
        !in_.house->house.request_source_npc_programme(request,e))return false;
    HouseProgrammeState actual;
    if(!in_.house->house.observe_programme_owner(actual,e)||
        actual.phase!=HouseProgrammePhase::WaitingReady||actual.programme!=program||
        actual.original_npc!=npc||actual.native_closed||
        in_.dialogue->context().talker!=body)
      return reject(e,"House NPC source open did not create the actual House programme/Ready lease");
    return true;
  };
  h.begin_talker=[this](uint32_t id,std::string&e){
    FieldObjectId body=0;return source(id,body,e)&&
        in_.global->set_object(FieldGlobalMemberRole::Talker,body,e);
  };
  h.close_commands=[this](std::string&e){return borrows(e)&&in_.ports.close_commands(e);};
  h.telepathy_effect=[this](uint32_t id,bool on,std::string&e){
    FieldObjectId body=0;return source(id,body,e)&&borrows(e)&&in_.ports.telepathy(body,on,e);
  };
  h.cached_raycast=[this](uint32_t id,Vec2 cast,bool&hit,std::string&e){
    const auto*l=in_.world_data->npc(id);FieldObjectId ray=0,collider=0;
    if(!l||!in_.rays->source_object(l->ray,ray,e)||!in_.rays->set_enabled(ray,true,e)||
        !in_.rays->set_cast_to(ray,cast,e)||!in_.rays->collider(ray,collider,e))return false;
    hit=collider!=0;e.clear();return true;
  };
  h.move_and_slide=[this](uint32_t id,Vec2 v,float delta,Vec2&p,Vec2&velocity,std::string&e){
    FieldObjectId body=0;return source(id,body,e)&&geometry_active_&&
        in_.kinematic->move_and_slide(body,v,delta,p,velocity,e);
  };
  h.present=[this](uint32_t id,auto op,const auto&d,const auto&v,std::string&e){return present(id,op,d,v,e);};
  h.timer=[this](uint32_t id,FieldNpcTimer op,double time,uint64_t receipt,std::string&e){
    const auto*l=in_.world_data->npc(id);FieldObjectId body=0;
    if(!l||!source(id,body,e)||!std::isfinite(time)||time<0||
        (op==FieldNpcTimer::ReturnDirection&&!receipt))
      return reject(e,"House NPC actual source Timer/SceneTreeTimer receipt rejected");
    const auto timer=in_.tree->source_object(l->timer);
    if(!timer||!in_.registry->object_exists(timer)||in_.registry->tree_owner(timer).get()!=in_.tree)
      return reject(e,"House NPC actual WanderTimer native owner unavailable");
    return in_.ports.timer(body,timer,op,time,receipt,e);
  };
  return h;
}
bool HouseReturnNpcRuntime::interaction_shape(uint32_t id,const FieldNpcInstance&pose,std::string&e){
  const auto*d=descriptor(id);const auto*l=in_.world_data->npc(id);
  if(!d||!l||pose.geometry.size()<8)return reject(e,"House NPC source interaction geometry unavailable");
  if(!d->extended_interact)return true;
  FieldObjectId shape=0;
  if(!in_.geometry->source_object(l->interact,shape,e))return false;
  const auto*s=in_.tree->state(shape);if(!s)return reject(e,"House NPC actual Interact shape absent");
  auto local=s->local;const Vec2 world{pose.position.x+pose.geometry[1].offset.x,
      pose.position.y+pose.geometry[1].offset.y};
  if(s->canvas_parent){FieldTransform parent;
    if(!in_.tree->world_transform(s->canvas_parent,parent,e)||!inverse(parent,world,local[2]))
      return reject(e,"House NPC actual Interact Canvas transform is singular");
  }else local[2]=world;
  if(!in_.tree->set_local(shape,local,e)||!in_.geometry->observe_transform(shape,e))return false;
  if(geometry_active_)return in_.space->apply_npc_interaction(runtime_,id,*in_.tree,*in_.registry,e);
  // Source Ready has changed the actual instance fields. Space is still the
  // outgoing World2D; activate_geometry commits them after real replacement.
  e.clear();return true;
}
bool HouseReturnNpcRuntime::present(uint32_t id,FieldNpcPresentation op,
    const FieldNpcDescriptor&d,const FieldNpcInstance&pose,std::string&e){
  FieldObjectId body=0;const auto*l=in_.world_data->npc(id);
  if(!l||d.id!=id||pose.id!=id||runtime_.data()!=in_.data||!source(id,body,e))
    return reject(e,"House NPC presentation crossed source/core receiver");
  switch(op){
  case FieldNpcPresentation::QueueFree:return in_.tree->queue_free(body,e);
  case FieldNpcPresentation::Pose:return in_.kinematic->set_world_position(body,pose.position,e);
  case FieldNpcPresentation::Visibility:{
    FieldObjectId collider=0,interaction=0;
    return in_.geometry->source_object(l->collider,collider,e)&&
        in_.geometry->source_object(l->interact,interaction,e)&&
        in_.tree->set_visible(body,pose.visible,e)&&
        in_.geometry->set_disabled(collider,!pose.collision_enabled,e)&&
        in_.geometry->set_disabled(interaction,!pose.interact_enabled,e)&&
        in_.tree->set_process(body,true,pose.physics,e);
  }
  case FieldNpcPresentation::Create:{
    if(pose.geometry.size()<8||!in_.kinematic->set_world_position(body,pose.position,e)||
        !interaction_shape(id,pose,e))return false;
    const auto shadow=in_.tree->source_object(l->shadow);const auto*s=in_.tree->state(shadow);
    if(!s||!in_.registry->object_exists(shadow))return reject(e,"House NPC actual Shadow child unavailable");
    auto local=s->local;local[0]={pose.geometry[7].scale.x,0};local[1]={0,pose.geometry[7].scale.y};
    if(!in_.tree->set_local(shadow,local,e)||
        !in_.tree->set_visible(shadow,!d.has(FieldNpcFlag::NoShadow),e))return false;
    if(d.has(FieldNpcFlag::NoCollision)){FieldObjectId collider=0;
      if(!in_.geometry->source_object(l->collider,collider,e)||
          !in_.geometry->set_disabled(collider,true,e))return false;
    }
    if(!in_.ports.sprite(body,op,d,pose,e)||!in_.ports.connect_source(body,e))return false;
    const FieldNpcWorldCallback*visibility=nullptr;
    for(const auto&c:in_.world_data->callbacks())if(c.op==12)visibility=&c;
    if(!visibility)return reject(e,"House NPC source visibility method absent from actual resource");
    return in_.signals->connect(body,in_.world_data->visibility_signal(),body,
        visibility->method,0,{},e);
  }
  case FieldNpcPresentation::Blend:case FieldNpcPresentation::Motion:case FieldNpcPresentation::Frame:
    return in_.ports.sprite(body,op,d,pose,e);
  }
  return reject(e,"House NPC unknown source presentation operation");
}
bool HouseReturnNpcRuntime::source_phase(FieldObjectId id,FieldTreePhase phase,float delta,
    bool paused,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  auto&i=instances_.at(id);const auto*s=in_.tree->state(id);
  if(!i.bound||!s||!s->bound)return reject(e,"House NPC source notification before actual binding");
  switch(phase){
  case FieldTreePhase::EnterScript:
    if(i.entered||!s->inside)return reject(e,"House NPC duplicate/unentered source Enter");
    i.entered=true;e.clear();return true;
  case FieldTreePhase::ReadyScript:
    if(!i.entered||!s->ready_notified)return reject(e,"House NPC source Ready not in actual Tree notification");
    return invoke(id,false,[&]{return runtime_.ready(stable);},e);
  case FieldTreePhase::Physics:
    if(!i.entered||!geometry_active_||!std::isfinite(delta)||delta<0)
      return reject(e,"House NPC source physics before actual complete same-space activation");
    if(!in_.tree->can_process(id,paused)){e.clear();return true;}
    return invoke(id,false,[&]{return runtime_.physics_step(stable,delta);},e);
  case FieldTreePhase::ExitScript:
    if(!i.entered||!s->inside)return reject(e,"House NPC source Exit without actual entered receiver");
    i.entered=false;e.clear();return true;
  default:return reject(e,"House NPC source notification has no checked npc.gd consumer");
  }
}
bool HouseReturnNpcRuntime::declaration(FieldObjectId id,std::string_view member,
    uint32_t&arity,std::string&e)const{
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  for(const auto&c:in_.world_data->callbacks())if(c.method==member){arity=c.arity;e.clear();return true;}
  return reject(e,"House NPC unknown source method declaration");
}
bool HouseReturnNpcRuntime::handles_callback(const FieldDeferredMessage&m)const{
  if(!owns(m.object)||m.kind!=FieldDeferredKind::Call)return false;
  for(const auto&c:in_.world_data->callbacks())if(c.method==m.member)return true;
  return false;
}
bool HouseReturnNpcRuntime::deferred(const FieldDeferredMessage&m,std::string&e){
  uint32_t stable=0;if(!handles_callback(m)||!actual(m.object,stable,e))
    return reject(e,"House NPC unknown actual source callback receiver");
  const FieldNpcWorldCallback*callback=nullptr;
  for(const auto&c:in_.world_data->callbacks())if(c.method==m.member)callback=&c;
  if(!callback||callback->arity!=m.args.size())
    return reject(e,"House NPC callback lacks its actual source/signal ownership receipt");
  if(!in_.ports.admit_callback(m,*callback,e))return false;
  bool player=false;
  if(callback->op>=3&&callback->op<=6){
    const auto*ref=std::get_if<FieldObjectRef>(&m.args.front());
    if(!ref||!in_.registry->object_exists(ref->id))return reject(e,"House NPC callback body is not a live ObjectDB argument");
    if(callback->op>=5){if(!in_.ports.party_player(ref->id,player,e))return false;}
    else player=ref->id==in_.player;
  }
  return invoke(m.object,callback->op==11,[&]{
    switch(callback->op){
    case 1:return runtime_.wander_timeout(stable);
    case 2:return reject(e,"House NPC return-direction requires the real SceneTreeTimer waiter receipt");
    case 3:return runtime_.view_entered(stable,player);
    case 4:return runtime_.view_exited(stable,player);
    case 5:return runtime_.near_entered(stable,player);
    case 6:return runtime_.near_exited(stable,player);
    case 7:return runtime_.screen_entered(stable);
    case 8:return runtime_.screen_exited(stable);
    case 9:return runtime_.stop_interaction(stable);
    case 10:return runtime_.interact(stable);
    case 11:return runtime_.telepathy(stable);
    case 12:return runtime_.visibility_changed(stable);
    case 13:{bool persistent=false;
      return in_.ports.persistent(m.object,persistent,e)&&runtime_.tree_exiting(stable,persistent);}
    default:return reject(e,"House NPC unsupported source callback opcode");
    }
  },e);
}
bool HouseReturnNpcRuntime::return_direction_timeout(FieldObjectId id,uint64_t receipt,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,false,[&]{return runtime_.return_direction_timeout(stable,receipt);},e);
}
bool HouseReturnNpcRuntime::set_talking(FieldObjectId id,bool on,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e)||!borrows(e))return false;
  return invoke(id,false,[&]{return runtime_.set_talking(stable,on);},e);
}
bool HouseReturnNpcRuntime::stop_interaction(FieldObjectId id,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,false,[&]{return runtime_.stop_interaction(stable);},e);
}
bool HouseReturnNpcRuntime::recheck_flags(FieldObjectId id,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,false,[&]{return runtime_.recheck_flags(stable);},e);
}
bool HouseReturnNpcRuntime::has_dialog(FieldObjectId id,bool thoughts,bool&out,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,thoughts,[&]{return runtime_.has_dialog(stable,thoughts,out);},e);
}
bool HouseReturnNpcRuntime::interact(FieldObjectId id,bool thoughts,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,thoughts,[&]{return thoughts?runtime_.telepathy(stable):runtime_.interact(stable);},e);
}
bool HouseReturnNpcRuntime::activate_geometry(std::string&e){
  if(!prepared_||poisoned_||geometry_active_||callback_depth_||
      in_.space->source()!=&in_.sources->geometry()||
      in_.tree->lifecycle_pending()||instances_.size()!=in_.data->npcs().size())
    return reject(e,"House NPC activation requires actual full source Ready and replaced World2D");
  for(const auto&v:runtime_.npcs()){
    FieldObjectId body=0;if(!v.ready||v.destroyed||v.queued_free||!source(v.id,body,e))
      return reject(e,"House NPC activation source Ready/deletion state incomplete");
    const auto*s=in_.tree->state(body);
    if(!s||!s->inside||!s->ready_notified||!instances_.at(body).bound)
      return reject(e,"House NPC activation lacks actual Tree/native body ownership");
    const auto*d=descriptor(v.id);
    if(d->extended_interact&&!in_.space->apply_npc_interaction(runtime_,v.id,*in_.tree,*in_.registry,e))return false;
  }
  geometry_active_=true;e.clear();return true;
}
bool HouseReturnNpcRuntime::release_deleted(FieldObjectId id,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  const auto*s=in_.tree->state(id);
  if(callback_depth_||instances_.at(id).entered||!s||s->inside)
    return reject(e,"House NPC source delete while actual callback/tree borrower remains live");
  if(!result(runtime_.destroy(stable),e))return false;
  instances_.erase(id);e.clear();return true;
}
}
