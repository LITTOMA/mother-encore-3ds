#pragma once
#include "encore/items_data.hpp"
#include <vector>
namespace encore::upstream {
class InventoryState {
public:
 bool initialize(ItemView);
 // Import already resolved item definitions with saved identities. Performs
 // generic capacity/definition/dose/equipment/UID checks before replacing state.
 // UID zero is valid for saved instances (the source UID is an unsigned draw).
 bool restore(ItemView,const std::vector<ItemInstance>&,std::string& error);
 bool valid()const{return data_.valid();}
 bool has_space()const{return valid()&&size()<data_.metadata().capacity;}
 // Preflight before the caller performs the source UID draw. append checks again
 // and commits only a valid, unequipped item with a unique externally owned UID.
 bool can_append(uint32_t definition,uint32_t doses,std::string& error)const;
 bool append(uint32_t definition,uint32_t doses,uint32_t uid,std::string& error);
 // Source ordinary-inventory transfer primitives preserve opaque saved UIDs.
 bool erase_uid(uint32_t uid,std::string& error);
 bool equip_uid(uint32_t uid,bool equipped,std::string& error);
 const std::vector<ItemInstance>& instances()const{return instances_;}
 const ItemView& content()const{return data_;}
 uint32_t size()const{return uint32_t(instances_.size());}
 const ItemInstance& instance(uint32_t i)const{return instances_.at(i);}
 bool can_use(uint32_t i)const;
private:
 ItemView data_;std::vector<ItemInstance>instances_;
};
enum class ItemMenuResult:uint8_t {None,Back,Restricted,Selected};
struct ItemMenuPose {ItemLayout source{};BattleValue rect{},color{};Vec2 scale{1,1},offset{};uint32_t frame=0;bool visible=false;};
class BattleItemsMenu {
public:
 bool initialize(InventoryState&);
 bool open(bool reset=true);
 bool input(int x,int y,bool confirm,bool cancel,bool scope,bool navigation_pulse=false);
 bool idle_frame(double delta);
 bool active()const{return active_;}
 bool visible()const{return active_||closing_;}
 uint32_t selection()const{return selection_;}
 uint32_t row_offset()const{return row_offset_;}
 bool info_visible()const{return info_visible_;}
 const InventoryState& inventory()const{return *inventory_;}
 ItemMenuPose pose(uint32_t layout)const;
 ItemMenuResult take_result(){const auto r=result_;result_=ItemMenuResult::None;return r;}
 std::vector<ItemSoundEvent> take_sounds(){std::vector<ItemSoundEvent>r;r.swap(sounds_);return r;}
 const char*error()const{return error_;}
private:
 bool fail(const char*);
 void move(int,int);void retarget_cursor(bool);
 void sample_clip(uint32_t,float,uint32_t,ItemMenuPose&)const;
 InventoryState*inventory_=nullptr;ItemView data_;
 uint32_t selection_=0,row_offset_=0;bool active_=false,closing_=false,info_visible_=true;
 float menu_time_=0,cursor_time_=0,move_time_=0,info_time_=0,repeat_remaining_=0;
 float info_y_=0,info_start_=0,info_target_=0;Vec2 cursor_{},cursor_start_{},cursor_target_{};
 ItemMenuResult result_=ItemMenuResult::None;std::vector<ItemSoundEvent>sounds_;const char*error_="";
};
}
