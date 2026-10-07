#pragma once
#include "encore/drawer_program.hpp"
#include "encore/field_interact_dialog.hpp"
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"

namespace encore::upstream {
// Own the actual complete new Room resource. Its view must be selected before
// FreshHouse World initialization and Reentry/Room lifecycle cross-binding.
// No new VM, SceneTree, native objects, Ready or source callbacks are created.
class HouseReturnInspectionProgrammes final {
public:
  bool load(const uint8_t*,size_t,upstream::RoomView original,
            upstream::HouseView text,upstream::DrawerProgramView drawer,
            const upstream::FieldInteractData&,std::string&);
  upstream::RoomView view()const{return room_.view();}
  static bool admit(upstream::RoomView original,upstream::RoomView next,
                    upstream::HouseView,upstream::DrawerProgramView,
                    const upstream::FieldInteractData&,std::string&);
  // Actual immutable Program source identity, never an invented Room index.
  bool resolve(std::string_view relative_yaml,uint32_t&,std::string&)const;
private:
  upstream::RoomData room_;
};
} // namespace encore::ctr
