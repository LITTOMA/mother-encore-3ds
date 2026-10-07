#include "house_return_interact_dialog.hpp"
#include <algorithm>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
struct Call {
  size_t&depth;FieldObjectId&receiver;FieldObjectId previous;
  Call(size_t&d,FieldObjectId&r,FieldObjectId id):depth(d),receiver(r),previous(r){++depth;receiver=id;}
  ~Call(){receiver=previous;--depth;}
};
struct Constructor {bool&live;explicit Constructor(bool&v):live(v){live=true;}~Constructor(){live=false;}};
}
bool HouseReturnInteractDialog::admit_source(const FieldInteractData&d,
    const HouseReturnSources&s,std::string&e){
  const auto&t=s.tree();const auto identity=t.identity();std::array<uint8_t,32>sha;
  if(!s.valid()||!d.valid()||d.scene()!=t.source_scene()||d.scene_id()!=identity.scene_id||
      d.source_pin()!=identity.upstream_commit||d.script()!="Scripts/Main/Interact Dialog.gd"||
      !d.source_hash(d.scene(),sha)||sha!=identity.source_sha256)
    return reject(e,"House InteractDialog data lacks the actual full House identity");
  size_t count=0;
  for(const auto&n:t.records())if(n.script==d.script()){
    ++count;const auto*r=d.record(n.id);const auto*p=r?t.record(r->prompt):nullptr;
    if(!r||r->node!=n.path||r->ready!=n.ready||n.native_class!="Area2D"||n.script_methods!=1||
        !(n.flags&1)||bool(n.flags&2)!=bool(r->flags&8)||
        !d.source_hash(n.script,sha)||sha!=n.script_sha||!p||p->parent!=n.id||
        p->path!=n.path+"/ButtonPrompt"||p->script!="Scripts/UI/Button Prompt.gd"||
        p->native_class!="Node2D"||p->ready>=n.ready||
        !d.source_hash(p->script,sha)||sha!=p->script_sha)
      return reject(e,"House InteractDialog source Area/Ready/onready Prompt differs");
  }
  if(!count||count!=d.records().size())return reject(e,"House InteractDialog lacks its exact scene instance closure");
  for(const auto&n:t.records())if(!n.script.empty()){
    const auto path=n.script.substr(0,n.script.find("::"));std::array<uint8_t,32>original;
    if(!t.source_hash(path,original)||!d.source_hash(path,sha)||original!=sha)
      return reject(e,"House InteractDialog full original script closure differs");
  }
  e.clear();return true;
}
bool HouseReturnInteractDialog::load(const uint8_t*p,size_t n,const HouseReturnSources&s,
    FieldInteractData&out,std::string&e){
  FieldInteractData candidate;
  if(!candidate.load(p,n,e)||!admit_source(candidate,s,e))return false;
  out=std::move(candidate);e.clear();return true;
}
bool HouseReturnInteractDialog::prepare(HouseReturnInteractInput in,std::string&e){
  if(prepared_||!in.sources||!in.data||!in.doors||!in.tree||!in.registry||!in.signals||
      !in.geometry||!in.global||!in.global_data||!in.house||!in.text.valid()||
      !in.dialogue||!in.prompts||!in.programmes)
    return reject(e,"House InteractDialog requires its real Area/Prompt/global/House programme owners");
  if(!admit_source(*in.data,*in.sources,e))return false;
  if(!in.registry->kernel()||in.registry->poisoned()||in.signals->registry()!=in.registry||
      (in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel())||
      in.prompts->tree()!=in.tree||in.prompts->registry()!=in.registry||
      in.programmes->tree()!=in.tree||in.programmes->registry()!=in.registry||
      in.programmes->house()!=in.house||!in.registry->object_exists(in.global->owner())||
      !in.registry->object_exists(in.global_data->runtime().globaldata_object()))
    return reject(e,"House InteractDialog borrowed different actual native/source owners");
  in_=in;FieldInteractHost h;
  h.admit_ready=[this](const auto&r,auto&e){
    FieldObjectId id=0;if(!source(r.id,id,e))return false;
    const auto*n=in_.tree->state(id);const auto i=instances_.find(id);
    const auto*p=in_.tree->state(in_.tree->source_object(r.prompt));HouseReturnInteractPromptState prompt;
    if(!factory_||in_.data->record(r.id)!=&r||!n||!n->inside||!n->bound||!n->ready_notified||
        i==instances_.end()||!i->second.bound||!i->second.entered||!p||!p->ready_notified||
        !in_.prompts->observe(p->object,prompt,e)||!prompt.constructed||!prompt.ready||
        prompt.object!=p->object||prompt.parent!=id||prompt.source_id!=r.prompt||
        prompt.tree!=in_.tree||prompt.registry!=in_.registry||
        ((r.flags&16)&&!same(prompt.offset,r.button_offset)))
      return reject(e,"House InteractDialog Ready lacks the actual source/Prompt notification");
    e.clear();return true;
  };
  h.read_flag=[this](std::string_view name,bool&value,auto&e){
    if(!borrows(e))return false;bool present=false;
    return in_.global_data->flags().read(false,name,present,value,e);
  };
  h.visible=[this](uint32_t stable,bool value,auto&e){FieldObjectId id=0;return source(stable,id,e)&&in_.tree->set_visible(id,value,e);};
  h.queue_free=[this](uint32_t stable,auto&e){FieldObjectId id=0;return source(stable,id,e)&&in_.tree->queue_free(id,e);};
  h.apply_serialized_offset=[this](uint32_t prompt,Vec2 value,auto&e){return offset(prompt,value,e);};
  h.connect_flags=[this](uint32_t stable,std::function<bool(std::string&)>cb,auto&e){
    FieldObjectId id=0;if(!cb||!source(stable,id,e))return reject(e,"House InteractDialog source flags receiver unavailable");
    auto&i=instances_.at(id);bool connected=false;
    if(i.flags||!in_.signals->connected(in_.global->owner(),"flags_updated",id,"_check_flags",connected,e))return false;
    if(connected)return reject(e,"House InteractDialog flags source connection was duplicated before Ready");
    if(!in_.signals->connect(in_.global->owner(),"flags_updated",id,"_check_flags",0,{},e))return false;
    i.flags=std::move(cb);e.clear();return true;
  };
  h.admit_programme=[this](std::string_view path,auto&e){
    HouseReturnInteractProgramme r;return programme(path,r,e)&&in_.programmes->admit(r,e);
  };
  h.open_programme=[this](uint32_t stable,std::string_view path,auto&e){
    HouseReturnInteractProgramme r;if(!programme(path,r,e)||r.source_id!=stable)
      return reject(e,"House InteractDialog programme caller changed before source open");
    // The target's typed owner calls HouseRuntime, which alone owns the lease.
    const auto old_generation=in_.house->world.story_generation();
    if(!in_.programmes->request(r,e))return false;
    HouseProgrammeState after;
    if(!in_.house->house.observe_programme_owner(after,e)||
        after.world!=&in_.house->world||after.runtime!=&in_.house->house||
        after.world_owner!=in_.dialogue||after.printer!=&in_.house->presentation||
        after.phase!=HouseProgrammePhase::WaitingReady||after.native_closed||
        after.programme!=r.programme||after.original_npc!=kRoomNoIndex||
        !after.request_generation||after.request_generation==after.vm_generation||
        after.vm_generation!=old_generation)
      return reject(e,"House InteractDialog request did not create the actual same-owner native Ready wait/House lease");
    e.clear();return true;
  };
  h.telepathy_effect=[](bool,auto&e){return reject(e,"House InteractDialog telepathy requires an actual native thoughts owner");};
  if(!runtime_.initialize(*in.data,std::move(h),e))return false;
  prepared_=true;e.clear();return true;
}
bool HouseReturnInteractDialog::borrows(std::string&e)const{
  if(!prepared_||poisoned_||runtime_.data()!=in_.data||!in_.sources->valid()||
      in_.registry->poisoned()||in_.signals->registry()!=in_.registry||
      in_.tree->object_domain()!=in_.registry->kernel()||
      in_.global_data->runtime().registry()!=in_.registry||
      !in_.registry->object_exists(in_.global->owner())||
      !in_.registry->object_exists(in_.global_data->runtime().globaldata_object())||
      in_.prompts->tree()!=in_.tree||in_.prompts->registry()!=in_.registry)
    return reject(e,"House InteractDialog lost its actual source/ObjectDB/global/Prompt borrowers");
  const auto*global_data=in_.global_data->runtime().data();
  const auto*global=in_.global->data();std::array<uint8_t,32>expected,actual;
  FieldGlobalDataMemberState flags;
  if(!global||!global_data||!global_data->valid()||
      global_data->identity().upstream_commit!=in_.data->source_pin()||
      global->identity().upstream_commit!=in_.data->source_pin()||
      !in_.data->source_hash(global_data->owner_source(),expected)||
      !global_data->source_hash(global_data->owner_source(),actual)||actual!=expected||
      !in_.global_data->runtime().read_global_member("flags",flags,e)||
      flags.flags!=&in_.global_data->flags())
    return reject(e,"House InteractDialog does not borrow the actual original globalData.flags Dictionary");
  e.clear();return true;
}
bool HouseReturnInteractDialog::owns(const FieldNodeDescriptor&d)const{
  const auto*r=in_.data?in_.data->record(d.id):nullptr;
  const auto*n=in_.sources?in_.sources->tree().record(d.id):nullptr;
  return r&&n&&r->node==d.path&&n->script==d.script&&n->script_sha==d.script_sha&&n->native_class==d.native_class;
}
bool HouseReturnInteractDialog::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnInteractDialog::actual(FieldObjectId id,uint32_t&stable,std::string&e)const{
  if(!borrows(e))return false;
  const auto i=instances_.find(id);const auto*n=in_.tree->state(id);const auto*d=in_.tree->descriptor(id);FieldIdentity identity;
  if(i==instances_.end()||!n||!n->alive||!d||!owns(*d)||d->id!=i->second.source||
      !in_.geometry->owns(id)||!in_.registry->object_exists(id)||
      in_.registry->tree_owner(id).get()!=in_.tree||in_.tree->source_object(d->id)!=id||
      !in_.tree->object_identity(id,identity)||!same(identity,in_.sources->tree().identity()))
    return reject(e,"House InteractDialog receiver is not its actual source/native Area");
  stable=d->id;e.clear();return true;
}
bool HouseReturnInteractDialog::source(uint32_t stable,FieldObjectId&out,std::string&e)const{
  out=in_.tree->source_object(stable);uint32_t check=0;
  if(!out||!actual(out,check,e)||check!=stable)return reject(e,"House InteractDialog source receiver unavailable");
  return true;
}
bool HouseReturnInteractDialog::construct(FieldObjectId id,const FieldNodeDescriptor&d,
    const FieldIdentity&identity,std::string&e){
  if(!borrows(e))return false;const auto*n=in_.tree->state(id);FieldObjectId actual_area=0;
  if(!owns(d)||!n||!n->alive||n->inside||n->parent||n->bound||n->ready_notified||
      instances_.count(id)||!same(identity,in_.sources->tree().identity())||
      !in_.geometry->owns(id)||!in_.geometry->source_object(d.id,actual_area,e)||actual_area!=id||
      !in_.registry->object_exists(id)||in_.registry->tree_owner(id).get()!=in_.tree||
      in_.tree->source_object(d.id)!=id)
    return reject(e,"House InteractDialog constructor requires its already allocated native Area");
  if(!runtime_.instantiate(d.id,true)){e=runtime_.error();poisoned_=true;return false;}
  instances_.emplace(id,Instance{d.id,false,false,{}});e.clear();return true;
}
bool HouseReturnInteractDialog::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;const auto*d=in_.sources->tree().record(stable);auto&i=instances_.at(id);
  if(i.bound||!d||b.stable_id!=stable||b.class_index!=d->class_index||b.native_class!=d->native_class||
      b.script_sha!=d->script_sha||!same(b.identity,in_.sources->tree().identity()))
    return reject(e,"House InteractDialog combined native/source binding differs");
  i.bound=true;e.clear();return true;
}
bool HouseReturnInteractDialog::offset(uint32_t prompt,Vec2 value,std::string&e){
  if(!borrows(e))return false;const FieldInteractDescriptor*r=nullptr;
  for(const auto&d:in_.data->records())if(d.prompt==prompt){if(r)return reject(e,"House InteractDialog serialized Prompt is ambiguous");r=&d;}
  FieldObjectId parent=0;const auto p=in_.tree->source_object(prompt);
  if(!r||!(r->flags&16)||!same(value,r->button_offset)||!source(r->id,parent,e)||source_call_!=parent||!p)
    return reject(e,"House InteractDialog offset is outside its actual source constructor");
  const auto*n=in_.tree->state(p);HouseReturnInteractPromptState before,after;
  if(!source_offset_live(parent,p,value,e))return false;
  if(!n||n->parent!=parent||n->inside||n->ready_notified||
      !in_.prompts->observe(p,before,e)||before.tree!=in_.tree||before.registry!=in_.registry||
      before.object!=p||before.parent!=parent||before.source_id!=prompt||!before.constructed||before.ready)
    return reject(e,"House InteractDialog source setter requires its constructed unready child Prompt");
  if(!in_.prompts->source_set_offset(parent,p,value,e)||!in_.prompts->observe(p,after,e))return false;
  if(after.tree!=before.tree||after.registry!=before.registry||after.object!=p||after.parent!=parent||
      after.source_id!=prompt||!after.constructed||after.ready||!same(after.offset,value))
    return reject(e,"House InteractDialog source Prompt field setter did not commit to the same receiver");
  e.clear();return true;
}
bool HouseReturnInteractDialog::source_offset_live(FieldObjectId parent,FieldObjectId prompt,
    Vec2 value,std::string&e)const{
  uint32_t stable=0;if(!actual(parent,stable,e))return false;
  const auto*r=in_.data->record(stable);const auto*s=runtime_.state(stable);
  const auto*n=in_.tree->state(prompt);const auto*d=in_.tree->descriptor(prompt);
  if(factory_||!constructor_call_||source_call_!=parent||!callback_depth_||
      !r||!(r->flags&16)||!s||!s->instantiated||s->ready||s->deleted||
      !n||!n->alive||n->inside||n->ready_notified||n->parent!=parent||
      !d||d->id!=r->prompt||d->path!=r->node+"/ButtonPrompt"||
      in_.tree->source_object(r->prompt)!=prompt||!same(value,r->button_offset)||
      in_.registry->tree_owner(prompt).get()!=in_.tree||!in_.registry->object_exists(prompt))
    return reject(e,"House InteractDialog Prompt offset write lacks its actual source setget caller lease");
  e.clear();return true;
}
bool HouseReturnInteractDialog::finish_factory(std::string&e){
  if(!borrows(e))return false;
  if(factory_||callback_depth_||instances_.size()!=in_.data->records().size())
    return reject(e,"House InteractDialog actual source factory incomplete/repeated");
  for(const auto&r:in_.data->records()){
    FieldObjectId id=0;if(!source(r.id,id,e))return false;const auto*n=in_.tree->state(id);
    if(!n||n->inside||n->ready_notified||!instances_.at(id).bound)
      return reject(e,"House InteractDialog source constructor completion must precede Enter/Ready");
    Call call(callback_depth_,source_call_,id);
    Constructor constructor(constructor_call_);
    if(!runtime_.complete_source_constructor(r.id)){e=runtime_.error();poisoned_=true;return false;}
  }
  factory_=true;e.clear();return true;
}
bool HouseReturnInteractDialog::invoke(FieldObjectId id,const std::function<bool()>&call,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  if(source_call_&&source_call_!=id)return reject(e,"House InteractDialog nested call crossed actual source receivers");
  Call guard(callback_depth_,source_call_,id);
  if(!call()){if(e.empty())e=runtime_.error();poisoned_=true;return false;}
  e.clear();return true;
}
bool HouseReturnInteractDialog::source_phase(FieldObjectId id,FieldTreePhase phase,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;auto&i=instances_.at(id);const auto*n=in_.tree->state(id);
  if(!i.bound||!n||!n->bound)return reject(e,"House InteractDialog source phase before actual combined binding");
  if(phase==FieldTreePhase::EnterScript){
    if(!factory_||i.entered||!n->inside)return reject(e,"House InteractDialog actual Enter cursor differs");i.entered=true;
  }else if(phase==FieldTreePhase::ReadyScript){
    if(!i.entered||!n->ready_notified)return reject(e,"House InteractDialog actual Ready notification unavailable");
    return invoke(id,[&]{return runtime_.ready(stable);},e);
  }else if(phase==FieldTreePhase::ExitScript){
    if(!i.entered||!n->inside)return reject(e,"House InteractDialog actual Exit cursor differs");
    // Original source has no exit body. Its global signal remains until free.
    i.entered=false;
  }else return reject(e,"House InteractDialog source has no process/native/input phase body");
  e.clear();return true;
}
bool HouseReturnInteractDialog::programme_borrows(std::string&e)const{
  if(!borrows(e))return false;HouseProgrammeState owner;PodunkDialogueProgrammeBinding binding;
  if(!in_.dialogue->programme_binding(binding,e))return false;
  if(in_.programmes->tree()!=in_.tree||in_.programmes->registry()!=in_.registry||
      in_.programmes->house()!=in_.house||in_.dialogue->world()!=&in_.house->world||
      in_.house->world.house_programme_owner()!=in_.dialogue||
      in_.house->house.programme_owner()!=in_.dialogue||
      !in_.house->house.observe_programme_owner(owner,e)||
      owner.world!=&in_.house->world||owner.runtime!=&in_.house->house||
      owner.printer!=&in_.house->presentation||owner.world_owner!=in_.dialogue||
      !owner.choices||owner.choices!=binding.choices||binding.printer!=owner.printer||
      binding.vm_owner!=&in_.house->world||!same(binding.identity,in_.sources->tree().identity())||
      owner.phase!=HouseProgrammePhase::Closed||!owner.native_closed||owner.request_generation||
      owner.house.bytes()!=in_.text.bytes()||owner.house.byte_size()!=in_.text.byte_size()||
      in_.registry->current_scene()!=in_.tree->root())
    return reject(e,"House InteractDialog requires the same actual closed native/World/House lease owners");
  return in_.sources->reentry().matches(*in_.doors,in_.house->world.content(),in_.text,e);
}
bool HouseReturnInteractDialog::programme(std::string_view path,HouseReturnInteractProgramme&out,std::string&e)const{
  if(!source_call_||!callback_depth_||path.empty()||!programme_borrows(e))
    return reject(e,"House InteractDialog open is outside an actual nullable-NPC source call");
  return programme_input(source_call_,path,out,e);
}
bool HouseReturnInteractDialog::programme_input(FieldObjectId id,std::string_view path,
    HouseReturnInteractProgramme&out,std::string&e)const{
  uint32_t stable=0;if(!actual(id,stable,e))return false;const auto*r=in_.data->record(stable);
  const auto*s=runtime_.state(stable);const auto*n=in_.tree->state(id);
  if(source_call_!=id||!callback_depth_||!factory_||!r||!s||!s->ready||s->deleted||s->queued||
      !n||!n->inside||n->queued||!n->ready_notified||!programme_borrows(e))
    return reject(e,"House InteractDialog programme receipt lacks its real live Ready source caller");
  std::string selected=r->dialogue;
  for(const auto&v:r->choices)if(!v.flag.empty()){
    bool present=false,on=false;if(!in_.global_data->flags().read(false,v.flag,present,on,e))return false;
    if(on)selected=v.programme;
  }
  if(path!=selected)return reject(e,"House InteractDialog programme does not match its actual source flag selection");
  const auto room=in_.house->world.content();uint32_t programme=kRoomNoIndex;
  for(uint32_t i=0;i<room.program_count();++i)if(room.string(room.program(i).source_path_string)==path){
    if(programme!=kRoomNoIndex)return reject(e,"House InteractDialog Room programme source is ambiguous");programme=i;
  }
  if(programme==kRoomNoIndex)return reject(e,"House InteractDialog source dialogue has no admitted original Room programme consumer");
  std::array<uint8_t,32>sha;
  if(!in_.data->source_hash("Data/Dialogue/"+std::string(path)+".yaml",sha))
    return reject(e,"House InteractDialog selected dialogue lacks its original source proof");
  out={this,&runtime_,in_.data,in_.tree,&in_.sources->tree(),in_.registry,&in_.sources->reentry(),
       in_.doors,in_.house,in_.text,id,stable,programme,path,false};
  e.clear();return true;
}
bool HouseReturnInteractDialog::interact(FieldObjectId id,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  return invoke(id,[&]{return runtime_.interact(stable);},e);
}
bool HouseReturnInteractDialog::interact_item(FieldObjectId id,FieldObjectId item,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  const auto reference=in_.registry->native_reference(item);
  const auto*actual_item=reference?reference->item_source_reference():nullptr;
  FieldOwnedItem value;const auto*definitions=in_.global_data->items_cache().definitions();
  if(!actual_item||static_cast<const FieldGlobalNativeReference*>(actual_item)!=reference.get()||
      actual_item->binding().object!=item||actual_item->registry()!=in_.registry||
      !actual_item->read_item(value,e)||!definitions)
    return reject(e,"House InteractDialog item argument lacks its actual constructed Item Reference");
  const auto*d=definitions->definition(value.definition);
  if(!d)return reject(e,"House InteractDialog actual Item.item_name definition unavailable");
  return invoke(id,[&]{return runtime_.interact_item(stable,d->item_name);},e);
}
bool HouseReturnInteractDialog::has_thoughts(FieldObjectId id,bool&out,std::string&e)const{
  uint32_t stable=0;if(!actual(id,stable,e)||!runtime_.has_thoughts(stable,out))return false;
  e.clear();return true;
}
bool HouseReturnInteractDialog::telepathy(FieldObjectId id,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  // All eight actual House _thoughts fields are empty. The source telepathy
  // prefix still turns on an effect before open; absent real ownership rejects.
  return reject(e,"House InteractDialog telepathy requires the original native effect/thoughts consumer");
}
bool HouseReturnInteractDialog::handles_method(const FieldDeferredMessage&m)const{
  return owns(m.object)&&m.kind==FieldDeferredKind::Call&&
      (m.member=="_check_flags"||m.member=="interact"||m.member=="interact_item"||m.member=="telepathy");
}
bool HouseReturnInteractDialog::dispatch(const FieldDeferredMessage&m,std::string&e){
  uint32_t stable=0;if(!handles_method(m)||!actual(m.object,stable,e))return reject(e,"House InteractDialog source method/receiver unknown");
  if(m.member=="_check_flags"){
    auto&i=instances_.at(m.object);
    if(!m.args.empty()||!i.flags||!in_.signals->emitting_to(in_.global->owner(),"flags_updated",m.object,m.member))
      return reject(e,"House InteractDialog flags callback lacks its actual synchronous global emission frame");
    return invoke(m.object,[&]{return i.flags(e);},e);
  }
  if(m.member=="interact_item"){
    if(m.args.size()!=1)return reject(e,"House InteractDialog interact_item signature rejected");
    const auto*item=std::get_if<FieldObjectRef>(&m.args.front());
    return item?interact_item(m.object,item->id,e):reject(e,"House InteractDialog item argument is not an actual ObjectDB Reference");
  }
  if(!m.args.empty())return reject(e,"House InteractDialog source call signature rejected");
  return m.member=="interact"?interact(m.object,e):telepathy(m.object,e);
}
bool HouseReturnInteractDialog::release_deleted(FieldObjectId id,std::string&e){
  if(!borrows(e))return false;const auto i=instances_.find(id);
  if(i==instances_.end()||callback_depth_||source_call_||i->second.entered||
      in_.registry->object_exists(id)||in_.tree->state(id))
    return reject(e,"House InteractDialog release precedes actual source exit/native/ObjectDB deletion");
  const auto*s=runtime_.state(i->second.source);
  // The core's commit_deleted describes its own _check_flags queue_free branch.
  // Ordinary parent scene deletion is not relabelled as that source branch.
  if(s&&s->queued&&!runtime_.commit_deleted(i->second.source)){e=runtime_.error();return false;}
  if(!in_.signals->release(id,e))return false;
  instances_.erase(i);e.clear();return true;
}
} // namespace encore::ctr
