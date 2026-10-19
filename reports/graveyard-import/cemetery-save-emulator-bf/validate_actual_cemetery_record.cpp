#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <iostream>
using namespace encore::upstream;
int main(int argc,char**argv){if(argc!=3)return 2;std::string error;std::string root=argv[1];NativeSessionData d;RoomData r;HouseData h;BattleRoundData b;ItemData i;SessionSnapshot s;
auto path=[&](const char*p){return root+"/"+p;};auto fail=[&](){std::cerr<<error<<"\n";return 1;};
if(!d.load_file(path("session/opening.encsession").c_str(),error)||!r.load_file(path("world/house/opening.encroom").c_str(),error)||!h.load_file(path("world/house/opening.enchouse").c_str(),error)||!b.load_file(path("battle/doll/doll-entry.encround").c_str(),error)||!i.load_file(path("ui/opening.encitems").c_str(),error))return fail();
if(!read_session_save(argv[2],d.compatibility(),s,error)||!validate_native_session_snapshot(d,r.view(),h.view(),b.view(),i.view(),s,error))return fail();
bool roots=false,saved=false;for(const auto&f:s.flags){if(f.id=="cem_roots_grown")roots=f.value;if(f.id==d.saved_flag_id())saved=f.value;}
std::vector<uint8_t>raw,again;if(!encore::read_file(argv[2],raw,1024*1024,error)||!encode_session_save(s,d.compatibility(),again,error))return fail();std::cerr<<"Observed Record roots="<<roots<<" saved="<<saved<<" raw="<<raw.size()<<" encoded="<<again.size()<<" canonical="<<(raw==again)<<"\n";if(!roots||saved||raw!=again)return 3;
auto leader=std::find_if(s.characters.begin(),s.characters.end(),[&](const SessionCharacter&c){return c.character_id==d.leader_id();});if(leader==s.characters.end()||s.party.size()!=1||s.characters.size()!=5||leader->level!=2||leader->experience!=17||leader->hp!=12||leader->pp!=27||s.key_items.size()!=3)return 4;
std::cout<<"Actual bf normal-input telephone Record validated against its exact embedded resources; "<<raw.size()<<" canonical bytes; cemetery flag true; first persisted saved flag false in source write-before-live-flag order;5 source characters, original UID/domain policies; no hardware claim\n";
}
