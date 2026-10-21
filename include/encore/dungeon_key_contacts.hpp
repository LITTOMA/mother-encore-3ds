#pragma once
#include "encore/dungeon_key_media.hpp"
#include "encore/field_area_contacts.hpp"
namespace encore::upstream {
struct DungeonKeyContactBody {uint32_t layer=0,mask=0;bool enabled=false;};
// Samples only the current player's body against source pickup rectangles.
// Geometry comes from the world; other bodies and door interaction selection
// have separate owners. Paused entries are consumed, not retried on unpause.
class DungeonKeyContacts {
public:
 bool prepare(const DungeonKeyMediaData&,uint64_t,std::string&);
 bool valid()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}
 size_t count()const{return indices_.size();}size_t shape_index(size_t i)const{return i<count()?indices_[i]:SIZE_MAX;}
 bool sampling()const{return pairs_.sampling();}
 bool begin(uint64_t,const DungeonKeyContactBody&,DungeonKeyClocks&,bool player_paused,std::string&);
 bool end(uint64_t,const DungeonKeyContactBody&,const DungeonKeyClocks&,const uint8_t*overlap,size_t,std::string&);
 bool retire(uint64_t,std::string&);
private:
 bool enabled(uint64_t,const DungeonKeyContactBody&,const DungeonKeyClocks&,std::array<uint8_t,128>&,std::string&)const;
 DungeonKeyMediaData data_;uint64_t epoch_=0,last_epoch_=0;std::vector<size_t>indices_;FieldAreaContacts pairs_;
};
}
