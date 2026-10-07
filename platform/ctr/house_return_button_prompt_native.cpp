#include "house_return_button_prompt_native.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
struct Call {
 size_t&depth;FieldObjectId&slot;FieldObjectId old;
 Call(size_t&d,FieldObjectId&s,FieldObjectId id):depth(d),slot(s),old(s){++depth;slot=id;}
 ~Call(){slot=old;--depth;}
};
}
struct HouseReturnButtonPromptNative::State {
 struct Root {
  FieldNodeBinding binding{};bool bound=false,entered=false;
  std::function<bool(uint32_t,bool)>nearby;
  std::function<bool()>pause,keys;
 };
 struct Wait final:FieldGlobalNativeReference {
  State*owner=nullptr;FieldGlobalExternalBinding proof{};
  FieldObjectId prompt=0,player=0;
  bool pending=true;
  FieldGlobalExternalBinding binding()const override{return proof;}
  const char*native_class()const override{return proof.source.native_class.c_str();}
  const FieldGlobalRegistry*registry()const override{return owner?owner->in.registry:nullptr;}
  bool checked_source_hash(std::string_view p,std::array<uint8_t,32>&h)const override{
   if(!owner||p!=owner->in.data->wait().source)return false;h=owner->in.data->wait().source_sha;return true;
  }
  bool dispatch(const FieldDeferredMessage&m,std::string&e)override{
   if(!owner||!pending||m.object!=proof.object||m.kind!=FieldDeferredKind::Call||m.member!=owner->in.data->wait().method||m.args.size()!=2||
    !std::holds_alternative<std::string>(m.args[0])||!std::holds_alternative<FieldObjectRef>(m.args[1])||std::get<FieldObjectRef>(m.args[1]).id!=proof.object||
    owner->wait_scope||!owner->in.signals->emitting_to(player,owner->in.data->native().finished_signal(),proof.object,m.member))
    return fail(e,"House Prompt actual FunctionState signature/reentry rejected");
   uint32_t source=0;if(!owner->actual(prompt,source,e))return false;
   const auto*clip=owner->in.data->core().clip(FieldPromptClipRole::Press);
   if(!clip||std::get<std::string>(m.args[0])!=clip->name)return fail(e,"House Prompt waiter resumed by wrong AP/clip");
   pending=false;Call call(owner->depth,owner->source_call,prompt);
   owner->wait_scope=proof.object;
   const bool ok=owner->core.source_press_resume(source,FieldPromptClipRole::Press,e);
   owner->wait_scope=0;if(!ok)owner->poisoned=true;return ok;
  }
 };
 HouseButtonPromptNativeInput in{};
 FieldPromptRuntime core;PodunkPromptNative leaves;
 std::map<FieldObjectId,Root>roots;
 std::map<FieldObjectId,std::shared_ptr<Wait>>waits;
 std::set<FieldObjectId>material_objects;
 bool prepared=false,factory=false,poisoned=false;
 size_t depth=0;FieldObjectId source_call=0,wait_scope=0,retained_player=0;
 bool global_player(FieldObjectId&out,std::string&e)const{
  std::shared_ptr<const GlobalLoadObjectArray>party;
  std::array<uint8_t,32>source{},actual{};
  const auto*data=in.global_owner?in.global_owner->data():nullptr;
  if(!data||!data->valid()||data->identity().upstream_commit!=in.data->identity().upstream_commit||!in.data->source_hash(data->owner_source(),source)||!data->source_hash(data->owner_source(),actual)||source!=actual||in.global_owner->owner()!=in.global||
   !in.global_owner->array(FieldGlobalMemberRole::PartyObjects,party,e)||!party||party->values.empty())
   return fail(e,"House Prompt original global.get_player has no real party array");
  out=party->values.front();
  if(!out||out!=retained_player||out!=in.player->body().object()||!in.registry->object_exists(out)||
   in.registry->tree_owner(out).get()!=in.player->tree())
   return fail(e,"House Prompt original global Player changed actual ObjectID/owner");
  return true;
 }
 bool actual(FieldObjectId object,uint32_t&source,std::string&e)const{
  const auto*n=in.tree?in.tree->state(object):nullptr;const auto*d=in.tree?in.tree->descriptor(object):nullptr;auto i=roots.find(object);FieldIdentity identity;
  if(!prepared||poisoned||!n||!d||!n->alive||i==roots.end()||!in.registry->object_exists(object)||in.registry->poisoned()||in.registry->tree_owner(object).get()!=in.tree||
   in.tree->object_domain()!=in.registry->kernel()||
   !in.tree->object_identity(object,identity)||!same(identity,in.data->identity())||d->script!=in.data->wait().source||d->script_sha!=in.data->wait().source_sha||!in.data->core().record(d->id)||
   i->second.binding.stable_id!=d->id||i->second.binding.script_sha!=d->script_sha)
   return fail(e,"House Prompt actual source/ObjectDB/owner rejected");
  source=d->id;return true;
 }
 bool root(uint32_t source,FieldObjectId&object,std::string&e)const{
  object=in.tree->source_object(source);uint32_t actual_source=0;
  return actual(object,actual_source,e)&&actual_source==source;
 }
 bool player(uint32_t source,FieldObjectId&object,std::string&e)const{
  const auto*r=in.data->native().leaf(source,PromptNativeRole::Player);object=r?in.tree->source_object(r->id):0;
  if(!object||!leaves.owns(object)||!in.registry->object_exists(object)||in.registry->tree_owner(object).get()!=in.tree)return fail(e,"House Prompt real native AnimationPlayer unavailable");return true;
 }
 FieldObjectId emitter(const HouseButtonPromptConnection&c,uint32_t source,std::string&e)const{
  if(c.emitter==1){FieldObjectId object=0;return global_player(object,e)?object:0;}if(c.emitter==2)return in.global;
  FieldObjectId object=0;return player(source,object,e)?object:0;
 }
 bool connect(uint32_t source,std::function<bool(uint32_t,bool)>near,
  std::function<bool()>pause,std::function<bool()>keys,std::string&e){
  FieldObjectId object=0;if(!root(source,object,e)||source_call!=object||!depth||!factory||!near||!pause||!keys)return fail(e,"House Prompt Ready connect outside actual source call");
  auto&r=roots.at(object);if(r.nearby||r.pause||r.keys)return fail(e,"House Prompt source Ready connections duplicated");
  for(const auto&c:in.data->connections())if(c.role<7){
   const auto from=emitter(c,source,e);if(!from)return false;
   std::vector<FieldDeferredValue>binds;if(c.bind<2)binds.push_back(c.bind==1);
   if(!in.signals->connect(from,c.signal,object,c.method,c.flags,std::move(binds),e))return false;
  }
  r.nearby=std::move(near);r.pause=std::move(pause);r.keys=std::move(keys);return true;
 }
 bool reap_waits(std::string&e){
  for(auto i=waits.begin();i!=waits.end();){
   if(i->second->pending){++i;continue;}
   const auto&w=*i->second;bool connected=false;
   if(!in.signals->connected(w.player,in.data->native().finished_signal(),w.proof.object,in.data->wait().method,connected,e))return false;
   if(connected)return fail(e,"House Prompt resumed FunctionState still connected");
   auto id=w.proof.object;i=waits.erase(i);
   if(!in.registry->retire_object(id,e)||!in.signals->release(id,e))return false;
  }
  return true;
 }
 bool disconnect(FieldObjectId object,uint32_t source,std::string&e){
  if(depth||wait_scope)return fail(e,"House Prompt disconnection inside live source callback");
  if(!reap_waits(e))return false;
  auto w=waits.find(object);if(w!=waits.end()){
   if(!in.signals->disconnect(w->second->player,in.data->native().finished_signal(),w->second->proof.object,in.data->wait().method,e))return false;
   auto id=w->second->proof.object;waits.erase(w);
   if(!in.registry->retire_object(id,e)||!in.signals->release(id,e))return false;
  }
  for(const auto&c:in.data->connections())if(c.role<7){
   auto from=emitter(c,source,e);if(!from)return false;bool linked=false;
   if(!in.signals->connected(from,c.signal,object,c.method,linked,e)||(linked&&!in.signals->disconnect(from,c.signal,object,c.method,e)))return false;
  }
  return true;
 }
 bool player_deleting(FieldObjectId ap,std::string&e){
  const auto*n=in.tree->descriptor(ap);const auto*r=n?in.data->native().record(n->id):nullptr;
  if(!r||r->role!=PromptNativeRole::Player||depth||wait_scope)return fail(e,"House Prompt AP deletion inside live source callback");
  auto root=in.tree->source_object(r->prompt);auto w=waits.find(root);
  if(w!=waits.end()){
   bool linked=false;if(!in.signals->connected(ap,in.data->native().finished_signal(),w->second->proof.object,in.data->wait().method,linked,e)||
    (linked&&!in.signals->disconnect(ap,in.data->native().finished_signal(),w->second->proof.object,in.data->wait().method,e)))return false;
   auto id=w->second->proof.object;waits.erase(w);if(!in.registry->retire_object(id,e)||!in.signals->release(id,e))return false;
  }
  const auto*c=in.data->connection(7);bool linked=false;
  return in.signals->connected(ap,c->signal,root,c->method,linked,e)&&(!linked||in.signals->disconnect(ap,c->signal,root,c->method,e));
 }
};
HouseReturnButtonPromptNative::HouseReturnButtonPromptNative():state_(std::make_unique<State>()){}
HouseReturnButtonPromptNative::~HouseReturnButtonPromptNative()=default;
bool HouseReturnButtonPromptNative::prepare(HouseButtonPromptNativeInput in,std::string&e){
 auto&s=*state_;
 if(s.prepared||!in.data||!in.data->valid()||!in.source||!in.source->valid()||!same(in.data->identity(),in.source->identity())||!in.tree||!in.registry||!in.signals||in.signals->registry()!=in.registry||
  !in.global_data||!in.global_owner||!in.player||!in.font||!in.asset_root||!in.interact||!in.materials||(in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel())||in.player->registry()!=in.registry||
  in.font->catalog_sha256()!=in.data->font().sha||!in.registry->data()||in.global!=in.registry->autoload_object(in.registry->data()->global_autoload())||!in.registry->object_exists(in.global))
  return fail(e,"House Prompt real source/tree/global/player/font/material owners missing");
 s.in=in;s.retained_player=in.player->body().object();FieldObjectId player=0;
 if(!s.global_player(player,e))return false;
 if(!s.leaves.prepare(in.data->native(),*in.source,in.data->core(),s.core,*in.tree,*in.registry,*in.signals,*in.global_data,*in.player,in.equipment,*in.font,in.asset_root,e))return false;
 FieldPromptHost h;
 h.connect=[&s](uint32_t id,auto near,auto pause,auto keys,auto&e){return s.connect(id,std::move(near),std::move(pause),std::move(keys),e);};
 h.visibility=[&s](uint32_t id,bool visible,auto&e){FieldObjectId object=0;return s.root(id,object,e)&&s.in.tree->set_visible(object,visible,e);};
 h.hide_signal=[&s](uint32_t id,auto&e){FieldObjectId object=0;return s.root(id,object,e)&&s.in.signals->emit(object,s.in.data->hide_signal(),{},e);};
 h.completion_owner=this;
 if(!s.leaves.apply(h,e)||!s.core.initialize(in.data->core(),std::move(h),e))return false;
 s.prepared=true;e.clear();return true;
}
bool HouseReturnButtonPromptNative::owns(const FieldNodeDescriptor&n)const{
 const auto&s=*state_;return s.prepared&&((s.in.data->core().record(n.id)&&n.script==s.in.data->wait().source&&n.script_sha==s.in.data->wait().source_sha)||s.leaves.owns(n));
}
bool HouseReturnButtonPromptNative::owns(FieldObjectId id)const{return state_->roots.count(id)||state_->leaves.owns(id);}
bool HouseReturnButtonPromptNative::construct(FieldObjectId object,const FieldNodeDescriptor&n,const FieldIdentity&identity,std::string&e){
 auto&s=*state_;if(!owns(n)||s.poisoned||s.factory||!same(identity,s.in.data->identity()))return fail(e,"House Prompt constructor source identity rejected");
 if(s.leaves.owns(n))return s.leaves.construct(object,n,identity,e);
 const auto*actual=s.in.tree->descriptor(object);const auto*live=s.in.tree->state(object);
 if(s.roots.count(object)||!actual||actual->id!=n.id||!live||live->inside||live->ready_notified||!s.in.registry->object_exists(object)||s.in.registry->tree_owner(object).get()!=s.in.tree||!s.core.create(n.id))return fail(e,"House Prompt actual source constructor allocation rejected");
 HouseButtonPromptMaterialState material;std::array<uint8_t,32>source{},shader{};
 if(!s.in.materials->observe(object,material,e)||material.registry!=s.in.registry||material.root!=object||!material.material||!material.owner||material.owner!=s.in.registry->source_resource(material.material)||
  material.owner->binding().object!=material.material||!material.owner->resource_class()||material.owner->binding().source.native_class!=material.owner->resource_class()||std::string(material.owner->resource_class())!="ShaderMaterial"||
  material.source!=s.in.data->material_source()||material.shader!=s.in.data->shader_source()||!s.in.data->source_hash(material.source,source)||source!=material.source_sha||!s.in.data->source_hash(material.shader,shader)||shader!=material.shader_sha||
  material.shader_declaration!=s.in.data->shader_declaration()||material.shader_code_sha!=s.in.data->shader_code_sha256()||material.local_to_scene!=s.in.data->material_local_to_scene()||material.glow!=s.in.data->material_glow()||s.material_objects.count(material.material)||
  material.owner->binding().source.source!=material.source||material.owner->binding().source.source_sha!=material.source_sha||material.owner->binding().source.identity.upstream_commit!=identity.upstream_commit||
  !s.core.assign_source_material(n.id,material.flash,material.flash_modifier,material.glow_modifier,e)){
  s.poisoned=true;return fail(e,"House Prompt actual original Flash Resource/fields missing");
 }
 s.material_objects.insert(material.material);
 State::Root root;root.binding={identity,n.id,n.class_index,0x454e0080,1,n.script_sha,n.native_class};s.roots.emplace(object,std::move(root));e.clear();return true;
}
bool HouseReturnButtonPromptNative::bind(FieldObjectId object,FieldNodeBinding&out,std::string&e){
 auto&s=*state_;if(s.leaves.owns(object))return s.leaves.bind(object,out,e);uint32_t source=0;if(!s.actual(object,source,e)||s.roots.at(object).bound)return fail(e,"House Prompt actual source bind rejected");
 out=s.roots.at(object).binding;s.roots.at(object).bound=true;e.clear();return true;
}
bool HouseReturnButtonPromptNative::finish_factory(std::string&e){
 auto&s=*state_;if(!s.prepared||s.factory||s.poisoned||s.depth||s.roots.size()!=s.in.data->core().records().size()||!s.leaves.finish_factory(e))return fail(e,"House Prompt complete actual factory missing");
 const auto*c=s.in.data->connection(7);
 for(const auto&r:s.in.data->core().records()){
  FieldObjectId object=0,ap=0;if(!s.root(r.id,object,e)||!s.player(r.id,ap,e))return false;const auto*n=s.in.tree->state(object);
  if(!n||n->inside||n->ready_notified||!n->bound||!s.roots.at(object).bound||!s.core.instance(r.id)||s.core.instance(r.id)->ready||!s.core.instance(r.id)->material_assigned)
   return fail(e,"House Prompt factory completion must precede actual Enter/Ready");
  if(!s.in.signals->connect(ap,c->signal,object,c->method,c->flags,{},e))return false;
 }
 s.factory=true;e.clear();return true;
}
bool HouseReturnButtonPromptNative::phase(FieldObjectId object,FieldTreePhase phase,float dt,bool paused,bool pending,std::string&e){
 auto&s=*state_;if(s.leaves.owns(object)){
  const auto*n=s.in.tree->descriptor(object);const auto*r=n?s.in.data->native().record(n->id):nullptr;
  if(phase==FieldTreePhase::Deleting&&r&&r->role==PromptNativeRole::Player&&!s.player_deleting(object,e))return false;
  if(!s.leaves.phase(object,phase,dt,paused,pending,e))return false;
  return phase!=FieldTreePhase::IdleInternal||s.reap_waits(e);
 }
 uint32_t source=0;if(!s.actual(object,source,e))return false;auto&r=s.roots.at(object);const auto*n=s.in.tree->state(object);
 if(!s.factory||!r.bound||!n->bound)return fail(e,"House Prompt lifecycle before actual source factory/binding");
 if(phase==FieldTreePhase::EnterScript){if(r.entered||!n->inside)return fail(e,"House Prompt actual source Enter rejected");r.entered=true;}
 else if(phase==FieldTreePhase::ReadyScript){
  if(!r.entered||!n->inside||!n->ready_notified||s.core.instance(source)->ready)return fail(e,"House Prompt actual source Ready cursor rejected");
  Call call(s.depth,s.source_call,object);if(!s.core.ready(source)){e=s.core.error();s.poisoned=true;return false;}
 }else if(phase==FieldTreePhase::ExitScript){if(!r.entered||!n->inside)return fail(e,"House Prompt actual source Exit rejected");r.entered=false;}
 else return fail(e,"House Prompt unsupported source lifecycle phase");
 e.clear();return true;
}
bool HouseReturnButtonPromptNative::handles_method(const FieldDeferredMessage&m)const{
 const auto&s=*state_;if(!s.roots.count(m.object))return false;
 if(m.kind==FieldDeferredKind::Set)return m.member==s.in.data->offset_member()||m.member==s.in.data->enabled_member();
 if(m.kind!=FieldDeferredKind::Call)return false;
 return std::any_of(s.in.data->connections().begin(),s.in.data->connections().end(),[&](const auto&c){return c.method==m.member;})||
  std::find(s.in.data->methods().begin(),s.in.data->methods().end(),m.member)!=s.in.data->methods().end();
}
bool HouseReturnButtonPromptNative::dispatch(const FieldDeferredMessage&m,std::string&e){
 auto&s=*state_;uint32_t source=0;if(!handles_method(m)||!s.actual(m.object,source,e))return fail(e,"House Prompt source method actual receiver rejected");
 if(m.kind==FieldDeferredKind::Set){
  HouseButtonPromptSourceCall receipt;
  if(m.args.size()!=1||!s.in.source_calls||!s.in.source_calls->observe(m.object,m.member,receipt,e)||receipt.tree!=s.in.tree||receipt.registry!=s.in.registry||receipt.receiver!=m.object||!receipt.caller||!receipt.depth||receipt.method!=m.member||
   !s.in.registry->object_exists(receipt.caller)||s.in.registry->tree_owner(receipt.caller).get()!=s.in.tree||receipt.caller!=s.in.tree->state(m.object)->parent)
   return fail(e,"House Prompt exported field assignment lacks actual parent source lease");
  if(m.member==s.in.data->offset_member()){
   if(!std::holds_alternative<Vec2>(m.args[0]))return fail(e,"House Prompt source offset field type rejected");
   return s.core.assign_offset(source,std::get<Vec2>(m.args[0]),e);
  }
  if(!std::holds_alternative<bool>(m.args[0])||!s.core.instance(source)->ready)return fail(e,"House Prompt source enabled field type/lifecycle rejected");
  if(!s.core.assign_enabled(source,std::get<bool>(m.args[0]))){e=s.core.error();return false;}e.clear();return true;
 }
 if(!s.factory||!s.core.instance(source)->ready)return fail(e,"House Prompt source method before actual Ready");
 if(s.source_call&&s.source_call!=m.object)return fail(e,"House Prompt nested source call crossed receiver");
 const auto*clip=s.in.data->core().clip(FieldPromptClipRole::Show);const auto*press=s.in.data->core().clip(FieldPromptClipRole::Press);
 for(const auto&c:s.in.data->connections())if(c.method==m.member){
  const auto from=s.emitter(c,source,e);if(!from||!s.in.signals->emitting_to(from,c.signal,m.object,c.method))continue;
  Call call(s.depth,s.source_call,m.object);bool ok=false;
  if(c.role==7){
   if(m.args.size()!=1||!std::holds_alternative<std::string>(m.args[0]))return fail(e,"House Prompt source finished arguments rejected");
   FieldPromptClipRole role{};for(uint32_t i=1;i<=5;++i){const auto*v=s.in.data->core().clip(FieldPromptClipRole(i));if(v&&v->name==std::get<std::string>(m.args[0]))role=v->role;}
   if(!clip||!press||uint32_t(role)==0)return fail(e,"House Prompt source finished unknown clip");
   ok=s.core.source_animation_finished(source,role,e);
  }else if(c.role<=2){
   if(m.args.size()!=2||!std::holds_alternative<FieldObjectRef>(m.args[0])||!std::holds_alternative<bool>(m.args[1])||std::get<bool>(m.args[1])!=(c.bind==1)||!s.roots.at(m.object).nearby)return fail(e,"House Prompt actual nearby signal/bool bind rejected");
   auto other=std::get<FieldObjectRef>(m.args[0]).id;if(!s.in.registry->object_exists(other))return fail(e,"House Prompt actual nearby object absent");
   ok=s.roots.at(m.object).nearby(other==s.in.tree->state(m.object)->parent?s.in.data->core().record(source)->parent_id:0,c.bind==1);
  }else if(c.role<=4){if(!m.args.empty()||!s.roots.at(m.object).keys)return fail(e,"House Prompt key callback signature rejected");ok=s.roots.at(m.object).keys();}
  else{if(!m.args.empty()||!s.roots.at(m.object).pause)return fail(e,"House Prompt pause callback signature rejected");ok=s.roots.at(m.object).pause();}
  if(!ok){if(e.empty())e=s.core.error();s.poisoned=true;return false;}e.clear();return true;
 }
 auto method=std::find(s.in.data->methods().begin(),s.in.data->methods().end(),m.member);
 if(method==s.in.data->methods().end()||!s.in.source_calls)return fail(e,"House Prompt source invocation has no actual caller owner");
 HouseButtonPromptSourceCall receipt;
 if(!s.in.source_calls->observe(m.object,m.member,receipt,e)||receipt.tree!=s.in.tree||receipt.registry!=s.in.registry||receipt.receiver!=m.object||!receipt.caller||!receipt.depth||receipt.method!=m.member||
  !s.in.registry->object_exists(receipt.caller)||s.in.registry->tree_owner(receipt.caller).get()!=s.in.tree)
  return fail(e,"House Prompt actual source caller lease rejected");
 const auto role=static_cast<size_t>(method-s.in.data->methods().begin());bool quick=false,value=false;
 if(role==0){if(!m.args.empty())return fail(e,"House Prompt press arguments rejected");}
 else if(role==1){if(m.args.empty()||m.args.size()>2||!std::holds_alternative<bool>(m.args[0])||(m.args.size()==2&&!std::holds_alternative<bool>(m.args[1])))return fail(e,"House Prompt set_enabled arguments rejected");value=std::get<bool>(m.args[0]);quick=m.args.size()==2&&std::get<bool>(m.args[1]);}
 else{if(m.args.size()>1||(!m.args.empty()&&!std::holds_alternative<bool>(m.args[0])))return fail(e,"House Prompt force arguments rejected");quick=!m.args.empty()&&std::get<bool>(m.args[0]);}
 Call call(s.depth,s.source_call,m.object);
 const bool ok=role==0?s.core.press(source):role==1?s.core.set_enabled(source,value,quick):s.core.force(source,role==2?1:role==3?-1:0,quick);
 if(!ok){e=s.core.error();s.poisoned=true;return false;}e.clear();return true;
}
bool HouseReturnButtonPromptNative::declaration(FieldObjectId object,std::string_view name,uint32_t&arity,std::string&e)const{
 auto&s=*state_;if(s.leaves.owns(object))return s.leaves.declaration(object,name,arity,e);uint32_t source=0;
 if(!s.actual(object,source,e)||name!=s.in.data->hide_signal())return fail(e,"House Prompt unknown source signal declaration");arity=0;e.clear();return true;
}
bool HouseReturnButtonPromptNative::release_deleted(FieldObjectId object,std::string&e){
 auto&s=*state_;uint32_t source=0;if(!s.actual(object,source,e)||s.roots.at(object).entered||s.depth)return fail(e,"House Prompt release before actual source Exit/closed frame");
 if(!s.disconnect(object,source,e))return false;
 if(!s.core.destroy(source)){e=s.core.error();return false;}s.roots.erase(object);e.clear();return true;
}
const FieldNodeTreeRuntime*HouseReturnButtonPromptNative::tree()const{return state_->in.tree;}
const FieldGlobalRegistry*HouseReturnButtonPromptNative::registry()const{return state_->in.registry;}
bool HouseReturnButtonPromptNative::observe(FieldObjectId object,HouseReturnInteractPromptState&out,std::string&e)const{
 const auto&s=*state_;uint32_t source=0;if(!s.actual(object,source,e))return false;const auto*n=s.in.tree->state(object);const auto*p=s.core.instance(source);const auto*d=s.in.data->core().record(source);
 const auto*parent=n?s.in.tree->descriptor(n->parent):nullptr;
 if(!n||!p||!parent||parent->id!=d->parent_id)return fail(e,"House Prompt actual source parent not yet assigned");
 out={};out.tree=s.in.tree;out.registry=s.in.registry;out.object=object;out.parent=n->parent;out.source_id=source;out.offset=p->offset;out.constructed=true;out.ready=p->ready;e.clear();return true;
}
bool HouseReturnButtonPromptNative::source_set_offset(FieldObjectId parent,FieldObjectId object,Vec2 value,std::string&e){
 auto&s=*state_;HouseReturnInteractPromptState before;
 if(!observe(object,before,e)||before.parent!=parent||before.ready||s.factory||!s.in.interact||!s.in.interact->source_offset_live(parent,object,value,e))return fail(e,"House Prompt offset write lacks actual InteractDialog setget lease");
 return s.core.assign_offset(before.source_id,value,e);
}
bool HouseReturnButtonPromptNative::native_control_snapshot(FieldObjectId object,HouseButtonPromptControlState&out,std::string&e)const{
 const auto&s=*state_;PromptNativeControlState actual;if(!s.leaves.control_state(object,actual,e)||actual.tree!=s.in.tree||actual.registry!=s.in.registry)return false;
 const auto*n=s.in.tree->state(object);const auto*d=s.in.data->native().record(actual.source_id);const auto*parent=n?s.in.tree->descriptor(n->parent):nullptr;
 if(!n||!d||!parent||parent->id!=d->parent||actual.binding.stable_id!=d->id)return fail(e,"House Prompt native Control actual parent/source binding rejected");
 out={};out.tree=actual.tree;out.registry=actual.registry;out.object=object;out.parent=n->parent;out.source_id=actual.source_id;out.source_parent=d->parent;out.binding=actual.binding;
 out.position=actual.position;out.size=actual.size;out.text=actual.text;out.constructed=true;out.entered=actual.entered;out.ready=actual.ready;e.clear();return true;
}
PodunkPromptNative&HouseReturnButtonPromptNative::native(){return state_->leaves;}
const PodunkPromptNative&HouseReturnButtonPromptNative::native()const{return state_->leaves;}
const FieldPromptRuntime&HouseReturnButtonPromptNative::runtime()const{return state_->core;}
bool HouseReturnButtonPromptNative::source_frame_closed()const{return !state_->depth&&!state_->source_call&&!state_->wait_scope;}
size_t HouseReturnButtonPromptNative::pending_press_waits()const{
 size_t result=0;for(const auto&w:state_->waits)result+=w.second->pending;return result;
}
bool HouseReturnButtonPromptNative::wait_state(FieldObjectId prompt,
 HouseButtonPromptWaitState&out,std::string&e)const{
 const auto&s=*state_;uint32_t source=0;if(!s.actual(prompt,source,e))return false;auto i=s.waits.find(prompt);
 if(i==s.waits.end()||s.in.registry->native_reference(i->second->proof.object).get()!=i->second.get())return fail(e,"House Prompt has no actual live FunctionState owner");
 out={};out.object=i->second->proof.object;out.prompt=i->second->prompt;out.player=i->second->player;out.binding=i->second->proof;out.pending=i->second->pending;e.clear();return true;
}
bool HouseReturnButtonPromptNative::arm_press(FieldPromptRuntime&core,uint32_t source,std::string&e){
 auto&s=*state_;FieldObjectId object=0,ap=0;
 if(&core!=&s.core||!s.root(source,object,e)||!s.player(source,ap,e)||s.source_call!=object||!s.depth||!s.core.instance(source)->pressing||s.core.instance(source)->clip!=FieldPromptClipRole::Press||!s.reap_waits(e)||s.waits.count(object))return fail(e,"House Prompt Press has no actual caller/new waiter lease");
 auto wait=std::make_shared<State::Wait>();wait->owner=&s;wait->prompt=object;wait->player=ap;
 FieldObjectId id=0;if(!s.in.registry->allocate_object(id,e))return false;auto&b=wait->proof;b.object=id;b.family=0x454e0068;b.capability=1;
 auto&spec=b.source;const auto&w=s.in.data->wait();spec.identity=s.in.data->identity();spec.identity.source_sha256=w.source_sha;spec.stable_id=w.id;spec.role=5;spec.source=spec.script=w.source;spec.source_sha=spec.script_sha=w.source_sha;spec.native_class=w.native_class;
 if(!s.in.registry->publish_native_reference(spec,id,wait,e)){std::string ignored;s.in.registry->retire_object(id,ignored);return false;}
 if(!s.in.signals->connect(ap,s.in.data->native().finished_signal(),id,w.method,w.flags,{FieldObjectRef{id}},e)){wait.reset();std::string ignored;s.in.registry->retire_object(id,ignored);return false;}
 s.waits.emplace(object,std::move(wait));e.clear();return true;
}
bool HouseReturnButtonPromptNative::source_completion_live(const FieldPromptRuntime&core,uint32_t source,FieldPromptClipRole clip,bool resume,std::string&e)const{
 const auto&s=*state_;FieldObjectId object=0,ap=0;
 if(&core!=&s.core||!s.root(source,object,e)||!s.player(source,ap,e)||s.source_call!=object||!s.depth)return fail(e,"House Prompt completion outside actual source owner");
 if(!resume){const auto*c=s.in.data->connection(7);if(s.wait_scope||!c||!s.in.signals->emitting_to(ap,c->signal,object,c->method))return fail(e,"House Prompt persistent finished callback receiver/AP rejected");}
 else{auto i=s.waits.find(object);if(clip!=FieldPromptClipRole::Press||i==s.waits.end()||i->second->pending||s.wait_scope!=i->second->proof.object||i->second->player!=ap||s.in.registry->native_reference(s.wait_scope).get()!=i->second.get()||!s.in.signals->emitting_to(ap,s.in.data->native().finished_signal(),s.wait_scope,s.in.data->wait().method))return fail(e,"House Prompt actual FunctionState resume proof rejected");}
 e.clear();return true;
}
} // namespace encore::ctr
