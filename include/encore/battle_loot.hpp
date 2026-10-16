#pragma once
#include "encore/battle_round_data.hpp"
#include "encore/source_random.hpp"
#include <string>
namespace encore::upstream {
// The live inventory/UID owner supplies transactional Item.new allocation.
// Unknown items and a full inventory fail before source pool RNG is consumed;
// the original all-full DroppedItem branch is outside this admitted slice.
class BattleLootHost {
public:
 virtual ~BattleLootHost()=default;
 virtual bool validate_sound(std::string_view source,std::string& error)const=0;
 virtual bool can_receive(std::string_view item,uint32_t doses,std::string& error)const=0;
 virtual bool reserve(std::string_view item,uint32_t doses,SourceRandom& random,uint32_t& uid,std::string& error)=0;
 virtual bool grant(std::string_view item,uint32_t doses,uint32_t uid,std::string& error)=0;
 virtual bool play_sound(std::string_view source,std::string& error)=0;
};
class BattleLoot {
public:
 bool prepare(RoundView,SourceRandom&,BattleLootHost&,std::string& error);
 bool grant(std::string& error);
 bool play_sound(std::string& error);
 bool matches(RoundView view)const;
 bool ready()const{return ready_;}
 bool selected()const{return selected_;}
 bool granted()const{return granted_;}
 uint32_t uid()const{return uid_;}
 RoundLoot policy()const{return content_.loot();}
private:
 RoundView content_;BattleLootHost* host_=nullptr;uint32_t uid_=0;
 bool ready_=false,selected_=false,granted_=false,sound_played_=false;
};
}
