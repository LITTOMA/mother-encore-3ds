#pragma once
#include "encore/field_data.hpp"
#include "encore/items_data.hpp"
#include <string>
#include <vector>
namespace encore::upstream {
struct FieldStoryProgramme {uint32_t programme=UINT32_MAX;FieldText boundary{},source{};uint32_t flags=0;};
struct FieldStoryText {uint32_t id=0;FieldText locale{},speaker{},body{},voice{};};
struct FieldStoryEncounter {uint32_t battle=0,actor=UINT32_MAX;FieldText basic{},music{};uint32_t basic_slot=UINT32_MAX,programme=UINT32_MAX;float return_music_seconds=0;uint32_t return_music_curve=0;};
struct FieldBasicOverride {uint32_t definition=0;FieldText source{};};
class FieldStoryView {
public:
 bool valid()const{return bytes_!=nullptr;}
 uint32_t scene_id()const;
 uint32_t programme_count()const;uint32_t text_count()const;uint32_t encounter_count()const;uint32_t override_count()const;
 std::string_view text(FieldText)const;
 FieldStoryProgramme programme(uint32_t)const;
 FieldStoryText dialogue(uint32_t id,std::string_view locale)const;
 FieldStoryText text_record(uint32_t index)const;
 FieldStoryEncounter encounter(uint32_t battle)const;
 FieldStoryEncounter encounter_for_programme(uint32_t programme)const;
 FieldBasicOverride basic_override(uint32_t stable_definition)const;
 bool bind(RoomView,FieldMapView,ItemView,std::string& error)const;
private:
 friend class FieldStoryData;
 const uint8_t*record(uint32_t section,uint32_t index)const;
 uint32_t count(uint32_t section)const;
 const uint8_t*bytes_=nullptr;size_t size_=0;
};
class FieldStoryData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 FieldStoryView view()const;
private:
 std::vector<uint8_t>bytes_;
};
}
