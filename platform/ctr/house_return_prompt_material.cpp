#include "house_return_prompt_material.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e,const char *text){e=text;return false;}
bool same(const FieldIdentity &a,const FieldIdentity &b) {
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&
         a.source_sha256==b.source_sha256;
}
bool nonzero(const std::array<uint8_t,32>&a) {
  return std::any_of(a.begin(),a.end(),[](uint8_t v){return v!=0;});
}
bool valid_uniforms(const HouseButtonPromptMaterialState &s) {
  for(float value:s.flash)if(!std::isfinite(value)||value<0||value>1)return false;
  for(float value:s.glow)if(!std::isfinite(value)||value<0||value>1)return false;
  return std::isfinite(s.flash_modifier)&&s.flash_modifier>=0&&s.flash_modifier<=1&&
         std::isfinite(s.glow_modifier)&&s.glow_modifier>=0;
}
struct MaterialBody {
  FieldGlobalExternalBinding binding{};
  FieldObjectId root=0;
  uint32_t source=0;
  const FieldGlobalSourceResource *resource=nullptr;
  bool alive=true;
};
// Same real non-Node Resource publication mechanism as PodunkSceneMaterials.
// The Registry owns this Resource; the scene owner borrows the same body.
class MaterialResource final : public FieldGlobalSourceResource {
  std::shared_ptr<MaterialBody> body_;
public:
  explicit MaterialResource(std::shared_ptr<MaterialBody> body):body_(std::move(body)){}
  const char *resource_class()const override{return "ShaderMaterial";}
  FieldGlobalExternalBinding binding()const override{return body_->binding;}
  bool state(FieldGlobalExternalState &out,std::string &e)const override {
    if(!body_->alive)return fail(e,"House Prompt ShaderMaterial Resource retired");
    out={};out.name=body_->binding.source.name;e.clear();return true;
  }
  bool deferred(const FieldDeferredMessage&,std::string &e)override {
    return fail(e,"House Prompt ShaderMaterial is not a deferred Node");
  }
  bool persist_append(FieldObjectId,std::string &e)override {
    return fail(e,"House Prompt ShaderMaterial cannot own persistent Nodes");
  }
  bool assign_stable_canvas(FieldObjectId,std::string &e)override {
    return fail(e,"House Prompt ShaderMaterial cannot own a CanvasLayer");
  }
};
}
struct HouseReturnPromptMaterialOwner::State {
  const HouseReturnButtonPromptData *data=nullptr;
  const FieldNodeTreeData *source=nullptr;
  FieldNodeTreeRuntime *tree=nullptr;
  FieldGlobalRegistry *registry=nullptr;
  HouseReturnButtonPromptNative *native=nullptr;
  std::array<uint8_t,32> ir{},material_sha{},shader_sha{};
  std::map<FieldObjectId,std::shared_ptr<MaterialBody>> materials;
  bool factory=false;
  bool live(std::string &e)const {
    if(!data||!source||!tree||!registry||!native||!data->valid()||
       data->ir_sha256()!=ir||!source->valid()||registry->poisoned()||
       !same(data->identity(),source->identity())||
       tree->object_domain()!=registry->kernel()||
       native->tree()!=tree||native->registry()!=registry||
       native->runtime().data()!=&data->core()||!native->runtime().error().empty())
      return fail(e,"House Prompt material immutable/native/ObjectDB borrow differs");
    std::array<uint8_t,32> material{},shader{};
    if(!data->source_hash(data->material_source(),material)||material!=material_sha||
       !data->source_hash(data->shader_source(),shader)||shader!=shader_sha)
      return fail(e,"House Prompt material source receipt changed");
    return true;
  }
  bool root(FieldObjectId object,const FieldPromptDescriptor *&record,
            std::string &e)const {
    if(!live(e))return false;
    const auto *n=tree->state(object);const auto *d=tree->descriptor(object);
    FieldIdentity actual;
    record=d?data->core().record(d->id):nullptr;
    const auto *original=d?source->record(d->id):nullptr;
    if(!n||!n->alive||!d||!record||!original||!registry->object_exists(object)||
       registry->tree_owner(object).get()!=tree||tree->source_object(d->id)!=object||
       !tree->object_identity(object,actual)||!same(actual,data->identity())||
       d->path!=record->node||d->parent!=record->parent_id||
       d->ready!=record->ready_ordinal||
       d->script!=data->wait().source||d->script_sha!=data->wait().source_sha||
       d->native_class!=original->native_class||d->script!=original->script||
       d->script_sha!=original->script_sha)
      return fail(e,"House Prompt ShaderMaterial has no same actual source root");
    return true;
  }
};
HouseReturnPromptMaterialOwner::HouseReturnPromptMaterialOwner():state_(new State){}
// Owners/resource buffers/Registry must outlive checked shutdown. Destruction
// does not fake source deletion, finish live Press waits or force retirement.
HouseReturnPromptMaterialOwner::~HouseReturnPromptMaterialOwner()=default;
bool HouseReturnPromptMaterialOwner::prepare(const HouseReturnButtonPromptData &data,
    const FieldNodeTreeData &source,FieldNodeTreeRuntime &tree,
    FieldGlobalRegistry &registry,HouseReturnButtonPromptNative &native,std::string &e) {
  if(state_->data||!data.valid()||!source.valid()||registry.poisoned()||
     !same(data.identity(),source.identity())||!data.material_local_to_scene()||
     data.core().records().empty()||!data.shader_declaration()||
     !nonzero(data.shader_code_sha256())||
     tree.object_domain()!=registry.kernel())
    return fail(e,"House Prompt local material checked source preparation rejected");
  auto next=std::make_unique<State>();
  next->data=&data;next->source=&source;next->tree=&tree;
  next->registry=&registry;next->native=&native;next->ir=data.ir_sha256();
  if(!data.source_hash(data.material_source(),next->material_sha)||
     !data.source_hash(data.shader_source(),next->shader_sha))
    return fail(e,"House Prompt Flash.tres embedded Shader source unavailable");
  HouseButtonPromptMaterialState defaults;
  defaults.flash=data.core().initial()[8];defaults.glow=data.material_glow();
  defaults.flash_modifier=data.core().initial()[9][0];
  defaults.glow_modifier=data.core().initial()[7][0];
  if(!valid_uniforms(defaults))return fail(e,"House Prompt serialized uniform values rejected");
  std::set<uint32_t> roots;
  for(const auto &p:data.core().records()) {
    const auto *d=source.record(p.id);
    if(!roots.insert(p.id).second||!d||d->path!=p.node||d->parent!=p.parent_id||
       d->ready!=p.ready_ordinal||d->script!=data.wait().source||
       d->script_sha!=data.wait().source_sha)
      return fail(e,"House Prompt local material source root table differs");
  }
  state_=std::move(next);e.clear();return true;
}
bool HouseReturnPromptMaterialOwner::construct(FieldObjectId object,
    const FieldNodeDescriptor &descriptor,const FieldIdentity &identity,std::string &e) {
  auto &s=*state_;const FieldPromptDescriptor *record=nullptr;
  if(s.factory||s.materials.count(object)||!s.root(object,record,e)||
     record->id!=descriptor.id||!same(identity,s.data->identity())||
     s.tree->state(object)->inside||s.tree->state(object)->ready_notified||
     s.native->owns(object)||s.native->runtime().instance(record->id))
    return fail(e,"House Prompt material requires its actual pre-script constructor cursor");
  auto body=std::make_shared<MaterialBody>();
  body->root=object;body->source=record->id;
  auto &b=body->binding;auto &spec=b.source;
  spec.identity=s.data->identity();spec.identity.source_sha256=s.material_sha;
  // Local clone identity is its real source Prompt owner, not an exported
  // process handle from the authoring extractor or an invented ObjectID.
  spec.stable_id=record->id;spec.role=4;spec.native_class="ShaderMaterial";
  spec.source=s.data->material_source();spec.source_sha=s.material_sha;
  spec.name=spec.source+"::"+record->node;
  b.family=0x454e0080;b.capability=1;
  if(!s.registry->allocate_object(b.object,e))return false;
  std::unique_ptr<FieldGlobalSourceResource> resource(new MaterialResource(body));
  if(!s.registry->publish_source_resource(spec,b.object,std::move(resource),e)) {
    std::string ignored;s.registry->retire_object(b.object,ignored);return false;
  }
  body->resource=s.registry->source_resource(b.object);
  s.materials.emplace(object,std::move(body));e.clear();return true;
}
bool HouseReturnPromptMaterialOwner::uniform_state(FieldObjectId object,
    HousePromptMaterialUniformState &out,std::string &e)const {
  const auto &s=*state_;const FieldPromptDescriptor *record=nullptr;
  if(!s.root(object,record,e))return false;
  const auto entry=s.materials.find(object);
  if(entry==s.materials.end()||!entry->second->alive)
    return fail(e,"House Prompt actual local ShaderMaterial was not constructed");
  const auto &body=*entry->second;
  const auto *resource=s.registry->source_resource(body.binding.object);
  if(!resource||resource!=body.resource||!s.registry->object_exists(body.binding.object)||
     resource->binding().object!=body.binding.object||
     resource->binding().source.stable_id!=record->id||
     body.root!=object||body.source!=record->id)
    return fail(e,"House Prompt Resource ObjectDB owner/lifetime changed");
  HousePromptMaterialUniformState result;
  auto &m=result.material;
  m.registry=s.registry;m.root=object;m.material=body.binding.object;m.owner=resource;
  m.source=s.data->material_source();m.shader=s.data->shader_source();
  m.source_sha=s.material_sha;m.shader_sha=s.shader_sha;
  m.shader_declaration=s.data->shader_declaration();
  m.shader_code_sha=s.data->shader_code_sha256();m.local_to_scene=true;
  m.glow=s.data->material_glow();
  m.flash=s.data->core().initial()[8];
  m.flash_modifier=s.data->core().initial()[9][0];
  m.glow_modifier=s.data->core().initial()[7][0];
  result.runtime=&s.native->runtime();result.instance=result.runtime->instance(record->id);
  if(result.instance&&result.instance->material_assigned) {
    if(!s.native->owns(object))return fail(e,"House Prompt assigned material has no real native script owner");
    // Source script attachment precedes set_name/add_child. Reading the
    // actual assigned Resource in that interval must not invent a parent or
    // require future Ready. Once parented, inspect the full native receipt.
    const auto *node=s.tree->state(object);
    if(node->parent) {
      HouseReturnInteractPromptState native;
      if(!s.native->observe(object,native,e)||!native.constructed||
         native.source_id!=record->id||native.tree!=s.tree||native.registry!=s.registry)
        return fail(e,"House Prompt live uniform receiver is not the same source instance");
    }else if(result.instance->ready||node->inside||node->ready_notified)
      return fail(e,"House Prompt unparented material cannot report source Ready");
    m.flash=result.instance->properties[8];
    m.flash_modifier=result.instance->properties[9][0];
    m.glow_modifier=result.instance->properties[7][0];
    result.phase=result.instance->ready?HousePromptMaterialPhase::SourceReady:
                                       HousePromptMaterialPhase::AssignedBeforeReady;
  }else if(s.native->owns(object)||(result.instance&&result.instance->ready))
    return fail(e,"House Prompt native script skipped its actual material assignment");
  if(!valid_uniforms(m))return fail(e,"House Prompt actual AP uniform state rejected");
  out=std::move(result);e.clear();return true;
}
bool HouseReturnPromptMaterialOwner::observe(FieldObjectId object,
    HouseButtonPromptMaterialState &out,std::string &e)const {
  HousePromptMaterialUniformState actual;
  if(!uniform_state(object,actual,e))return false;
  out=std::move(actual.material);return true;
}
bool HouseReturnPromptMaterialOwner::shader_parameter(FieldObjectId material,
    std::string_view name,std::vector<float> &out,std::string &e)const {
  for(const auto &entry:state_->materials) {
    if(entry.second->binding.object!=material)continue;
    HousePromptMaterialUniformState actual;
    if(!uniform_state(entry.first,actual,e))return false;
    const auto &m=actual.material;
    if(name=="flash_color")out.assign(m.flash.begin(),m.flash.end());
    else if(name=="glow_color")out.assign(m.glow.begin(),m.glow.end());
    else if(name=="flash_modifier")out={m.flash_modifier};
    else if(name=="glow_modifier")out={m.glow_modifier};
    else return fail(e,"House Prompt unknown embedded shader uniform");
    e.clear();return true;
  }
  return fail(e,"House Prompt unknown ShaderMaterial ObjectID");
}
bool HouseReturnPromptMaterialOwner::finish_factory(std::string &e) {
  auto &s=*state_;
  if(s.factory||!s.live(e)||!s.native->source_frame_closed()||
     s.materials.size()!=s.data->core().records().size())
    return fail(e,"House Prompt local material factory is incomplete");
  std::set<FieldObjectId> handles;
  for(const auto &p:s.data->core().records()) {
    const auto object=s.tree->source_object(p.id);HousePromptMaterialUniformState actual;
    const auto *n=s.tree->state(object);
    if(!uniform_state(object,actual,e)||!n||!n->bound||n->inside||n->ready_notified||
       actual.phase!=HousePromptMaterialPhase::AssignedBeforeReady||
       !handles.insert(actual.material.material).second)
      return fail(e,"House Prompt local material factory cannot replace actual Enter/Ready");
  }
  s.factory=true;e.clear();return true;
}
bool HouseReturnPromptMaterialOwner::release_deleted(FieldObjectId object,std::string &e) {
  auto &s=*state_;const auto entry=s.materials.find(object);
  if(!s.data||entry==s.materials.end()||!s.native->source_frame_closed()||
     s.tree->state(object)||s.native->owns(object)||
     s.native->runtime().instance(entry->second->source))
    return fail(e,"House Prompt material release requires actual native/source root deletion");
  auto body=entry->second;
  if(!body->alive||s.registry->source_resource(body->binding.object)!=body->resource||
     !s.registry->retire_object(body->binding.object,e))return false;
  body->alive=false;s.materials.erase(entry);e.clear();return true;
}
bool HouseReturnPromptMaterialOwner::shutdown(std::string &e) {
  auto &s=*state_;if(!s.data){e.clear();return true;}
  if(!s.native->source_frame_closed())return fail(e,"House Prompt material has a live source callback");
  // Check every root before changing any Resource lifetime.
  for(const auto &entry:s.materials)
    if(s.tree->state(entry.first)||s.native->owns(entry.first)||
       s.native->runtime().instance(entry.second->source))
      return fail(e,"House Prompt material still borrowed by a live actual root");
  while(!s.materials.empty())if(!release_deleted(s.materials.begin()->first,e))return false;
  state_=std::make_unique<State>();e.clear();return true;
}
const FieldGlobalRegistry *HouseReturnPromptMaterialOwner::registry()const{return state_->registry;}
const FieldNodeTreeRuntime *HouseReturnPromptMaterialOwner::tree()const{return state_->tree;}
const HouseReturnButtonPromptData *HouseReturnPromptMaterialOwner::data()const{return state_->data;}
size_t HouseReturnPromptMaterialOwner::material_count()const{return state_->materials.size();}
} // namespace encore::ctr
