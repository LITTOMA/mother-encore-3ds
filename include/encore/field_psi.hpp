#pragma once
#include "encore/session_save.hpp"
#include "encore/source_random.hpp"
#include "encore/items_data.hpp"
#include <array>
#include <functional>
namespace encore::upstream {
enum class FieldPsiOperation:uint32_t {Excluded=0,Telepathy=1,Heal=2,Pending=3};
enum class FieldPsiPhase:uint8_t {Closed,Skills,Targets,Message,Closing};
enum class FieldPsiSound:uint32_t {Open=1,Close,Move,Confirm,Cancel,Restricted,Heal};
enum class FieldPsiLayout:uint32_t {Panel=1,RowOrigin,LevelOrigin,Description,Divider,TargetPanel,TargetTitle,TargetOrigin,CharacterBar,MenuTitle,Portrait,CostPanel,CostLabel,CostValue,CursorOffset,CursorCenter,PanelPatch,TargetPatch,TargetExpand,BarPatch,CostPatch,SourceViewport,PlatformViewport,RowStride,LevelStride,DescriptionPolicy,TargetPolicy,DividerColor,TargetTitleColor,TitleBox,TitlePatch,HighlightColor,TitleExpand};
struct FieldPsiLocale {std::string code,title,whom,pp,insufficient,forgetful,hp_max,hp_up;};
struct FieldPsiSkill {uint32_t id=0;std::string source,key,name_key,name_en,name_zh,desc_en,desc_zh;FieldPsiOperation operation=FieldPsiOperation::Excluded;int32_t level=-1;uint32_t target=0,pp=0,heal=0,variance=0,iq_divisor=0;bool target_unconscious=false,target_incapacitated=false;};
struct FieldPsiItem {std::string source,key,skill;bool equippable=false;std::vector<std::string>users;};
struct FieldPsiStatus {std::string id;bool forgetful=false,unconscious=false,incapacitated=false;};
struct FieldPsiAsset {uint32_t role=0,width=0,height=0,file_bytes=0,crc32=0;std::string source,path;};
struct FieldPsiAnimationKey {float time=0,value=0,ease=1;};
struct FieldPsiAnimation {float length=0;std::vector<FieldPsiAnimationKey>keys;};
struct FieldPsiPortrait {std::string character;uint32_t normal=0,highlight=0;};
struct FieldPsiEffect {BattleValue color{};float cut=0,duration=0,spin=0,restore_cut=0;uint32_t transition=0,ease_in=0,ease_out=0;};
class FieldPsiData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const FieldPsiSkill*skill(uint32_t)const;const FieldPsiSkill*skill(std::string_view)const;
 const FieldPsiItem*item(std::string_view)const;
 const FieldPsiLocale*locale(std::string_view)const;const FieldPsiStatus*status(std::string_view)const;
 const std::vector<std::string>&party_order()const{return order_;}const std::vector<FieldPsiPortrait>&portraits()const{return portraits_;}
 const std::vector<FieldPsiAsset>&assets()const{return assets_;}BattleValue layout(FieldPsiLayout role)const{return layouts_.at(size_t(role)-1);}
 const FieldPsiAnimation&animation(bool closing)const{return animations_[closing?1:0];}
 std::string_view sound(FieldPsiSound r)const{return sounds_.at(size_t(r)-1);}
 const std::array<std::string,4>&level_labels()const{return levels_;}double message_delay()const{return message_delay_;}
 const FieldPsiAnimation&cost_animation()const{return cost_animation_;}float cost_visible_after()const{return cost_visible_after_;}float cost_base_y()const{return cost_base_y_;}
 const FieldPsiEffect&effect()const{return effect_;}std::string_view fallback(uint32_t role)const{return fallbacks_.at(role-1);}
 std::string_view source_font()const{return source_font_;}std::string_view number_font()const{return number_font_;}
 const std::vector<std::string>&telepathy_users()const{return telepathy_users_;}std::string_view telepathy_skill()const{return telepathy_skill_;}
private:
 bool valid_=false;std::array<uint8_t,20>pin_{};std::vector<FieldPsiSkill>skills_;std::vector<FieldPsiStatus>statuses_;std::vector<FieldPsiItem>items_;std::vector<std::string>order_,telepathy_users_;std::string telepathy_skill_,source_font_,number_font_;
 std::vector<FieldPsiLocale>locales_;std::vector<FieldPsiAsset>assets_;std::vector<FieldPsiPortrait>portraits_;std::array<BattleValue,33>layouts_{};std::array<FieldPsiAnimation,2>animations_;std::array<std::string,7>sounds_;std::array<std::string,4>levels_,fallbacks_;FieldPsiEffect effect_;double message_delay_=0;FieldPsiAnimation cost_animation_;float cost_visible_after_=0,cost_base_y_=0;
};
// Party remains in the source stored order. The menu derives canonical target
// and character-tab order from its resource. Effective skills include source
// inventory-granted skills, in the exact learned-list plus inventory-list source order.
struct FieldPsiMember {SessionCharacter character;int64_t maximum_hp=0,maximum_pp=0,iq=0,guts=0;std::vector<std::string>effective_skills;};
struct FieldPsiSnapshot {std::vector<FieldPsiMember>party;std::string player;};
struct FieldPsiResult {uint32_t skill=0;std::string target,nickname;int64_t old_hp=0,unclamped_hp=0,hp=0,heal=0;bool maximum=false;};
struct FieldPsiCandidate {FieldPsiSnapshot snapshot;std::vector<FieldPsiResult>results;uint64_t random_state=0;SourceRandom random{0};};
bool validate_field_psi_snapshot(const FieldPsiData&,const FieldPsiSnapshot&,std::string&);
bool field_psi_targetable(const FieldPsiData&,const FieldPsiSkill&,const FieldPsiMember&,std::string&);
// Detached source execution with copied RNG. Caller atomically commits session
// and final RNG state after re-reading authoritative state; failures publish none.
bool prepare_field_psi(const FieldPsiData&,const FieldPsiSnapshot&,std::string_view caster,uint32_t skill,std::string_view target,const SourceRandom&,FieldPsiCandidate&,std::string&);
struct FieldPsiHost {
 std::function<bool(FieldPsiSnapshot&,std::string&)>read;
 std::function<bool(std::string_view,uint32_t,std::string_view,FieldPsiCandidate&,std::string&)>commit;
 // Required complete preflight of Player.use_telepathy for this live world.
 std::function<bool(std::string&)>admit_telepathy;
 // Invoked after PP commit and source silent PSI + commands close/unpause.
 std::function<bool(std::string&)>close_commands_for_telepathy;
 std::function<bool(std::string&)>telepathy;
 // Actual source font measurements: description string width and wrapped height.
 std::function<bool(std::string_view,float,float&,float&,float&,std::string&)>measure;
 std::function<void(const std::vector<std::string>&)>highlight;
};
class FieldPsiMenu {
public:
 bool initialize(const FieldPsiData*,FieldPsiHost,std::string_view locale="en");bool open();bool close(bool silent=false);
 bool input(int x,int y,bool confirm,bool cancel,int character_step=0);bool idle_frame(double);bool set_locale(std::string_view);
 bool active()const{return phase_==FieldPsiPhase::Skills||phase_==FieldPsiPhase::Targets||phase_==FieldPsiPhase::Message;}
 bool visible()const{return phase_!=FieldPsiPhase::Closed;}FieldPsiPhase phase()const{return phase_;}
 const FieldPsiData*content()const{return data_;}const FieldPsiSnapshot&snapshot()const{return snapshot_;}const FieldPsiLocale&locale()const{return *data_->locale(locale_);}
 const FieldPsiMember*caster()const;const FieldPsiSkill*selected()const;const std::vector<std::vector<uint32_t>>&groups()const{return groups_;}
 const std::vector<std::string>&targets()const{return targets_;}uint32_t target_selection()const{return target_;}uint32_t page()const{return page_;}uint32_t row()const{return row_;}uint32_t column()const{return column_;}uint32_t rows()const{return rows_;}
 std::string_view description()const{return description_;}bool description_wraps()const{return wraps_;}bool selectable(const FieldPsiSkill&)const;
 bool take_back_request(){bool r=back_;back_=false;return r;}bool telepathy_closed()const{return telepathy_closed_;}
 float animation_y()const;float cost_animation_y()const;bool cost_visible()const{return selected()&&cursor_time_>=data_->cost_visible_after();}double cursor_time()const{return cursor_time_;}const std::string&error()const{return error_;}
 std::vector<FieldPsiSound>take_sounds(){std::vector<FieldPsiSound>r;r.swap(sounds_);return r;}
private:
 bool fail(const char*);bool refresh();bool rebuild();bool describe(std::string);bool move_row(int);void update_page(int);bool confirm_skill();bool cast(std::string_view);bool show_next_message();void highlight();
 const FieldPsiData*data_=nullptr;FieldPsiHost host_;FieldPsiSnapshot snapshot_;std::string locale_,caster_,description_,error_;FieldPsiPhase phase_=FieldPsiPhase::Closed;
 std::vector<std::vector<uint32_t>>groups_;std::vector<std::string>targets_,messages_;std::vector<FieldPsiSound>sounds_;uint32_t page_=0,row_=0,column_=0,rows_=0,target_=0;bool wraps_=false,back_=false,telepathy_closed_=false,closing_=false;double animation_time_=0,cursor_time_=0,message_time_=0;
};
// Typed native endpoints for the original Player parent-first target dispatch.
// Probe uses the live cached EventDetector collider, not an invented radius.
struct FieldTelepathyProbe {uint32_t collider=0,parent=0;bool parent_method=false,collider_method=false,has_thoughts=false,no_problem_thoughts=false,button_prompt=false;};
struct FieldTelepathyHost {
 std::function<bool(bool&,std::string&)>player_has_field_skill;
 std::function<bool(FieldTelepathyProbe&,std::string&)>probe;
 std::function<bool(std::string_view,std::string&)>admit_dialogue;
 std::function<bool(uint32_t,std::string&)>admit_target;
 std::function<bool(std::string&)>clear_event_collider;
 std::function<bool(uint32_t,std::string&)>turn;
 std::function<bool(uint32_t,const FieldPsiEffect&,std::string&)>effect;
 std::function<bool(uint32_t,std::string&)>target_telepathy;
 std::function<bool(std::string_view,std::string&)>dialogue;
 std::function<bool(uint32_t,std::string&)>press_prompt;
};
class FieldTelepathyRuntime {
public:bool initialize(const FieldPsiData*,FieldTelepathyHost);bool admit(std::string&);bool execute(std::string&);
private:bool plan(FieldTelepathyProbe&,bool&,uint32_t&,uint32_t&,bool&,std::string&);const FieldPsiData*data_=nullptr;FieldTelepathyHost host_;
};
}
