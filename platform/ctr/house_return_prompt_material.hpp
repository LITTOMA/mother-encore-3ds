#pragma once
#include "house_return_button_prompt_native.hpp"

namespace encore::ctr {
enum class HousePromptMaterialPhase : uint8_t {
  Constructor, AssignedBeforeReady, SourceReady
};
struct HousePromptMaterialUniformState {
  HouseButtonPromptMaterialState material;
  const upstream::FieldPromptRuntime *runtime=nullptr;
  const upstream::FieldPromptInstance *instance=nullptr;
  HousePromptMaterialPhase phase=HousePromptMaterialPhase::Constructor;
};
// The actual local-to-scene ShaderMaterial Resource per checked Prompt root.
// Embedded Shader declaration/code are source receipts, not a Shader ObjectID.
// There is no second AP/uniform clock. Live parameters borrow the same actual
// FieldPromptRuntime instance that owns the original animation tracks.
class HouseReturnPromptMaterialOwner final : public HouseButtonPromptMaterialOwner {
public:
  HouseReturnPromptMaterialOwner();
  ~HouseReturnPromptMaterialOwner();
  HouseReturnPromptMaterialOwner(const HouseReturnPromptMaterialOwner&)=delete;
  HouseReturnPromptMaterialOwner&operator=(const HouseReturnPromptMaterialOwner&)=delete;
  HouseReturnPromptMaterialOwner(HouseReturnPromptMaterialOwner&&)=delete;
  HouseReturnPromptMaterialOwner&operator=(HouseReturnPromptMaterialOwner&&)=delete;
  // Prepare immutable borrows before Native.prepare(materials=this). Each
  // construct then runs immediately before that SAME Native.construct(root).
  // Neither operation sends Enter, Ready or AnimationPlayer notifications.
  bool prepare(const upstream::HouseReturnButtonPromptData&,
               const upstream::FieldNodeTreeData&,
               upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
               HouseReturnButtonPromptNative&,std::string&);
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                 const upstream::FieldIdentity&,std::string&);
  bool observe(upstream::FieldObjectId,HouseButtonPromptMaterialState&,
               std::string&)const override;
  bool uniform_state(upstream::FieldObjectId,HousePromptMaterialUniformState&,
                     std::string&)const;
  bool shader_parameter(upstream::FieldObjectId material,std::string_view,
                        std::vector<float>&,std::string&)const;
  bool finish_factory(std::string&);
  // Native.release_deleted(root) follows the real Tree deletion first. Only
  // then may this Resource be retired. Pending source waits are never cleared.
  bool release_deleted(upstream::FieldObjectId,std::string&);
  bool shutdown(std::string&);
  const upstream::FieldGlobalRegistry *registry()const;
  const upstream::FieldNodeTreeRuntime *tree()const;
  const upstream::HouseReturnButtonPromptData *data()const;
  size_t material_count()const;
private:
  struct State;
  std::unique_ptr<State>state_;
};
} // namespace encore::ctr
