// Manual CTR cases only; not registered, compiled or executed. Fixtures must
// supply actual loaded resources/native Tree/ObjectDB/source constructor calls.
#include "../platform/ctr/house_return_prompt_material.hpp"
#include <cassert>

namespace encore::ctr::manual {
void house_prompt_material_unprepared_rejects() {
  HouseReturnPromptMaterialOwner material;
  HouseButtonPromptMaterialState state;
  std::vector<float> value;
  std::string error;
  assert(!material.observe(0,state,error)&&!error.empty());
  assert(!material.shader_parameter(0,"flash_color",value,error)&&!error.empty());
  assert(!material.finish_factory(error)&&!error.empty());
}
void house_prompt_material_foreign_source_rejects(
    const upstream::HouseReturnButtonPromptData &house,
    const upstream::FieldNodeTreeData &different_source,
    upstream::FieldNodeTreeRuntime &tree,upstream::FieldGlobalRegistry &registry,
    HouseReturnButtonPromptNative &native) {
  HouseReturnPromptMaterialOwner material;
  std::string error;
  assert(!material.prepare(house,different_source,tree,registry,native,error)&&!error.empty());
}
void house_prompt_material_constructor_is_not_ready(HouseReturnPromptMaterialOwner &material,
    upstream::FieldObjectId actual_root) {
  HousePromptMaterialUniformState state;
  std::string error;
  assert(material.uniform_state(actual_root,state,error));
  assert(state.phase==HousePromptMaterialPhase::Constructor);
  assert(!state.instance||!state.instance->material_assigned);
  assert(state.material.flash==material.data()->core().initial()[8]);
  assert(state.material.glow==material.data()->material_glow());
  assert(state.material.flash_modifier==material.data()->core().initial()[9][0]);
  assert(state.material.glow_modifier==material.data()->core().initial()[7][0]);
  assert(!material.finish_factory(error)&&!error.empty());
}
void house_prompt_material_alias_and_live_release_reject(HouseReturnPromptMaterialOwner &material,
    upstream::FieldObjectId root,const upstream::FieldNodeDescriptor &source,
    const upstream::FieldIdentity &identity) {
  HouseButtonPromptMaterialState before,after;
  std::string error;
  assert(material.observe(root,before,error));
  assert(!material.construct(root,source,identity,error)&&!error.empty());
  assert(!material.release_deleted(root,error)&&!error.empty());
  assert(material.observe(root,after,error));
  assert(before.material==after.material&&before.owner==after.owner);
  assert(!material.shutdown(error)&&!error.empty());
}
void house_prompt_material_unknown_uniform_rejects(HouseReturnPromptMaterialOwner &material,
    upstream::FieldObjectId actual_root) {
  HouseButtonPromptMaterialState state;
  std::vector<float> value;
  std::string error;
  assert(material.observe(actual_root,state,error));
  assert(!material.shader_parameter(state.material,"unsupported_uniform",value,error)&&!error.empty());
}
// Invoke after the real Tree Deleting/Source releases and ObjectDB flush,
// never by pretending that a Ready/root/Press callback completed.
void house_prompt_material_deleted_lifetime(HouseReturnPromptMaterialOwner &material,
    upstream::FieldObjectId actual_root,upstream::FieldObjectId actual_resource) {
  const auto *registry=material.registry();
  std::string error;
  assert(material.release_deleted(actual_root,error));
  assert(!registry->object_exists(actual_resource));
  HouseButtonPromptMaterialState state;
  assert(!material.observe(actual_root,state,error)&&!error.empty());
}
} // namespace encore::ctr::manual
