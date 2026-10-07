#include "encore/field_node_tree.hpp"
#include "encore/field_node_recipe.hpp"
#include "encore/field_node_sort.hpp"
#include "encore/field_sprite_bridge.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "field_node_native_classes.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
 namespace {
  bool fail(std::string&e,const char*s){
   e=s;
   return false;
  }
  bool equal_identity(const FieldIdentity&a,const FieldIdentity&b){
   return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
  }
  bool finite(const FieldTransform&t){
   for(auto v:t)if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;
   return true;
  }
  bool finite(const FieldColor&c){
   for(auto v:c)if(!std::isfinite(v))return false;
   return true;
  }
  Vec2 xform(const FieldTransform&t,Vec2 v){
   return {
    t[0].x*v.x+t[1].x*v.y+t[2].x,t[0].y*v.x+t[1].y*v.y+t[2].y
   }
   ;
  }
  FieldTransform multiply(const FieldTransform&a,const FieldTransform&b){
   return {
    {
     {
      a[0].x*b[0].x+a[1].x*b[0].y,a[0].y*b[0].x+a[1].y*b[0].y
     }
     ,{
      a[0].x*b[1].x+a[1].x*b[1].y,a[0].y*b[1].x+a[1].y*b[1].y
     }
     ,xform(a,b[2])
    }
   }
   ;
  }
  bool text(std::string_view s){
   size_t n=0;
   return !s.empty()&&s.find('\0')==s.npos&&utf8_count(s,n);
  }
 }
 FieldNodeState*FieldNodeTreeRuntime::live(FieldObjectId id){
  auto i=nodes_.find(id);
  return i==nodes_.end()||!i->second.alive?nullptr:&i->second;
 }
 const FieldNodeState*FieldNodeTreeRuntime::state(FieldObjectId id)const{
  auto i=nodes_.find(id);
  return i==nodes_.end()||!i->second.alive?nullptr:&i->second;
 }
 const FieldNodeTreeRuntime::OwnedSource*FieldNodeTreeRuntime::source(FieldObjectId id)const{
  auto i=sources_.find(id);
  return i==sources_.end()?nullptr:&i->second;
 }
 FieldObjectId FieldNodeTreeRuntime::source_object(uint32_t id)const{
  auto i=source_index_.find(id);
  return i==source_index_.end()||!state(i->second)?0:i->second;
 }
 bool FieldNodeTreeRuntime::initialize(const FieldNodeTreeData&d,FieldNodeTreeHost h,std::string&e){
  if(!d.valid()||!h.allocate_object||!h.allocate_fast_name||!h.bind||!h.dispatch||!h.deferred||!h.object_exists||!h.input_registration||!h.external_pause_process||!h.release)return fail(e,"NodeTree live typed host incomplete");
  if(!nodes_.empty())return fail(e,"NodeTree live ownership already initialized");
  if(h.enqueue_global&&!h.object_domain)return fail(e,"NodeTree global ObjectDB domain missing");
  if(bool(h.enqueue_global)!=bool(h.flush_global))return fail(e,"NodeTree partial global MessageQueue binding rejected");
  host_=std::move(h);
  FieldObjectId id=0;
  if(!instantiate(d,d.identity().scene_id,id,e))return false;
  root_=id;
  for(const auto&v:nodes_)source_index_.emplace(v.second.source,v.first);
  e.clear();
  return true;
 }
 bool FieldNodeTreeRuntime::initialize_recipe(const FieldNodeRecipeData&d,FieldNodeTreeHost h,std::string&e){
  if(!d.valid()||!h.construct_source||!h.allocate_object||!h.allocate_fast_name||!h.bind||!h.dispatch||!h.deferred||!h.object_exists||!h.input_registration||!h.external_pause_process||!h.release)return fail(e,"NodeTree source recipe constructor host incomplete");
  if(root_||!nodes_.empty()||poisoned_)return fail(e,"NodeTree source recipe already owns objects");
  if((h.enqueue_global&&!h.object_domain)||bool(h.enqueue_global)!=bool(h.flush_global))return fail(e,"NodeTree source recipe global queue/domain rejected");
  // Internal native children must be allocated inside their actual parent's
  // native constructor, before script attachment. This capability does not
  // substitute a later record allocation for that original constructor.
  for(const auto&r:d.records())if(r.native_generated)return fail(e,"NodeTree source constructor internal native child unsupported");
  host_=std::move(h);FieldObjectId id=0;
  if(!instantiate_recipe(d,id,e))return false;
  root_=id;for(const auto&v:nodes_)source_index_.emplace(v.second.source,v.first);
  e.clear();return true;
 }
 bool FieldNodeTreeRuntime::instantiate_audio_source(const FieldIdentity&identity,const FieldNodeDescriptor&r,FieldObjectId&out,std::string&e){
 if(poisoned_||!host_.construct_source||!host_.native_allocated||!identity.scene_id||std::all_of(identity.upstream_commit.begin(),identity.upstream_commit.end(),[](uint8_t v){return !v;})||std::all_of(identity.source_sha256.begin(),identity.source_sha256.end(),[](uint8_t v){return !v;})||r.id!=identity.scene_id||r.path!="."||!r.name.empty()||r.parent||r.owner||r.canvas_parent||r.index!=-1||r.class_index!=14||r.native_class!="AudioStreamPlayer"||r.native_generated||r.pause||r.flags||r.priority||r.z||r.ready||!r.script.empty()||!std::all_of(r.script_sha.begin(),r.script_sha.end(),[](uint8_t v){return !v;})||!r.groups.empty()||!finite(r.local)||!finite(r.world)||!finite(r.modulate)||!finite(r.self_modulate))return fail(e,"NodeTree audited native AudioStreamPlayer.new rejected");
 return instantiate_records(identity,{r},r.id,false,out,e);
}
bool FieldNodeTreeRuntime::initialize_source_node(const FieldIdentity&identity,const FieldNodeDescriptor&r,FieldNodeTreeHost h,std::string&e){
  const auto zero=[](const auto&a){return std::all_of(a.begin(),a.end(),[](uint8_t v){return v==0;});};
  if(!identity.scene_id||zero(identity.upstream_commit)||zero(identity.source_sha256)||r.id!=identity.scene_id||r.path!="."||!r.name.empty()||r.parent||r.owner||r.canvas_parent||r.index!=-1||r.class_index||r.native_class!="Node"||r.native_generated||r.pause||r.flags||r.priority||r.z||r.ready||r.script.empty()||zero(r.script_sha)||!r.groups.empty()||!finite(r.local)||!finite(r.world)||!finite(r.modulate)||!finite(r.self_modulate))return fail(e,"NodeTree standalone source Node constructor descriptor rejected");
  if(!h.construct_source||!h.allocate_object||!h.allocate_fast_name||!h.bind||!h.dispatch||!h.deferred||!h.object_exists||!h.input_registration||!h.external_pause_process||!h.release)return fail(e,"NodeTree standalone source Node constructor host incomplete");
  if(root_||!nodes_.empty()||poisoned_)return fail(e,"NodeTree standalone source Node already owns objects");
  if((h.enqueue_global&&!h.object_domain)||bool(h.enqueue_global)!=bool(h.flush_global))return fail(e,"NodeTree standalone source Node queue/domain rejected");
  host_=std::move(h);FieldObjectId id=0;
  if(!instantiate_records(identity,{r},r.id,true,id,e))return false;
  root_=id;source_index_.emplace(r.id,id);e.clear();return true;
 }
 bool FieldNodeTreeRuntime::set_name(FieldObjectId id,std::string_view name,std::string&e){
  auto*n=live(id);
  // The currently mapped source call renames a detached Player instance. Live
  // parent uniqueness, unique-name-in-owner and SceneTree rename signals need
  // their own source consumers; never silently skip those mechanisms.
  if(poisoned_||!n||n->inside||n->parent||n->blocked||n->queued||!text(name)||name=="."||name==".."||name.find_first_of(".:@/\\")!=name.npos)return fail(e,"NodeTree detached source name boundary rejected");
  n->name=std::string(name);
  std::function<bool(FieldObjectId)>notify=[&](FieldObjectId object){
   auto*current=live(object);if(!current)return fail(e,"NodeTree source path notification object disappeared");
   if(!emit(object,FieldTreePhase::PathChanged,e))return false;
   current=live(object);if(!current)return fail(e,"NodeTree source path notification deleted owner");
   ++current->blocked;const auto children=current->children;
   for(auto child:children)if(!notify(child)){if(auto*alive=live(object))--alive->blocked;return false;}
   if(auto*alive=live(object))--alive->blocked;
   return true;
  };
  if(!notify(id)){poisoned_=true;return false;}
  ++order_version_;e.clear();return true;
 }
 bool FieldNodeTreeRuntime::instantiate(const FieldNodeTreeData&d,uint32_t first,FieldObjectId&out,std::string&e){
  if(!d.valid()||!d.record(first))return fail(e,"NodeTree dynamic source root missing");
  auto records=d.records();
  for(auto&r:records){
   if(!detail::field_native_class_schema(uint32_t(d.classes().size()))||
      r.class_index>=d.classes().size()||d.classes()[r.class_index]!=detail::field_native_classes[r.class_index]||
      bool(r.flags&1)!=detail::field_native_canvas(r.class_index))return fail(e,"NodeTree native class source rejected");
   r.native_class=d.classes()[r.class_index];
  }
  return instantiate_records(d.identity(),records,first,false,out,e);
 }
 bool FieldNodeTreeRuntime::instantiate_recipe(const FieldNodeRecipeData&d,FieldObjectId&out,std::string&e){
  if(!d.valid()||!d.record(d.identity().scene_id))return fail(e,"NodeTree original factory recipe rejected");
  std::vector<FieldNodeDescriptor>records;
  records.reserve(d.records().size());
  for(const auto&r:d.records())records.push_back(r);
  return instantiate_records(d.identity(),records,d.identity().scene_id,true,out,e);
 }
 bool FieldNodeTreeRuntime::instantiate_builtin_source(FieldObjectId caller,
       const FieldNodeTreeData&checked,const FieldSpriteData&sprites,
       FieldObjectId&out,std::string&e){
  out=0;
  const auto*n=state(caller);const auto*s=source(caller);
  const auto*r=n?checked.record(n->source):nullptr;
  const auto*v=n?sprites.record(n->source):nullptr;
  std::array<uint8_t,32>sha{};
  if(poisoned_||!root_||!host_.construct_source||!host_.native_allocated||
     !checked.valid()||!sprites.valid()||!n||!s||!r||!v||n->queued||
     v->kind!=FieldSpriteKind::Character||r->class_index>=checked.classes().size()||
     checked.classes()[r->class_index]!="Sprite"||r->script.empty()||
     !checked.source_hash(r->script,sha)||sha!=r->script_sha||
     s->descriptor.id!=r->id||s->descriptor.script!=r->script||
     s->descriptor.script_sha!=sha||s->descriptor.native_class!="Sprite"||
     s->descriptor.class_index!=r->class_index||s->descriptor.path!=r->path||
     !equal_identity(s->identity,checked.identity())||
     sprites.source_pin()!=checked.identity().upstream_commit||
     sprites.scene_id()!=checked.identity().scene_id)
   return fail(e,"NodeTree CharacterSprite builtin source caller/proof rejected");
  // Identity belongs to this source call site, not the global ObjectDB slot.
  const std::string key=r->path+"#AnimationTree.new";
  const uint32_t stable=encore::crc32(reinterpret_cast<const uint8_t*>(key.data()),key.size());
  if(!stable||checked.record(stable)||source_index_.count(stable))
   return fail(e,"NodeTree builtin source stable identity collision");
  for(const auto&entry:sources_)if(entry.second.descriptor.id==stable)
   return fail(e,"NodeTree duplicate builtin source constructor");
  FieldNodeDescriptor d;d.id=stable;d.index=-1;d.path=key;
  d.native_class="AnimationTree";
  d.local=d.world={{{1,0},{0,1},{0,0}}};
  d.modulate=d.self_modulate={{1,1,1,1}};
  if(!instantiate_records(s->identity,{d},stable,true,out,e))return false;
  source_index_.emplace(stable,out);
  return true;
 }
 bool FieldNodeTreeRuntime::transfer_detached_subtree(FieldNodeTreeRuntime&destination,FieldObjectId first,std::string&e){
  if(&destination==this||!host_.object_domain||host_.object_domain!=destination.host_.object_domain||!host_.enqueue_global||!destination.host_.enqueue_global||poisoned_||destination.poisoned_||driving_||destination.driving_||deleting_||destination.deleting_||flushing_||destination.flushing_||!frames_.empty()||!destination.frames_.empty()||!deletes_.empty()||!destination.deletes_.empty()||!messages_.empty()||!destination.messages_.empty())return fail(e,"NodeTree transfer domain/lifecycle/delete/local message boundary rejected");
  auto*root=live(first);auto*target_root=destination.live(destination.root_);
  if(!root||first==root_||root->parent||root->inside||!target_root||!destination.source(destination.root_))return fail(e,"NodeTree transfer requires actual detached subtree and destination kernel");
  std::vector<FieldObjectId>pending{first};std::set<FieldObjectId>seen;
  for(size_t at=0;at<pending.size();++at){
   auto id=pending[at];auto*n=live(id);auto*owned=source(id);
   if(!seen.insert(id).second||!n||!owned||n->inside||n->blocked||n->queued||transform_index_.count(id)||destination.nodes_.count(id)||destination.sources_.count(id)||destination.source_index_.count(n->source))return fail(e,"NodeTree transfer complete subtree/state/source-index conflict rejected");
   if(owned->identity.upstream_commit!=destination.source(destination.root_)->identity.upstream_commit)return fail(e,"NodeTree transfer original source pin mismatch");
   for(auto child:n->children){auto*c=live(child);if(!c||c->parent!=id)return fail(e,"NodeTree transfer source child topology mismatch");pending.push_back(child);}
  }
  for(auto id:pending){auto*n=live(id);if((n->owner&&!seen.count(n->owner))||(n->canvas_parent&&!seen.count(n->canvas_parent)))return fail(e,"NodeTree transfer retains foreign owner/canvas ancestor");}
  // No callbacks are emitted here: remove_child already emitted source exit,
  // and the subsequent actual add_child performs enter with ready_first kept.
  for(auto id:pending){
   auto node=nodes_.extract(id);auto owned=sources_.extract(id);
   const auto source_id=node.mapped().source;
   for(const auto&g:node.mapped().groups){auto i=group_index_.find(g);if(i!=group_index_.end())i->second.erase(std::remove(i->second.begin(),i->second.end(),id),i->second.end());destination.group_index_[g].push_back(id);}
   auto original=source_index_.find(source_id);
   if(original!=source_index_.end()&&original->second==id){destination.source_index_.emplace(source_id,id);source_index_.erase(original);}
   destination.nodes_.insert(std::move(node));destination.sources_.insert(std::move(owned));
  }
  ++order_version_;++destination.order_version_;e.clear();return true;
 }
 bool FieldNodeTreeRuntime::instantiate_records(const FieldIdentity&identity,const std::vector<FieldNodeDescriptor>&records,uint32_t first,bool recipe,FieldObjectId&out,std::string&e){
  if(poisoned_||!host_.allocate_object||!host_.allocate_fast_name)return fail(e,"NodeTree dynamic source/host rejected");
  std::map<uint32_t,FieldObjectId>created;
  for(const auto&r:records){
   if(r.id!=first&&!created.count(r.parent))continue;
   FieldObjectId id=0;
   if(!host_.allocate_object(id,e)||!id||nodes_.count(id)){
    poisoned_=true;
    return fail(e,"NodeTree actual global ObjectID allocation rejected");
   }
   created.emplace(r.id,id);
   FieldNodeState n;
   n.object=id;
   n.source=r.id;
   n.name=host_.construct_source?std::string():r.name;
   if(r.native_generated){
    if(!recipe||r.native_class!="VScrollBar"||!created.count(r.parent)||sources_.at(created.at(r.parent)).descriptor.native_class!="RichTextLabel"){
     poisoned_=true;return fail(e,"NodeTree original internal constructor owner rejected");
    }
    uint64_t counter=0;
    if(!host_.allocate_fast_name(counter,e)||!counter){poisoned_=true;return fail(e,"NodeTree original constructor global name allocation rejected");}
    n.name="@@"+std::to_string(counter);
   }
   n.pause=r.pause;
   n.priority=r.priority;
   n.z=r.z;
   n.flags=r.flags;
   n.local=r.local;
   n.world=r.world;
   n.modulate=r.modulate;
   n.self_modulate=r.self_modulate;
   if(!host_.construct_source)n.groups=r.groups;
   n.world_dirty=true;
   if(r.id!=first&&!host_.construct_source){
    n.parent=created.at(r.parent);
    nodes_.at(n.parent).children.push_back(id);
   }
   if(!host_.construct_source&&created.count(r.owner))n.owner=created.at(r.owner);
   if(!host_.construct_source&&created.count(r.canvas_parent))n.canvas_parent=created.at(r.canvas_parent);
   for(const auto&g:n.groups)group_index_[g].push_back(id);
   nodes_.emplace(id,std::move(n));
   sources_.emplace(id,OwnedSource{
    identity,r,recipe
   }
   );
   if(host_.native_allocated&&!host_.native_allocated(id,r,identity,e)){poisoned_=true;return false;}
   if(host_.construct_source){
    if(r.native_generated||!host_.construct_source(id,r,identity,e)){poisoned_=true;return false;}
    auto*actual=live(id);
    if(!actual||actual->parent||actual->inside||actual->ready_notified){poisoned_=true;return fail(e,"NodeTree source constructor changed instance lifecycle");}
    actual->name=r.name;
    if(r.id!=first){actual->parent=created.at(r.parent);nodes_.at(actual->parent).children.push_back(id);}
    if(created.count(r.owner))actual->owner=created.at(r.owner);
    if(created.count(r.canvas_parent))actual->canvas_parent=created.at(r.canvas_parent);
    // PackedScene adds source groups after native/script construction.
    // Preserve native internal-process groups already registered by setters.
    for(const auto&g:r.groups)if(!add_group(id,g,e)){poisoned_=true;return false;}
   }
  }
  ++order_version_;
  if(!created.count(first)){poisoned_=true;return fail(e,"NodeTree complete factory root missing");}
  out=created.at(first);
  e.clear();
  return true;
 }
 bool FieldNodeTreeRuntime::bind_source_object(FieldObjectId id,std::string&e){
 return bind(id,e);
}
bool FieldNodeTreeRuntime::bind(FieldObjectId id,std::string&e){
  auto*n=live(id);
  auto*s=source(id);
  if(!n||!s)return fail(e,"NodeTree dead lifecycle owner");
  if(n->bound)return true;
  const auto expected=*s;
  FieldNodeBinding b;
  if(!host_.bind(id,expected.descriptor,b,e))return false;
  if(!equal_identity(b.identity,expected.identity)||b.stable_id!=expected.descriptor.id||b.class_index!=expected.descriptor.class_index||b.script_sha!=expected.descriptor.script_sha||!b.family||!b.capability)return fail(e,"NodeTree typed native/script receipt rejected");
  if(expected.recipe&&b.native_class!=expected.descriptor.native_class)return fail(e,"NodeTree recipe canonical native owner receipt rejected");
  n=live(id);
  if(!n)return fail(e,"NodeTree owner deleted during bind");
  n->binding=b;
  n->bound=true;
  return true;
 }
 bool FieldNodeTreeRuntime::emit(FieldObjectId id,FieldTreePhase p,std::string&e){
  if(!bind(id,e))return false;
  auto*n=live(id);
  auto binding=n->binding;
  return host_.dispatch(id,binding,p,e);
 }
 bool FieldNodeTreeRuntime::enter(std::string&e){
  if(poisoned_||driving_||!frames_.empty()||split_lifecycle_!=SplitLifecycle::None||!live(root_)||live(root_)->inside)return fail(e,"NodeTree enter state rejected");
  frames_.push_back({
   root_,1,0,0
  }
  );
  frames_.push_back({
   root_,0,0,0
  }
  );
  return drive(e);
 }
 void FieldNodeTreeRuntime::complete_split_lifecycle(){
  if(!frames_.empty())return;
  if(split_lifecycle_==SplitLifecycle::Entering)split_lifecycle_=SplitLifecycle::AwaitingReady;
  else if(split_lifecycle_==SplitLifecycle::Readying)split_lifecycle_=SplitLifecycle::None;
 }
 bool FieldNodeTreeRuntime::enter_branch_only(std::string&e){
  if(poisoned_||driving_||!frames_.empty()||split_lifecycle_!=SplitLifecycle::None||!live(root_)||live(root_)->inside)return fail(e,"NodeTree split Enter state rejected");
  split_lifecycle_=SplitLifecycle::Entering;
  frames_.push_back({root_,0,0,0});
  if(!drive(e))return false;
  complete_split_lifecycle();
  return true;
 }
 bool FieldNodeTreeRuntime::ready_entered_branch(std::string&e){
  if(poisoned_||driving_||!frames_.empty()||split_lifecycle_!=SplitLifecycle::AwaitingReady||!live(root_)||!live(root_)->inside)return fail(e,"NodeTree split Ready requires completed actual Enter");
  split_lifecycle_=SplitLifecycle::Readying;
  frames_.push_back({root_,1,0,0});
  if(!drive(e))return false;
  complete_split_lifecycle();
  return true;
 }
 bool FieldNodeTreeRuntime::exit(std::string&e){
  if(poisoned_||driving_||!frames_.empty()||!live(root_)||!live(root_)->inside)return fail(e,"NodeTree exit state rejected");
  // A branch removed during its parent's Enter need never receive Ready.
  split_lifecycle_=SplitLifecycle::None;
  frames_.push_back({
   root_,2,0,0
  }
  );
  return drive(e);
 }
 bool FieldNodeTreeRuntime::resume_lifecycle(std::string&e){
  if(poisoned_||driving_)return fail(e,"NodeTree lifecycle state rejected");
  if(!drive(e))return false;
  complete_split_lifecycle();
  return true;
 }
 bool FieldNodeTreeRuntime::branch(FieldObjectId id,uint32_t mode,std::string&e){
  auto saved=std::move(frames_);
  bool previous=driving_;
  driving_=false;
  frames_.clear();
  frames_.push_back({
   id,mode,0,0
  }
  );
  bool ok=drive(e);
  // A nested incomplete lifecycle cannot be discarded. Refuse further execution
  // after a consumer has already mutated this branch.
  if(!ok){
   poisoned_=true;
   frames_.clear();
  }
  frames_=std::move(saved);
  driving_=previous;
  return ok;
 }
 bool FieldNodeTreeRuntime::drive(std::string&e){
  if(driving_)return fail(e,"NodeTree reentrant lifecycle rejected");
  driving_=true;
  auto stop=[&](){
   driving_=false;
   return false;
  }
  ;
  while(!frames_.empty()){
   auto f=frames_.back();
   auto*n=live(f.id);
   if(!n){
    frames_.pop_back();
    continue;
   }
   if(!bind(f.id,e))return stop();
   if(f.mode==0){
    if(f.stage==0){
     if(n->inside){
      frames_.pop_back();
      continue;
     }
     n->inside=true;
     ++order_version_;
     if(n->flags&1){n->world_dirty=true;queue_transform(f.id);}
     for(uint32_t i=0;i<3;++i)if(n->input_enabled[i]&&!host_.input_registration(f.id,i,true,e))return stop();
     frames_.back().stage=1;
     continue;
    }
    if(f.stage<=5){
     const FieldTreePhase phases[]={
      FieldTreePhase::EnterNative,FieldTreePhase::EnterScript,FieldTreePhase::TreeEntered,FieldTreePhase::NodeAdded,FieldTreePhase::ChildEntered
     }
     ;
     if(!emit(f.id,phases[f.stage-1],e))return stop();
     ++frames_.back().stage;
     continue;
    }
    if(f.stage==6){
     ++n->blocked;
     frames_.back().stage=7;
     continue;
    }
    if(f.child<n->children.size()){
     auto child=n->children[f.child];
     ++frames_.back().child;
     if(state(child)&&!state(child)->inside)frames_.push_back({
      child,0,0,0
     }
     );
     continue;
    }
    --n->blocked;
    frames_.pop_back();
    continue;
   }
   if(f.mode==1){
    if(f.stage==0){
     if(!n->inside){e="NodeTree Ready owner outside tree";return stop();
 }
     n->ready_notified=true;
     ++n->blocked;
     frames_.back().stage=1;
     continue;
    }
    if(f.stage==1){
     if(f.child<n->children.size()){
      auto child=n->children[f.child];
      ++frames_.back().child;
      if(state(child))frames_.push_back({
       child,1,0,0
      }
      );
      continue;
     }
     --n->blocked;
     frames_.back().stage=2;
     continue;
    }
    if(f.stage==2){
     if(!emit(f.id,FieldTreePhase::PostEnterNative,e))return stop();
     frames_.back().stage=3;
     continue;
    }
    if(f.stage==3){
     if(!n->ready_first){frames_.pop_back();continue;}
     n->ready_first=false;frames_.back().stage=4;continue;
    }
    if(f.stage==4){
     auto*s=source(f.id);
     for(uint32_t i=0;i<3;++i)if(s->descriptor.script_methods&(8u<<i))if(!set_input_process(f.id,i,true,e))return stop();
     if(s->descriptor.script_methods&64)if(!set_process(f.id,false,true,e))return stop();
     if(s->descriptor.script_methods&128)if(!set_process(f.id,true,true,e))return stop();
     frames_.back().stage=5;continue;
    }
    if(f.stage==5){if(!emit(f.id,FieldTreePhase::ReadyScript,e))return stop();
 frames_.back().stage=6;continue;}
    if(f.stage==6){if(!emit(f.id,FieldTreePhase::ReadyNative,e))return stop();
 frames_.back().stage=7;continue;}
    if(!emit(f.id,FieldTreePhase::ReadySignal,e))return stop();
    frames_.pop_back();
    continue;
   }
   if(f.mode==2){
    if(f.stage==0){
     if(!n->inside){
      frames_.pop_back();
      continue;
     }
     ++n->blocked;
     frames_.back().child=n->children.size();
     frames_.back().stage=1;
     continue;
    }
    if(f.stage==1){
     if(f.child){
      auto child=n->children[f.child-1];
      --frames_.back().child;
      if(state(child)&&state(child)->inside)frames_.push_back({
       child,2,0,0
      }
      );
      continue;
     }
     --n->blocked;
     frames_.back().stage=2;
     continue;
    }
    if(f.stage<=6){
     const FieldTreePhase phases[]={
      FieldTreePhase::ExitScript,FieldTreePhase::TreeExiting,FieldTreePhase::ExitNative,FieldTreePhase::NodeRemoved,FieldTreePhase::ChildExiting
     }
     ;
     if(!emit(f.id,phases[f.stage-2],e))return stop();
     ++frames_.back().stage;
     continue;
    }
    for(uint32_t i=0;i<3;++i)if(n->input_enabled[i]&&!host_.input_registration(f.id,i,false,e))return stop();
    erase_transform(f.id);
    n->world_dirty=true;
    n->inside=false;
    ++order_version_;
    n->ready_notified=false;
    frames_.pop_back();
    continue;
   }
   e="NodeTree unsupported lifecycle opcode";return stop();
  }
  driving_=false;
  e.clear();
  return true;
 }
 bool FieldNodeTreeRuntime::request_ready(FieldObjectId id,std::string&e){
  auto*n=live(id);
  if(!n)return fail(e,"NodeTree request_ready dead owner");
  n->ready_first=true;
  return true;
 }
 bool FieldNodeTreeRuntime::ancestor(FieldObjectId a,FieldObjectId b)const{
  auto*n=state(b);
  while(n&&n->parent){
   if(n->parent==a)return true;
   n=state(n->parent);
  }
  return false;
 }
 bool FieldNodeTreeRuntime::get_node(FieldObjectId from,std::string_view p,FieldObjectId&out,std::string&e)const{
  auto*n=state(from);
  if(!n||p.empty()||p.find('\\')!=p.npos||p.find('\0')!=p.npos)return fail(e,"NodeTree NodePath rejected");
  auto sub=p.find(':');
  if(sub!=p.npos)p=p.substr(0,sub);
  if(p.empty()){
   out=from;
   return true;
  }
  if(p.front()=='/'){
   if(!n->inside||!host_.external_path)return fail(e,"NodeTree actual absolute root unavailable");
   return host_.external_path(from,p,out,e);
  }
  size_t b=0;
  FieldObjectId at=from;
  while(b<p.size()){
   auto end=p.find('/',b);
   if(end==p.npos)end=p.size();
   auto name=p.substr(b,end-b);
   b=end+1;
   if(name.empty()||name==".")continue;
   n=state(at);
   if(!n)return fail(e,"NodeTree NodePath owner deleted");
   if(name==".."){
    if(!n->parent){
     if(host_.external_path)return host_.external_path(at,p.substr(end-name.size()),out,e);
     return fail(e,"NodeTree external parent unavailable");
    }
    at=n->parent;
    continue;
   }
   if(name.front()=='%')return fail(e,"NodeTree unique-owner names are not present/admitted in source");
   FieldObjectId next=0;
   for(auto child:n->children){
    auto*c=state(child);
    if(c&&c->name==name){
     next=child;
     break;
    }
   }
   if(!next)return fail(e,"NodeTree NodePath target missing");
   at=next;
  }
  out=at;
  e.clear();
  return true;
 }
 bool FieldNodeTreeRuntime::get_path_to(FieldObjectId a,FieldObjectId b,std::string&out,std::string&e)const{
  if(!state(a)||!state(b))return fail(e,"NodeTree get_path_to dead owner");
  std::vector<FieldObjectId>aa,bb;
  auto collect=[&](FieldObjectId id,std::vector<FieldObjectId>&v){
   while(state(id)){
    v.push_back(id);
    id=state(id)->parent;
   }
  }
  ;
  collect(a,aa);
  collect(b,bb);
  if(aa.back()!=bb.back())return fail(e,"NodeTree get_path_to unrelated trees");
  size_t ai=aa.size(),bi=bb.size();
  while(ai&&bi&&aa[ai-1]==bb[bi-1]){
   --ai;
   --bi;
  }
  out.clear();
  for(size_t i=0;i<ai;++i){
   if(!out.empty())out+='/';
   out+="..";
  }
  while(bi){
   if(!out.empty())out+='/';
   out+=state(bb[--bi])->name;
  }
  if(out.empty())out=".";
  return true;
 }
 bool FieldNodeTreeRuntime::set_owner(FieldObjectId id,FieldObjectId owner,std::string&e){
  auto*n=live(id);
  if(!n||(owner&&!ancestor(owner,id)))return fail(e,"NodeTree owner must be real ancestor");
  n->owner=owner;
  return true;
 }
 bool FieldNodeTreeRuntime::move_child(FieldObjectId parent,FieldObjectId child,int32_t index,std::string&e){
  auto*p=live(parent);
  auto*c=live(child);
  if(!p||!c||c->parent!=parent||p->blocked||index<0||size_t(index)>p->children.size())return fail(e,"NodeTree move_child parent/index/blocked rejected");
  if(size_t(index)==p->children.size())--index;
  auto it=std::find(p->children.begin(),p->children.end(),child);
  auto old=it-p->children.begin();
  if(old==index)return true;
  p->children.erase(it);
  p->children.insert(p->children.begin()+index,child);
  ++order_version_;
  ++p->blocked;
  bool ok=emit(child,FieldTreePhase::ChildMoved,e);
  --p->blocked;
  return ok;
 }
 bool FieldNodeTreeRuntime::detach(FieldObjectId parent,FieldObjectId child,std::string&e){
  auto*p=live(parent);
  auto*c=live(child);
  if(!p||!c||c->parent!=parent||p->blocked)return fail(e,"NodeTree detach owner/blocked rejected");
  if(!emit(child,FieldTreePhase::Unparented,e))return false;
  auto it=std::find(p->children.begin(),p->children.end(),child);
  auto index=size_t(it-p->children.begin());
  p->children.erase(it);
  ++order_version_;
  c->parent=0;
  c->canvas_parent=0;
  invalidate(child);
  (void)index;
  std::function<bool(FieldObjectId)>removed=[&](FieldObjectId id){
   auto*n=live(id);
   if(!n)return true;
   if(n->owner&&!ancestor(n->owner,id))n->owner=0;
   auto kids=n->children;
   ++n->blocked;
   for(auto k:kids)if(!removed(k)){
    --n->blocked;
    return false;
   }
   --n->blocked;
   return !p->inside||emit(id,FieldTreePhase::TreeExited,e);
  }
  ;
  return removed(child);
 }
 bool FieldNodeTreeRuntime::remove_child(FieldObjectId parent,FieldObjectId child,std::string&e){
  auto*p=live(parent);
  auto*c=live(child);
  if(!p||!c||c->parent!=parent||p->blocked)return fail(e,"NodeTree remove_child parent/blocked rejected");
  if(c->inside&&!branch(child,2,e))return false;
  return detach(parent,child,e);
 }
 bool FieldNodeTreeRuntime::add_child(FieldObjectId parent,FieldObjectId child,std::string&e){
  auto*p=live(parent);
  auto*c=live(child);
  if(!p||!c||p->blocked||c->parent||c->inside||child==parent||ancestor(child,parent))return fail(e,"NodeTree add_child parent/blocked/cycle rejected");
  bool duplicate=false;
  for(auto id:p->children)if(state(id)&&state(id)->name==c->name){
   duplicate=true;
   break;
  }
  if(c->name.empty()||duplicate){
   uint64_t counter=0;
   if(!host_.allocate_fast_name(counter,e)||!counter)return fail(e,"NodeTree global fast-name allocator rejected");
   c->name="@"+c->name+"@"+std::to_string(counter);
  }
  p->children.push_back(child);
  ++order_version_;
  c->parent=parent;
  c->canvas_parent=((c->flags&1)&&!(c->flags&4)&&(p->flags&1))?parent:0;
  invalidate(child);
  if(!emit(child,FieldTreePhase::Parented,e))return false;
  if(p->inside){
   if(!branch(child,0,e))return false;
   if(p->ready_notified&&!branch(child,1,e))return false;
  }
  return true;
 }
 void FieldNodeTreeRuntime::queue_transform(FieldObjectId id){
  auto*n=live(id);if(!n||n->block_transform_notify||!n->inside||transform_index_.count(id))return;
  transforms_.push_front(id);transform_index_[id]=transforms_.begin();
 }
 void FieldNodeTreeRuntime::erase_transform(FieldObjectId id){auto it=transform_index_.find(id);if(it!=transform_index_.end()){transforms_.erase(it->second);transform_index_.erase(it);}}
 void FieldNodeTreeRuntime::invalidate(FieldObjectId id){
  auto*n=live(id);if(!n||n->world_dirty)return;
  n->world_dirty=true;
  if(n->flags&256)queue_transform(id);
  for(auto c:n->children){auto*child=live(c);if(child&&child->canvas_parent==id)invalidate(c);}
 }
 bool FieldNodeTreeRuntime::set_transform_notification(FieldObjectId id,bool local,bool enabled,std::string&e){
  auto*n=live(id);if(!n||!(n->flags&1))return fail(e,"NodeTree Canvas notify owner rejected");
  auto bit=local?512u:256u;if(bool(n->flags&bit)==enabled)return true;
  n->flags=enabled?n->flags|bit:n->flags&~bit;
  if(!local&&enabled&&n->inside){FieldTransform t;return world_transform(id,t,e);}return true;
 }
 bool FieldNodeTreeRuntime::block_transform_notifications(FieldObjectId id,bool blocked,std::string&e){auto*n=live(id);if(!n||!(n->flags&1))return fail(e,"NodeTree block-transform owner rejected");
 n->block_transform_notify=blocked;return true;
 }
 bool FieldNodeTreeRuntime::force_update_transform(FieldObjectId id,std::string&e){auto*n=live(id);if(!n||!n->inside||!(n->flags&1))return fail(e,"NodeTree force-transform owner rejected");
 if(!transform_index_.count(id))return true;
 erase_transform(id);return emit(id,FieldTreePhase::TransformChanged,e);}
 bool FieldNodeTreeRuntime::flush_transform_notifications(std::string&e){
  // SelfList.add inserts at the head. Newly added notifications during a
  // callback are excluded by the native saved-next traversal.
  std::vector<FieldObjectId>read(transforms_.begin(),transforms_.end());
  for(auto id:read)if(transform_index_.count(id)){erase_transform(id);if(state(id)&&!emit(id,FieldTreePhase::TransformChanged,e))return false;
 }return true;
 }
 bool FieldNodeTreeRuntime::set_local(FieldObjectId id,const FieldTransform&t,std::string&e){
  auto*n=live(id);
  if(!n||!(n->flags&1)||!finite(t))return fail(e,"NodeTree Canvas transform rejected");
  n->local=t;
  // Preserve Node2D's outside-tree cache semantics.
  if(!n->inside)return true;
  invalidate(id);
  if((n->flags&512)&&!n->block_transform_notify)return emit(id,FieldTreePhase::LocalTransformChanged,e);
  return true;
 }
 bool FieldNodeTreeRuntime::world_transform(FieldObjectId id,FieldTransform&out,std::string&e){
  auto*n=live(id);
  if(!n||!(n->flags&1))return fail(e,"NodeTree nonCanvas/dead transform owner");
  if(n->world_dirty){
   auto w=n->local;
   if(n->canvas_parent){
    FieldTransform parent;
    if(!world_transform(n->canvas_parent,parent,e))return false;
    w=multiply(parent,w);
   }
   if(!finite(w))return fail(e,"NodeTree Canvas transform overflow");
   n->world=w;
   n->world_dirty=false;
  }
  out=n->world;
  return true;
 }
 bool FieldNodeTreeRuntime::visible_in_tree(FieldObjectId id)const{
  auto*n=state(id);
  if(!n||!n->inside||!(n->flags&1))return false;
  while(n){
   if(!(n->flags&2))return false;
   if(!n->canvas_parent)break;
   n=state(n->canvas_parent);
  }
  return true;
 }
 bool FieldNodeTreeRuntime::visibility_changed(FieldObjectId id,bool visible,std::string&e){
  auto*n=live(id);
  if(!n)return true;
  if(!emit(id,FieldTreePhase::VisibilityChanged,e))return false;
  if(!visible&&!emit(id,FieldTreePhase::Hide,e))return false;
  n=live(id);
  if(!n)return true;
  ++n->blocked;
  auto kids=n->children;
  for(auto c:kids){
   auto*child=live(c);
   if(child&&child->canvas_parent==id&&(child->flags&2))if(!visibility_changed(c,visible,e)){
    --n->blocked;
    return false;
   }
  }
  --n->blocked;
  return true;
 }
 bool FieldNodeTreeRuntime::set_z_index(FieldObjectId id,int32_t value,std::string&e){
  auto*n=live(id);
  if(!n || !(n->flags&1) || value < -4096 || value > 4096)
   return fail(e,"NodeTree actual CanvasItem z_index rejected");
  n->z=value;e.clear();return true;
 }
 bool FieldNodeTreeRuntime::set_visible(FieldObjectId id,bool value,std::string&e){
  auto*n=live(id);
  if(!n||!(n->flags&1))return fail(e,"NodeTree Canvas visibility rejected");
  if(bool(n->flags&2)==value)return true;
  n->flags=value?n->flags|2:n->flags&~2u;
  if(!n->inside||(n->canvas_parent&&!visible_in_tree(n->canvas_parent)))return true;
  return visibility_changed(id,value,e);
 }
 bool FieldNodeTreeRuntime::set_behind_parent(FieldObjectId id,bool value,std::string&e){
  auto*n=live(id);
  if(!n||!(n->flags&1))return fail(e,"NodeTree Canvas draw-behind owner rejected");
  if(bool(n->flags&8)==value){e.clear();return true;}
  n->flags=value?n->flags|8:n->flags&~8u;
  ++order_version_;
  e.clear();return true;
 }
 bool FieldNodeTreeRuntime::effective_color(FieldObjectId id,FieldColor&out,std::string&e)const{
  auto*n=state(id);
  if(!n||!(n->flags&1))return fail(e,"NodeTree Canvas color rejected");
  out=n->self_modulate;
  while(n){
   for(size_t i=0;i<4;++i)out[i]*=n->modulate[i];
   if(!n->canvas_parent)break;
   n=state(n->canvas_parent);
  }
  if(!finite(out))return fail(e,"NodeTree Canvas color overflow");
  return true;
 }
 bool FieldNodeTreeRuntime::effective_z(FieldObjectId id,int32_t&out,std::string&e)const{
  auto*n=state(id);if(!n||!(n->flags&1))return fail(e,"NodeTree Canvas z owner rejected");
  std::vector<const FieldNodeState*>chain;while(n){chain.push_back(n);if(!n->canvas_parent)break;n=state(n->canvas_parent);}
  out=0;for(auto it=chain.rbegin();it!=chain.rend();++it){auto*v=*it;out=std::clamp<int32_t>((v->flags&64)?out+v->z:v->z,-4096,4096);}return true;
 }
 bool FieldNodeTreeRuntime::set_modulate(FieldObjectId id,const FieldColor&value,bool self,std::string&e){
  auto*n=live(id);
  if(!n||!(n->flags&1)||!finite(value))return fail(e,"NodeTree Canvas modulation rejected");
  if(self)n->self_modulate=value;
  else n->modulate=value;
  return true;
 }
 bool FieldNodeTreeRuntime::set_pause_mode(FieldObjectId id,uint32_t v,std::string&e){
  auto*n=live(id);
  if(!n||v>2)return fail(e,"NodeTree pause opcode rejected");
  n->pause=v;
  return true;
 }
 bool FieldNodeTreeRuntime::set_process_priority(FieldObjectId id,int32_t v,std::string&e){
  auto*n=live(id);
  if(!n)return fail(e,"NodeTree priority owner rejected");
  n->priority=v;
  return true;
 }
 bool FieldNodeTreeRuntime::set_input_process(FieldObjectId id,uint32_t kind,bool enabled,std::string&e){
  auto*n=live(id);if(!n||kind>2)return fail(e,"NodeTree input processing opcode/owner rejected");
  if(n->input_enabled[kind]==enabled)return true;
  if(n->inside&&!host_.input_registration(id,kind,enabled,e))return false;
  n->input_enabled[kind]=enabled;return true;
 }
 const FieldNodeDescriptor*FieldNodeTreeRuntime::descriptor(FieldObjectId id)const{auto*s=source(id);return state(id)&&s?&s->descriptor:nullptr;}
 bool FieldNodeTreeRuntime::object_identity(FieldObjectId id,FieldIdentity&out)const{auto*s=source(id);if(!state(id)||!s)return false;
 out=s->identity;return true;
 }
 bool FieldNodeTreeRuntime::can_process(FieldObjectId id,bool paused)const{
  auto*n=state(id);
  if(!n||!n->inside)return false;
  if(!paused)return true;
  while(n){
   if(n->pause)return n->pause==2;
   if(!n->parent)return host_.external_pause_process(n->object);
   n=state(n->parent);
  }
  return false;
 }
 std::vector<size_t>FieldNodeTreeRuntime::tree_key(FieldObjectId id)const{
  std::vector<size_t>key;
  auto*n=state(id);
  while(n&&n->parent){
   auto*p=state(n->parent);
   if(!p)break;
   key.push_back(size_t(std::find(p->children.begin(),p->children.end(),id)-p->children.begin()));
   id=n->parent;
   n=p;
  }
  std::reverse(key.begin(),key.end());
  return key;
 }
 bool FieldNodeTreeRuntime::group(std::string_view name,std::vector<FieldObjectId>&out,std::string&e)const{
  if(!text(name))return fail(e,"NodeTree group name rejected");
  auto&cache=group_cache_[std::string(name)];
  if(cache.version!=order_version_){
   cache.ordered.clear();auto members=group_index_.find(std::string(name));
   std::map<FieldObjectId,std::vector<size_t>>keys;
   if(members!=group_index_.end())for(auto id:members->second)if(state(id)&&state(id)->inside){cache.ordered.push_back(id);keys.emplace(id,tree_key(id));}
   std::sort(cache.ordered.begin(),cache.ordered.end(),[&](auto a,auto b){return keys.at(a)<keys.at(b);});cache.version=order_version_;
  }
  out=cache.ordered;return true;
 }
 bool FieldNodeTreeRuntime::add_group(FieldObjectId id,std::string_view name,std::string&e){
  auto*n=live(id);
  if(!n||!text(name))return fail(e,"NodeTree group owner/name rejected");
  if(std::find(n->groups.begin(),n->groups.end(),name)==n->groups.end()){
   n->groups.emplace_back(name);
   group_index_[std::string(name)].push_back(id);++order_version_;
  }
  return true;
 }
 bool FieldNodeTreeRuntime::remove_group(FieldObjectId id,std::string_view name,std::string&e){
  auto*n=live(id);
  if(!n||!text(name))return fail(e,"NodeTree group owner/name rejected");
  ++order_version_;
  n->groups.erase(std::remove(n->groups.begin(),n->groups.end(),name),n->groups.end());
  auto it=group_index_.find(std::string(name));
  if(it!=group_index_.end())it->second.erase(std::remove(it->second.begin(),it->second.end(),id),it->second.end());
  return true;
 }
 bool FieldNodeTreeRuntime::set_process(FieldObjectId id,bool physics,bool enabled,std::string&e){
  return enabled?add_group(id,physics?"physics_process":"idle_process",e):remove_group(id,physics?"physics_process":"idle_process",e);
 }
 bool FieldNodeTreeRuntime::process(bool physics,bool paused,std::string&e){
  if(poisoned_||!frames_.empty())return fail(e,"NodeTree process before admitted lifecycle rejected");
  for(unsigned pass=0;pass<2;++pass){
   std::vector<FieldObjectId>ids;
   const char*name=physics?(pass?"physics_process":"physics_process_internal"):(pass?"idle_process":"idle_process_internal");
   if(!group(name,ids,e))return false;
   // group() already has unique source tree order; a stable priority sort
   // preserves that exact secondary ordering without repeated path allocation.
   std::stable_sort(ids.begin(),ids.end(),[&](auto a,auto b){return state(a)->priority<state(b)->priority;});
   for(auto id:ids)if(can_process(id,paused)){
    auto phase=physics?(pass?FieldTreePhase::Physics:FieldTreePhase::PhysicsInternal):(pass?FieldTreePhase::Idle:FieldTreePhase::IdleInternal);
    if(!emit(id,phase,e))return false;
   }
  }
  return true;
 }
 bool FieldNodeTreeRuntime::dispatch_input(FieldObjectId id,uint32_t kind,bool paused,std::string&e){
  auto*n=live(id);if(!n||kind>2||!n->input_enabled[kind]||!can_process(id,paused)||!frames_.empty())return fail(e,"NodeTree actual Viewport input owner rejected");
  auto phase=kind==0?FieldTreePhase::Input:kind==1?FieldTreePhase::UnhandledInput:FieldTreePhase::UnhandledKeyInput;return emit(id,phase,e);
 }
 bool FieldNodeTreeRuntime::enqueue(FieldDeferredMessage m,std::string&e){
  if(poisoned_||!m.object||uint32_t(m.kind)>2||messages_.size()>=65536||m.args.size()>64)return fail(e,"NodeTree deferred message budget/opcode rejected");
  if((m.kind!=FieldDeferredKind::Notification&&!text(m.member))||(m.kind==FieldDeferredKind::Set&&m.args.size()!=1)||(m.kind==FieldDeferredKind::Notification&&(!m.args.empty()||m.notification<0)))return fail(e,"NodeTree deferred signature rejected");
  for(const auto&v:m.args){
   if(auto*f=std::get_if<double>(&v);f&&!std::isfinite(*f))return fail(e,"NodeTree deferred nonfinite number");
   if(auto*f=std::get_if<Vec2>(&v);f&&(!std::isfinite(f->x)||!std::isfinite(f->y)))return fail(e,"NodeTree deferred nonfinite vector");
   if(auto*s=std::get_if<std::string>(&v);s&&(s->size()>65536||s->find('\0')!=s->npos))return fail(e,"NodeTree deferred string rejected");
  }
  if(host_.enqueue_global)return host_.enqueue_global(std::move(m),e);
  messages_.push_back(std::move(m));
  return true;
 }
 bool FieldNodeTreeRuntime::flush_messages(std::string&e){
  if(host_.flush_global)return host_.flush_global(e);
  if(poisoned_||flushing_)return fail(e,"NodeTree reentrant/poisoned message flush rejected");
  flushing_=true;
  size_t done=0;
  while(!messages_.empty()){
   std::deque<FieldDeferredMessage>read;
   read.swap(messages_);
   while(!read.empty()){
    auto m=std::move(read.front());
    read.pop_front();
    if(++done>1000000){
     flushing_=false;
     poisoned_=true;
     return fail(e,"NodeTree deferred recursion budget rejected");
    }
    // Native ObjectDB silently discards a message whose Object has died.
    if(host_.object_exists(m.object)&&!host_.deferred(m,e)){
     flushing_=false;
     poisoned_=true;
     return false;
    }
   }
  }
  flushing_=false;
  return true;
 }
 bool FieldNodeTreeRuntime::queue_free(FieldObjectId id,std::string&e){
  auto*n=live(id);
  if(!n)return fail(e,"NodeTree queue_free dead owner");
  if(n->queued)return true;
  n->queued=true;
  int32_t key=-1;
  if(auto*p=state(n->parent)){
   key=int32_t(std::find(p->children.begin(),p->children.end(),id)-p->children.begin());
   key+=int32_t(uint32_t(p->object*937u)&~(7u<<29));
  }
  deletes_.push_back({
   id,key
  }
  );
  return true;
 }
 bool FieldNodeTreeRuntime::destroy(FieldObjectId id,std::string&e){
  auto*n=live(id);
  if(!n)return true;
  if(n->blocked)return fail(e,"NodeTree delete blocked owner rejected");
  if(n->inside&&!branch(id,2,e))return false;
  if(n->parent&&!detach(n->parent,id,e))return false;
  // Original Node PREDELETE removes this node from its parent first.
  while((n=live(id))&&!n->children.empty()){auto child=n->children.back();if(!destroy(child,e))return false;
 }
  n=live(id);if(!n)return true;
  if(!emit(id,FieldTreePhase::Deleting,e))return false;
  n=live(id);
  if(!n)return true;
  auto binding=n->binding;
  if(!host_.release(id,binding,e))return false;
  auto source_id=n->source;
  auto groups=n->groups;
  for(const auto&g:groups){auto it=group_index_.find(g);if(it!=group_index_.end())it->second.erase(std::remove(it->second.begin(),it->second.end(),id),it->second.end());}
  erase_transform(id);
  auto original=source_index_.find(source_id);if(original!=source_index_.end()&&original->second==id)source_index_.erase(original);
  sources_.erase(id);nodes_.erase(id);++order_version_;
  return true;
 }
 bool FieldNodeTreeRuntime::flush_delete_queue(std::string&e){
  if(poisoned_||deleting_||driving_||!frames_.empty())return fail(e,"NodeTree delete flush state rejected");
  deleting_=true;
  struct Compare {
   bool operator()(const Deletion&a,const Deletion&b)const{
    return a.key>b.key;
   }
  }
  ;
  FieldNodeSort<Deletion,Compare>().sort(deletes_.data(),int(deletes_.size()));
  for(size_t i=0;i<deletes_.size();++i)if(!destroy(deletes_[i].id,e)){
   deleting_=false;
   poisoned_=true;
   return false;
  }
  deletes_.clear();
  deleting_=false;
  return true;
 }
}
