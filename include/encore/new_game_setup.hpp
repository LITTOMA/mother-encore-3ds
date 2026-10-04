#pragma once
#include "encore/battle_round_data.hpp"
#include "encore/session_save.hpp"
#include "encore/native_session.hpp"
#include "encore/restore_data.hpp"
#include "encore/load_rng.hpp"
#include "encore/startup_settings.hpp"
#include "encore/localization.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
struct NamingRect {float x=0,y=0,w=0,h=0;};
struct NamingKey {uint32_t codepoint=0,kind=0;std::string value;NamingRect rect;std::array<uint32_t,4> neighbors{};};
struct NamingResource {std::string path;uint32_t width=0,height=0,columns=0,rows=0;};
struct NamingFrame {double time=0;uint32_t frame=0;};
struct NamingAnimation {double length=0;std::vector<NamingFrame>keys;uint32_t frame(double)const;};
struct NamingBattleText {uint32_t battle_id=0,index=0;std::string key,source_text,expected;std::vector<std::string>parts;};
struct NamingField {
 uint32_t kind=0,maximum=0,resource=0;std::string target,initial,prompt;std::vector<std::string>defaults;bool shadow=false;
 std::array<float,2>actor_position{},shadow_position{};NamingAnimation actor;
};
struct NamingPresentation {
 uint32_t source_width=0,source_height=0,box=0,cursor=0,actor=0,shadow=0;
 std::array<uint32_t,5>layouts{};std::array<uint32_t,7>texts{};std::array<uint32_t,3>sounds{};
 std::vector<uint32_t>keyboard;std::array<float,4>field_bevel{};
};
class NewGameSetupData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 uint32_t maximum=0;std::string target,initial;std::vector<std::string>other_initial,defaults,blacklist,texts,sounds;
 double cancel_delay=0,error_duration=0;float font_height=0;std::vector<NamingRect>layouts;std::array<uint32_t,4>patch{};std::array<uint32_t,5>colors{};
 std::vector<NamingResource>resources;std::vector<std::vector<NamingKey>>panels;
 std::array<float,2>actor_position{},shadow_position{},arrow_offset{};NamingAnimation actor,arrow;double arrow_move=0;
 // Resolves every indexed text only after matching its source key, template and
 // compiled value. Unknown encounters/records fail closed. Nickname is never
 // interpreted as a template, so punctuation cannot inject another tag.
 bool battle_text(RoundView,uint32_t,std::string_view nickname,std::string&)const;
 bool supported_name(std::string_view)const;
 bool supported_field(uint32_t,std::string_view)const;
 std::vector<NamingField> fields;NamingPresentation presentation;
private:bool valid_=false;std::vector<NamingBattleText>battle_texts_;
};
// Call on copies at New Game selection, before entering naming. Source UID
// creation order/timing is preserved; cancellation discards the copies.
bool stage_new_game_startup(const NativeSessionData&,const RestoreData&,
 SourceRandom&,std::vector<uint32_t>&,const LoadRngClockProvider&,
 SessionSnapshot&,std::string&);
enum class NamingPhase:uint32_t {Closed,Editing,Accepted,Cancelled,Settings,SettingOption,Confirmation};
struct NamingInput {int x=0,y=0;bool accept=false,cancel=false,toggle_panel=false,command=false,next=false,previous=false;};
class NewGameSetup {
public:
 bool open(const NewGameSetupData&,const StartupSettingsData&,std::string&);void close(){phase_=NamingPhase::Closed;}
 bool step(double,const NamingInput&,std::string&);
 NamingPhase phase()const{return phase_;}bool active()const{return phase_==NamingPhase::Editing||phase_==NamingPhase::Settings||phase_==NamingPhase::SettingOption||phase_==NamingPhase::Confirmation;}
 const StartupSettingsData*settings_data()const{return settings_data_;}
 const SessionSettings&settings()const{return settings_;}uint32_t settings_row()const{return settings_row_;}uint32_t option()const{return option_;}uint32_t confirmation()const{return confirmation_;}
 uint32_t preview_characters()const{return preview_visible_;}std::string preview_flavor()const;
 const NewGameSetupData*data()const{return data_;}uint32_t panel()const{return panel_;}uint32_t selected()const{return selected_;}
 uint32_t field_index()const{return field_;}const NamingField&field()const{return data_->fields.at(field_);}
 const std::vector<std::string>&values()const{return values_;}
 const std::string&name()const{return value_;}std::string prompt()const;
 void set_locale(const LocaleSelection*value){locale_=value;}
 std::string localized(std::string_view identity,std::string_view expected)const;
 const std::string&locale_error()const{return locale_error_;}std::string dotted_name()const;
 double elapsed()const{return elapsed_;}std::array<float,2>cursor()const;
 // Builds a detached source startup candidate; never changes RNG or saves.
 // Takes the complete staged startup roster and preserves its allocated UIDs.
 bool apply(SessionSnapshot&,const NativeSessionData&,std::string&)const;
 std::vector<std::string>take_sounds(){auto s=std::move(sounds_);sounds_.clear();return s;}
private:
 void enter_settings();void restart();void step_settings(double,const NamingInput&);void reset_preview();
 void enter(uint32_t);void previous();void select(uint32_t);void erase();void accept();void sound(uint32_t);std::string normalized(std::string_view)const;
 mutable std::string locale_error_;const LocaleSelection*locale_=nullptr;
 const StartupSettingsData*settings_data_=nullptr;SessionSettings settings_;uint32_t settings_row_=0,option_=0,confirmation_=0,preview_visible_=0;double preview_time_=0;
 const NewGameSetupData*data_=nullptr;NamingPhase phase_=NamingPhase::Closed;uint32_t panel_=0,selected_=0,error_prompt_=0,field_=0;std::vector<std::string>values_;
 std::string value_;double elapsed_=0,error_remaining_=0,cursor_time_=0;std::array<float,2>cursor_from_{},cursor_to_{};std::vector<std::string>sounds_;
};
}
