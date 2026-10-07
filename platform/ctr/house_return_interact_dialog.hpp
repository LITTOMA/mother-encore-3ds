#pragma once
#include "encore/field_interact_dialog.hpp"
#include "house_return_geometry_native.hpp"
#include "house_return_dialogue.hpp"
#include "podunk_global_data_host.hpp"

namespace encore::ctr {
class HouseReturnInteractDialog;
// The real current InteractDialog invocation produces this receipt. The target
// converts it to HouseRuntime's typed request; it must use that same lease and
// original_npc=kRoomNoIndex, never call driver.request directly.
struct HouseReturnInteractProgramme {
  const HouseReturnInteractDialog *source=nullptr;
  const upstream::FieldInteractRuntime *runtime=nullptr;
  const upstream::FieldInteractData *data=nullptr;
  const upstream::FieldNodeTreeRuntime *tree=nullptr;
  const upstream::FieldNodeTreeData *tree_data=nullptr;
  const upstream::FieldGlobalRegistry *registry=nullptr;
  const upstream::HouseReentryData *reentry=nullptr;
  const upstream::FieldDoorData *doors=nullptr;
  const upstream::FreshHouseState *house=nullptr;
  upstream::HouseView text{};
  upstream::FieldObjectId object=0;
  uint32_t source_id=0,programme=upstream::kRoomNoIndex;
  std::string_view dialogue;
  bool thoughts=false;
};
class HouseReturnInteractProgrammeOwner {
public:
  virtual ~HouseReturnInteractProgrammeOwner()=default;
  virtual const upstream::FreshHouseState *house()const=0;
  virtual const upstream::FieldNodeTreeRuntime *tree()const=0;
  virtual const upstream::FieldGlobalRegistry *registry()const=0;
  virtual bool admit(const HouseReturnInteractProgramme&,std::string&)const=0;
  virtual bool request(const HouseReturnInteractProgramme&,std::string&)=0;
};
// ButtonPrompt.offset is a source FIELD. Its source Ready later writes position.
// Native Node2D.position is not a substitute for this serialized setget.
struct HouseReturnInteractPromptState {
  const upstream::FieldNodeTreeRuntime *tree=nullptr;
  const upstream::FieldGlobalRegistry *registry=nullptr;
  upstream::FieldObjectId object=0,parent=0;
  uint32_t source_id=0;
  upstream::Vec2 offset{};
  bool constructed=false,ready=false;
};
class HouseReturnInteractPromptOwner {
public:
  virtual ~HouseReturnInteractPromptOwner()=default;
  virtual const upstream::FieldNodeTreeRuntime *tree()const=0;
  virtual const upstream::FieldGlobalRegistry *registry()const=0;
  virtual bool observe(upstream::FieldObjectId,HouseReturnInteractPromptState&,
                       std::string&)const=0;
  virtual bool source_set_offset(upstream::FieldObjectId parent,
      upstream::FieldObjectId prompt,upstream::Vec2,std::string&)=0;
};
struct HouseReturnInteractInput {
  const upstream::HouseReturnSources *sources=nullptr;
  const upstream::FieldInteractData *data=nullptr;
  const upstream::FieldDoorData *doors=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FieldGlobalRegistry *registry=nullptr;
  upstream::FieldObjectSignals *signals=nullptr;
  HouseReturnGeometryNative *geometry=nullptr;
  upstream::FieldGlobalConstructorRuntime *global=nullptr;
  PodunkGlobalDataHost *global_data=nullptr;
  upstream::FreshHouseState *house=nullptr;
  upstream::HouseView text{};
  HouseReturnDialogue *dialogue=nullptr;
  HouseReturnInteractPromptOwner *prompts=nullptr;
  HouseReturnInteractProgrammeOwner *programmes=nullptr;
};
// Fixed address, same ObjectDB bus, flags, native Areas and original Room VM.
// No process/timer/input loop and no whole-House Ready approval.
class HouseReturnInteractDialog final {
public:
  HouseReturnInteractDialog()=default;
  HouseReturnInteractDialog(const HouseReturnInteractDialog&)=delete;
  HouseReturnInteractDialog&operator=(const HouseReturnInteractDialog&)=delete;
  static bool admit_source(const upstream::FieldInteractData&,
                          const upstream::HouseReturnSources&,std::string&);
  static bool load(const uint8_t*,size_t,const upstream::HouseReturnSources&,
                   upstream::FieldInteractData&,std::string&);
  bool prepare(HouseReturnInteractInput,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                 const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  // After all actual Area/Prompt factories and parenting, before any Enter.
  bool finish_factory(std::string&);
  bool source_phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  bool handles_method(const upstream::FieldDeferredMessage&)const;
  bool dispatch(const upstream::FieldDeferredMessage&,std::string&);
  bool interact(upstream::FieldObjectId,std::string&);
  bool interact_item(upstream::FieldObjectId,upstream::FieldObjectId actual_item,
                     std::string&);
  bool has_thoughts(upstream::FieldObjectId,bool&,std::string&)const;
  bool telepathy(upstream::FieldObjectId,std::string&);
  bool programme_input(upstream::FieldObjectId,std::string_view,
                       HouseReturnInteractProgramme&,std::string&)const;
  // Only the core's actual complete_source_constructor setget callback may
  // write the borrowed Prompt.offset field; native constructor flags alone
  // do not grant this source caller lease.
  bool source_offset_live(upstream::FieldObjectId parent,
      upstream::FieldObjectId prompt,upstream::Vec2,std::string&)const;
  bool release_deleted(upstream::FieldObjectId,std::string&);
  const upstream::FieldInteractRuntime&runtime()const{return runtime_;}
  const upstream::FieldNodeTreeRuntime*tree()const{return in_.tree;}
  bool source_frame_closed()const{return !callback_depth_&&!source_call_;}
private:
  struct Instance {uint32_t source=0;bool bound=false,entered=false;
    std::function<bool(std::string&)> flags;};
  HouseReturnInteractInput in_{};
  upstream::FieldInteractRuntime runtime_;
  std::map<upstream::FieldObjectId,Instance>instances_;
  bool prepared_=false,factory_=false,poisoned_=false,constructor_call_=false;
  size_t callback_depth_=0;
  upstream::FieldObjectId source_call_=0;
  bool borrows(std::string&)const;
  bool actual(upstream::FieldObjectId,uint32_t&,std::string&)const;
  bool source(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool invoke(upstream::FieldObjectId,const std::function<bool()>&,std::string&);
  bool offset(uint32_t,upstream::Vec2,std::string&);
  bool programme(std::string_view,HouseReturnInteractProgramme&,std::string&)const;
  bool programme_borrows(std::string&)const;
};
} // namespace encore::ctr
