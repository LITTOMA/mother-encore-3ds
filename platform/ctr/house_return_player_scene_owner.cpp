#include "house_return_player_scene_owner.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&
      a.source_sha256==b.source_sha256;
}
}
struct HouseReturnPlayerSceneOwner::Press {
  HouseReturnPlayerSceneOwner&owner;FieldObjectId object;std::string_view method;
  Press(HouseReturnPlayerSceneOwner&o,FieldObjectId id,std::string_view name)
      :owner(o),object(id),method(name){owner.press_=this;}
  ~Press(){owner.press_=nullptr;}
  Press(const Press&)=delete;Press&operator=(const Press&)=delete;
};
bool HouseReturnPlayerSceneOwner::prepare(HouseReturnPlayerSceneInput in,std::string&e){
  if(prepared_||!in.sources||!in.sources->valid()||!in.tree||!in.registry||
      !in.npcs||!in.interact||!in.prompts||!in.player||!in.continuation||
      !in.continuation->initialized()||in.continuation->registry()!=in.registry||
      in.player->registry()!=in.registry||!in.registry->kernel()||
      (in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel()))
    return fail(e,"House Player scene owner requires actual fixed source/native borrows");
  in_=std::move(in);prepared_=true;e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::borrowed(std::string&e)const{
  if(!prepared_||!in_.sources->valid()||!in_.continuation->initialized()||
      in_.continuation->registry()!=in_.registry||in_.registry->poisoned()||
      in_.player->registry()!=in_.registry||
      (in_.tree->object_domain()&&in_.tree->object_domain()!=in_.registry->kernel()))
    return fail(e,"House Player scene service lost its actual Registry/source owner");
  e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::actual(FieldObjectId id,
    const FieldNodeDescriptor*&d,const FieldNodeState*&n,std::string&e)const{
  if(!borrowed(e))return false;
  d=in_.tree->descriptor(id);n=in_.tree->state(id);FieldIdentity identity;
  const auto*r=d?in_.sources->tree().record(d->id):nullptr;
  if(!id||!d||!n||!r||!n->alive||!n->inside||!n->bound||n->queued||
      !n->ready_notified||n->ready_first||!in_.registry->object_exists(id)||
      in_.registry->tree_owner(id).get()!=in_.tree||in_.tree->source_object(d->id)!=id||
      in_.tree->object_domain()!=in_.registry->kernel()||
      !in_.tree->object_identity(id,identity)||!same(identity,in_.sources->tree().identity())||
      r->path!=d->path||r->native_class!=d->native_class||r->script!=d->script||
      r->script_sha!=d->script_sha||r->ready!=d->ready)
    return fail(e,"House Player target lacks the actual full-tree source/native Ready identity");
  e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::current_root(std::string&e)const{
  if(!borrowed(e)||!in_.continuation->global())return false;
  FieldObjectId global=0;const FieldNodeDescriptor*d;const FieldNodeState*n;
  const auto&source=in_.sources->tree();const auto&re=in_.sources->reentry();
  if(!in_.continuation->global()->core().object(FieldGlobalMemberRole::CurrentScene,global,e)||
      global!=in_.tree->root()||global!=in_.registry->current_scene()||
      !actual(global,d,n,e)||source.records().empty()||d->id!=source.records().front().id||
      source.source_scene()!=re.target_scene()||d->native_class!=source.records().front().native_class||
      re.native_nodes().empty()||d->id!=re.native_nodes().front().id||
      d->script!=re.native_nodes().front().script||d->script_sha!=re.native_nodes().front().script_sha)
    return fail(e,"House currentScene is not its actual mapped full House AreaRoom source root");
  e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::live_player(std::string&e)const{
  if(!current_root(e))return false;
  const auto id=in_.player->body().object();const auto*n=in_.tree->state(id);
  const auto*d=in_.tree->descriptor(id);const auto*data=in_.player->body().data();FieldIdentity identity;
  std::shared_ptr<const GlobalLoadObjectArray>party;
  if(!in_.player->ready_complete()||!in_.player->body().constructed()||!data||!data->valid()||
      in_.player->tree()!=in_.tree||in_.player->body().tree()!=in_.tree||
      !n||!n->alive||!n->inside||!n->bound||n->queued||n->ready_first||!d||
      in_.registry->tree_owner(id).get()!=in_.tree||!in_.registry->object_exists(id)||
      !in_.tree->object_identity(id,identity)||!same(identity,data->identity())||
      data->recipe().records().empty()||d->id!=data->recipe().records().front().id||
      d->script!=data->recipe().records().front().script||
      d->script_sha!=data->recipe().records().front().script_sha||
      !in_.continuation->global()->core().array(FieldGlobalMemberRole::PartyObjects,party,e)||
      !party||party->values.empty()||party->values.front()!=id)
    return fail(e,"House Player operation precedes the same retained Player's actual transfer/Enter");
  e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::current_scene_area(bool&area,std::string&e)const{
  if(!current_root(e))return false;
  area=true;e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::respawn_path(std::string&out,std::string&e)const{
  if(!live_player(e))return false;
  const auto&path=in_.sources->tree().source_scene();std::array<uint8_t,32>sha{};
  if(path!=in_.sources->reentry().target_scene()||!in_.sources->tree().source_hash(path,sha)||
      sha!=in_.sources->tree().identity().source_sha256)
    return fail(e,"House respawn path is not the checked actual currentScene source");
  out=path.rfind("res://",0)==0?path:"res://"+path;e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::collider_info(FieldObjectId id,PlayerColliderInfo&out,std::string&e)const{
  if(!live_player(e))return false;
  const FieldNodeDescriptor*d;const FieldNodeState*n;if(!actual(id,d,n,e))return false;
  PlayerColliderInfo value;value.name=n->name;value.parent=n->parent;value.area=d->native_class=="Area2D";
  if(!d->script.empty()){
    if(in_.npcs->owns(id)){
      const FieldNpcDescriptor*row=nullptr;
      for(const auto&v:in_.sources->npcs().npcs())if(v.id==d->id)row=&v;
      if(!row||in_.npcs->tree()!=in_.tree||in_.npcs->registry()!=in_.registry||
          in_.npcs->runtime().data()!=&in_.sources->npcs()||
          !in_.npcs->has_dialog(id,false,value.dialog,e)||
          !in_.npcs->has_dialog(id,true,value.has_thoughts,e))return false;
      value.interact=value.telepathy=value.has_dialog=true;
      value.no_problem_thoughts=row->has(FieldNpcFlag::NoProblemThoughts);
    }else if(in_.interact->owns(id)){
      if(in_.interact->tree()!=in_.tree||in_.interact->runtime().data()!=&in_.sources->interact()||
          !in_.interact->has_thoughts(id,value.has_thoughts,e))return false;
      value.interact=value.telepathy=true;
    }else return fail(e,"House collider script has no complete actual interaction consumer");
  }
  out=std::move(value);e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::turn_player(FieldObjectId id,bool party,std::string&e){
  if(!live_player(e))return false;
  const FieldNodeDescriptor*d;const FieldNodeState*n;if(!actual(id,d,n,e))return false;
  bool x=false,y=false;
  if(in_.npcs->owns(id)){
    const FieldNpcDescriptor*row=nullptr;for(const auto&v:in_.sources->npcs().npcs())if(v.id==d->id)row=&v;
    if(!row)return fail(e,"House player_turn has no actual source NPC record");
    x=row->has(FieldNpcFlag::PlayerTurnX);y=row->has(FieldNpcFlag::PlayerTurnY);
  }else if(in_.interact->owns(id)){
    const auto*row=in_.sources->interact().record(d->id);if(!row)return fail(e,"House player_turn lacks source Interact record");
    x=bool(row->flags&2u);y=bool(row->flags&4u);
  }else return fail(e,"House player_turn source family is unimplemented");
  if(party){std::shared_ptr<const GlobalLoadObjectArray>members;
    if(!in_.continuation->global()->core().array(FieldGlobalMemberRole::PartyObjects,members,e)||
        !members||members->values.size()!=1||members->values.front()!=in_.player->body().object())
      return fail(e,"House party_turn requires real follower owners beyond the singleton Player");}
  if(!x&&!y){e.clear();return true;}
  FieldTransform from,to;
  if(!in_.tree->world_transform(in_.player->body().object(),from,e)||
      !in_.tree->world_transform(id,to,e))return false;
  return in_.player->motion().turn_to({to[2].x-from[2].x,to[2].y-from[2].y},x,y,e);
}
bool HouseReturnPlayerSceneOwner::interact(FieldObjectId id,bool thoughts,std::string&e){
  if(!live_player(e))return false;
  const FieldNodeDescriptor*d;const FieldNodeState*n;if(!actual(id,d,n,e))return false;
  if(in_.npcs->owns(id))return in_.npcs->interact(id,thoughts,e);
  if(in_.interact->owns(id))return thoughts?in_.interact->telepathy(id,e):in_.interact->interact(id,e);
  return fail(e,"House interact/telepathy lacks an actual mapped source receiver");
}
bool HouseReturnPlayerSceneOwner::press_prompt(FieldObjectId id,std::string&e){
  if(press_||!live_player(e))return fail(e,"House Prompt source press is nested or before actual Player transfer");
  const FieldNodeDescriptor*d;const FieldNodeState*n;if(!actual(id,d,n,e))return false;
  HouseReturnInteractPromptState prompt;
  const auto&methods=in_.sources->button_prompts().methods();
  if(methods.empty()||!in_.prompts->owns(id)||in_.prompts->tree()!=in_.tree||
      in_.prompts->registry()!=in_.registry||!in_.prompts->observe(id,prompt,e)||!prompt.ready)
    return fail(e,"House press target is not its actual Ready ButtonPrompt owner");
  Press call(*this,id,methods.front());
  FieldDeferredMessage message;message.object=id;message.kind=FieldDeferredKind::Call;message.member=call.method;
  return in_.prompts->dispatch(message,e);
}
bool HouseReturnPlayerSceneOwner::observe(FieldObjectId id,std::string_view method,
    HouseButtonPromptSourceCall&out,std::string&e)const{
  if(!press_||press_->object!=id||press_->method!=method||!live_player(e))
    return fail(e,"House Prompt caller receipt is outside the real Player scene press invocation");
  HouseButtonPromptSourceCall value;value.tree=in_.tree;value.registry=in_.registry;
  value.caller=in_.player->body().object();value.receiver=id;value.method=std::string(method);value.depth=1;
  out=std::move(value);e.clear();return true;
}
bool HouseReturnPlayerSceneOwner::update_key_indicator(std::string&e){
  if(!current_root(e)||!in_.continuation->ui()||!in_.continuation->characters())return false;
  return in_.continuation->ui()->source_update_key_indicator(
      in_.continuation->characters()->runtime(),in_.sources->reentry().target_region(),e);
}
}
