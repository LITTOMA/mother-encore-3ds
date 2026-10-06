#pragma once
#include "encore/movement.hpp"
#include <array>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
struct FieldPayphoneDispatch {std::string flag,program;};
struct FieldPayphoneDescriptor {
 uint32_t id=0,parent_id=0,ready_ordinal=0,sprite_id=0,prompt_id=0,audio_id=0,sprite_flags=0;Vec2 sprite_offset{};
 std::string node,dialogue,item,location,appear,disappear;
 std::vector<FieldPayphoneDispatch>dispatch;
};
struct FieldPayphoneSound {uint32_t id=0;std::string source,pcm,bus;float volume=0;};
struct FieldPayphoneTexture {uint32_t width=0,height=0,columns=0,rows=0;std::string source,path;std::array<uint8_t,32>sha{};};
class FieldPayphoneData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 uint32_t scene_id()const{return scene_id_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::array<uint8_t,32>&scene_hash()const{return scene_;}const std::array<uint8_t,32>&script_hash()const{return script_;}
 const std::vector<FieldPayphoneDescriptor>&records()const{return records_;}const FieldPayphoneDescriptor*record(uint32_t)const;
 const FieldPayphoneSound&sound()const{return sound_;}uint32_t cash_cost()const{return cash_cost_;}uint32_t card_max_doses()const{return card_max_doses_;}uint32_t card_step()const{return card_step_;}uint32_t idle_frame()const{return idle_frame_;}
 const FieldPayphoneTexture&texture()const{return texture_;}
 float update_seconds()const{return update_seconds_;}float close_seconds()const{return close_seconds_;}const std::string&no_money_program()const{return no_money_;}const std::string&card_name()const{return card_name_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;uint32_t scene_id_=0,cash_cost_=0,card_max_doses_=0,card_step_=0,idle_frame_=0;float update_seconds_=0,close_seconds_=0;
 std::array<uint8_t,20>pin_{};std::array<uint8_t,32>scene_{},script_{};std::string no_money_,card_name_;FieldPayphoneSound sound_;FieldPayphoneTexture texture_;std::vector<FieldPayphoneDescriptor>records_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
// A nonempty checked item name denotes presence. Original Item.get_uid()
// permits zero; UID zero alone must never mean a missing PhoneCard.
struct FieldPayphoneCard {uint64_t uid=0;uint32_t doses=0;std::string name;};
struct FieldPayphoneHost {
 // All callbacks bind checked source node/program/resource identities. This
 // core does not approve unknown Session items or create a phone card.
 std::function<bool(const FieldPayphoneData&,std::string&)>bind;
 std::function<bool(std::string_view,bool&present,bool&value,std::string&)>read_flag;
 std::function<bool(uint32_t,std::function<bool()>,std::string&)>connect_flags;
 std::function<bool(uint32_t,bool,std::string&)>visibility;
 std::function<bool(uint32_t,std::string&)>queue_free;
 // Search party inventories in source order, then key inventory, excluding
 // storage. Returns the actual saved UID, never an item-name stand-in.
 std::function<bool(std::string_view,FieldPayphoneCard&,std::string&)>find_card;
 std::function<bool(uint64_t,FieldPayphoneCard&,std::string&)>lookup_card;
 std::function<bool(uint64_t,uint32_t,std::string&)>reduce_or_drop;
 std::function<bool(int64_t&,std::string&)>read_cash;
 std::function<bool(int64_t,std::string&)>add_cash;
 std::function<bool(uint32_t,const FieldPayphoneSound&,std::string&)>set_stream;
 std::function<bool(uint32_t,uint32_t,std::string&)>play_idle;
 std::function<bool(std::string_view,std::string&)>set_location;
 // box_open performs actual CashBox.open() including initial update; the
 // returned object identity is retained by each source coroutine independently.
 std::function<bool(bool phone_units,uint64_t&,std::string&)>box_open;
 std::function<bool(uint64_t,std::string&)>box_update,box_close;
 std::function<bool(uint32_t,std::string&)>audio_play;
 std::function<bool(uint32_t,std::string_view,std::string&)>open_dialogue;
};
struct FieldPayphoneInstance {uint32_t id=0;bool ready=false,visible=true,queued=false,deleted=false;};
class FieldPayphoneRuntime {
public:
 bool initialize(const FieldPayphoneData&,FieldPayphoneHost,std::string&);bool create(uint32_t);bool ready(uint32_t);bool flags_updated(uint32_t);bool interact(uint32_t);bool interact_item(uint32_t,uint64_t);bool idle_frame(float);bool commit_deleted(uint32_t);
 const FieldPayphoneInstance*instance(uint32_t)const;const std::string&error()const{return error_;}
private:
 struct Waiter{uint64_t box=0;float left=0;bool update=true;};
 const FieldPayphoneData*data_=nullptr;FieldPayphoneHost host_;std::map<uint32_t,FieldPayphoneInstance>instances_;std::vector<Waiter>waiters_;std::string error_;bool poisoned_=false,had_ready_=false;uint32_t last_ready_=0;
 bool fail(const char*);bool effect(bool);FieldPayphoneInstance*get(uint32_t,bool ready=true);bool use(uint32_t,const FieldPayphoneCard&);bool dialogue(const FieldPayphoneDescriptor&,std::string&);
};
}
