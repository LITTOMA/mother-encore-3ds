#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <set>
using namespace encore::upstream;
int main(){std::string error;NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;ItemData items;SessionSnapshot snapshot;
 auto fail=[&](){std::cerr<<error<<"\n";return 1;};
 if(!data.load_file("romfs/session/opening.encsession",error)||!room.load_file("romfs/world/house/opening.encroom",error)||!house.load_file("romfs/world/house/opening.enchouse",error)||!round.load_file("romfs/battle/doll/doll-entry.encround",error)||!items.load_file("romfs/ui/opening.encitems",error))return fail();
 if(!read_session_save("build/podunk-routes/mayor-with-map.encsave",data.compatibility(),snapshot,error)||!validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),snapshot,error))return fail();
 std::ifstream input("build/full-port/post-mayor-fixture-values.txt");std::string marker;std::getline(input,marker);if(marker!="post-observed-mayor-v1"||snapshot.party!=std::vector<std::string>{data.leader_id()}){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}
 auto leader=std::find_if(snapshot.characters.begin(),snapshot.characters.end(),[&](const SessionCharacter&c){return c.character_id==data.leader_id();});if(leader==snapshot.characters.end())return 2;auto&c=*leader;if(!(input>>c.level>>c.experience>>c.hp>>c.pp>>snapshot.cash>>snapshot.bank>>snapshot.earned_cash)){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}
 unsigned count=0;if(!(input>>count)||count!=snapshot.flags.size()){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}std::set<std::string>seen;
 for(unsigned i=0;i<count;++i){std::string name;unsigned value;if(!(input>>name>>value)||value>1||!seen.insert(name).second){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}bool found=false;for(auto&flag:snapshot.flags)if(flag.id==name){flag.value=value;found=true;}if(!found){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}}
 std::string encounter,extra;if(!(input>>encounter)||(input>>extra)){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}snapshot.encountered={{encounter,true}};
 if(!validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),snapshot,error))return fail();
 std::vector<uint8_t>bytes;if(!encode_session_save(snapshot,data.compatibility(),bytes,error))return fail();SessionSnapshot decoded;
 if(!decode_session_save(bytes.data(),bytes.size(),data.compatibility(),decoded,error)||!validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),decoded,error))return fail();
 std::ofstream output("build/full-port/post-mayor-checkpoint.encsave",std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!output){std::cerr<<"Fixture configuration rejected at line "<<__LINE__<<"; flags="<<snapshot.flags.size()<<" characters="<<snapshot.characters.size()<<"\n";return 2;}
 std::cout<<"Artificial post-Major checkpoint validated and encoded: "<<bytes.size()<<" bytes; observed HP/PP/EXP/funds/flags, original inventory identities, no RNG serialization\n";
}