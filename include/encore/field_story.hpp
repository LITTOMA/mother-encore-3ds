#pragma once
#include "encore/field_data.hpp"
#include "encore/items_data.hpp"
#include "encore/dialogue_choices_data.hpp"
#include "encore/house_presentation.hpp"
#include <string>
#include <vector>
namespace encore::upstream {
class NewGameSetupData;class LocaleSelection;class OpeningWorld;class DialogueChoices;struct SessionSnapshot;
// Checked source character tags; literal nicknames never become templates.
bool format_field_story_text(std::string_view,const NewGameSetupData&,const SessionSnapshot&,const LocaleSelection&,std::string&,std::string&);
bool format_field_story_span(std::string_view,const NewGameSetupData&,const SessionSnapshot&,const LocaleSelection&,LocalizedHouseSpan&,std::string&);
struct FieldInteraction {uint32_t actor=UINT32_MAX,programme=UINT32_MAX;FieldText flag{};uint32_t turns=0,status=0;FieldText source{};};
struct FieldStoryProgramme {uint32_t programme=UINT32_MAX;FieldText boundary{},source{};uint32_t flags=0;};
struct FieldStoryText {uint32_t id=0;FieldText locale{},speaker{},body{},voice{};};
struct FieldStoryEncounter {uint32_t battle=0,actor=UINT32_MAX;FieldText basic{},music{};uint32_t basic_slot=UINT32_MAX,programme=UINT32_MAX;float return_music_seconds=0;uint32_t return_music_curve=0;};
struct FieldBasicOverride {uint32_t definition=0;FieldText source{};};
class FieldStoryView {
public:
 bool valid()const{return bytes_!=nullptr;}
 uint32_t scene_id()const;uint32_t interaction_count()const;
 const DialogueChoicesData* choices()const{return choices_;}
 FieldInteraction interaction(uint32_t)const;
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
 const uint8_t*bytes_=nullptr;size_t size_=0;const DialogueChoicesData* choices_=nullptr;
};
// Shared field UI handoff; the caller owns real text completion, fresh input,
// rendering and audio. Selection clears old text before dispatching its target.
bool prepare_field_story_choices(OpeningWorld&,FieldStoryView,DialogueChoices&,uint32_t&generation,std::string&);
bool select_field_story_choice(OpeningWorld&,FieldStoryView,HousePresentation&,DialogueChoices&,uint32_t pc,uint32_t generation,std::string&);
class FieldStoryData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 FieldStoryView view()const;
private:
 std::vector<uint8_t>bytes_;DialogueChoicesData choices_;
};
}
