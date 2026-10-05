#pragma once
#include "encore/storage_data.hpp"
#include "encore/items_menu.hpp"
#include "encore/battle_round.hpp"
namespace encore::upstream {
class StorageState {
public:
 bool restore(ItemView,uint32_t,const std::vector<ItemInstance>&,std::string&);
 bool initialize(ItemView v,uint32_t capacity){std::string e;return restore(v,capacity,{},e);}
 bool valid()const{return data_.valid()&&capacity_!=0;}
 bool has_space()const{return valid()&&size()<capacity_;}
 uint32_t size()const{return uint32_t(items_.size());}uint32_t capacity()const{return capacity_;}
 const ItemInstance& instance(uint32_t i)const{return items_.at(i);}
 ItemView content()const{return data_;}const std::vector<ItemInstance>& instances()const{return items_;}
 bool append(ItemInstance,std::string&);bool erase_uid(uint32_t,std::string&);bool sort(StorageView,bool chinese,std::string&);
private:ItemView data_;uint32_t capacity_=0;std::vector<ItemInstance>items_;
};
enum class StorageMenuPhase:uint8_t {Closed,List,AskUnequip,AskEquip,Warning};
enum class StorageSoundEvent:uint32_t {Move=1,Confirm,Restricted,Clear,Equip,Back};
class StorageMenu {
public:
 bool initialize(StorageView,InventoryState&,StorageState&,BattleSessionStats&,bool chinese=false);
 bool open();bool input(int x,int y,bool confirm,bool cancel);bool idle_frame(double);
 bool active()const{return phase_!=StorageMenuPhase::Closed;}StorageMenuPhase phase()const{return phase_;}
 bool storage_panel()const{return storage_panel_;}uint32_t selection()const{return selection_;}uint32_t row_offset()const{return row_offset_;}
 bool question_yes()const{return question_yes_;}std::string prompt()const;
 uint32_t prompt_definition()const{return pending_definition_;}
 uint32_t cursor_frame()const;
 float cursor_row(bool stored)const{return cursor_rows_[stored?1:0];}
 float question_cursor_row()const{return question_row_;}
 uint32_t panel_selection(bool stored)const{return stored==storage_panel_?selection_:panel_selection_[stored?1:0];}
 uint32_t panel_row_offset(bool stored)const{return stored==storage_panel_?row_offset_:panel_offsets_[stored?1:0];}
 StorageView content()const{return data_;}const InventoryState& inventory()const{return *inventory_;}const StorageState& storage()const{return *storage_;}
 bool chinese()const{return chinese_;}void set_chinese(bool v){chinese_=v;}
 std::vector<StorageSoundEvent>take_sounds(){std::vector<StorageSoundEvent>v;v.swap(sounds_);return v;}
 const char*error()const{return error_.c_str();}
private:
 bool reject(const char*);bool checked_state();bool store(uint32_t uid);bool withdraw(uint32_t uid);bool equip(uint32_t uid,bool value);
 void clamp();void refresh_panels();void warning(StorageBinding);uint32_t size()const;
 StorageView data_;InventoryState*inventory_=nullptr;StorageState*storage_=nullptr;BattleSessionStats*stats_=nullptr;
 StorageMenuPhase phase_=StorageMenuPhase::Closed;bool storage_panel_=false,chinese_=false,question_yes_=true;
 uint32_t selection_=0,row_offset_=0,pending_uid_=0,pending_definition_=0;StorageBinding warning_=StorageBinding::StorageFullEn;float warning_time_=0;
 std::array<uint32_t,2>panel_selection_{},panel_offsets_{};
 std::array<float,2>cursor_rows_{},cursor_start_{},cursor_target_{},cursor_move_time_{};
 double cursor_time_=0;float question_row_=0,question_start_=0,question_target_=0,question_move_time_=0;
 std::vector<StorageSoundEvent>sounds_;std::string error_;
};
}
