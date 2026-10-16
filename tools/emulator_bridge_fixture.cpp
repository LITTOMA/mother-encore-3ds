// Artificial validated post-basement bridge checkpoint; no natural progression claim.
#include "encore/native_session.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"
#include "encore/battle_round_data.hpp"
#include <cstdio>
#include <fstream>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=2)return 1;std::string e;NativeSessionData session;RoomData room;HouseData house;BattleRoundData round;ItemData items;
 auto fail=[&](){std::fprintf(stderr,"%s\n",e.c_str());return 1;};
 if(!session.load_file("romfs/session/opening.encsession",e)||!room.load_file("romfs/world/house/opening.encroom",e)||!house.load_file("romfs/world/house/opening.enchouse",e)||!round.load_file("romfs/battle/lamp/opening.encround",e)||!items.load_file("romfs/ui/opening.encitems",e))return fail();
 auto fixture=session.defaults();NewGameSetupData names;if(!names.load_file("romfs/ui/opening.encnewgame",e))return fail();fixture.characters=session.startup_characters();
 for(auto&character:fixture.characters)for(const auto&field:names.fields)if(field.kind==0&&field.target==character.character_id){if(field.defaults.empty()){e="Source default name unavailable";return fail();}character.nickname=field.defaults.front();if(character.character_id=="pippi")character.nickname="Peep";}

 if(session.levels().size()<2||session.levels()[1].level!=2){e="Fixture requires admitted level 2";return fail();}
 const auto&level=session.levels()[1];auto&lead=fixture.characters.front();
 lead.level=level.level;lead.experience=level.minimum_exp;lead.learned_skills=level.skills;
 std::array<int32_t,7>stats{};if(!native_session_derived_stats(session,items.view(),lead,stats,e))return fail();
 lead.hp=stats[0];lead.pp=stats[1];
 fixture.key_items.push_back({"KeyBasement",false,1,0x7edca});
 lead.inventory.push_back({"BatPlastic",true,1,0x7edcb});
 fixture.object_flags.push_back({"Ninten's House/Present2",true});
 fixture.position_x=136;fixture.position_y=800;fixture.direction_x=0;fixture.direction_y=1;
 for(auto&f:fixture.flags)if(f.id=="talked_to_dad"||f.id=="got_dog_treats"||f.id=="gave_treats"||f.id=="mick_telepathy"||f.id=="ninten_basement_door"||f.id=="Ninten's House/Present2")f.value=true;
 if(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),fixture,e))return fail();
 std::vector<uint8_t> encoded;if(!encode_session_save(fixture,session.compatibility(),encoded,e))return fail();SessionSnapshot check;
 if(!decode_session_save(encoded.data(),encoded.size(),session.compatibility(),check,e)||!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),check,e))return fail();
 std::ofstream file(argv[1],std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(encoded.data()),std::streamsize(encoded.size()));file.flush();if(!file.good())return 1;
 std::printf("Checked artificial post-basement bridge snapshot, complete named roster and custom Pippi nickname; %zu bytes; no played progress claim\n",encoded.size());
}
