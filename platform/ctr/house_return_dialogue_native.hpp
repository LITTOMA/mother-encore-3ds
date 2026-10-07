#pragma once
#include "house_return_dialogue.hpp"
#include <optional>

namespace encore::ctr {
class HouseReturnNpcRuntime;
struct HouseReturnDialogueNativeInput {
  PodunkMickSession *session=nullptr;
  HouseReturnDialogue *driver=nullptr;
  upstream::FreshHouseState *house=nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
  upstream::HouseView text{};
  const upstream::HouseReentryData *source=nullptr;
  const upstream::FieldNodeTreeData *source_tree=nullptr;
  const upstream::FieldDoorData *doors=nullptr;
};
// Fixed borrow owner for the retained actual Canvas/DialogueBox composition.
// Target owns this object before installing callbacks and keeps it alive until
// the real source waits, factory deletes and exact World unbind have completed.
class HouseReturnDialogueNativeOwner final : public HouseReturnDialogueNativePort,
                                            public PodunkMickHouseTalkerOwner {
public:
  HouseReturnDialogueNativeOwner()=default;
  HouseReturnDialogueNativeOwner(const HouseReturnDialogueNativeOwner&)=delete;
  HouseReturnDialogueNativeOwner&operator=(const HouseReturnDialogueNativeOwner&)=delete;
  HouseReturnDialogueNativeOwner(HouseReturnDialogueNativeOwner&&)=delete;
  HouseReturnDialogueNativeOwner&operator=(HouseReturnDialogueNativeOwner&&)=delete;
  bool prepare(HouseReturnDialogueNativeInput,std::string&);
  bool driver_input(HouseReturnDialogueInput&,std::string&)const;
  // Call after the driver's real World bind and before any new native factory.
  bool bind_programme(std::string&);
  // Same fixed source owner, after real House Ready and closed programme bind.
  // Keep this borrower alive until real NPC deletion/Room waits complete.
  bool bind_npc_source(HouseReturnNpcRuntime&,std::string&);
  bool admit_npc_programme(const HouseReturnNpcRuntime&,
      const upstream::HouseSourceNpcProgramme&,std::string&)const;
  const upstream::FreshHouseState *house()const override{return in_.house;}
  bool set_talking(upstream::FieldObjectId,bool,std::string&)override;
  bool observe(const HouseReturnDialogueContext&,HouseReturnDialogueReceipt&,
               std::string&)const override;
  bool admit_command(const HouseReturnDialogueContext&,uint32_t,
                     const upstream::RoomCommand&,std::string&)const override;
  bool open(const HouseReturnDialogueContext&,upstream::FieldObjectId&,
            std::string&)override;
  bool source_started(const HouseReturnDialogueContext&,std::string&)override;
  bool apply_native(const HouseReturnDialogueContext&,uint32_t,
                    const upstream::DialogueAction&,std::string&)override;
  bool select_choices(const HouseReturnDialogueContext&,int32_t,bool,bool,
                      uint32_t&,std::string&)override;
  bool choice_target_committed(const HouseReturnDialogueContext&,std::string&)override;
private:
  HouseReturnDialogueNativeInput in_{};
  PodunkMickHouseNativeState owners_{};
  HouseReturnNpcRuntime *npc_source_=nullptr;
  bool prepared_=false,room_installed_=false,programme_bound_=false;
  size_t callback_depth_=0;
  // This is the actual Selected event removed from the source queue, retained
  // solely for its source post-target InputSound tail. Never a parallel queue.
  std::optional<upstream::DialogueChoicesEvent> selected_;
  upstream::FieldObjectId selected_dialogue_=0;
  uint32_t selected_generation_=0;
  bool state(PodunkMickHouseNativeState&,std::string&)const;
  bool context(const HouseReturnDialogueContext&,upstream::FieldProgrammeContext&,
               std::string&)const;
  bool actor(upstream::FieldObjectId,uint32_t&,uint32_t&,std::string&)const;
  bool room_actor(upstream::RoomView,uint32_t,const upstream::FieldProgrammeContext&,
                  std::string&)const;
  bool input_sound(const HouseReturnDialogueContext&,upstream::DialogueChoiceSound,
                   std::string&)const;
};
} // namespace encore::ctr
