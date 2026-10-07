#include "house_return_ui_context.hpp"
#include <algorithm>
#include <cmath>
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
bool HouseReturnUiContext::prepare(HouseReturnUiContextInput in,std::string&e){
  if(prepared_||!in.sources||!in.sources->valid()||!in.tree||!in.house||
      !in.continuation||!in.continuation->initialized()||!in.session||
      !in.player||!in.dialogue)
    return reject(e,"House NPC UI requires the actual retained session and destination owners");
  auto*r=in.continuation->registry();auto*g=in.continuation->global();
  auto*u=in.continuation->ui();const auto*d=in.continuation->ui_continuation_data();
  if(!r||!r->kernel()||!g||!u||!d||!d->valid()||!d->dialogue_continuation()||
      !g->core().data()||!r->object_exists(g->core().owner())||
      g->core().data()->identity().upstream_commit!=in.sources->tree().identity().upstream_commit||
      d->identity().upstream_commit!=in.sources->tree().identity().upstream_commit||
      in.player->registry()!=r||
      (in.tree->object_domain()&&in.tree->object_domain()!=r->kernel()))
    return reject(e,"House NPC UI/Global/Player source pins or ObjectDB owners differ");
  in_=std::move(in);registry_=r;global_=&g->core();ui_=u;
  prepared_=true;e.clear();return true;
}
bool HouseReturnUiContext::domains(std::string&e)const{
  if(!prepared_||!in_.sources->valid()||!in_.continuation->initialized()||
      in_.continuation->registry()!=registry_||in_.continuation->ui()!=ui_||
      !in_.continuation->global()||&in_.continuation->global()->core()!=global_||
      !global_->data()||!registry_->object_exists(global_->owner())||
      in_.player->registry()!=registry_||in_.tree->object_domain()!=registry_->kernel()||
      global_->data()->identity().upstream_commit!=in_.sources->tree().identity().upstream_commit)
    return reject(e,"House NPC UI lost its actual retained source/domain borrowers");
  e.clear();return true;
}
bool HouseReturnUiContext::live_ui(std::string&e,FieldObjectId ready_npc)const{
  if(!domains(e))return false;
  PodunkMickHouseNativeState s;
  if(!in_.session->house_native_state(*in_.house,s,e))return false;
  FieldIdentity identity;
  const auto current=registry_->current_scene();
  if(!current||registry_->tree_owner(current).get()!=in_.tree||
      current!=in_.tree->root()||!in_.tree->object_identity(current,identity)||
      !same(identity,in_.sources->tree().identity())||
      s.registry!=registry_||s.ui!=ui_||!s.retired||
      s.random!=in_.continuation->random()||s.uid_ledger!=in_.continuation->uid_ledger()||
      in_.dialogue->world()!=&in_.house->world||
      in_.house->world.house_programme_owner()!=in_.dialogue||
      (in_.house->house.programme_owner()!=in_.dialogue&&
       (!ready_npc||!in_.npc_source||
        !in_.dialogue->staged_npc_context(*in_.npc_source,ready_npc,e))))
    return reject(e,"House NPC UI is not bound to the actual House/native programme receiver");
  e.clear();return true;
}
bool HouseReturnUiContext::npc(FieldObjectId id,bool entered,std::string&e)const{
  if(!domains(e))return false;
  const auto*s=in_.tree->state(id);const auto*d=in_.tree->descriptor(id);
  FieldIdentity identity;const FieldNpcDescriptor*n=nullptr;
  if(d)for(const auto&v:in_.sources->npcs().npcs())if(v.id==d->id)n=&v;
  const auto*r=d?in_.sources->tree().record(d->id):nullptr;
  bool source_hash=false;
  if(r)for(const auto&v:in_.sources->npcs().sources())
    if(v.path==r->script&&v.sha256==r->script_sha)source_hash=true;
  if(!id||!s||!s->alive||!d||!n||!r||!source_hash||
      !registry_->object_exists(id)||registry_->tree_owner(id).get()!=in_.tree||
      in_.tree->source_object(n->id)!=id||!in_.tree->object_identity(id,identity)||
      !same(identity,in_.sources->tree().identity())||
      r->path!=d->path||r->script_sha!=d->script_sha||r->script!=d->script||
      r->native_class!=d->native_class||n->node!=d->path||n->ready_ordinal!=d->ready||
      (entered&&(!s->inside||!s->bound||!s->ready_notified)))
    return reject(e,"House NPC UI receiver differs from the actual full House source NPC");
  e.clear();return true;
}
bool HouseReturnUiContext::player(FieldObjectId&out,std::string&e,FieldObjectId ready_npc)const{
  if(!domains(e))return false;
  auto&body=in_.player->body();const auto*data=body.data();
  out=body.object();std::shared_ptr<const GlobalLoadObjectArray>party;
  auto player_tree=registry_->tree_owner(out);
  const bool transferred=body.tree()==in_.tree&&in_.player->tree()==in_.tree&&
      player_tree.get()==in_.tree;
  const bool staged=!transferred&&ready_npc&&in_.npc_source&&player_tree&&
      body.tree()==player_tree.get()&&in_.player->tree()==player_tree.get()&&
      in_.dialogue->staged_player(*in_.npc_source,ready_npc,out,*player_tree,e);
  if(!body.constructed()||!data||!data->valid()||!in_.player->ready_complete()||
      data!=in_.continuation->player_initialization()||
      body.registry()!=registry_||(!transferred&&!staged)||
      !registry_->object_exists(out)||!player_tree||
      data->identity().upstream_commit!=in_.sources->tree().identity().upstream_commit||
      !global_->array(FieldGlobalMemberRole::PartyObjects,party,e)||
      !party||party->values.empty()||party->values.front()!=out)
    return reject(e,"House NPC global player is not the retained ready Player source body");
  const auto*s=player_tree->state(out);const auto*d=player_tree->descriptor(out);
  const auto&records=data->recipe().records();FieldIdentity identity;
  if(!s||!s->alive||(!s->inside&&!staged)||!s->bound||!d||records.empty()||
      !player_tree->object_identity(out,identity)||!same(identity,data->identity())||
      d->id!=records.front().id||d->script_sha!=records.front().script_sha||
      d->script!=records.front().script)
    return reject(e,"House NPC player class lost its checked source constructor identity");
  e.clear();return true;
}
bool HouseReturnUiContext::context(FieldObjectId id,FieldNpcContext&out,std::string&e)const{
  if(!npc(id,true,e)||!live_ui(e,id))return false;
  FieldObjectId actual_player=0,talker=0;FieldTransform transform;FieldNpcContext value;
  if(!player(actual_player,e,id))return false;
  const auto player_tree=registry_->tree_owner(actual_player);
  if(!player_tree||!player_tree->world_transform(actual_player,transform,e)||
      !in_.player->motion().source_paused(value.player_paused,e)||
      !ui_->source_is_in_battle(ui_->binding().object,value.in_battle,e)||
      !ui_->source_is_in_cutscene(ui_->binding().object,value.cutscene,e)||
      !ui_->source_stack_empty(ui_->binding().object,value.stack_empty,e)||
      !ui_->source_current_talker(talker,e))return false;
  value.player=transform[2];
  if(!std::isfinite(value.player.x)||!std::isfinite(value.player.y))
    return reject(e,"House NPC player world position is not finite");
  const auto*s=in_.tree->state(id);
  value.current_talker=talker==id;
  value.ancestor_visible=!s->canvas_parent||in_.tree->visible_in_tree(s->canvas_parent);
  value.debug_build=in_.actual_debug_build;
  out=value;e.clear();return true;
}
bool HouseReturnUiContext::party_player(FieldObjectId id,bool&out,std::string&e)const{
  FieldObjectId actual_player=0;if(!player(actual_player,e))return false;
  if(!id||!registry_->object_exists(id))return reject(e,"House NPC near body is not a live ObjectDB object");
  const auto owner=registry_->tree_owner(id);const auto*d=owner?owner->descriptor(id):nullptr;
  const auto*s=owner?owner->state(id):nullptr;FieldIdentity identity;
  const auto*data=in_.player->body().data();
  if(!owner||!s||!s->alive||!d||!owner->object_identity(id,identity))
    return reject(e,"House NPC near class test lacks its actual checked source owner");
  const auto&root=data->recipe().records().front();
  // npc.gd uses `is PartyMemberPlayer`, not PartyObjects membership or a name.
  // Read the same checked constructor identity as the actual Podunk consumer.
  out=id==actual_player||(same(identity,data->identity())&&d->id==root.id&&
      d->script_sha==root.script_sha&&d->script==root.script);
  e.clear();return true;
}
bool HouseReturnUiContext::persistent(FieldObjectId id,bool&out,std::string&e)const{
  if(!npc(id,false,e))return false;
  std::shared_ptr<const GlobalLoadObjectArray>objects;
  if(!global_->array(FieldGlobalMemberRole::Persistent,objects,e))return false;
  if(!objects)return reject(e,"House NPC persistence has no actual Global Array root");
  out=std::find(objects->values.begin(),objects->values.end(),id)!=objects->values.end();
  e.clear();return true;
}
bool HouseReturnUiContext::close_commands(std::string&e){
  if(!live_ui(e))return false;
  bool active=false;
  if(!ui_->source_is_pause_menu_active(ui_->binding().object,active,e))return false;
  if(active)return reject(e,"House NPC close_commands_menu requires the actual open commands close owner");
  // source interact calls close_commands_menu(true,false): keep player pause,
  // keep bars. A Closed menu has no animation/input tail to execute. The actual
  // business guard also rejects PauseClosing/suspended commands and live info.
  if(!ui_->source_business_closed(e))return false;
  const auto*data=in_.continuation->ui_continuation_data();
  if(!data||!data->valid()||!data->dialogue_continuation())
    return reject(e,"House NPC commands close lost the retained checked UI resource");
  // Business schema role 3 is PartyInfo; source member/method come from that
  // loaded policy, and the concrete UI owner checks its actual closed state.
  const auto&widgets=data->business_policy().widgets;
  if(widgets.size()<3||widgets[2].role!=3)
    return reject(e,"House NPC commands close has no reviewed PartyInfo source endpoint");
  return ui_->source_close_closed_widget(widgets[2].role,e);
}
bool HouseReturnUiContext::telepathy(FieldObjectId id,bool,std::string&e)const{
  if(!npc(id,true,e)||!live_ui(e))return false;
  return reject(e,"House NPC telepathy requires an actual House native thoughts/effect owner");
}
bool HouseReturnUiContext::install(HouseReturnNpcPorts&ports,std::string&e){
  if(!prepared_||installed_||ports.context||ports.close_commands||ports.party_player||
      ports.persistent||ports.telepathy)
    return reject(e,"House NPC UI endpoints must be installed once without replacing live callbacks");
  ports.context=[this](auto id,auto&out,auto&e){return context(id,out,e);};
  ports.close_commands=[this](auto&e){return close_commands(e);};
  ports.party_player=[this](auto id,auto&out,auto&e){return party_player(id,out,e);};
  ports.persistent=[this](auto id,auto&out,auto&e){return persistent(id,out,e);};
  ports.telepathy=[this](auto id,bool enabled,auto&e){return telepathy(id,enabled,e);};
  ports.actual_debug_build=in_.actual_debug_build;
  installed_=true;e.clear();return true;
}
} // namespace encore::ctr
