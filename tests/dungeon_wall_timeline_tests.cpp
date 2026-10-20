#include "encore/dungeon_wall_timeline.hpp"
#include "encore/dungeon_switches.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Wall metadata %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
void number(std::vector<uint8_t>&b,size_t at,double value){uint64_t bits;std::memcpy(&bits,&value,8);put(b,at,uint32_t(bits));put(b,at+4,uint32_t(bits>>32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1];std::vector<uint8_t>b;CHECK(encore::read_file((root+"/source.encwall").c_str(),b,65536,error));DungeonWallTimelineData data;
 CHECK(data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.scene_id()==4&&data.owner_id()==3761819854u&&data.owner_path()=="Bottom2/Wall Door");
 CHECK(data.roles()==(std::array<uint32_t,4>{4,7,5,6}));CHECK(data.properties().frame_count()==15&&data.properties().clips().size()==4&&data.cues().size()==4);
 CHECK(data.audio().id==90211837u&&data.audio().bus==1&&data.audio().gain_db==0&&data.audio().pitch==1&&!data.audio().loop);
 CHECK(data.camera().magnitude==5&&data.camera().interval==.02&&data.camera().weight==1&&!data.camera().diminish&&data.camera().direction.x==0&&data.camera().direction.y==0);
 CHECK(data.clip(4)&&data.clip(4)->count==3&&data.clip(5)&&data.clip(5)->count==0&&!data.clip(0));
 DungeonSwitchesData graph;AudioBank metadata_only,actual_root;
 CHECK(graph.load_file((root+"/graph.encdsw").c_str(),error));CHECK(metadata_only.load_file((root+"/metadata-only.encaudio").c_str(),error));
 // This positive check verifies only metadata identity comparison. The fixture
 // has no waveform and is never treated as original audio preparation.
 CHECK(data.matches(graph.view(),metadata_only,error));CHECK(actual_root.load_file(argv[2],error));CHECK(!data.matches(graph.view(),actual_root,error));
 CHECK(!data.matches({},metadata_only,error)&&!data.matches(graph.view(),AudioBank{},error));
 CHECK(!data.load(nullptr,b.size(),error)&&data.valid());CHECK(!data.load_file(nullptr,error)&&data.valid());
 for(size_t size=0;size<b.size();++size){CHECK(!data.load(b.data(),size,error));CHECK(data.valid()&&data.owner_id()==3761819854u&&data.cues().size()==4);}
 const size_t policy=get(b,84),prop=get(b,100),rows=get(b,116),cues=get(b,132),nested_policy=prop+get(b,prop+68),nested_keys=prop+get(b,prop+100);
 auto reject=[&](std::vector<uint8_t>v){CHECK(!data.load(v.data(),v.size(),error));CHECK(data.valid()&&data.owner_id()==3761819854u&&data.cues().size()==4);};
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,2},{12,uint32_t(b.size()+1)},{20,0},{20,2},{24,0},{24,65536},{28,1},{32,0},{52,4},{56,1},{60,1},{64,2},{68,0},{72,UINT32_MAX},{76,2},{84,0},{88,2},{92,64},{112,0},{policy,0},{policy+4,0},{policy+8,0},{policy+12,4},{policy+24,1},{policy+28,0},{policy+32,0},{policy+36,0},{policy+76,2},{policy+112,2},{policy+116,0},{policy+120,0},{policy+124,1},{rows,99},{rows+4,1},{rows+8,999},{rows+12,1},{cues+4,3}}){auto v=b;put(v,entry.first,entry.second);sign(v);reject(v);}
 for(auto entry:std::vector<std::pair<size_t,double>>{{policy+80,NAN},{policy+96,0},{policy+104,2},{cues+8,.5},{cues+24,.001}}){auto v=b;number(v,entry.first,entry.second);sign(v);reject(v);}
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{nested_policy+8,4},{nested_keys+4,2}}){auto v=b;put(v,entry.first,entry.second);put(v,prop+16,encore::crc32(v.data()+prop+32,get(v,104)-32));sign(v);reject(v);}
 {auto v=b;std::fill(v.begin()+policy+40,v.begin()+policy+72,0);sign(v);reject(v);}
 {auto v=b;v.push_back(0);reject(v);}
 {auto v=b;v[policy]^=1;reject(v);}
 CHECK(data.load(b.data(),b.size(),error)&&data.matches(graph.view(),metadata_only,error));
 std::printf("Wall timeline metadata: %u checks;all truncations,versions,caps,source identities,nested scope and atomic rejection; metadata audio fixture only,no PCM or scene preparation\n",checks);
}
