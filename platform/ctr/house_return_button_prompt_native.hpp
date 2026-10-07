#pragma once
#include "encore/house_return_button_prompt.hpp"
#include "encore/field_global_constructor.hpp"
#include "house_return_interact_dialog.hpp"
#include "podunk_prompt_native.hpp"
namespace encore::ctr {
struct HouseButtonPromptMaterialState {
  const upstream::FieldGlobalRegistry*registry=nullptr;
  upstream::FieldObjectId root=0,material=0;
  const upstream::FieldGlobalSourceResource*owner=nullptr;
  std::string source,shader;
  std::array<uint8_t,32>source_sha{},shader_sha{};
  std::array<uint8_t,32>shader_code_sha{};
  uint32_t shader_declaration=0;
  bool local_to_scene=false;
  upstream::FieldColor flash{},glow{};
  float flash_modifier=0,glow_modifier=0;
};
class HouseButtonPromptMaterialOwner {
public:
  virtual ~HouseButtonPromptMaterialOwner()=default;
  virtual bool observe(upstream::FieldObjectId,HouseButtonPromptMaterialState&,
                       std::string&)const=0;
};
struct HouseButtonPromptSourceCall {
  const upstream::FieldNodeTreeRuntime*tree=nullptr;
  const upstream::FieldGlobalRegistry*registry=nullptr;
  upstream::FieldObjectId caller=0,receiver=0;
  std::string method;
  size_t depth=0;
};
class HouseButtonPromptSourceOwner {
public:
  virtual ~HouseButtonPromptSourceOwner()=default;
  virtual bool observe(upstream::FieldObjectId,std::string_view,
                       HouseButtonPromptSourceCall&,std::string&)const=0;
};
struct HouseButtonPromptControlState {
  const upstream::FieldNodeTreeRuntime*tree=nullptr;
  const upstream::FieldGlobalRegistry*registry=nullptr;
  upstream::FieldObjectId object=0,parent=0;
  uint32_t source_id=0,source_parent=0;
  upstream::FieldNodeBinding binding{};
  upstream::Vec2 position{},size{};
  std::string text;
  bool constructed=false,entered=false,ready=false;
};
struct HouseButtonPromptWaitState {
  upstream::FieldObjectId object=0,prompt=0,player=0;
  upstream::FieldGlobalExternalBinding binding{};
  bool pending=false;
};
struct HouseButtonPromptNativeInput {
  const upstream::HouseReturnButtonPromptData*data=nullptr;
  const upstream::FieldNodeTreeData*source=nullptr;
  upstream::FieldNodeTreeRuntime*tree=nullptr;
  upstream::FieldGlobalRegistry*registry=nullptr;
  upstream::FieldObjectSignals*signals=nullptr;
  const upstream::FieldGlobalDataRuntime*global_data=nullptr;
  // The existing global source owner. Its PartyObjects[0] is resolved at each
  // real source Ready/signal connection; Player may still be in the old scene
  // then, and must retain that same live ObjectID through actual reparenting.
  const upstream::FieldGlobalConstructorRuntime*global_owner=nullptr;
  PodunkPlayerHost*player=nullptr;
  upstream::FieldObjectId global=0;
  upstream::FieldEquipmentView equipment{};
  SourceFontRenderer*font=nullptr;
  const char*asset_root=nullptr;
  HouseReturnInteractDialog*interact=nullptr;
  const HouseButtonPromptMaterialOwner*materials=nullptr;
  const HouseButtonPromptSourceOwner*source_calls=nullptr;
};
// Fixed-address source owner and the SAME PromptNative leaf owner. No second
// animation clock, scene tree, ObjectDB, prompt fields or Control layout.
class HouseReturnButtonPromptNative final:
    public HouseReturnInteractPromptOwner,
    public upstream::FieldPromptCompletionOwner {
public:
  HouseReturnButtonPromptNative();
  ~HouseReturnButtonPromptNative();
  HouseReturnButtonPromptNative(const HouseReturnButtonPromptNative&)=delete;
  HouseReturnButtonPromptNative&operator=(const HouseReturnButtonPromptNative&)=delete;
  bool prepare(HouseButtonPromptNativeInput,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                 const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,float,bool,bool,
             std::string&);
  // For roots phase consumes only actual EnterScript/ReadyScript/ExitScript;
  // basic native Node2D notifications belong to the actual scene native owner.
  // For the four leaves this forwards the sole original native notification,
  // including AP IdleInternal. Never call it again from a Control delegate.
  bool handles_method(const upstream::FieldDeferredMessage&)const;
  bool dispatch(const upstream::FieldDeferredMessage&,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,
                   std::string&)const;
  bool release_deleted(upstream::FieldObjectId,std::string&);
  const upstream::FieldNodeTreeRuntime*tree()const override;
  const upstream::FieldGlobalRegistry*registry()const override;
  bool observe(upstream::FieldObjectId,HouseReturnInteractPromptState&,
               std::string&)const override;
  bool source_set_offset(upstream::FieldObjectId,upstream::FieldObjectId,
                         upstream::Vec2,std::string&)override;
  bool native_control_snapshot(upstream::FieldObjectId,
                              HouseButtonPromptControlState&,std::string&)const;
  PodunkPromptNative&native();
  const PodunkPromptNative&native()const;
  const upstream::FieldPromptRuntime&runtime()const;
  bool source_frame_closed()const;
  bool wait_state(upstream::FieldObjectId,HouseButtonPromptWaitState&,
                  std::string&)const;
  size_t pending_press_waits()const;
  bool arm_press(upstream::FieldPromptRuntime&,uint32_t,std::string&)override;
  bool source_completion_live(const upstream::FieldPromptRuntime&,uint32_t,
      upstream::FieldPromptClipRole,bool,std::string&)const override;
private:
  struct State;
  std::unique_ptr<State>state_;
};
} // namespace encore::ctr
