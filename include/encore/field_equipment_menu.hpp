#pragma once
#include "encore/field_equipment_data.hpp"
#include "encore/items_menu.hpp"
#include "encore/source_random.hpp"
#include <functional>

namespace encore::upstream {
struct SessionSnapshot;
struct FieldHealingResult {int32_t hp=0,pp=0,rolled=0;bool full=false;};
bool field_healing(FieldPsiSkill,int32_t hp,int32_t maxhp,int32_t pp,int32_t iq,SourceRandom&,FieldHealingResult&,std::string&);
// Select the admitted active party member from either a legacy singleton or a
// complete startup roster. Inactive character records do not expand the party.
// Failed selection leaves the previous index unchanged.
bool field_equipment_character(FieldEquipmentView,const SessionSnapshot&,uint32_t&,std::string&);
struct FieldEquipmentSnapshot {
 std::string owner,nickname;
 uint32_t level=0;
 int64_t cash=0;
 InventoryState inventory;
 std::array<int32_t,7> stats{};
 int32_t pp=0;std::vector<std::string> learned_skills;
};
// UID zero and UINT32_MAX are legitimate identities. None is a separate tag.
struct FieldEquipmentCandidate {bool none=true;ItemInstance item{};};
struct FieldEquipmentHost {
 std::function<bool(FieldEquipmentSnapshot&,std::string&)> read;
 std::function<bool(uint32_t,bool,uint32_t,std::array<int32_t,7>&,std::string&)> preview;
 std::function<bool(uint32_t,bool,uint32_t,std::string&)> commit;
 std::function<bool(uint32_t,std::string&,std::string&)> use_skill;
 std::function<bool(uint32_t,std::string&)> open_command;
};
enum class FieldEquipmentPhase:uint8_t {Closed,PauseOpening,Pause,EquipOpening,Slots,Candidates,EquipClosing,PauseClosing,PsiOpening,Psi,PsiFailed,PsiTarget,PsiResult,Submenu};
enum class FieldEquipmentSound:uint8_t {PauseOpen=1,PauseClose,EquipOpen,EquipClose,Move,Confirm,Restricted,Clear,Equip,Back};
class FieldEquipmentMenu {
public:
 bool initialize(FieldEquipmentView,FieldEquipmentHost,bool chinese=false);
 bool open();
 bool input(int x,int y,bool confirm,bool cancel,bool scope=false,bool pause_toggle=false);
 bool idle_frame(double);
 bool resume_submenu(){if(phase_!=FieldEquipmentPhase::Submenu)return fail("Field submenu resume state rejected");if(!read_snapshot())return false;phase_=FieldEquipmentPhase::Pause;preview_=snapshot_.stats;retarget_cursor(true);return true;}
 bool active()const{return phase_!=FieldEquipmentPhase::Closed;}
 bool visible()const{return active();}
 bool equipment_visible()const;
 bool psi_visible()const{return psi_closing_||phase_==FieldEquipmentPhase::PsiOpening||phase_==FieldEquipmentPhase::Psi||phase_==FieldEquipmentPhase::PsiFailed||phase_==FieldEquipmentPhase::PsiTarget||phase_==FieldEquipmentPhase::PsiResult;}
 bool psi_learned()const;
 uint32_t psi_skill_count()const;uint32_t psi_skill_index(uint32_t row)const;
 uint32_t psi_selection()const{return psi_;}
 uint32_t psi_requested_skill()const{return psi_requested_;}
 FieldPsiSkill psi_selected_skill()const{return data_.psi_skill(psi_skill_index(psi_));}
 std::string_view psi_message()const{return psi_message_;}
 // Delivered only after the silent menu close; the caller validates the live
 // skill/PP again before unpausing and dispatching the checked field action.
 bool take_psi_request(){if(active()||!psi_request_)return false;psi_request_=false;return true;}
 bool description_visible()const{return equipment_visible()&&description_progress()>0;}
 FieldEquipmentPhase phase()const{return phase_;}
 uint32_t command_selection()const{return command_;}
 uint32_t slot_selection()const{return slot_;}
 uint32_t candidate_selection()const{return candidate_;}
 uint32_t candidate_offset()const{return offset_;}
 uint32_t candidate_count()const{return uint32_t(candidates_.size());}
 const FieldEquipmentCandidate& candidate(uint32_t i)const{return candidates_.at(i);}
 const ItemInstance* selected_item()const;
 const ItemInstance* description_item()const{return has_description_?&description_item_:nullptr;}
 const ItemInstance* equipped_item(uint32_t slot)const;
 const FieldEquipmentSnapshot& snapshot()const{return snapshot_;}
 const std::array<int32_t,7>& preview_stats()const{return preview_;}
 FieldEquipmentView content()const{return data_;}
 bool chinese()const{return chinese_;}
 void set_chinese(bool v){chinese_=v;}
 uint32_t cursor_frame()const;
 float cursor_row()const{return cursor_row_;}
 float cursor_column()const{return cursor_col_;}
 float pause_progress()const;
 float equip_progress()const;
 float description_progress()const;
 // Relative Y from source-owned animation; open roles automatically select
 // the current closing direction as well, including the description panel.
 float animation_value(FieldClipRole)const;
 std::vector<FieldEquipmentSound>take_sounds(){std::vector<FieldEquipmentSound>v;v.swap(sounds_);return v;}
 const char*error()const{return error_.c_str();}
private:
 bool fail(const char*);
 bool read_snapshot();bool validate_snapshot(const FieldEquipmentSnapshot&);
 bool update_preview();bool enter_candidates();
 void update_description();void retarget_cursor(bool instant=false);
 float sample(FieldClipRole,float)const;
 FieldEquipmentView data_;FieldEquipmentHost host_;FieldEquipmentSnapshot snapshot_;
 std::array<int32_t,7>preview_{};std::vector<FieldEquipmentCandidate>candidates_;
 FieldEquipmentPhase phase_=FieldEquipmentPhase::Closed;
 uint32_t command_=0,slot_=0,candidate_=0,offset_=0,psi_=0,psi_requested_=0;std::string psi_message_;
 bool chinese_=false,description_open_=false,equip_closing_=false,psi_closing_=false,psi_request_=false;
 bool has_description_=false;ItemInstance description_item_{};
 float phase_time_=0,pause_time_=0,equip_time_=0,description_time_=0,psi_time_=0;
 float description_from_=0,description_to_=0;
 float cursor_row_=0,cursor_col_=0,cursor_from_row_=0,cursor_from_col_=0,cursor_target_row_=0,cursor_target_col_=0,cursor_move_time_=0;
 double cursor_time_=0;
 std::string error_;std::vector<FieldEquipmentSound>sounds_;
};
}
