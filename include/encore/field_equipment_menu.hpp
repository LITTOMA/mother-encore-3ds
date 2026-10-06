#pragma once
#include "encore/field_equipment_data.hpp"
#include "encore/items_menu.hpp"
#include <functional>

namespace encore::upstream {
struct FieldEquipmentSnapshot {
 std::string owner,nickname;
 uint32_t level=0;
 int64_t cash=0;
 InventoryState inventory;
 std::array<int32_t,7> stats{};
};
// UID zero and UINT32_MAX are legitimate identities. None is a separate tag.
struct FieldEquipmentCandidate {bool none=true;ItemInstance item{};};
struct FieldEquipmentHost {
 std::function<bool(FieldEquipmentSnapshot&,std::string&)> read;
 std::function<bool(uint32_t,bool,uint32_t,std::array<int32_t,7>&,std::string&)> preview;
 std::function<bool(uint32_t,bool,uint32_t,std::string&)> commit;
};
enum class FieldEquipmentPhase:uint8_t {Closed,PauseOpening,Pause,EquipOpening,Slots,Candidates,EquipClosing,PauseClosing};
enum class FieldEquipmentSound:uint8_t {PauseOpen=1,PauseClose,EquipOpen,EquipClose,Move,Confirm,Restricted,Clear,Equip,Back};
class FieldEquipmentMenu {
public:
 bool initialize(FieldEquipmentView,FieldEquipmentHost,bool chinese=false);
 bool open();
 bool input(int x,int y,bool confirm,bool cancel,bool scope=false,bool pause_toggle=false);
 bool idle_frame(double);
 bool take_items_request(){const bool value=items_request_;items_request_=false;return value;}
 bool items_suspended()const{return items_suspended_;}
 bool resume_items_checked();
 bool active()const{return phase_!=FieldEquipmentPhase::Closed;}
 bool visible()const{return active();}
 bool equipment_visible()const;
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
 uint32_t command_=0,slot_=0,candidate_=0,offset_=0;
 bool chinese_=false,description_open_=false,equip_closing_=false;
 bool items_request_=false,items_suspended_=false;
 bool has_description_=false;ItemInstance description_item_{};
 float phase_time_=0,pause_time_=0,equip_time_=0,description_time_=0;
 float description_from_=0,description_to_=0;
 float cursor_row_=0,cursor_col_=0,cursor_from_row_=0,cursor_from_col_=0,cursor_target_row_=0,cursor_target_col_=0,cursor_move_time_=0;
 double cursor_time_=0;
 std::string error_;std::vector<FieldEquipmentSound>sounds_;
};
}
