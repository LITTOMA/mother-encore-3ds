#pragma once
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"
#include <string>
#include <vector>

namespace encore::upstream {
// Checked additions to fresh-scene initialization. Existing Room/House
// authored transforms, flag conditions and deferred deletions remain canonical.
struct RestoreCondition {
 uint32_t flag_index=0,flag_id=0; bool expected_value=false;
 std::string flag_name;
};
struct RestoreNpcEventPosition {
 uint32_t npc_index=0,npc_id=0,actor_index=0,actor_id=0,body_id=0;
 std::string source_path;
 RestoreCondition condition;
 Vec2 position{};
};
struct RestoreMusicArea {
 uint32_t id=0,room_resource_index=kRoomNoIndex,room_resource_id=0;
 std::string source_path,resource_path;
 bool supported=false;
 Vec2 center{},extents{};
 double volume_db=0,fadein_seconds=0,fadeout_seconds=0;
 std::vector<RestoreCondition> conditions;
};
enum class RestoreInventoryKind:uint32_t {KeyItems=1,Storage=2,Character=3};
struct RestoreInventoryItem {std::string item_id;bool equipped=false;uint32_t doses=0;};
struct RestoreInventoryLoad {
 uint32_t order_id=0;
 RestoreInventoryKind kind=RestoreInventoryKind::KeyItems;
 std::string character_id;
 bool rebuilds_inventory=false;
 // Absent native characters are projected to their frozen source initial
 // inventory. This is NOT the source's empty missing-character fallback.
 std::vector<RestoreInventoryItem> projected_items;
};
enum class RestoreUidPolicy:uint32_t {EagerFallbackBeforeSavedUid=1};
class RestoreData {
public:
 bool load(const uint8_t*,size_t,RoomView,HouseView,std::string&);
 bool load_file(const char*,RoomView,HouseView,std::string&);
 bool valid()const{return valid_;}
 bool matches(RoomView,HouseView)const;
 const std::vector<RestoreNpcEventPosition>& npc_event_positions()const{return npc_event_positions_;}
 const std::vector<RestoreMusicArea>& music_areas()const{return music_areas_;}
 const std::vector<RestoreInventoryLoad>& inventory_load_order()const{return inventory_load_order_;}
 RestoreUidPolicy uid_policy()const{return uid_policy_;}
private:
 bool valid_=false;
 uint32_t room_bytes_=0,house_bytes_=0;
 std::array<uint8_t,32> room_sha256_{},house_sha256_{};
 RestoreUidPolicy uid_policy_=RestoreUidPolicy::EagerFallbackBeforeSavedUid;
 std::vector<RestoreNpcEventPosition> npc_event_positions_;
 std::vector<RestoreMusicArea> music_areas_;
 std::vector<RestoreInventoryLoad> inventory_load_order_;
};
}
