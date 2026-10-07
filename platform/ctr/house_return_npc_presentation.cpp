#include "house_return_npc_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&
      a.source_sha256==b.source_sha256;
}
}
bool HouseReturnNpcPresentation::prepare(HouseReturnNpcPresentationInput in,std::string&e){
  if(prepared_||!in.sources||!in.sources->valid()||!in.tree||!in.registry||
      !in.signals||!in.npcs||!in.sprites||!in.animations||!in.native_sprites||
      !in.node_timers||!in.scene_timers||!in.registry->kernel()||in.registry->poisoned()||
      in.tree->object_domain()!=in.registry->kernel()||in.signals->registry()!=in.registry||
      in.scene_timers->registry()!=in.registry||in.node_timers->registry()!=in.registry||
      in.native_sprites->canvas_tree()!=in.tree||!in.native_sprites->canvas_data()||
      !same(in.native_sprites->canvas_data()->identity(),in.sources->tree().identity())||
      in.native_sprites->canvas_data()->source_scene()!=in.sources->tree().source_scene()||
      !in.sources->sprites().valid()||!in.sources->timers().valid()||
      in.sources->sprites().scene_id()!=in.sources->tree().identity().scene_id||
      in.sources->sprites().source_pin()!=in.sources->tree().identity().upstream_commit||
      !field_sprite_npc_binding(in.sources->sprites(),in.sources->npcs(),e))
    return reject(e,"House NPC presentation requires actual House Sprite/Timer/shared-list owners");
  const FieldNpcWorldCallback*returned=nullptr;const FieldNpcWorldCallback*wander=nullptr;
  for(const auto&c:in.sources->npc_world().callbacks()){
    if(c.op==2){if(returned)return reject(e,"House NPC return callback is ambiguous");returned=&c;}
    if(c.op==1){if(wander)return reject(e,"House NPC wander callback is ambiguous");wander=&c;}
  }
  if(!returned||!wander||returned->arity||wander->arity||returned->method.empty()||wander->method.empty())
    return reject(e,"House NPC source Timer callbacks are incomplete");
  size_t characters=0;
  for(const auto&s:in.sources->sprites().records()){
    const auto*n=in.sources->tree().record(s.id);std::array<uint8_t,32>sha{};
    if(!n||n->path!=s.node||n->parent!=s.parent_id||n->ready!=s.ready_ordinal||
        n->script.empty()||!in.sources->tree().source_hash(n->script,sha)||sha!=n->script_sha||
        (s.kind==FieldSpriteKind::Character?n->native_class!="Sprite":
          s.kind!=FieldSpriteKind::Fetcher||n->native_class!="Node"))
      return reject(e,"House Sprite source/native complete-tree declaration differs");
    if(s.kind!=FieldSpriteKind::Character)continue;
    ++characters;const auto*l=in.sources->npc_world().npc(s.parent_id);
    const auto*t=l?in.sources->tree().record(l->timer):nullptr;
    if(!l||l->sprite!=s.id||!t||t->parent!=s.parent_id||t->native_class!="Timer"||
        !t->script.empty()||!in.sources->timers().record(in.sources->tree().identity(),t->id))
      return reject(e,"House NPC CharacterSprite/WanderTimer source linkage differs");
  }
  if(characters!=in.sources->npcs().npcs().size())
    return reject(e,"House NPC presentation omits actual CharacterSprite source receivers");
  in_=in;return_method_=returned->method;wander_method_=wander->method;
  prepared_=true;e.clear();return true;
}
bool HouseReturnNpcPresentation::borrows(std::string&e)const{
  if(!prepared_||!in_.sources->valid()||in_.registry->poisoned()||
      in_.signals->registry()!=in_.registry||in_.tree->object_domain()!=in_.registry->kernel()||
      in_.npcs->tree()!=in_.tree||in_.npcs->runtime().data()!=&in_.sources->npcs()||
      !in_.npcs->runtime().error().empty()||in_.scene_timers->registry()!=in_.registry||
      in_.node_timers->registry()!=in_.registry||in_.native_sprites->canvas_tree()!=in_.tree||
      !in_.native_sprites->canvas_data()||
      !same(in_.native_sprites->canvas_data()->identity(),in_.sources->tree().identity()))
    return reject(e,"House NPC presentation borrowed a different actual source/core/native owner");
  e.clear();return true;
}
bool HouseReturnNpcPresentation::apply(HouseReturnNpcPorts&p,std::string&e){
  if(!prepared_||applied_)return reject(e,"House NPC presentation ports are unprepared/rebound");
  p.sprite=[this](auto object,auto op,const auto&d,const auto&pose,auto&error){
    return sprite(object,op,d,pose,error);
  };
  p.timer=[this](auto object,auto timer_object,auto op,auto time,auto receipt,auto&error){
    return timer(object,timer_object,op,time,receipt,error);
  };
  applied_=true;e.clear();return true;
}
bool HouseReturnNpcPresentation::initialize_sprites(FieldSpriteHost host,std::string&e){
  if(!borrows(e)||!applied_||sprites_initialized_||in_.sprites->data())
    return reject(e,"House Sprite/AnimationTree source core is missing/already initialized");
  // Keep caller's concrete native publisher and synchronous source receivers.
  // apply wraps it with the same real AnimationTree observer; neither consumer
  // gets a second idle clock. Missing concrete Host endpoints fail initialize.
  if(!in_.animations->prepare(in_.sources->tree(),in_.sources->sprites(),in_.sources->npcs(),
      *in_.tree,*in_.registry,*in_.sprites,in_.npcs->runtime(),e)||
      !in_.animations->apply(host,e)||!in_.sprites->initialize(in_.sources->sprites(),std::move(host),e))
    return false;
  sprites_initialized_=true;e.clear();return true;
}
bool HouseReturnNpcPresentation::node(uint32_t stable,FieldObjectId&object,bool ready,std::string&e)const{
  object=in_.tree->source_object(stable);const auto*n=in_.tree->descriptor(object);
  const auto*s=in_.tree->state(object);const auto*original=in_.sources->tree().record(stable);
  FieldIdentity identity;
  if(!object||!n||!s||!s->alive||!original||n->id!=stable||n->path!=original->path||
      n->parent!=original->parent||n->native_class!=original->native_class||
      n->script!=original->script||n->script_sha!=original->script_sha||
      !in_.registry->object_exists(object)||in_.registry->tree_owner(object).get()!=in_.tree||
      !in_.tree->object_identity(object,identity)||!same(identity,in_.sources->tree().identity())||
      (ready&&(!s->inside||!s->bound||!s->ready_notified)))
    return reject(e,"House presentation actual native node/source/lifecycle receipt differs");
  e.clear();return true;
}
bool HouseReturnNpcPresentation::npc(FieldObjectId object,uint32_t&stable,
    const FieldNpcDescriptor*&d,const FieldNpcInstance*&pose,bool ready,std::string&e)const{
  if(!borrows(e))return false;
  const auto*n=in_.tree->descriptor(object);FieldObjectId exact=0;
  d=nullptr;pose=nullptr;
  if(!n||n->native_class!="KinematicBody2D"||!in_.npcs->owns(object)||
      !node(n->id,exact,ready,e)||exact!=object||!in_.sources->npc_world().npc(n->id))
    return reject(e,"House presentation receiver is not its actual NPC source owner");
  stable=n->id;
  for(const auto&v:in_.sources->npcs().npcs())if(v.id==stable)d=&v;
  for(const auto&v:in_.npcs->runtime().npcs())if(v.id==stable)pose=&v;
  if(!d||!pose||pose->destroyed||(ready&&!pose->ready))
    return reject(e,"House presentation source/core NPC body is unavailable");
  e.clear();return true;
}
bool HouseReturnNpcPresentation::finish_factory(std::string&e){
  if(!borrows(e)||!sprites_initialized_||factory_||!in_.node_timers->finish_factory(e))
    return reject(e,"House NPC presentation requires complete actual native factory once");
  for(const auto&d:in_.sources->npcs().npcs()){
    const auto*l=in_.sources->npc_world().npc(d.id);FieldObjectId body=0,clock=0,sprite_object=0;
    if(!l||!node(d.id,body,false,e)||!in_.npcs->owns(body)||
        !node(l->timer,clock,false,e)||!in_.node_timers->owns(clock)||
        !node(l->sprite,sprite_object,false,e)||!in_.native_sprites->owns(sprite_object)||
        !in_.tree->state(body)->bound||!in_.tree->state(clock)->bound||
        !in_.tree->state(sprite_object)->bound||!in_.sprites->instance(l->sprite)||
        !in_.signals->connect(clock,"timeout",body,wander_method_,FieldSignalPersist,{},e))
      return reject(e,"House actual WanderTimer source connection/factory differs");
  }
  factory_=true;e.clear();return true;
}
bool HouseReturnNpcPresentation::sprite(FieldObjectId object,FieldNpcPresentation op,
    const FieldNpcDescriptor&given,const FieldNpcInstance&given_pose,std::string&e){
  uint32_t stable=0;const FieldNpcDescriptor*d=nullptr;const FieldNpcInstance*pose=nullptr;
  if(!factory_||!sprites_initialized_||in_.sprites->data()!=&in_.sources->sprites()||
      !in_.sprites->error().empty()||!npc(object,stable,d,pose,true,e)||
      &given!=d||&given_pose!=pose)
    return reject(e,"House NPC Sprite operation did not come from the actual source core");
  const auto*l=in_.sources->npc_world().npc(stable);FieldObjectId child=0;
  const auto*decl=l?in_.sources->sprites().record(l->sprite):nullptr;
  const auto*state=l?in_.sprites->instance(l->sprite):nullptr;
  const auto graph=l?in_.animations->animation_tree(l->sprite):0;
  const auto*gs=in_.tree->state(graph);const auto*gn=in_.tree->descriptor(graph);FieldIdentity gi;
  if(!decl||decl->parent_id!=stable||decl->kind!=FieldSpriteKind::Character||!state||!state->ready||
      !node(l->sprite,child,true,e)||!in_.native_sprites->owns(child)||
      in_.tree->state(child)->parent!=object||!graph||!gs||!gs->alive||!gs->inside||!gs->bound||
      !gs->ready_notified||gs->parent!=child||!gn||gn->native_class!="AnimationTree"||
      !gn->script.empty()||!in_.animations->owns(graph)||!in_.registry->object_exists(graph)||
      in_.registry->tree_owner(graph).get()!=in_.tree||!in_.tree->object_identity(graph,gi)||
      !same(gi,in_.sources->tree().identity())||!in_.animations->observe_sprite(l->sprite,*state,e))
    return reject(e,"House NPC Sprite/AnimationTree actual child Ready/source binding is incomplete");
  bool ok=false;
  switch(op){
  case FieldNpcPresentation::Create:ok=in_.sprites->parent_setup(l->sprite);break;
  case FieldNpcPresentation::Blend:ok=in_.sprites->blend_position(l->sprite,pose->blend);break;
  case FieldNpcPresentation::Motion:{
    const auto index=pose->pending_motion!=UINT32_MAX?pose->pending_motion:pose->current_motion;
    const auto&name=index<d->motions.size()?d->motions[index].name:d->idle;
    ok=in_.sprites->travel(l->sprite,name);break;
  }
  case FieldNpcPresentation::Frame:
    // Sprite::set_frame emits frame_changed even if the number is unchanged.
    // The subsequent checked source publication must not synthesize it again.
    if(!in_.native_sprites->sprite_set_frame(child,pose->frame,e))return false;
    ok=in_.sprites->frame(l->sprite,pose->frame);break;
  default:return reject(e,"House NPC presentation operation belongs to another actual owner");
  }
  if(!ok){e=in_.sprites->error();return false;}
  e.clear();return true;
}
bool HouseReturnNpcPresentation::timer(FieldObjectId object,FieldObjectId clock,
    FieldNpcTimer op,double seconds,uint64_t receipt,std::string&e){
  uint32_t stable=0;const FieldNpcDescriptor*d=nullptr;const FieldNpcInstance*pose=nullptr;
  if(!factory_||!npc(object,stable,d,pose,true,e)||!std::isfinite(seconds)||seconds<0||
      seconds>double(std::numeric_limits<float>::max()))
    return reject(e,"House NPC Timer value/source body receipt differs");
  const auto*l=in_.sources->npc_world().npc(stable);FieldObjectId exact=0;
  if(!l||!node(l->timer,exact,true,e)||exact!=clock||!in_.node_timers->owns(clock)||
      in_.tree->state(clock)->parent!=object||!in_.node_timers->state(clock))
    return reject(e,"House NPC operation is not its actual WanderTimer owner");
  switch(op){
  case FieldNpcTimer::SetWanderWait:
    if(receipt)return reject(e,"House native wait_time assignment received coroutine receipt");
    return in_.node_timers->set_wait(clock,float(seconds),e);
  case FieldNpcTimer::StartWander:
    if(receipt)return reject(e,"House native Timer start received coroutine receipt");
    return in_.node_timers->start(clock,0,e);
  case FieldNpcTimer::StopWander:
    if(receipt)return reject(e,"House native Timer stop received coroutine receipt");
    return in_.node_timers->stop(clock,e);
  case FieldNpcTimer::ReturnDirection:return create_return(object,stable,*pose,seconds,receipt,e);
  }
  return reject(e,"House NPC Timer source operation is unknown");
}
bool HouseReturnNpcPresentation::create_return(FieldObjectId object,uint32_t stable,
    const FieldNpcInstance&body,double seconds,uint64_t receipt,std::string&e){
  if(!receipt||seconds!=in_.sources->npcs().parameter(FieldNpcParameter::ReturnDelay)||
      std::find(body.return_waiters.begin(),body.return_waiters.end(),receipt)==body.return_waiters.end())
    return reject(e,"House NPC return direction lacks actual retained source waiter");
  for(const auto&v:waiting_)if(v.second.target==object&&v.second.waiter==receipt&&!v.second.timer.expired())
    return reject(e,"House NPC duplicate SceneTreeTimer source waiter");
  std::shared_ptr<FieldSceneTreeTimer> timer;
  // Omitted source process_pause argument uses the actual native true default.
  if(!in_.scene_timers->create_timer(float(seconds),true,timer,e))return false;
  const auto id=timer->binding().object;
  if(!id||timer->registry()!=in_.registry||!in_.scene_timers->owns(id)||
      !in_.signals->connect(id,FieldSceneTreeTimers::timeout_signal(),object,return_method_,0,{},e))
    return reject(e,"House NPC return timer is not owned by the retained actual SceneTree list");
  waiting_.emplace(id,Waiting{object,stable,receipt,timer,false});e.clear();return true;
}
bool HouseReturnNpcPresentation::handles_callback(const FieldDeferredMessage&m)const{
  if(!prepared_||m.kind!=FieldDeferredKind::Call||m.member!=return_method_)return false;
  const auto*n=in_.tree->descriptor(m.object);
  return n&&in_.sources->npc_world().npc(n->id)&&in_.npcs->owns(m.object);
}
bool HouseReturnNpcPresentation::deferred(const FieldDeferredMessage&m,std::string&e){
  if(!borrows(e)||!handles_callback(m)||!m.args.empty())
    return reject(e,"House NPC return method is not an actual source receiver");
  const auto id=in_.scene_timers->emitting();auto i=waiting_.find(id);
  const auto timer=i==waiting_.end()?nullptr:i->second.timer.lock();
  uint32_t stable=0;const FieldNpcDescriptor*d=nullptr;const FieldNpcInstance*pose=nullptr;
  if(!timer||i->second.returned||i->second.target!=m.object||timer->time_left()>=0||
      timer->binding().object!=id||!in_.scene_timers->owns(id)||
      !in_.signals->emitting_to(id,FieldSceneTreeTimers::timeout_signal(),m.object,m.member)||
      !npc(m.object,stable,d,pose,true,e)||stable!=i->second.source||
      std::find(pose->return_waiters.begin(),pose->return_waiters.end(),i->second.waiter)==pose->return_waiters.end())
    return reject(e,"House NPC return requires the actual synchronous timer timeout and source waiter");
  if(!in_.npcs->return_direction_timeout(m.object,i->second.waiter,e))return false;
  i->second.returned=true;e.clear();return true;
}
bool HouseReturnNpcPresentation::collect_expired(std::string&e){
  if(!prepared_||in_.scene_timers->registry()!=in_.registry||in_.scene_timers->emitting())
    return reject(e,"House timer receipt collection is not after the real shared idle tail");
  for(auto i=waiting_.begin();i!=waiting_.end();)if(i->second.timer.expired())i=waiting_.erase(i);else ++i;
  e.clear();return true;
}
}
