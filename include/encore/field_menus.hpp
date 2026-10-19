#pragma once
#include "encore/native_session.hpp"
#include "encore/startup_settings.hpp"
#include "encore/item_details.hpp"
#include <functional>

namespace encore::upstream {
// UI schema identities. Labels, ordering, geometry, bindings and game rules
// are supplied by ENCFMN01; no game definitions are embedded in this controller.
enum class FieldMenuText:uint32_t {Goods,Status,Map,Options,Equip,Unequip,Use,Sort,Drop,Manual,Auto,DropConfirm,Target,Yes,No,KeyItems,Empty,Skills,Field,Battle,Psi,HP,PP,Level,Exp,Next,Offense,Defense,Speed,IQ,Guts,Weapon,Body,Arms,Other,None,File,General,Title,Quit,TitleConfirm,QuitConfirm,TextSpeed,Flavor,Prompts,Audio,Video,Controls,Language,Master,Music,Sfx,Resolution,Fullscreen,IntegerScaling,Vsync,On,Off,MapMissing,MapFast,MapClose,MapMarkers,ControlAccept,ControlCancel,ControlMenu,ControlScope,ControlToggle,ControlPrev,ControlNext,ControlMap,Capture,Display,PlatformVideo,Back,Reset,ResetConfirm,Count};
struct FieldMenuLabel {std::string en,zh;const std::string&get(bool chinese)const{return chinese?zh:en;}};
struct FieldMenuItem {
 std::string id;FieldMenuLabel name,description,sorting,result;
 uint32_t key=0,drop=0,action=0,doses=0,score=0,equipped_score=0,slot=UINT32_MAX;
 uint32_t definition=UINT32_MAX;
};
struct FieldMenuSkill {
 std::string id,flag,battle_skill,weapon;FieldMenuLabel name,description;
 uint32_t category=0,cost=0; // 0 field, 1 battle, 2 PSI
 std::vector<std::string>weapons;
};
struct FieldMenuImage {uint32_t width=0,height=0;std::vector<uint8_t>rgba;};
struct FieldMenuMarker {uint32_t image=0;float x=0,y=0;bool flip=false;FieldMenuLabel label;};
struct FieldMenuGeometry {float x=0,y=0,w=0,h=0;};
struct FieldMenuPreferences {
 std::array<uint32_t,3>volume{};uint32_t display=0;
 // Logical masks are schema input identities; values are physical HID masks.
 std::vector<uint32_t>buttons;
};
bool expand_field_menu_text(std::string_view,std::string_view,const std::vector<std::string>&,std::string&,std::string&);
class FieldMenusData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 const std::string&text(FieldMenuText id,bool zh)const{return labels.at(size_t(id)).get(zh);}
 const FieldMenuItem*item(std::string_view)const;
 bool bind_items(ItemView,ItemDetailsView,std::string&)const;
 bool supports(const FieldMenuPreferences&)const;
 uint32_t remap(uint32_t raw,const FieldMenuPreferences&)const;
 std::vector<FieldMenuLabel>labels;std::vector<FieldMenuItem>items;std::vector<FieldMenuSkill>skills;
 std::vector<FieldMenuImage>images;std::vector<FieldMenuMarker>markers;
 // panel, header, body, description, footer, cursor; adapted from source UI.
 std::array<FieldMenuGeometry,6>geometry{};
 std::vector<double>volume_db;std::vector<uint32_t>logical_buttons;std::vector<std::string>button_names,locales,display_names;std::vector<FieldMenuLabel>locale_names;std::array<std::vector<FieldMenuLabel>,3>setting_labels;FieldMenuLabel capture_cancel;
 FieldMenuPreferences defaults;uint32_t rows=0,columns=0;float row_pitch=0,column_pitch=0,line_height=0,map_offset_x=0,map_offset_y=0,map_divisor=0,map_speed=0,map_fast=0;
 std::string map_item;bool description_toggle=false;float map_home_x=0,map_home_y=0;std::array<std::array<uint32_t,2>,2>display_sizes{};
private:bool valid_=false;
};
enum class FieldPreferenceRead {Loaded,Missing,Error};
bool encode_field_preferences(const FieldMenusData&,const FieldMenuPreferences&,std::vector<uint8_t>&,std::string&);
bool decode_field_preferences(const FieldMenusData&,const uint8_t*,size_t,FieldMenuPreferences&,std::string&);
FieldPreferenceRead read_field_preferences(const FieldMenusData&,const char*,FieldMenuPreferences&,std::string&);
bool write_field_preferences(const FieldMenusData&,const char*,const FieldMenuPreferences&,std::string&);

// The save snapshot retains its admitted house checkpoint. Actual field map
// coordinates are transient context, never an unsupported field-save migration.
struct FieldMenusState {SessionSnapshot session;std::array<int32_t,7>stats{};uint32_t next_exp=0;FieldMenuPreferences preferences;bool chinese=false,in_field=false;float world_x=0,world_y=0;bool area_map=false;std::string area_map_item;std::array<float,2>area_map_position{};};
std::array<float,2>field_menu_map_position(const FieldMenusData&,const FieldMenusState&);
struct FieldMenusChanges {std::vector<SessionItem>inventory,keys;SessionSettings settings;FieldMenuPreferences preferences;};
bool merge_field_menu_session(const FieldMenusState&,const FieldMenusChanges&,const SessionSnapshot&,SessionSnapshot&,std::string&);
// Every write stages the entire candidate and commits only after the host has
// validated the live session, resource scope and persistent settings.
struct FieldMenusHost {
 std::function<bool(FieldMenusState&,std::string&)>read;
 std::function<bool(const FieldMenusState&,const FieldMenusChanges&,std::string&)>commit;
 std::function<bool(uint32_t,bool,uint32_t,std::string&)>equip;
 std::function<bool(std::string_view,std::string&)>locale;
};
enum class FieldMenusPhase:uint32_t {Closed,Goods,Actions,Sort,SortTarget,Drop,Target,Message,Status,SkillTabs,Skills,Map,Options,File,General,Audio,Video,Controls,Choice,Confirm,Capture};
enum class FieldMenusExit:uint32_t {None,Pause,Title,Quit};
struct FieldMenuLine {std::string label,value;bool disabled=false;uint32_t column=0;bool equipped=false;};
struct FieldMenuPage {std::string title,subtitle,description,footer;std::vector<FieldMenuLine>lines;uint32_t selected=0,offset=0;bool cursor=true,grid=false,has_item_description=false;ItemInstance described_item{};};
class FieldMenus {
public:
 bool initialize(const FieldMenusData&,const StartupSettingsData&,FieldMenusHost,std::string&);
 bool open(uint32_t command,std::string&);
 bool input(int x,int y,bool confirm,bool cancel,bool scope,int tab,uint32_t raw_down,std::string&);
 bool tick_map(int x,int y,bool fast,double dt,std::string&);
 void abort_map(){if(phase_==FieldMenusPhase::Map)back();}
 bool active()const{return phase_!=FieldMenusPhase::Closed;}
 FieldMenusPhase phase()const{return phase_;}uint32_t owner_token()const{return uint32_t(phase_);}
 uint32_t selection()const{return phase_==FieldMenusPhase::Choice?choice_:selection_;}bool key_inventory()const{return key_;}
 FieldMenusExit take_exit(){auto e=exit_;exit_=FieldMenusExit::None;return e;}
 FieldMenuPage page()const;
 int flavor_preview_index()const{return !settings_?-1:phase_==FieldMenusPhase::Choice&&choice_kind_==1?int(choice_):settings_->flavor_index(state_.session.settings.menu_flavor);}
 const FieldMenusState&state()const{return state_;}
 const FieldMenusData*content()const{return data_;}
 bool map_missing()const{return missing_map_;}bool markers_visible()const{return markers_;}
 float map_x()const{return map_x_;}float map_y()const{return map_y_;}
 bool take_changed(){bool v=changed_;changed_=false;return v;}
 // Sound IDs use the already checked FieldEquipmentSound schema.
 std::vector<uint32_t>take_sounds(){std::vector<uint32_t>v;v.swap(sounds_);return v;}
private:
 bool refresh(std::string&);bool commit(FieldMenusState,std::string&);bool apply_choice(std::string&);void back();
 const std::vector<SessionItem>&goods()const;std::vector<SessionItem>&goods(FieldMenusState&)const;
 const FieldMenuItem*selected_item()const;std::vector<uint32_t>actions()const;std::vector<const FieldMenuSkill*>skills()const;
 std::string text(FieldMenuText)const;bool open_map(bool force,std::string&);void clamp_map();
 const FieldMenusData*data_=nullptr;const StartupSettingsData*settings_=nullptr;FieldMenusHost host_;FieldMenusState state_;
 FieldMenusPhase phase_=FieldMenusPhase::Closed,parent_=FieldMenusPhase::Closed,map_parent_=FieldMenusPhase::Closed;
 FieldMenusExit exit_=FieldMenusExit::None;uint32_t selection_=0,item_selection_=0,action_selection_=0,sort_source_=0,skill_tab_=0,choice_=0,choice_kind_=0,choice_previous_=0;bool key_=false,missing_map_=true,markers_=true,changed_=false;
 float map_x_=0,map_y_=0;double map_fraction_=0;std::string message_;std::vector<uint32_t>sounds_;
};
}
