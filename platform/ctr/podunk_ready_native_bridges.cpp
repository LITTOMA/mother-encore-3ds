#include "podunk_ready_native_bridges.hpp"
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e,const char *s){e=s;return false;}
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y);}
}
bool PodunkReadyNativeBridges::prepare(PodunkReadyNativeBridgeInput in,std::string &e){
  if(input_.sources||!in.sources||!in.sources->valid()||!in.tree||!in.registry||
     !in.signals||in.signals->registry()!=in.registry||!in.native||!in.consumers||
     !in.visibility||!in.source||in.sources->bush().capability()!=2)
    return fail(e,"Ready native bridge actual source/owner composition rejected");
  input_=std::move(in);e.clear();return true;
}
bool PodunkReadyNativeBridges::actual(uint32_t source,FieldObjectId &out,std::string &e)const{
  FieldObjectId id=0;if(!input_.source||!input_.source(source,id,e))return false;
  const auto *d=input_.tree->descriptor(id);const auto *s=input_.tree->state(id);
  const auto *original=input_.sources->tree().record(source);FieldIdentity identity;
  if(!d||!s||!original||!s->alive||s->queued||!input_.registry->object_exists(id)||
     input_.registry->tree_owner(id).get()!=input_.tree||d->id!=source||
     d->script!=original->script||d->script_sha!=original->script_sha||
     d->native_class!=original->native_class||!input_.tree->object_identity(id,identity)||
     identity.scene_id!=input_.sources->tree().identity().scene_id||
     identity.source_sha256!=input_.sources->tree().identity().source_sha256||
     identity.upstream_commit!=input_.sources->tree().identity().upstream_commit)
    return fail(e,"Ready bridge actual same-tree source ObjectDB rejected");
  out=id;e.clear();return true;
}
bool PodunkReadyNativeBridges::emote(uint32_t id,const FieldEmoteInstance &pose,std::string &e){
  const auto *d=input_.sources->emote().record(id);FieldObjectId actualId=0;
  if(!d||pose.id!=id||!actual(id,actualId,e)||!finite(pose.position)||!finite(pose.scale)||
     !finite(pose.offset)||!pose.scale.x||!pose.scale.y)return fail(e,"Emotes source pose rejected");
  FieldCanvasAppearance sprite;if(!input_.native->sprite_snapshot(actualId,sprite,e))return false;
  const auto *asset=input_.native->canvas_data()->texture(sprite.texture);
  std::array<uint8_t,32> textureSha{};
  if(!asset||asset->source!=input_.sources->emote().texture_source()||
     !input_.sources->emote().source_hash(asset->source,textureSha)||asset->source_sha!=textureSha||
     asset->width!=input_.sources->emote().width()||asset->height!=input_.sources->emote().height()||
     sprite.hframes!=d->columns||sprite.vframes!=d->rows||pose.frame>=sprite.hframes*sprite.vframes)
    return fail(e,"Emotes actual native source texture/grid differs");
  const auto *state=input_.tree->state(actualId);
  if(!state->inside||!state->ready_notified||state->ready_first||!pose.ready)
    return fail(e,"Emotes publish outside actual script Ready/entered lifetime");
  auto local=input_.tree->state(actualId)->local;
  // Source Emotes has scale-only bases; reject a new rotation/shear rather than erase it.
  if(local[0].y||local[1].x)return fail(e,"Emotes source scale assignment requires audited axis basis");
  local[0].x=pose.scale.x;local[1].y=pose.scale.y;local[2]=pose.position;
  sprite.offset=pose.offset;sprite.frame=pose.frame;
  return input_.native->sprite_publish(actualId,sprite,e)&&
     input_.tree->set_local(actualId,local,e)&&input_.tree->set_visible(actualId,pose.visible,e);
}
bool PodunkReadyNativeBridges::bush(uint32_t id,const FieldBushInstance &pose,std::string &e){
  const auto *d=input_.sources->bush().record(id);FieldObjectId root=0,spriteId=0;
  if(!d||pose.id!=id||!actual(id,root,e)||!actual(d->sprite_id,spriteId,e))return false;
  FieldCanvasAppearance sprite;if(!input_.native->sprite_snapshot(spriteId,sprite,e))return false;
  const auto *asset=input_.native->canvas_data()->texture(sprite.texture);
  std::array<uint8_t,32> textureSha{};
  if(!asset||asset->source!=input_.sources->bush().texture_source()||
     !input_.sources->bush().source_hash(asset->source,textureSha)||asset->source_sha!=textureSha||
     asset->width!=input_.sources->bush().width()||asset->height!=input_.sources->bush().height()||
     sprite.hframes!=d->columns||sprite.vframes!=d->rows||pose.frame>=sprite.hframes*sprite.vframes)
    return fail(e,"Bush actual native source texture/grid differs");
  const auto *state=input_.tree->state(root);
  if(!state->inside||!state->ready_notified||state->ready_first)
    return fail(e,"Bush publish outside actual script Ready/entered lifetime");
  sprite.frame=pose.frame;
  if(!input_.native->sprite_publish(spriteId,sprite,e)||
     !input_.tree->set_visible(root,pose.visible,e)||
     !input_.tree->set_visible(spriteId,pose.sprite_visible,e))return false;
  for(const auto &shape:std::array<std::pair<uint32_t,bool>,3>{{
       {d->body_shape_id,pose.body_disabled},{d->hit_shape_id,pose.hit_disabled},
       {d->interact_shape_id,pose.interact_disabled}}}){
    FieldObjectId object=0;if(!actual(shape.first,object,e)||
       !input_.native->set_disabled(object,shape.second,e))return false;
  }
  e.clear();return true;
}
bool PodunkReadyNativeBridges::connect(uint32_t id,uint32_t role,
    std::function<bool(const Args&,std::string&)> callback,std::string &e){
  const FieldBushConnection *binding=nullptr;
  for(const auto &c:input_.sources->bush().connections())if(c.node==id&&c.role==role){
    if(binding)return fail(e,"Bush source connection is ambiguous");
    binding=&c;
  }
  FieldObjectId emitter=0,target=0;
  if(!binding||!callback||!actual(binding->emitter,emitter,e)||!actual(id,target,e))return false;
  const auto *state=input_.tree->state(target);
  if(!state->inside||!state->ready_notified||state->ready_first)
    return fail(e,"Bush source connection outside actual Ready cursor");
  auto key=std::make_pair(target,binding->method);
  if(methods_.count(key))return fail(e,"Bush source callback already connected");
  if(!input_.signals->connect(emitter,binding->signal,target,binding->method,
       role==3?uint32_t(FieldSignalPersist):0u,{},e))return false;
  methods_.emplace(std::move(key),std::move(callback));e.clear();return true;
}
bool PodunkReadyNativeBridges::apply(PodunkSceneMechanismOwners &owners,std::string &e){
  if(!input_.sources||applied_)return fail(e,"Ready native bridges missing/duplicate prepare");
  owners.emote.publish=[this](auto id,const auto &s,auto &error){return emote(id,s,error);};
  owners.bush.bind=[this](const FieldBushData &data,auto &error){
    if(&data!=&input_.sources->bush()||!data.valid()||data.capability()!=2)
      return fail(error,"Bush source connection capability unavailable");
    error.clear();return true;
  };
  owners.bush.resolve_node=[this](uint32_t source,bool &present,auto &error){
    FieldObjectId id=0;if(!actual(source,id,error))return false;present=true;return true;
  };
  owners.bush.publish=[this](auto id,const auto &s,auto &error){return bush(id,s,error);};
  owners.bush.connect_viewport=[this](uint32_t id,std::function<bool(bool)> cb,auto &error){
    if(!cb)return fail(error,"Bush actual viewport receiver missing");
    return connect(id,1,[cb](const Args &a,auto &err){if(!a.empty())return fail(err,"Bush screen-entered signature rejected");return cb(true);},error)&&
       connect(id,2,[cb](const Args &a,auto &err){if(!a.empty())return fail(err,"Bush screen-exited signature rejected");return cb(false);},error);
  };
  owners.bush.connect_hitbox=[this](uint32_t id,std::function<bool(uint32_t)> cb,auto &error){
    return connect(id,3,[this,cb](const Args &a,auto &err){
      if(!cb||a.size()!=1||!std::holds_alternative<FieldObjectRef>(a.front()))
        return fail(err,"Bush actual Hitbox signal argument rejected");
      auto object=std::get<FieldObjectRef>(a.front()).id;const auto *d=input_.tree->descriptor(object);
      if(!d||d->native_class!="Area2D"||!input_.registry->object_exists(object)||
         input_.registry->tree_owner(object).get()!=input_.tree)
        return fail(err,"Bush actual entered Area ObjectDB rejected");
      return cb(d->id);
    },error);
  };
  applied_=true;e.clear();return true;
}
bool PodunkReadyNativeBridges::method_owned(const FieldDeferredMessage &m)const{
  return m.kind==FieldDeferredKind::Call&&methods_.count({m.object,m.member});
}
bool PodunkReadyNativeBridges::dispatch(const FieldDeferredMessage &m,std::string &e){
  if(!method_owned(m))return fail(e,"Ready bridge source method is not owned");
  const auto *d=input_.tree->descriptor(m.object);FieldObjectId actualId=0;
  if(!d||!actual(d->id,actualId,e)||actualId!=m.object||!input_.tree->state(actualId)->inside)
    return fail(e,"Ready bridge receiver left actual scene");
  return methods_.at({m.object,m.member})(m.args,e);
}
} // namespace encore::ctr
