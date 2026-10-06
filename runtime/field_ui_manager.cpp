#include "encore/field_ui_manager.hpp"
#include "encore/field_battle_bg_resources.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool reject_node_call(std::string&e){return fail(e,"Source Resource is not a Node; lifecycle/deferred Node operation rejected");}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
}
FieldUiPackedScene::FieldUiPackedScene(FieldGlobalExternalBinding b,std::shared_ptr<const FieldNodeRecipeData>r):binding_(std::move(b)),recipe_(std::move(r)){}
bool FieldUiPackedScene::valid()const{return recipe_&&recipe_->valid()&&binding_.object&&binding_.source.native_class=="PackedScene"&&binding_.source.source==recipe_->source_scene()&&same(binding_.source.identity,recipe_->identity())&&binding_.source.source_sha==recipe_->identity().source_sha256;}
bool FieldUiPackedScene::state(FieldGlobalExternalState&s,std::string&e)const{if(!valid())return fail(e,"PackedScene actual complete checked recipe unavailable");s={};s.name=binding_.source.name;e.clear();return true;}
bool FieldUiPackedScene::deferred(const FieldDeferredMessage&,std::string&e){return reject_node_call(e);}
bool FieldUiPackedScene::persist_append(FieldObjectId,std::string&e){return reject_node_call(e);}
bool FieldUiPackedScene::assign_stable_canvas(FieldObjectId,std::string&e){return reject_node_call(e);}
bool FieldUiPackedScene::instance(FieldNodeTreeRuntime&t,FieldObjectId&id,std::string&e)const{if(!valid())return fail(e,"PackedScene source recipe rejected");return t.instantiate_recipe(*recipe_,id,e);}
FieldUiMenuShader::FieldUiMenuShader(FieldGlobalExternalBinding b,const FieldUiManagerData&d):binding_(std::move(b)),data_(&d),old_(d.old_colors()),new_(d.new_colors()){}
bool FieldUiMenuShader::state(FieldGlobalExternalState&s,std::string&e)const{std::array<uint8_t,32>h;if(!data_||!data_->valid()||binding_.source.native_class!="ShaderMaterial"||!data_->source_hash(binding_.source.source,h)||h!=binding_.source.source_sha)return fail(e,"Menu ShaderMaterial original source rejected");s={};s.name=binding_.source.name;e.clear();return true;}
bool FieldUiMenuShader::deferred(const FieldDeferredMessage&,std::string&e){return reject_node_call(e);}
bool FieldUiMenuShader::persist_append(FieldObjectId,std::string&e){return reject_node_call(e);}
bool FieldUiMenuShader::assign_stable_canvas(FieldObjectId,std::string&e){return reject_node_call(e);}
bool FieldUiMenuShader::set_flavor(int32_t index,std::string&e){
 if(!data_||!data_->valid())return fail(e,"Menu shader data unavailable");
 // Godot Array[-1] selects the last row when FLAVORS.find returns -1.
 if(index<0)index+=static_cast<int32_t>(data_->palette().size());
 if(index<0||static_cast<size_t>(index)>=data_->palette().size())return fail(e,"Source menu flavor Array index rejected");
 const auto&row=data_->palette()[static_cast<size_t>(index)];for(size_t i=0;i<8;++i){uint32_t h=row[i];new_[i]={float((h>>16)&255)/255.0f,float((h>>8)&255)/255.0f,float(h&255)/255.0f,1.0f};}e.clear();return true;
}
FieldColor FieldUiMenuShader::shade(FieldColor color)const{
 // Source shader uses four-component Euclidean distance and ordered else-if.
 for(size_t i=0;i<8;++i){float sum=0;for(size_t k=0;k<4;++k){float v=color[k]-old_[i][k];sum+=v*v;}if(std::sqrt(sum)<data_->color_distance_threshold())return new_[i];}return color;
}
bool FieldUiManagerRuntime::initialize(const FieldUiManagerData&d,FieldGlobalRegistry&r,SourceRandom&random,FieldGlobalExternalBinding b,FieldUiManagerHost h,std::string&e){
 if(data_||!d.valid()||!b.object||!b.source.stable_id||b.source.role!=3||b.source.script!=d.source_script()||b.source.script_sha!=d.identity().source_sha256||b.source.identity.upstream_commit!=d.identity().upstream_commit||b.source.native_class!="Node"||b.family!=0x454e0045||b.capability!=1||!h.tree||h.tree->object_domain()!=r.kernel()||!h.dispatch)return fail(e,"UiManager actual source/autoload/Tree domain rejected");
 data_=&d;registry_=&r;random_=&random;binding_=std::move(b);host_=std::move(h);e.clear();return true;
}
bool FieldUiManagerRuntime::state(FieldGlobalExternalState&s,std::string&e)const{if(!data_)return fail(e,"UiManager not initialized");s={};s.name=binding_.source.name;s.parent=parent_;s.inside=inside_;s.ready=ready_;s.ui_before_canvas=ready_cursor_>=4;s.stable_canvas=stable_;e.clear();return true;}
bool FieldUiManagerRuntime::deferred(const FieldDeferredMessage&,std::string&e){return fail(e,"UiManager deferred native method pending actual typed Node owner");}
bool FieldUiManagerRuntime::persist_append(FieldObjectId,std::string&e){return fail(e,"UiManager does not own global persistNodes");}
bool FieldUiManagerRuntime::assign_stable_canvas(FieldObjectId id,std::string&e){
 if(!registry_||ready_cursor_<4||stable_||!registry_->object_exists(id))return fail(e,"UiManager stableCanvas source cursor/identity rejected");
 auto tree=registry_->tree_owner(id);auto*s=tree?tree->state(id):nullptr;if(!s||!s->inside||!s->ready_notified)return fail(e,"UiManager actual Canvas Ready pending");stable_=id;e.clear();return true;
}
bool FieldUiManagerRuntime::entered(FieldObjectId parent,std::string&e){if(!registry_||inside_||!parent||parent!=registry_->root()||!registry_->object_exists(parent))return fail(e,"UiManager actual native enter parent rejected");parent_=parent;inside_=true;e.clear();return true;}
bool FieldUiManagerRuntime::exited(std::string&e){if(!inside_)return fail(e,"UiManager native exit while outside tree");inside_=false;parent_=0;e.clear();return true;}
bool FieldUiManagerRuntime::preload(const FieldUiPreload&p,std::string&e){
 if(resources_.count(p.id))return true;
 FieldGlobalExternalSpec spec;spec.identity=data_->identity();spec.identity.scene_id=p.id;spec.identity.source_sha256=p.sha;spec.stable_id=p.id;spec.role=4;spec.name=p.name;spec.native_class=p.native_class;spec.source=p.path;spec.source_sha=p.sha;
 std::shared_ptr<const FieldNodeRecipeData>recipe;
 if(p.native_class=="PackedScene"){
  if(auto*r=data_->recipe(p.path))recipe=std::make_shared<FieldNodeRecipeData>(*r);
  else if(!host_.load_recipe||!host_.load_recipe(p,recipe,e))return fail(e,"UiManager actual preload complete PackedScene recipe pending");
  if(!recipe||!recipe->valid()||recipe->source_scene()!=p.path||recipe->identity().upstream_commit!=data_->identity().upstream_commit||recipe->identity().source_sha256!=p.sha)return fail(e,"UiManager preload checked original recipe rejected");
  spec.identity=recipe->identity();
 }
 FieldObjectId id=0;if(!registry_->allocate_object(id,e))return false;
 FieldGlobalExternalBinding b;b.object=id;b.source=spec;b.family=0x454e0045;b.capability=1;
 std::unique_ptr<FieldGlobalSourceResource>owner;FieldUiPackedScene*packed=nullptr;FieldUiMenuShader*shader=nullptr;
 if(recipe){auto ptr=std::make_unique<FieldUiPackedScene>(b,recipe);packed=ptr.get();owner=std::move(ptr);}
 else{auto ptr=std::make_unique<FieldUiMenuShader>(b,*data_);shader=ptr.get();owner=std::move(ptr);}
 if(!registry_->publish_source_resource(spec,id,std::move(owner),e))return false;
 resources_.emplace(p.id,id);if(packed)packed_.emplace(p.id,packed);if(shader){if(shader_)return fail(e,"UiManager duplicate source ShaderMaterial rejected");shader_=shader;}e.clear();return true;
}
bool FieldUiManagerRuntime::initialize_fields(std::string&e){
 if(!data_||inside_)return fail(e,"UiManager source constructor outside Tree required");
 while(fields_cursor_<data_->preloads().size()){const auto&p=data_->preloads()[fields_cursor_];if(!p.onready&&!preload(p,e))return false;++fields_cursor_;}e.clear();return true;
}
bool FieldUiManagerRuntime::advance_ready(std::string&e){
 if(!data_||!inside_||fields_cursor_!=data_->preloads().size()||ready_)return fail(e,"UiManager source Ready construction/native enter pending");
 if(ready_cursor_==0){
  for(const auto&p:data_->preloads())if(p.onready&&!preload(p,e))return false;
  while(instances_cursor_<data_->instances().size()){
   const auto&a=data_->instances()[instances_cursor_];auto pi=packed_.find(a.resource);if(pi==packed_.end()||pi->second->recipe().identity().scene_id!=a.recipe)return fail(e,"UiManager onready actual PackedScene owner missing");
   FieldObjectId id=0;if(!pi->second->instance(*host_.tree,id,e))return false;
   auto*s=host_.tree->state(id);auto*d=host_.tree->descriptor(id);if(!s||!d||s->inside||s->parent||d->native_class!=a.native_class||!registry_->publish_branch(host_.tree,id,host_.dispatch,e))return fail(e,"UiManager actual onready ObjectDB instance rejected");instances_.emplace(a.name,id);++instances_cursor_;
  }ready_cursor_=1;
 }
 if(ready_cursor_==1){uint64_t seconds=0,ticks=0;if(!host_.clock||!host_.clock(seconds,ticks,e))return fail(e,"UiManager native randomize OS clock pending");random_->seed((seconds+ticks)*random_->state()+UINT64_C(1442695040888963407));ready_cursor_=2;}
 if(ready_cursor_==2){std::string flavor;if(!shader_||!host_.menu_flavor||!host_.menu_flavor(flavor,e))return fail(e,"UiManager actual globaldata/menu shader pending");auto i=std::find(data_->flavors().begin(),data_->flavors().end(),flavor);int32_t index=i==data_->flavors().end()?-1:static_cast<int32_t>(i-data_->flavors().begin());if(!shader_->set_flavor(index,e)||!host_.emit_menu_flavor_updated||!host_.emit_menu_flavor_updated(e))return false;ready_cursor_=3;}
 if(ready_cursor_==3){
  std::array<uint8_t,32>source;
  if(!host_.backgrounds||host_.backgrounds->object_domain()!=registry_->kernel()||host_.backgrounds->identity().upstream_commit!=data_->identity().upstream_commit||!host_.backgrounds->source_hash(data_->source_script(),source)||source!=data_->identity().source_sha256)return fail(e,"UiManager original source directory/typed background loader pending");
  if(!host_.backgrounds->load(backgrounds_,e)||!host_.backgrounds->owns(backgrounds_,e))return false;
  ready_cursor_=4;
 }
 if(ready_cursor_==4){
  FieldObjectId id=0;if(!registry_->create_stable_canvas(id,e))return false;
  if(stable_!=id||!registry_->tree_owner(id))return fail(e,"UiManager source actual stableCanvas same-ObjectID binding rejected");
  ready_cursor_=5;
 }
 return fail(e,"UiManager remaining actual native add_to_canvas/Fade/deferred Camera lifecycle pending");
}
}
