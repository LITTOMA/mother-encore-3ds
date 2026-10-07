#include "house_return_tint_runtime.hpp"
#include <algorithm>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
}
std::string_view native_class(const FieldNodeTreeData&tree,const FieldNodeDescriptor&node){
  return node.class_index<tree.classes().size()?std::string_view(tree.classes()[node.class_index]):std::string_view{};
}
bool resolve_path(std::string_view base,std::string_view relative,std::string&out){
  std::vector<std::string>parts;
  for(size_t begin=0;begin<base.size();){const auto end=base.find('/',begin);
    const auto stop=end==base.npos?base.size():end;const auto part=base.substr(begin,stop-begin);
    if(!part.empty()&&part!=".")parts.emplace_back(part);
    begin=stop+1;
  }
  for(size_t begin=0;begin<relative.size();){const auto end=relative.find('/',begin);
    const auto stop=end==relative.npos?relative.size():end;const auto part=relative.substr(begin,stop-begin);begin=stop+1;
    if(part.empty()||part==".")continue;
    if(part==".."){if(parts.empty())return false;parts.pop_back();}
    else {if(part.front()=='%')return false;parts.emplace_back(part);}
  }
  out.clear();for(const auto&part:parts){if(!out.empty())out+='/';out+=part;}
  if(out.empty())out=".";
  return true;
}
struct Callback {size_t&depth;explicit Callback(size_t&d):depth(d){++d;}~Callback(){--depth;}};
struct Emission {std::vector<FieldObjectId>&stack;Emission(std::vector<FieldObjectId>&s,FieldObjectId id):stack(s){s.push_back(id);}~Emission(){stack.pop_back();}};
}
bool HouseReturnTintRuntime::admit_source(const FieldTintData&data,
    const HouseReturnSources&sources,std::string&e){
  const auto&tree=sources.tree();const auto identity=tree.identity();std::array<uint8_t,32>sha;
  if(!sources.valid()||!data.valid()||data.scene_id()!=identity.scene_id||
      data.source_pin()!=identity.upstream_commit||
      !data.source_hash(tree.source_scene(),sha)||sha!=identity.source_sha256||
      data.prototype(FieldTintKind::EnemyPrototype)||data.prototype(FieldTintKind::ActorPrototype))
    return reject(e,"House Tint data does not own the exact full House source closure");
  std::map<std::string,std::array<uint8_t,32>>scripts;
  for(const auto&r:data.records()){
    const auto*n=tree.record(r.id);
    if(!n||n->script.empty())return reject(e,"House Tint actual source receiver is absent");
    const auto inserted=scripts.emplace(n->script,n->script_sha);
    if(!inserted.second&&inserted.first->second!=n->script_sha)
      return reject(e,"House Tint source script has conflicting fingerprints");
  }
  size_t count=0;
  for(const auto&n:tree.records())if(scripts.count(n.script)){
    ++count;const auto*r=data.record(n.id);
    if(!r||r->kind!=FieldTintKind::Scene||r->scene_id!=identity.scene_id||
        r->scene!=tree.source_scene()||r->node!=n.path||r->ready_ordinal!=n.ready||
        native_class(tree,n)!="Node"||n.script_methods!=1||
        !data.source_hash(n.script,sha)||sha!=n.script_sha)
      return reject(e,"House Tint actual source node/Ready fingerprint differs");
    for(const auto&t:r->targets){const auto*target=tree.record(t.source_id);
      if(t.exists!=bool(target)||(target&&(target->path!=t.node||native_class(tree,*target)!=t.kind||
          !(target->flags&1)||target->self_modulate!=t.initial_self_modulate)))
        return reject(e,"House Tint nullable target/native Canvas source proof differs");
      // Only the source's relative node lookup is implemented here.
      if(t.node_path.empty()||t.node_path.front()=='/'||t.node_path.find(':')!=t.node_path.npos||
          t.node_path.find('\\')!=t.node_path.npos)
        return reject(e,"House Tint source target requires an unsupported NodePath domain");
      std::string resolved;
      if(!resolve_path(r->node,t.node_path,resolved)||resolved!=t.node)
        return reject(e,"House Tint exported NodePath differs from its actual target source path");
    }
  }
  if(!count||count!=data.records().size())return reject(e,"House Tint omits/adds actual scene receivers");
  e.clear();return true;
}
bool HouseReturnTintRuntime::prepare(HouseReturnTintInput in,std::string&e){
  if(prepared_||!in.sources||!in.data||!in.tree||!in.registry||!in.signals||
      !in.canvas||!in.kinematic)
    return reject(e,"House Tint requires all concrete native/source/ObjectDB owners");
  if(!admit_source(*in.data,*in.sources,e))return false;
  if(in.data!=&in.sources->tint()||!in.registry->kernel()||
      in.signals->registry()!=in.registry||in.registry->poisoned()||
      (in.tree->object_domain()&&in.tree->object_domain()!=in.registry->kernel())||
      in.canvas->canvas_tree()!=in.tree||in.canvas->canvas_data()!=&in.sources->canvas())
    return reject(e,"House Tint requires its actual House native/source/ObjectDB owners");
  in_=in;
  FieldTintHost host;
  host.resolve=[this](auto id,const auto&d,const auto&t,bool&exists,uint32_t&out,auto&e){return resolve(id,d,t,exists,out,e);};
  host.self_modulate=[this](auto id,auto color,auto&e){return self_modulate(id,color,e);};
  if(!runtime_.initialize(*in.data,std::move(host),e))return false;
  prepared_=true;e.clear();return true;
}
bool HouseReturnTintRuntime::borrows(std::string&e)const{
  if(!prepared_||poisoned_||runtime_.data()!=in_.data||in_.data!=&in_.sources->tint()||
      !in_.sources->valid()||in_.registry->poisoned()||in_.signals->registry()!=in_.registry||
      in_.tree->object_domain()!=in_.registry->kernel()||in_.canvas->canvas_tree()!=in_.tree||
      in_.canvas->canvas_data()!=&in_.sources->canvas())
    return reject(e,"House Tint lost its actual source/Canvas/signal borrowers");
  e.clear();return true;
}
bool HouseReturnTintRuntime::owns(const FieldNodeDescriptor&d)const{
  const auto*r=in_.data?in_.data->record(d.id):nullptr;
  const auto*n=in_.sources?in_.sources->tree().record(d.id):nullptr;
  return r&&n&&r->kind==FieldTintKind::Scene&&r->node==d.path&&
      n->script==d.script&&n->script_sha==d.script_sha&&native_class(in_.sources->tree(),*n)==d.native_class;
}
bool HouseReturnTintRuntime::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnTintRuntime::actual(FieldObjectId id,uint32_t&stable,std::string&e)const{
  if(!borrows(e))return false;
  const auto i=instances_.find(id);const auto*n=in_.tree->state(id);const auto*d=in_.tree->descriptor(id);FieldIdentity identity;
  if(i==instances_.end()||!n||!n->alive||!d||!owns(*d)||i->second.source!=d->id||
      !in_.registry->object_exists(id)||in_.registry->tree_owner(id).get()!=in_.tree||
      in_.tree->source_object(d->id)!=id||!in_.tree->object_identity(id,identity)||
      !same(identity,in_.sources->tree().identity()))
    return reject(e,"House Tint receiver is not its actual live source/ObjectDB node");
  stable=d->id;e.clear();return true;
}
bool HouseReturnTintRuntime::source(uint32_t stable,FieldObjectId&out,std::string&e)const{
  out=in_.tree->source_object(stable);uint32_t actual_id=0;
  if(!out||!actual(out,actual_id,e)||actual_id!=stable)return reject(e,"House Tint source receiver unavailable");
  return true;
}
bool HouseReturnTintRuntime::construct(FieldObjectId id,const FieldNodeDescriptor&d,
    const FieldIdentity&identity,std::string&e){
  if(!borrows(e))return false;
  const auto*n=in_.tree->state(id);
  if(!owns(d)||!n||!n->alive||n->inside||n->parent||n->bound||n->ready_notified||
      instances_.count(id)||!same(identity,in_.sources->tree().identity())||
      !in_.registry->object_exists(id)||in_.registry->tree_owner(id).get()!=in_.tree||
      !in_.canvas->owns(id)||in_.tree->source_object(d.id)!=id)
    return reject(e,"House Tint constructor requires its actual allocated native Node before parent/Ready");
  if(!runtime_.create(d.id,d.id)){e=runtime_.error();poisoned_=true;return false;}
  instances_.emplace(id,Instance{d.id,false,false});e.clear();return true;
}
bool HouseReturnTintRuntime::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  const auto*d=in_.sources->tree().record(stable);auto&i=instances_.at(id);
  if(i.bound||!d||b.stable_id!=stable||b.class_index!=d->class_index||
      b.native_class!=native_class(in_.sources->tree(),*d)||b.script_sha!=d->script_sha||
      !same(b.identity,in_.sources->tree().identity())||!in_.canvas->owns(id))
    return reject(e,"House Tint source binding differs from the actual native/script receiver");
  i.bound=true;e.clear();return true;
}
bool HouseReturnTintRuntime::target(FieldObjectId id,const FieldTintTarget&t,std::string&e)const{
  const auto*n=in_.tree->state(id);const auto*d=in_.tree->descriptor(id);FieldIdentity identity;
  if(!t.exists||!n||!n->alive||!d||d->id!=t.source_id||d->path!=t.node||
      d->native_class!=t.kind||!(d->flags&1)||!in_.registry->object_exists(id)||
      in_.registry->tree_owner(id).get()!=in_.tree||in_.tree->source_object(d->id)!=id||
      !in_.tree->object_identity(id,identity)||!same(identity,in_.sources->tree().identity()))
    return reject(e,"House Tint target is not its actual source CanvasItem");
  if(d->native_class=="KinematicBody2D"){
    FieldObjectId body=0;
    if(!in_.kinematic->owns(id)||!in_.kinematic->source_object(d->id,body,e)||body!=id)
      return reject(e,"House Tint NPC CanvasItem lacks its actual Kinematic native owner");
  }else if(!in_.canvas->owns(id))return reject(e,"House Tint target lacks its actual House Canvas native owner");
  e.clear();return true;
}
bool HouseReturnTintRuntime::finish_factory(std::string&e){
  if(!borrows(e))return false;
  if(factory_||instances_.size()!=in_.data->records().size())return reject(e,"House Tint actual source factory incomplete/repeated");
  for(const auto&r:in_.data->records()){
    FieldObjectId body=0;if(!source(r.id,body,e))return false;
    const auto*n=in_.tree->state(body);
    if(n->inside||n->ready_notified)return reject(e,"House Tint factory must precede actual Enter/Ready");
    for(const auto&t:r.targets)if(t.exists){const auto id=in_.tree->source_object(t.source_id);
      if(!target(id,t,e))return false;
      const auto old=targets_.find(t.source_id);
      if(old!=targets_.end()&&old->second!=id)return reject(e,"House Tint source target has multiple ObjectDB identities");
      targets_[t.source_id]=id;
    }
  }
  factory_=true;e.clear();return true;
}
bool HouseReturnTintRuntime::resolve(uint32_t stable,const FieldTintDescriptor&d,
    const FieldTintTarget&t,bool&exists,uint32_t&out,std::string&e){
  FieldObjectId id=0;
  if(!factory_||d.id!=stable||in_.data->record(stable)!=&d||!source(stable,id,e))
    return reject(e,"House Tint target lookup requires its actual complete source factory");
  // Checked relative get_node_or_null: walk the actual parent/child objects.
  // A detached/deleted source node is nullable; a missing native adapter is not.
  FieldObjectId at=id;size_t begin=0;
  while(begin<t.node_path.size()&&at){
    const auto end=t.node_path.find('/',begin);const auto stop=end==std::string::npos?t.node_path.size():end;
    const auto part=std::string_view(t.node_path).substr(begin,stop-begin);begin=stop+1;
    if(part.empty()||part==".")continue;
    const auto*n=in_.tree->state(at);
    if(!n||!n->alive)return reject(e,"House Tint lookup traversed an unknown/freed owner");
    if(part==".."){at=n->parent;continue;}
    FieldObjectId child=0;
    for(const auto c:n->children){const auto*s=in_.tree->state(c);
      if(!s||!s->alive)return reject(e,"House Tint lookup found an invalid actual child list");
      if(s->name==part){if(child)return reject(e,"House Tint actual NodePath is ambiguous");child=c;}
    }
    at=child;
  }
  const auto receipt=targets_.find(t.source_id);
  if(receipt==targets_.end())return reject(e,"House Tint nullable lookup has no actual factory target receipt");
  if(!at){exists=false;out=0;e.clear();return true;}
  if(at!=receipt->second||!target(at,t,e))return reject(e,"House Tint NodePath resolved a replacement/foreign target");
  exists=true;out=t.source_id;e.clear();return true;
}
bool HouseReturnTintRuntime::self_modulate(uint32_t stable,FieldTintColor color,std::string&e){
  if(!borrows(e))return false;
  const auto receipt=targets_.find(stable);
  if(receipt==targets_.end())return reject(e,"House Tint cached target has no actual native receipt");
  const FieldTintTarget*t=nullptr;
  for(const auto&r:in_.data->records())for(const auto&v:r.targets)if(v.source_id==stable)t=&v;
  if(!t)return reject(e,"House Tint cached target lost its checked source descriptor");
  if(!target(receipt->second,*t,e))return false;
  return in_.tree->set_modulate(receipt->second,color,true,e);
}
bool HouseReturnTintRuntime::source_phase(FieldObjectId id,FieldTreePhase phase,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  auto&i=instances_.at(id);const auto*n=in_.tree->state(id);const auto*core=runtime_.instance(stable);
  if(phase==FieldTreePhase::EnterScript){
    if(!factory_||!i.bound||i.entered||!n->inside||!n->bound)return reject(e,"House Tint Enter cursor differs");
    i.entered=true;
  }else if(phase==FieldTreePhase::ReadyScript){
    if(!factory_||!i.entered||!n->inside||!n->bound||!n->ready_notified||!core||core->ready)
      return reject(e,"House Tint source Ready has no actual notification or was replayed");
    Callback callback(callback_depth_);
    if(!runtime_.ready(stable)){e=runtime_.error();poisoned_=true;return false;}
  }else if(phase==FieldTreePhase::ExitScript){
    if(!i.entered||!n->inside)return reject(e,"House Tint Exit cursor differs");
    i.entered=false;
  }else return reject(e,"House Tint source has no native/process/input notification consumer");
  e.clear();return true;
}
bool HouseReturnTintRuntime::declaration(FieldObjectId id,std::string_view signal,uint32_t&arity,std::string&e)const{
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  if(signal!="changed_tint")return reject(e,"House Tint source signal unknown");
  arity=1;e.clear();return true;
}
bool HouseReturnTintRuntime::set_tint(FieldObjectId id,FieldTintColor color,std::string&e){
  uint32_t stable=0;if(!actual(id,stable,e))return false;
  if(!factory_||std::find(emitting_.begin(),emitting_.end(),id)!=emitting_.end())
    return reject(e,"House Tint unknown factory or unreviewed live signal cycle");
  Callback callback(callback_depth_);
  if(!runtime_.set_tint(stable,color)){e=runtime_.error();poisoned_=true;return false;}
  const auto*i=runtime_.instance(stable);
  if(!i||!i->connections.empty())return reject(e,"House Tint source has a second authoritative signal graph");
  // The core completed every real self_modulate write in source target order.
  // Deliver changed_tint synchronously before returning to the source caller.
  Emission emission(emitting_,id);
  if(!in_.signals->emit(id,"changed_tint",{FieldDeferredValue(color)},e)){poisoned_=true;return false;}
  e.clear();return true;
}
bool HouseReturnTintRuntime::reachable(FieldObjectId from,FieldObjectId to,
    std::vector<FieldObjectId>&seen,bool&out,std::string&e)const{
  out=false;if(from==to){out=true;return true;}
  if(std::find(seen.begin(),seen.end(),from)!=seen.end())return true;
  seen.push_back(from);
  for(const auto&v:instances_){
    if(!in_.registry->object_exists(v.first))continue;
    bool connected=false;
    if(!in_.signals->connected(from,"changed_tint",v.first,"set_tint",connected,e))return false;
    if(connected){bool found=false;if(!reachable(v.first,to,seen,found,e))return false;if(found){out=true;return true;}}
  }
  e.clear();return true;
}
bool HouseReturnTintRuntime::connect_tint(FieldObjectId sender,FieldObjectId receiver,std::string&e){
  uint32_t from=0,to=0;if(!actual(sender,from,e)||!actual(receiver,to,e))return false;
  if(!factory_)return reject(e,"House Tint connect requires the actual source factory");
  bool cycle=false,connected=false;std::vector<FieldObjectId>seen;
  if(!reachable(receiver,sender,seen,cycle,e))return false;
  if(cycle)return reject(e,"House Tint actual connection graph would introduce an unreviewed cycle");
  if(!in_.signals->connected(sender,"changed_tint",receiver,"set_tint",connected,e))return false;
  if(!connected&&!in_.signals->connect(sender,"changed_tint",receiver,"set_tint",0,{},e))return false;
  const auto*i=runtime_.instance(from);
  if(!i)return reject(e,"House Tint connect lost its actual source tint field");
  // Original source ignores duplicate connect return status, and still calls
  // receiver.set_tint(_tint) immediately on EVERY connect_tint invocation.
  return set_tint(receiver,i->tint,e);
}
bool HouseReturnTintRuntime::handles_method(const FieldDeferredMessage&m)const{
  return owns(m.object)&&(m.member=="set_tint"||m.member=="connect_tint");
}
bool HouseReturnTintRuntime::dispatch(const FieldDeferredMessage&m,std::string&e){
  uint32_t stable=0;if(!actual(m.object,stable,e))return false;
  if(m.kind!=FieldDeferredKind::Call||m.args.size()!=1)return reject(e,"House Tint source method signature unknown");
  if(m.member=="set_tint"){
    const auto*color=std::get_if<FieldColor>(&m.args.front());
    if(!color)return reject(e,"House Tint source set_tint argument is not Color");
    if(!emitting_.empty()&&!in_.signals->emitting_to(emitting_.back(),"changed_tint",m.object,m.member))
      return reject(e,"House Tint changed_tint callback is outside its actual emitter/receiver frame");
    return set_tint(m.object,*color,e);
  }
  if(m.member=="connect_tint"){
    const auto*target=std::get_if<FieldObjectRef>(&m.args.front());
    if(!target)return reject(e,"House Tint source connect_tint argument is not actual CharacterTint");
    return connect_tint(m.object,target->id,e);
  }
  return reject(e,"House Tint source method unsupported");
}
bool HouseReturnTintRuntime::release_deleted(FieldObjectId id,std::string&e){
  if(!borrows(e))return false;
  const auto i=instances_.find(id);
  if(i==instances_.end()||callback_depth_||!emitting_.empty()||
      in_.registry->object_exists(id)||in_.tree->state(id))
    return reject(e,"House Tint release precedes actual native/Tree/ObjectDB deletion");
  if(!runtime_.destroy(i->second.source)){e=runtime_.error();return false;}
  if(!in_.signals->release(id,e))return false;
  instances_.erase(i);e.clear();return true;
}
} // namespace encore::ctr
