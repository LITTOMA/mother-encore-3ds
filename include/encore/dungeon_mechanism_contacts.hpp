#pragma once
#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/dungeon_mechanism_state.hpp"
#include "encore/field_area_contacts.hpp"
namespace encore::upstream {
struct DungeonMechanismContactSource {
 uint32_t layer=0,mask=0;bool enabled=false,monitorable=true;
};
// One current-player body and one attack Area2D, against checked mechanism
// rectangles. The world supplies geometric overlap, never other bodies.
// This coordinates the existing sampled pair set and original methods;
// it is not a physics server, movement solver or scene admission.
class DungeonMechanismContacts {
public:
 bool prepare(const DungeonMechanismPresentationData&,uint64_t epoch,std::string&);
 bool valid()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}
 size_t count()const{return indices_.size();}bool sampling()const{return pairs_.sampling();}
 size_t shape_index(size_t i)const{return i<count()?indices_[i]:SIZE_MAX;}
 bool begin(uint64_t,const DungeonMechanismContactSource&body,const DungeonMechanismContactSource&attack,
            DungeonMechanismState&,bool externally_paused,bool can_switch,std::string&);
 bool end(uint64_t,const DungeonMechanismContactSource&body,const DungeonMechanismContactSource&attack,
          const uint8_t*overlap,size_t,std::string&);
private:
 bool enabled(uint64_t,const DungeonMechanismContactSource&,const DungeonMechanismContactSource&,
              std::array<uint8_t,128>&,std::string&)const;
 DungeonMechanismPresentationData data_;uint64_t epoch_=0;
 std::vector<size_t>indices_;FieldAreaContacts pairs_;
};
}
