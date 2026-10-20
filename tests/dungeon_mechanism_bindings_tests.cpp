#include "encore/dungeon_mechanism_bindings.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Mechanism binding %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(v) check((v),#v)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);std::vector<uint8_t>b;CHECK(encore::read_file(argv[1],b,256*1024,error));CHECK(b.size()==4060);
 DungeonMechanismBindingsData data;CHECK(!data.valid()&&!data.graph().valid()&&data.bindings().empty()&&!data.profile(1));
 CHECK(!data.load_file(nullptr,error)&&!data.load(nullptr,b.size(),error)&&!data.valid());CHECK(data.load_file(argv[1],error));
 CHECK(data.valid()&&data.graph().scene_id()==4&&data.bindings().size()==14);CHECK(data.graph().switch_count()==5&&data.graph().object_count()==9);
 std::set<uint32_t>owners,profiles;
 for(const auto&row:data.bindings()){
  CHECK(row.owner&&owners.insert(row.owner).second&&row.roles[0]==0);const auto*p=data.profile(row.profile);CHECK(p);profiles.insert(row.profile);
  if(p->family==DungeonMechanismFamily::Wall){CHECK(row.owner==3761819854u&&p->wall.valid()&&!p->properties.valid());CHECK(row.roles[3]==0&&row.roles[4]==p->wall.roles()[0]&&row.roles[5]==p->wall.roles()[2]&&row.roles[6]==p->wall.roles()[3]);}
  else CHECK(p->properties.valid()&&!p->wall.valid());
 }
 CHECK(profiles.size()==4&&!data.profile(0)&&!data.profile(65535));
 const auto identity=data.graph().bytes_identity();auto shared=data;data=DungeonMechanismBindingsData{};CHECK(!data.valid()&&shared.valid()&&shared.graph().bytes_identity()==identity&&shared.bindings().size()==14);
 CHECK(!shared.load(b.data(),b.size(),error)&&shared.graph().bytes_identity()==identity);
 CHECK(!shared.load_file(argv[1],error)&&shared.graph().bytes_identity()==identity);
 auto reject=[&](const std::vector<uint8_t>&v){DungeonMechanismBindingsData fresh;CHECK(!fresh.load(v.data(),v.size(),error));CHECK(!fresh.valid()&&!fresh.graph().valid()&&fresh.bindings().empty()&&!fresh.profile(1));CHECK(fresh.load(b.data(),b.size(),error));};
 for(size_t n=0;n<b.size();++n){DungeonMechanismBindingsData fresh;CHECK(!fresh.load(b.data(),n,error));CHECK(!fresh.valid());}
 const size_t bindings=get(b,68),table=get(b,84),payload=get(b,100),graph=get(b,116);const uint32_t kind=get(b,bindings+4);
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,0},{8,2},{12,uint32_t(b.size()+1)},{20,0},{20,2},{24,0},{24,65536},{28,1},{32,0},{52,5},{56,1},{60,1},{64,2},{68,0},{72,0},{76,1},{80,1},{84,0},{88,0},{92,40},{96,1},{100,0},{104,0},{112,1},{116,0},{120,0},{bindings,0},{bindings,1},{bindings+4,3-kind},{bindings+8,0},{bindings+8,65535},{bindings+12,7},{bindings+28,65535},{table,0},{table+4,0},{table+4,5},{table+8,1},{table+12,0},{table+12,UINT32_MAX},{payload+8,2},{payload+20,2},{graph+8,2},{graph+20,2},{graph+24,5}}){auto v=b;put(v,entry.first,entry.second);sign(v);reject(v);}
 const size_t policy=payload+get(b,payload+68),keys=payload+get(b,payload+100),size=get(b,table+12);
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{policy+8,4},{keys+4,5}}){auto v=b;put(v,entry.first,entry.second);put(v,payload+16,encore::crc32(v.data()+payload+32,size-32));sign(v);reject(v);}
 {auto v=b;std::copy(v.begin()+bindings,v.begin()+bindings+40,v.begin()+bindings+40);sign(v);reject(v);}
 {auto v=b;v.push_back(0);reject(v);}
 {auto v=b;v[180]^=1;reject(v);}
 const size_t wall=payload+get(b,table+3*16+8),wall_size=get(b,table+3*16+12),wall_strings=wall+get(b,wall+68);
 {auto v=b;v[wall_strings]='X';put(v,wall+16,encore::crc32(v.data()+wall+32,wall_size-32));sign(v);reject(v);}
 for(size_t i=0;i<14;++i)if(get(b,bindings+i*40)==3761819854u){auto v=b;put(v,bindings+i*40+32,get(b,bindings+i*40+36));put(v,bindings+i*40+36,get(b,bindings+i*40+32));sign(v);reject(v);}
 std::printf("Mechanism bindings: %u checks; fresh-owner loading, source graph/profile/role identity, all4060 truncations and shared lifetime; no scene/media/save\n",checks);
}
