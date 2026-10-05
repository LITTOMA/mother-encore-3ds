#pragma once
#include "encore/items_menu.hpp"
#include "encore/session_save.hpp"
#include <functional>

namespace encore::upstream {
enum class FieldItemUseSound:uint8_t {Open=1,Move,Confirm,Back,Restricted,Heal,Close};
// Bounded field consumption schema. Content IDs, doses, ailments, messages and
// layout are loaded; this does not enable unreviewed battle item actions.
enum class ItemUseLayoutRole:uint32_t {Inventory=1,Action,Targets,Message,Description,TargetTitle,Divider};
enum class ItemUseParameter:uint32_t {Grid=1,GridOrigin,TargetOrigin,TextInset,LabelSize,CursorOffsets,ActionOrigin,SourceViewport,PlatformViewport,SubmenuPlacement,SubmenuPoint,CursorCenter,MessageAlignment,MessagePatch};
struct ItemUseRule {
 uint32_t definition=0,max_doses=0;
 std::string source,status,heal_message,fail_message,success_sound;
 bool reusable=false;
};
struct ItemUseStatusPolicy {
 std::string id;bool persistent=false,passive_healing=false;
 uint32_t default_saved_turns=0;
 uint32_t refresh_hp_value=0;
};
struct ItemUseLocale {std::string code,action,title,heal,fail;};
struct ItemUseLayout {ItemUseLayoutRole role=ItemUseLayoutRole::Inventory;BattleValue rect{},color{};};
class ItemUseData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 const std::vector<ItemUseRule>&rules()const{return rules_;}
 const std::vector<ItemUseStatusPolicy>&status_policies()const{return statuses_;}
 const std::vector<std::string>&targets()const{return targets_;}
 const std::vector<ItemUseLocale>&locales()const{return locales_;}
 const std::string&reviewed_commit()const{return commit_;}
 bool bind_items(ItemView,std::string&)const;
 const ItemUseRule*rule(uint32_t definition)const;
 const ItemUseLocale*locale(std::string_view code)const;
 BattleValue layout(ItemUseLayoutRole)const;
 BattleValue layout_color(ItemUseLayoutRole)const;
 BattleValue parameter(ItemUseParameter)const;
 std::string_view sound(FieldItemUseSound)const;
private:
 bool valid_=false;std::string commit_;
 std::vector<ItemUseRule>rules_;std::vector<ItemUseStatusPolicy>statuses_;
 std::vector<std::string>targets_;std::vector<ItemUseLocale>locales_;
 std::vector<ItemUseLayout>layouts_;std::vector<BattleValue>parameters_;
 std::vector<std::string>sounds_;
};
using ItemUseView=const ItemUseData*;
struct FieldItemUseSnapshot {
 std::string owner;InventoryState inventory;
 // Actual active party in natural order, never inactive naming records.
 std::vector<SessionCharacter> party;
 // Source MAXHP after current level, equipment and permanent boosts; host must
 // derive these from the same admitted live session used by save validation.
 std::vector<int64_t> maximum_hp;
};
struct ItemUseResult {
 bool healed=false,removed=false;uint32_t doses_remaining=0;
 std::string target,nickname,message_key,sound_source;
};
struct FieldItemUseCandidate {
 InventoryState inventory;std::vector<SessionCharacter> party;
 ItemUseResult result;
};
// Detached atomic preparation. A failure preserves both live state and output.
// No UID creation, random draw, permanent boost or transform is performed.
bool prepare_item_use(ItemUseView,const FieldItemUseSnapshot&,uint32_t uid,
                     std::string_view target,FieldItemUseCandidate&,std::string&);
bool validate_item_use_snapshot(ItemUseView,const FieldItemUseSnapshot&,std::string&);
struct FieldItemUseHost {
 std::function<bool(FieldItemUseSnapshot&,std::string&)> read;
 // Host re-reads authoritative state and uses prepare_item_use before committing
// inventory, statuses and source HP refresh together. Failed requests mutate nothing.
 std::function<bool(uint32_t,std::string_view,ItemUseResult&,std::string&)> commit;
};
enum class FieldItemUsePhase:uint8_t {Closed,Items,Action,Targets,Message};
class FieldItemUseMenu {
public:
 bool initialize(ItemUseView,FieldItemUseHost,std::string_view locale="en");
 bool open();void close();
 bool input(int x,int y,bool confirm,bool cancel,bool scope=false);
 bool idle_frame(double);
 bool active()const{return phase_!=FieldItemUsePhase::Closed;}
 bool visible()const{return active();}
 bool description_visible()const{return info_visible_&&phase_!=FieldItemUsePhase::Message;}
 FieldItemUsePhase phase()const{return phase_;}
 uint32_t selection()const{return selection_;}
 uint32_t target_selection()const{return target_selection_;}
 double cursor_time()const{return cursor_time_;}
 const ItemInstance*selected_item()const;
 const FieldItemUseSnapshot&snapshot()const{return snapshot_;}
 const std::string&message()const{return message_;}
 const ItemUseResult&last_result()const{return last_result_;}
 ItemUseView content()const{return data_;}
 const ItemUseLocale&locale()const{return *data_->locale(locale_);}
 bool set_locale(std::string_view);
 std::vector<FieldItemUseSound>take_sounds(){std::vector<FieldItemUseSound>s;s.swap(sounds_);return s;}
 const char*error()const{return error_.c_str();}
private:
 bool fail(const char*);bool read_snapshot();
 ItemUseView data_=nullptr;FieldItemUseHost host_;FieldItemUseSnapshot snapshot_;
 FieldItemUsePhase phase_=FieldItemUsePhase::Closed;
 uint32_t selection_=0,target_selection_=0,uid_=0;
 bool info_visible_=true;std::string locale_,message_,error_;ItemUseResult last_result_;
 double cursor_time_=0;
 std::vector<FieldItemUseSound>sounds_;
};
}
