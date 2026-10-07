#pragma once
#include "house_return_interact_dialog.hpp"

namespace encore::ctr {
// Fixed address. Borrow the actual script call; HouseRuntime alone owns its
// request lease. There is no alternate VM, NPC, timer or dialogue factory.
class HouseReturnInteractProgrammeBridge final
    : public HouseReturnInteractProgrammeOwner,
      public upstream::HouseSourceInteractProgrammeCaller {
public:
 bool prepare(HouseReturnInteractDialog&,upstream::FreshHouseState&,
              upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
              std::string&);
 const upstream::FreshHouseState*house()const override{return house_;}
 const upstream::FieldNodeTreeRuntime*tree()const override{return tree_;}
 const upstream::FieldGlobalRegistry*registry()const override{return registry_;}
 bool admit(const HouseReturnInteractProgramme&,std::string&)const override;
 bool request(const HouseReturnInteractProgramme&,std::string&)override;
 bool observe(const upstream::HouseSourceInteractProgramme&,
              upstream::HouseSourceInteractProgramme&,std::string&)const override;
private:
 HouseReturnInteractDialog*source_=nullptr;
 upstream::FreshHouseState*house_=nullptr;
 upstream::FieldNodeTreeRuntime*tree_=nullptr;
 upstream::FieldGlobalRegistry*registry_=nullptr;
 bool convert(const HouseReturnInteractProgramme&,
              upstream::HouseSourceInteractProgramme&,std::string&)const;
};
}
