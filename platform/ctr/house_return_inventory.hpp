#pragma once
#include "house_return_dialogue.hpp"
#include "podunk_global_data_host.hpp"
#include "podunk_inventory_host.hpp"
#include "encore/native_session.hpp"

namespace encore::ctr {
struct HouseReturnInventoryInput {
  HouseReturnDialogue *dialogue=nullptr;
  upstream::OpeningWorld *world=nullptr;
  upstream::HouseRuntime *runtime=nullptr;
  upstream::FieldGlobalConstructorRuntime *global=nullptr;
  PodunkGlobalDataHost *globaldata=nullptr;
  PodunkInventoryHost *inventory=nullptr;
  const upstream::NativeSessionData *session=nullptr;
  upstream::DrawerProgramView drawer{};
};
// Borrows the retained source Inventory and Item References. The legacy
// session Inventory is a save projection and is never an input to this owner.
// All inputs, their resource buffers, ObjectDB and UID/RNG owners must outlive
// checked unbind. Fixed address: World/HouseRuntime borrow this exact owner.
class HouseReturnInventoryOwner final : public upstream::OpeningHouseInventoryOwner,
                                        public upstream::DrawerHost {
public:
  HouseReturnInventoryOwner()=default;
  HouseReturnInventoryOwner(const HouseReturnInventoryOwner&)=delete;
  HouseReturnInventoryOwner&operator=(const HouseReturnInventoryOwner&)=delete;
  HouseReturnInventoryOwner(HouseReturnInventoryOwner&&)=delete;
  HouseReturnInventoryOwner&operator=(HouseReturnInventoryOwner&&)=delete;
  // prepare -> HouseRuntime.bind_drawer(same view,*this) -> actual native
  // Dialogue/House programme binding -> bind. No Ready or VM start here.
  bool prepare(HouseReturnInventoryInput,std::string&);
  bool bind(std::string&);
  bool unbind(std::string&);
  const upstream::OpeningWorld *world()const override{return input_.world;}
  bool observe_inventory(upstream::OpeningHouseInventoryState&,std::string&)const override;
  // Actual text_tools.ItemReceiver: source global.item -> source Inventory
  // owner -> the same singleton Character nickname, checked after Grant.
  bool item_receiver(upstream::FieldObjectId&,std::string&,std::string&)const;
  bool validate_text(uint32_t,std::string&)override;
  bool validate_flag(std::string_view,std::string&)override;
  bool validate_item(upstream::DrawerItemTemplate,std::string_view,std::string&)override;
  bool validate_sound(std::string_view,std::string&)override;
  bool show_text(uint32_t,std::string&)override;
  bool flag(std::string_view,bool&,std::string&)override;
  bool inventory_space()const override;
  bool grant_item(upstream::DrawerItemTemplate,std::string_view,std::string&)override;
  bool play_sound(std::string_view,std::string&)override;
  bool set_flag(std::string_view,bool,std::string&)override;
  const std::string &error()const{return error_;}
private:
  HouseReturnInventoryInput input_{};
  bool prepared_=false,bound_=false;
  size_t business_depth_=0;
  mutable std::string error_;
  bool source(PodunkDrawerInventoryState&,upstream::FieldObjectId&character,
              uint32_t&inventory,std::string&nickname,std::string&)const;
  bool frame(HouseReturnDialogueReceipt&,std::string&)const;
  bool command(upstream::DialogueActionKind,upstream::RoomCommand&,std::string&)const;
  bool reject(std::string&,const char*)const;
};
} // namespace encore::ctr
