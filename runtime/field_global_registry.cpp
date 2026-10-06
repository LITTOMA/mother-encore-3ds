#include "encore/field_global_registry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool same_identity(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
bool member_name(std::string_view s){return !s.empty()&&s.size()<=65536&&s.find('\0')==s.npos;}
}
bool FieldGlobalRegistry::equal_spec(const FieldGlobalExternalSpec&a,const FieldGlobalExternalSpec&b)const{
 return same_identity(a.identity,b.identity)&&a.stable_id==b.stable_id&&a.role==b.role&&a.name==b.name&&a.native_class==b.native_class&&a.source==b.source&&a.script==b.script&&a.source_sha==b.source_sha&&a.script_sha==b.script_sha;
}
bool FieldGlobalRegistry::allocate_object(FieldObjectId&out,std::string&e){
 if(!data_||poisoned_||counter_==std::numeric_limits<uint64_t>::max())return fail(e,"Global ObjectDB counter unavailable/overflow");
 out=++counter_;objects_.emplace(out,Object{});e.clear();return true;
}
bool FieldGlobalRegistry::allocate_fast_name(uint64_t&out,std::string&e){
 if(!data_||poisoned_||fast_counter_==std::numeric_limits<uint64_t>::max())return fail(e,"Global native name counter unavailable/overflow");
 out=++fast_counter_;e.clear();return true;
}
bool FieldGlobalRegistry::object_exists(FieldObjectId id)const{
 auto i=objects_.find(id);if(i==objects_.end())return false;
 return bool(i->second.external)||!i->second.reference.expired()||(i->second.tree&&i->second.tree->state(id));
}
std::shared_ptr<FieldNodeTreeRuntime>FieldGlobalRegistry::tree_owner(FieldObjectId id)const{
 auto i=objects_.find(id);return i==objects_.end()||!i->second.tree||!i->second.tree->state(id)?nullptr:i->second.tree;
}
const FieldGlobalSourceResource*FieldGlobalRegistry::source_resource(FieldObjectId id)const{
 auto i=objects_.find(id);return i==objects_.end()||!i->second.external||i->second.definition==0?nullptr:i->second.resource;
}
std::shared_ptr<const FieldGlobalNativeReference>FieldGlobalRegistry::native_reference(FieldObjectId id)const{
 auto i=objects_.find(id);return i==objects_.end()?nullptr:i->second.reference.lock();
}
bool FieldGlobalRegistry::publish_native_reference(const FieldGlobalExternalSpec&spec,FieldObjectId id,const std::shared_ptr<FieldGlobalNativeReference>&owner,std::string&e){
 auto i=objects_.find(id);
 if(!initialized_||poisoned_||!owner||!id||i==objects_.end()||i->second.external||i->second.tree||i->second.reference_published||owner->registry()!=this||!spec.stable_id||spec.role!=5||spec.identity.upstream_commit!=data_->identity().upstream_commit||spec.source.empty()||spec.script!=spec.source||spec.source_sha!=spec.script_sha||spec.identity.source_sha256!=spec.source_sha||std::all_of(spec.source_sha.begin(),spec.source_sha.end(),[](uint8_t v){return !v;}))return fail(e,"Global actual native Reference slot/source rejected");
 auto type=owner->native_class();auto b=owner->binding();
 const bool supported=(spec.native_class=="Directory"&&b.family==0x454e0051)||((spec.native_class=="File"||spec.native_class=="Reference")&&b.family==0x454e0052);
 std::array<uint8_t,32>source{},namespace_source{};
 if(!type||spec.native_class!=type||!supported||b.capability!=1||!owner->checked_source_hash(spec.source,source)||source!=spec.source_sha||b.object!=id||!equal_spec(b.source,spec))return fail(e,"Global native Reference actual owner/domain proof rejected");
 // Nested source classes have their own checked resource closure. Namespace
 // declarations need not duplicate every nested script dependency.
 if(data_->source_hash(spec.source,namespace_source)&&namespace_source!=source)return fail(e,"Global native Reference conflicts with namespace source");
 i->second.reference=owner;i->second.reference_published=true;i->second.definition=spec.stable_id;e.clear();return true;
}
bool FieldGlobalRegistry::publish_source_resource(const FieldGlobalExternalSpec&spec,FieldObjectId id,std::unique_ptr<FieldGlobalSourceResource>owner,std::string&e){
 auto i=objects_.find(id);
 if(!initialized_||poisoned_||!owner||i==objects_.end()||i->second.external||i->second.tree||i->second.reference_published||!id||!spec.stable_id||spec.role!=4||spec.identity.upstream_commit!=data_->identity().upstream_commit||spec.source.empty()||!spec.script.empty()||!std::all_of(spec.script_sha.begin(),spec.script_sha.end(),[](uint8_t v){return !v;}))return fail(e,"Global actual source Resource slot/identity rejected");
 if((spec.native_class!="PackedScene"&&spec.native_class!="ShaderMaterial")||spec.native_class!=owner->resource_class()||spec.source_sha!=spec.identity.source_sha256||std::all_of(spec.source_sha.begin(),spec.source_sha.end(),[](uint8_t v){return !v;}))return fail(e,"Global source Resource native type/proof rejected");
 auto b=owner->binding();FieldGlobalExternalState s;
 if(b.object!=id||!b.family||!b.capability||!equal_spec(b.source,spec)||!owner->state(s,e)||s.name!=spec.name||s.parent||!s.children.empty()||s.inside||s.ready||s.ui_before_canvas||s.current_scene||s.stable_canvas)return fail(e,"Global source Resource actual non-Node owner rejected");
 i->second.resource=owner.get();i->second.external=std::move(owner);i->second.definition=spec.stable_id;e.clear();return true;
}
bool FieldGlobalRegistry::construct(const FieldGlobalExternalSpec&spec,FieldObjectId&out,std::string&e){
 if(!host_.construct)return fail(e,"Global actual external owner constructor missing");
 FieldObjectId id=0;if(!allocate_object(id,e))return false;
 std::unique_ptr<FieldGlobalExternalObject>owner;
 if(!host_.construct(id,spec,owner,e)||!owner){objects_.erase(id);return fail(e,"Global actual external owner constructor rejected");}
 auto binding=owner->binding();
 if(binding.object!=id||!binding.family||!binding.capability||!equal_spec(binding.source,spec)){objects_.erase(id);return fail(e,"Global external owner source/family receipt rejected");}
 FieldGlobalExternalState state;
 if(!owner->state(state,e)||state.name!=spec.name||state.parent||!state.children.empty()||state.inside||state.ready){objects_.erase(id);return fail(e,"Global external constructor must expose actual out-of-tree pending state");}
 auto&slot=objects_.at(id);slot.external=std::move(owner);slot.definition=spec.stable_id;out=id;return true;
}
bool FieldGlobalRegistry::initialize(const FieldGlobalRegistryData&d,FieldGlobalRegistryHost h,std::string&e){
 if(data_||!d.valid()||!h.construct)return fail(e,"Global registry data/typed host/initial ownership rejected");
 data_=&d;host_=std::move(h);counter_=d.initial_object_counter();fast_counter_=d.initial_fast_name_counter();
 FieldGlobalExternalSpec spec;spec.identity=d.identity();spec.role=1;spec.native_class=d.kernel_native();
 if(!construct(spec,kernel_,e)){poisoned_=true;return false;}
 spec.role=2;spec.name=d.root_name();spec.native_class=d.root_native();
 if(!construct(spec,root_,e)){poisoned_=true;return false;}
 initialized_=true;e.clear();return true;
}
bool FieldGlobalRegistry::snapshot(FieldObjectId id,FieldGlobalExternalState&out,std::string&e)const{
 auto i=objects_.find(id);if(i==objects_.end())return fail(e,"Global native object is dead or foreign");
 const auto&o=i->second;
 if(o.external){if(!o.external->state(out,e))return false;}
 else if(o.tree){
  auto*n=o.tree->state(id);if(!n)return fail(e,"Global source Tree object already freed");
  out={};out.name=n->name;out.parent=n->parent?n->parent:o.external_parent;out.children=n->children;out.inside=n->inside;out.ready=n->ready_notified;
 }else return fail(e,"Global ObjectDB allocation has no actual owner");
 if(out.name.find_first_of("/\\:")!=out.name.npos)return fail(e,"Global actual native name rejected");
 std::set<FieldObjectId>children;
 for(auto child:out.children)if(!child||!object_exists(child)||!children.insert(child).second)return fail(e,"Global actual child list contains dead/foreign/duplicate ObjectID");
 if(out.parent&&!object_exists(out.parent))return fail(e,"Global actual parent is dead or foreign");
 return true;
}
bool FieldGlobalRegistry::publish_branch(std::shared_ptr<FieldNodeTreeRuntime>tree,FieldObjectId first,NodeDispatch dispatch,std::string&e){
 if(!initialized_||poisoned_||!tree||tree->object_domain()!=kernel_||!tree->state(first)||!dispatch)return fail(e,"Global checked Tree branch owner unavailable");
 std::vector<FieldObjectId>pending{first};std::set<FieldObjectId>seen;
 for(size_t at=0;at<pending.size();++at){
  auto id=pending[at];auto*n=tree->state(id);FieldIdentity identity;
  auto i=objects_.find(id);
  if(!seen.insert(id).second||!n||!tree->descriptor(id)||!tree->object_identity(id,identity)||identity.upstream_commit!=data_->identity().upstream_commit||i==objects_.end()||i->second.external||i->second.reference_published||(i->second.tree&&i->second.tree!=tree))return fail(e,"Global branch is not actual same-domain checked source objects");
  for(auto child:n->children){auto*c=tree->state(child);if(!c||c->parent!=id)return fail(e,"Global branch source parent/child mismatch");pending.push_back(child);}
 }
 for(auto id:pending){auto&slot=objects_.at(id);slot.tree=tree;slot.dispatch=dispatch;slot.definition=tree->state(id)->source;}
 e.clear();return true;
}
bool FieldGlobalRegistry::construct_autoload(uint32_t stable,std::string&e){
 if(!initialized_||poisoned_||autoload_objects_.count(stable))return fail(e,"Global autoload already constructed/unavailable");
 auto i=std::find_if(data_->autoloads().begin(),data_->autoloads().end(),[&](const auto&a){return a.id==stable;});
 if(i==data_->autoloads().end())return fail(e,"Global autoload source declaration missing");
 // Source constructors have project order. A missing earlier typed owner is
 // an actual startup stop, never permission to skip to a later singleton.
 for(const auto&a:data_->autoloads())if(a.ordinal<i->ordinal&&!autoload_objects_.count(a.id))return fail(e,"Global earlier autoload actual constructor pending");
 FieldGlobalExternalSpec spec;spec.identity=data_->identity();spec.stable_id=i->id;spec.role=3;spec.name=i->name;spec.native_class=i->native_class;spec.source=i->path;spec.script=i->script;spec.source_sha=i->source_sha;spec.script_sha=i->script_sha;
 FieldObjectId id=0;if(!construct(spec,id,e))return false;autoload_objects_.emplace(stable,id);e.clear();return true;
}
bool FieldGlobalRegistry::attach_autoload(uint32_t stable,std::string&e){
 auto i=autoload_objects_.find(stable);
 if(!initialized_||poisoned_||i==autoload_objects_.end())return fail(e,"Global autoload actual source owner unavailable");
 FieldGlobalExternalState viewport,node;
 if(!snapshot(root_,viewport,e)||!viewport.inside||!snapshot(i->second,node,e)||node.parent||node.inside)return fail(e,"Global autoload actual enter boundary pending");
 for(const auto&a:data_->autoloads()){
  if(a.id==stable)break;
  auto before=autoload_objects_.find(a.id);FieldGlobalExternalState state;
  if(before==autoload_objects_.end()||!snapshot(before->second,state,e)||state.parent!=root_||!state.inside)return fail(e,"Global earlier autoload native enter pending");
 }
 FieldDeferredMessage m;m.object=root_;m.member="add_child";m.args.emplace_back(FieldObjectRef{i->second});
 if(!objects_.at(root_).external->deferred(m,e))return false;
 if(!snapshot(root_,viewport,e)||std::count(viewport.children.begin(),viewport.children.end(),i->second)!=1||!snapshot(i->second,node,e)||node.parent!=root_||!node.inside)return fail(e,"Global autoload add_child did not enter actual native owner");
 e.clear();return true;
}
bool FieldGlobalRegistry::observe_external_parent(FieldObjectId parent,FieldObjectId child,std::string&e){
 auto i=objects_.find(child);auto root=objects_.find(root_);
 if(!initialized_||poisoned_||!child||child==root_||child==kernel_||i==objects_.end()||!i->second.tree||root==objects_.end()||!root->second.external||i->second.tree->object_domain()!=kernel_||i->second.tree->root()!=child)return fail(e,"Global actual external parent source Tree/root binding rejected");
 auto*n=i->second.tree->state(child);FieldIdentity identity;FieldGlobalExternalState viewport;
 if(!n||!n->alive||n->parent||!i->second.tree->object_identity(child,identity)||identity.upstream_commit!=data_->identity().upstream_commit||!snapshot(root_,viewport,e))return fail(e,"Global actual external parent source object rejected");
 auto count=std::count(viewport.children.begin(),viewport.children.end(),child);
 if(parent){
  if(parent!=root_||count!=1||(i->second.external_parent&&i->second.external_parent!=parent))return fail(e,"Global external parent not backed by actual native child insertion");
 }else{
  if(i->second.external_parent!=root_||count||n->inside)return fail(e,"Global external parent clear precedes actual native removal/exit");
 }
 i->second.external_parent=parent;e.clear();return true;
}
bool FieldGlobalRegistry::attach_scene(FieldObjectId id,std::string&e){
 if(!initialized_||poisoned_||!object_exists(id)||id==root_||id==kernel_)return fail(e,"Global scene attach object unavailable");
 auto i=objects_.find(id);if(!i->second.tree||i->second.tree->state(id)->parent||i->second.external_parent)return fail(e,"Global source scene is already parented/not a Tree root");
 FieldGlobalExternalState viewport,scene;
 if(!snapshot(root_,viewport,e)||!viewport.inside||!snapshot(id,scene,e)||scene.inside)return fail(e,"Global actual Viewport/source scene enter boundary pending");
 FieldDeferredMessage m;m.object=root_;m.member="add_child";m.args.emplace_back(FieldObjectRef{id});
 if(!objects_.at(root_).external->deferred(m,e))return false;
 if(!snapshot(root_,viewport,e)||std::count(viewport.children.begin(),viewport.children.end(),id)!=1||!i->second.tree->state(id)||!i->second.tree->state(id)->inside)return fail(e,"Global Viewport add_child did not attach/enter actual source branch");
 i->second.external_parent=root_;e.clear();return true;
}
bool FieldGlobalRegistry::detach_scene(FieldObjectId id,std::string&e){
 auto i=objects_.find(id);
 if(!initialized_||poisoned_||i==objects_.end()||!i->second.tree||i->second.external_parent!=root_)return fail(e,"Global source scene detach actual parent missing");
 FieldGlobalExternalState viewport;
 if(!snapshot(root_,viewport,e)||std::count(viewport.children.begin(),viewport.children.end(),id)!=1)return fail(e,"Global actual Viewport child is absent/ambiguous");
 FieldDeferredMessage m;m.object=root_;m.member="remove_child";m.args.emplace_back(FieldObjectRef{id});
 if(!objects_.at(root_).external->deferred(m,e))return false;
 if(!snapshot(root_,viewport,e)||std::find(viewport.children.begin(),viewport.children.end(),id)!=viewport.children.end()||!i->second.tree->state(id)||i->second.tree->state(id)->inside)return fail(e,"Global actual scene remove_child/exit pending");
 i->second.external_parent=0;e.clear();return true;
}
bool FieldGlobalRegistry::select_current_scene(FieldNodeTreeRuntime&tree,FieldObjectId id,std::string&e){
 auto i=objects_.find(id);auto g=autoload_objects_.find(data_?data_->global_autoload():0);
 if(!initialized_||poisoned_||i==objects_.end()||i->second.tree.get()!=&tree||g==autoload_objects_.end())return fail(e,"Global actual currentScene owner unavailable");
 FieldGlobalExternalState viewport,global,scene;
 if(!snapshot(root_,viewport,e)||viewport.children.empty()||viewport.children.back()!=id||!snapshot(g->second,global,e)||global.current_scene!=id||!snapshot(id,scene,e)||scene.parent!=root_||!scene.inside)return fail(e,"Global source currentScene must equal actual last Viewport child/typed global state");
 if(current_scene_&&current_scene_!=id&&object_exists(current_scene_))return fail(e,"Global previous source scene free boundary pending");
 current_scene_=id;e.clear();return true;
}
bool FieldGlobalRegistry::observe_global_current_scene(FieldObjectId id,std::string&e){
 auto i=objects_.find(id);auto global=autoload_objects_.find(data_?data_->global_autoload():0);
 if(!initialized_||poisoned_||i==objects_.end()||!i->second.tree||global==autoload_objects_.end())return fail(e,"Global source currentScene candidate/owner unavailable");
 const auto*node=i->second.tree->state(id);FieldGlobalExternalState state;
 if(!node||node->parent||!snapshot(global->second,state,e)||state.current_scene!=id)return fail(e,"Global source currentScene actual assignment not observed");
 // The source assignment precedes candidate add_child and Ready. The port
 // must not impose inside-tree or native SceneTree.current_scene here.
 current_scene_=id;e.clear();return true;
}
bool FieldGlobalRegistry::observe_tree_current_scene(FieldObjectId id,std::string&e){
 auto i=objects_.find(id);FieldGlobalExternalState kernel,scene;
 if(!initialized_||poisoned_||id!=current_scene_||i==objects_.end()||!i->second.tree||!snapshot(kernel_,kernel,e)||kernel.current_scene!=id||!snapshot(id,scene,e)||!scene.inside||scene.parent!=root_)return fail(e,"Global actual SceneTree current scene assignment pending");
 tree_current_scene_=id;e.clear();return true;
}
bool FieldGlobalRegistry::observe_bootstrap_tree_current_scene(FieldObjectId id,std::string&e){
 auto i=objects_.find(id);FieldGlobalExternalState kernel,viewport;
 if(!initialized_||poisoned_||current_scene_||tree_current_scene_||i==objects_.end()||!i->second.tree||i->second.external_parent||i->second.tree->root()!=id||i->second.tree->object_domain()!=kernel_||i->second.tree->lifecycle_pending()||!snapshot(kernel_,kernel,e)||kernel.current_scene!=id||!snapshot(root_,viewport,e)||viewport.inside||viewport.ready||viewport.children.size()!=data_->autoloads().size())return fail(e,"Global original bootstrap SceneTree assignment boundary rejected");
 const auto*n=i->second.tree->state(id);FieldIdentity identity;std::array<uint8_t,32>sha;
 if(!n||!n->alive||n->parent||n->inside||n->ready_notified||!i->second.tree->object_identity(id,identity)||identity.upstream_commit!=data_->identity().upstream_commit||!data_->source_hash(data_->main_scene(),sha)||identity.source_sha256!=sha)return fail(e,"Global bootstrap original main scene source rejected");
 for(size_t at=0;at<data_->autoloads().size();++at){
  auto a=autoload_objects_.find(data_->autoloads()[at].id);FieldGlobalExternalState state;
  if(a==autoload_objects_.end()||viewport.children[at]!=a->second||!snapshot(a->second,state,e)||state.parent!=root_||state.inside||state.ready)return fail(e,"Global bootstrap complete source autoload staging order rejected");
 }
 tree_current_scene_=id;e.clear();return true;
}
bool FieldGlobalRegistry::create_stable_canvas(FieldObjectId&out,std::string&e){
 if(!initialized_||poisoned_||stable_canvas_||!current_scene_)return fail(e,"Global source mainCanvas creation boundary unavailable");
 auto u=autoload_objects_.find(data_->ui_autoload()),g=autoload_objects_.find(data_->global_autoload());
 if(u==autoload_objects_.end()||g==autoload_objects_.end())return fail(e,"Global UiManager/global actual constructors pending");
 FieldGlobalExternalState ui,global,scene;
 if(!snapshot(u->second,ui,e)||!ui.ui_before_canvas||ui.stable_canvas||!snapshot(g->second,global,e)||global.current_scene!=current_scene_||!snapshot(current_scene_,scene,e)||!scene.inside)return fail(e,"Global UiManager original onready/Ready prefix not actually complete");
 auto source=objects_.at(current_scene_).tree;auto dispatch=objects_.at(current_scene_).dispatch;FieldObjectId canvas=0;
 if(!source->instantiate_recipe(data_->canvas_recipe(),canvas,e)||!publish_branch(source,canvas,dispatch,e))return false;
 // This is the original immediate add_child, followed by append (not dedup)
 // to the real global persistent array, then assignment of the same instance.
 // Cold-start main has entered but not reached its own Ready. Node::_set_tree
 // propagates Ready only when the actual parent.ready_notified is already true.
 if(!source->add_child(current_scene_,canvas,e))return false;
 auto*n=source->state(canvas);
 auto*parent=source->state(current_scene_);
 if(!n||!parent||n->parent!=current_scene_||!n->inside||n->ready_notified!=parent->ready_notified)return fail(e,"Global mainCanvas actual parent-dependent Enter/Ready differs");
 if(!objects_.at(g->second).external->persist_append(canvas,e)||!objects_.at(u->second).external->assign_stable_canvas(canvas,e))return false;
 if(!snapshot(u->second,ui,e)||ui.stable_canvas!=canvas)return fail(e,"Global UiManager did not retain actual persistent mainCanvas ObjectID");
 stable_canvas_=canvas;out=canvas;e.clear();return true;
}
bool FieldGlobalRegistry::persistent_reparent(FieldObjectId id,FieldObjectId parent,std::string&e){
 auto i=objects_.find(id),p=objects_.find(parent);
 if(poisoned_||i==objects_.end()||p==objects_.end()||!i->second.tree||!p->second.tree)return fail(e,"Global persistent reparent actual Tree owners missing");
 auto original=i->second.tree;auto destination=p->second.tree;auto*n=original->state(id);
 if(!n)return fail(e,"Global persistent source node dead");
 if(n->parent&&!original->remove_child(n->parent,id,e))return false;
 if(original!=destination){
  std::vector<FieldObjectId>pending{id};
  for(size_t at=0;at<pending.size();++at){
   auto*c=original->state(pending[at]);auto slot=objects_.find(pending[at]);
   if(!c||slot==objects_.end()||slot->second.tree!=original)return fail(e,"Global persistent actual subtree ownership mismatch");
   pending.insert(pending.end(),c->children.begin(),c->children.end());
  }
  if(!original->transfer_detached_subtree(*destination,id,e))return false;
  for(auto child:pending){auto&slot=objects_.at(child);slot.tree=destination;slot.dispatch=p->second.dispatch;slot.external_parent=0;}
 }
 return destination->add_child(parent,id,e);
}
bool FieldGlobalRegistry::retire_object(FieldObjectId id,std::string&e){
 auto i=objects_.find(id);if(i==objects_.end()||id==root_||id==kernel_)return fail(e,"Global retire unknown/native kernel/root rejected");
 if(!i->second.reference.expired())return fail(e,"Global retire cannot discard live source Reference");
 if(i->second.tree&&i->second.tree->state(id))return fail(e,"Global retire cannot discard live source node");
 if(i->second.external){FieldGlobalExternalState s;if(!snapshot(id,s,e)||s.inside||s.parent||!s.children.empty())return fail(e,"Global retire actual external owner still attached");}
 for(auto a=autoload_objects_.begin();a!=autoload_objects_.end();){if(a->second==id)a=autoload_objects_.erase(a);else++a;}
 objects_.erase(i);e.clear();return true;
}
size_t FieldGlobalRegistry::collect_dead_tree_objects(){
 size_t removed=0;
 for(auto i=objects_.begin();i!=objects_.end();){
  if(i->second.tree&&!i->second.tree->state(i->first)){i=objects_.erase(i);++removed;}else ++i;
 }
 return removed;
}
bool FieldGlobalRegistry::lookup_absolute(std::string_view path,FieldObjectId&out,std::string&e)const{
 if(path.empty()||path.front()!='/')return fail(e,"Global absolute NodePath signature rejected");
 return resolve_path(root_,path,out,e);
}
bool FieldGlobalRegistry::resolve_path(FieldObjectId from,std::string_view path,FieldObjectId&out,std::string&e)const{
 if(!initialized_||poisoned_||!object_exists(from)||path.empty()||path.find('\\')!=path.npos||path.find('\0')!=path.npos)return fail(e,"Global live NodePath signature rejected");
 auto sub=path.find(':');if(sub!=path.npos)path=path.substr(0,sub);
 if(path.empty()){out=from;e.clear();return true;}
 FieldObjectId id=from;size_t begin=0;
 if(path.front()=='/'){
  FieldGlobalExternalState current;if(!snapshot(from,current,e)||!current.inside)return fail(e,"Global absolute NodePath requires actual tree membership");
  begin=1;auto end=path.find('/',begin);if(end==path.npos)end=path.size();
  if(path.substr(begin,end-begin)!=data_->root_name())return fail(e,"Global NodePath actual source root name rejected");
  id=root_;begin=end+1;
 }
 while(begin<path.size()){
  auto end=path.find('/',begin);if(end==path.npos)end=path.size();auto part=path.substr(begin,end-begin);begin=end+1;
  if(part.empty()||part==".")continue;
  FieldGlobalExternalState state;if(!snapshot(id,state,e))return false;
  if(part==".."){if(!state.parent)return fail(e,"Global NodePath cannot climb past actual root");id=state.parent;continue;}
  if(part.front()=='%')return fail(e,"Global unique-owner NodePath capability pending");
  FieldObjectId found=0;
  for(auto child:state.children){FieldGlobalExternalState c;if(!snapshot(child,c,e))return false;if(c.parent!=id)return fail(e,"Global actual native child parent mismatch");if(c.name==part){if(found)return fail(e,"Global actual sibling name ambiguity");found=child;}}
  if(!found)return fail(e,"Global actual NodePath target absent/pending");
  id=found;
 }
 out=id;e.clear();return true;
}
bool FieldGlobalRegistry::get_path_to(FieldObjectId a,FieldObjectId b,std::string&out,std::string&e)const{
 std::vector<FieldObjectId>aa,bb;
 auto collect=[&](FieldObjectId id,std::vector<FieldObjectId>&ids){
  std::set<FieldObjectId>seen;
  while(id){if(!seen.insert(id).second)return fail(e,"Global NodePath actual parent cycle");FieldGlobalExternalState s;if(!snapshot(id,s,e)||!s.inside)return fail(e,"Global relative NodePath requires actual inside-tree nodes");ids.push_back(id);id=s.parent;}
  return true;
 };
 if(!collect(a,aa)||!collect(b,bb)||aa.empty()||bb.empty()||aa.back()!=root_||bb.back()!=root_)return fail(e,"Global actual NodePath owners are in different trees");
 while(!aa.empty()&&!bb.empty()&&aa.back()==bb.back()){aa.pop_back();bb.pop_back();}
 out.clear();for(size_t i=0;i<aa.size();++i){if(!out.empty())out+='/';out+="..";}
 for(auto i=bb.rbegin();i!=bb.rend();++i){FieldGlobalExternalState s;if(!snapshot(*i,s,e))return false;if(!out.empty())out+='/';out+=s.name;}
 if(out.empty())out=".";
 e.clear();return true;
}
bool FieldGlobalRegistry::get_path(FieldObjectId id,std::string&out,std::string&e)const{
 std::vector<std::string>names;std::set<FieldObjectId>seen;
 while(id){if(!seen.insert(id).second)return fail(e,"Global actual ancestor cycle rejected");FieldGlobalExternalState s;if(!snapshot(id,s,e)||!s.inside)return fail(e,"Global source path requires actual inside-tree node");names.push_back(s.name);if(id==root_)break;id=s.parent;}
 if(!id)return fail(e,"Global actual path does not reach source Viewport root");
 out.clear();for(auto i=names.rbegin();i!=names.rend();++i){out+='/';out+=*i;}e.clear();return true;
}
bool FieldGlobalRegistry::enqueue(FieldDeferredMessage m,std::string&e){
 if(!initialized_||poisoned_||!m.object||uint32_t(m.kind)>2||messages_.size()>=65536||m.args.size()>64)return fail(e,"Global MessageQueue budget/opcode rejected");
 if((m.kind!=FieldDeferredKind::Notification&&!member_name(m.member))||(m.kind==FieldDeferredKind::Set&&m.args.size()!=1)||(m.kind==FieldDeferredKind::Notification&&(!m.args.empty()||m.notification<0)))return fail(e,"Global MessageQueue signature rejected");
 for(const auto&v:m.args){if(auto*f=std::get_if<double>(&v);f&&!std::isfinite(*f))return fail(e,"Global deferred number nonfinite");if(auto*f=std::get_if<Vec2>(&v);f&&(!std::isfinite(f->x)||!std::isfinite(f->y)))return fail(e,"Global deferred vector nonfinite");if(auto*s=std::get_if<std::string>(&v);s&&(s->size()>65536||s->find('\0')!=s->npos))return fail(e,"Global deferred string rejected");}
 messages_.push_back(std::move(m));e.clear();return true;
}
bool FieldGlobalRegistry::dispatch(const FieldDeferredMessage&m,std::string&e){
 if(!object_exists(m.object))return true;
 auto&o=objects_.at(m.object);if(o.external)return o.external->deferred(m,e);
 if(!o.tree||!o.dispatch)return fail(e,"Global actual deferred typed owner pending");
 if(m.kind==FieldDeferredKind::Call){
  if(m.member=="queue_free"&&m.args.empty())return o.tree->queue_free(m.object,e);
  if(m.member=="request_ready"&&m.args.empty())return o.tree->request_ready(m.object,e);
  if(m.member=="add_to_group"||m.member=="remove_from_group"){
   if(m.args.empty()||!std::holds_alternative<std::string>(m.args[0]))return fail(e,"Global native group signature rejected");
   if(m.member=="add_to_group"&&m.args.size()==2){
    if(!std::holds_alternative<bool>(m.args[1])||std::get<bool>(m.args[1]))return fail(e,"Global persistent dynamic group behavior pending");
   }else if(m.args.size()!=1)return fail(e,"Global native group argument count rejected");
   const auto&group=std::get<std::string>(m.args[0]);
   return m.member=="add_to_group"?o.tree->add_group(m.object,group,e):o.tree->remove_group(m.object,group,e);
  }
  if(m.member=="add_child"||m.member=="remove_child"||m.member=="move_child"){
   if(m.args.empty()||!std::holds_alternative<FieldObjectRef>(m.args[0]))return fail(e,"Global native child call signature rejected");
   auto child=std::get<FieldObjectRef>(m.args[0]).id;auto c=objects_.find(child);
   if(!object_exists(child)||c==objects_.end()||c->second.tree!=o.tree)return fail(e,"Global native child call actual kernel ownership mismatch");
   if(m.member=="move_child"){
    if(m.args.size()!=2||!std::holds_alternative<int64_t>(m.args[1]))return fail(e,"Global native move_child signature rejected");
    auto index=std::get<int64_t>(m.args[1]);if(index<std::numeric_limits<int32_t>::min()||index>std::numeric_limits<int32_t>::max())return fail(e,"Global native child index overflow");return o.tree->move_child(m.object,child,int32_t(index),e);
   }
   if(m.args.size()!=1)return fail(e,"Global native child call optional legible-name behavior pending");
   return m.member=="add_child"?o.tree->add_child(m.object,child,e):o.tree->remove_child(m.object,child,e);
  }
 }
 return o.dispatch(m,e);
}
bool FieldGlobalRegistry::flush_messages(std::string&e){
 if(poisoned_||flushing_)return fail(e,"Global MessageQueue recursive/poisoned flush rejected");
 flushing_=true;size_t done=0;
 while(!messages_.empty()){
  std::deque<FieldDeferredMessage>read;read.swap(messages_);
  while(!read.empty()){
   auto m=std::move(read.front());read.pop_front();
   if(++done>1000000){flushing_=false;poisoned_=true;return fail(e,"Global MessageQueue recursion budget rejected");}
   if(!dispatch(m,e)){flushing_=false;poisoned_=true;return false;}
  }
 }
 flushing_=false;e.clear();return true;
}
}
