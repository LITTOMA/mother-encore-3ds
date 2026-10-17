// Artificial source-validated menu checkpoint, never a played-progress claim.
#include "encore/native_session.hpp"
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"
#include "encore/battle_round_data.hpp"
#include <cstdio>
#include <fstream>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=2)return 1;std::string error;NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;ItemData items;
 auto fail=[&](){std::fprintf(stderr,"%s\n",error.c_str());return 1;};
 if(!data.load_file("romfs/session/opening.encsession",error)||!room.load_file("romfs/world/house/opening.encroom",error)||!house.load_file("romfs/world/house/opening.enchouse",error)||!round.load_file("romfs/battle/lamp/opening.encround",error)||!items.load_file("romfs/ui/opening.encitems",error))return fail();
 auto state=data.defaults();auto&hero=state.characters[0];const auto&level=data.levels().back();hero.level=level.level;hero.experience=level.minimum_exp;hero.learned_skills=level.skills;
 auto set=[&](const std::string&id,uint32_t policy){if(policy==2){for(auto&f:state.object_flags)if(f.id==id){f.value=true;return true;}state.object_flags.push_back({id,true});return true;}for(auto&f:state.flags)if(f.id==id){f.value=true;return true;}error="Fixture flag not admitted";return false;};
 uint32_t uid=100;
 for(const auto&a:data.acquisitions()){if(!set(a.flag_id,a.flag_policy))return fail();hero.inventory.push_back({a.item_id,false,a.doses,uid++});}
 for(const auto&k:data.key_acquisitions())if(!set(k.flag_id,k.flag_policy))return fail();
 for(const auto&s:data.skill_acquisitions()){if(!set(s.flag_id,1)||!native_session_learn_skill(data,s.skill_id,hero.learned_skills,error))return fail();}
 if(!native_session_expected_key_items(data,state,state.key_items,error))return fail();for(auto&k:state.key_items)k.uid=uid++;
 if(!set(data.saved_flag_id(),1))return fail();
 // Same reviewed house-exit precondition as emulator_mick_fixture.cpp.
 if(!set("talked_to_dad",1))return fail();
 std::array<int32_t,7>stats{};if(!native_session_derived_stats(data,items.view(),hero,stats,error))return fail();hero.hp=stats[0]-5;hero.pp=stats[1];
 state.position_x=88;state.position_y=977;state.direction_x=0;state.direction_y=1;
 if(!validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),state,error))return fail();
 std::vector<uint8_t>bytes;if(!encode_session_save(state,data.compatibility(),bytes,error))return fail();SessionSnapshot check;
 if(!decode_session_save(bytes.data(),bytes.size(),data.compatibility(),check,error)||!validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),check,error))return fail();
 std::ofstream file(argv[1],std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));file.flush();if(!file.good())return 1;
 std::printf("Artificial admitted menu save: %zu bytes; level %lld; %zu ordinary / %zu key items; not a played route\n",bytes.size(),static_cast<long long>(hero.level),hero.inventory.size(),state.key_items.size());
}
