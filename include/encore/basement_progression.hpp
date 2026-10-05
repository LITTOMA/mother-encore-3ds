#pragma once
#include "encore/session_save.hpp"
#include "encore/battle_data.hpp"
#include <functional>
#include <string_view>
namespace encore::upstream {
struct BasementKeyItem {uint32_t id=0,doses=0;bool grant=false;std::string source,name_key;};
struct BasementSkill {uint32_t id=0,sort_rank=0;std::string character,skill;};
struct BasementNpcOverride {bool thoughts=false,supported=false;std::string flag,program;};
struct BasementDoor {uint32_t key_id=0;bool remove_key=false;std::string node,flag,opened,locked;};
struct BasementPresent {uint32_t key_id=0,opened_frame=0;std::string node,flag,dialogue,empty;BattleValue geometry{},interaction{};bool can_pickup=false;uint32_t player_turn=0;bool emit_flag_updated_signal=false,is_object_flag=false,reset_when_leaving_area=false,reset_when_leaving_region=false;uint32_t closed_frame=0,stable_id=0;};
struct BasementMusicBinding {std::string scene,node,music;uint32_t track_id=0,region_id=0;float default_stop_seconds=0,volume_db=0,fadein_seconds=0,fadeout_seconds=0;BattleValue geometry{};std::string parent_disappear_flag;uint32_t source_ordinal=0;};
struct BasementFadeKey {float time=0;BattleValue color{};};
using BasementFlagQuery=std::function<bool(std::string_view,bool&,std::string&)>;
class BasementProgressionData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::string&reviewed_commit()const{return commit_;}
 bool bind_reviewed_commit(std::string_view,std::string&)const;
 const std::vector<BasementKeyItem>&key_items()const{return keys_;}
 const std::vector<BasementSkill>&skills()const{return skills_;}
 const std::vector<std::string>&skill_order()const{return order_;}
 const BasementKeyItem*key_item(uint32_t)const;const BasementSkill*skill(uint32_t)const;
 const BasementDoor&door()const{return door_;}const BasementPresent&present()const{return present_;}
 const BasementMusicBinding&music()const{return music_;}
 BattleValue fade_rect()const{return fade_rect_;}
 const std::vector<BasementMusicBinding>&music_regions()const{return music_regions_;}
 const std::vector<BasementFadeKey>&fade_keys()const{return fade_;}float fade_length()const{return fade_length_;}
 BattleValue white_fade(double elapsed)const;
 bool mick_program(bool thoughts,const BasementFlagQuery&,std::string&program,std::string&)const;
 const std::string&mick_scene()const{return mick_scene_;}
 const std::string&mick_node()const{return mick_node_;}
 const std::string&telepathy_skill()const{return telepathy_skill_;}
private:
 bool valid_=false;std::string commit_,mick_scene_,mick_node_,telepathy_skill_;
 std::vector<BasementKeyItem>keys_;std::vector<BasementSkill>skills_;std::vector<std::string>order_;
 std::vector<BasementNpcOverride>overrides_;BasementDoor door_;BasementPresent present_;BasementMusicBinding music_;std::vector<BasementMusicBinding>music_regions_;BattleValue fade_rect_{};
 std::vector<BasementFadeKey>fade_;float fade_length_=0;
};
// UID is staged by the authoritative host's shared source RNG/clock ledger.
// These helpers build detached candidates and never draw or commit a UID.
bool prepare_basement_key_item(const BasementProgressionData&,uint32_t item_id,uint32_t uid,
                              const SessionSnapshot&,SessionSnapshot&,std::string&);
bool prepare_basement_skill(const BasementProgressionData&,uint32_t skill_id,
                           const SessionCharacter&,SessionCharacter&,std::string&);
struct BasementProgressionHost {
 std::function<bool(const BasementKeyItem&,std::string&)>validate_key_item,grant_key_item;
 std::function<bool(const BasementSkill&,std::string&)>validate_skill,learn_skill;
};
// Effect endpoint for typed GrantKeyItem/LearnSkill instructions in the existing
// Room scheduler. It owns no dialogue clock, flag shortcuts or second VM.
class BasementProgressionConsumer {
public:
 bool bind(const BasementProgressionData&,BasementProgressionHost,std::string&);
 bool grant_key_item(uint32_t,std::string&);bool learn_skill(uint32_t,std::string&);
 bool bound()const{return data_!=nullptr;}const BasementProgressionData*data()const{return data_;}
private:const BasementProgressionData*data_=nullptr;BasementProgressionHost host_;
};
}
