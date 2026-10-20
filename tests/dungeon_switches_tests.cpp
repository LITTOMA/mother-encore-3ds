#include "encore/dungeon_switches.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Dungeon switches %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
}
int main(int argc,char**argv){
 CHECK(argc==2);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,65536,error));DungeonSwitchesData data;CHECK(data.load(bytes.data(),bytes.size(),error));auto v=data.view();CHECK(v.valid()&&v.scene_id()==4&&v.switch_count()==5&&v.object_count()==9&&v.edge_count()==9);auto policy=v.policy();CHECK(v.text(policy.region)=="Catacombs"&&policy.ready_delay>0&&policy.post_delay==0.5f&&policy.music_fade==0);
 CHECK(v.text(policy.denied_audio)=="Audio/Sound effects/M3/bump.wav"&&v.text(policy.denied_channel)=="switch");
 std::set<uint32_t>ids;unsigned one_way=0,reset=0,initial=0,focus=0;
 for(uint32_t i=0;i<v.switch_count();++i){auto r=v.switch_node(i);CHECK(ids.insert(r.id).second);CHECK(!v.text(r.path).empty()&&!v.text(r.flag).empty()&&!v.text(r.audio).empty());one_way+=bool(r.bits&16);reset+=bool(r.bits&4);focus+=bool(r.bits&64);CHECK(r.camera_delay==1);for(uint32_t j=0;j<r.edge_count;++j)CHECK(v.edge(r.first_edge+j).source==r.id);}
 for(uint32_t i=0;i<v.object_count();++i){auto r=v.object(i);CHECK(ids.insert(r.id).second);initial+=bool(r.bits&1);CHECK(r.order==0);if(r.id==policy.wall)CHECK(r.kind==DungeonControlledKind::Wall&&r.offset_y==44);}
 CHECK(one_way==3&&reset==1&&initial==2&&focus==2);CHECK(v.switch_node(999).id==0&&v.object(999).id==0&&v.edge(999).source==0&&v.text({UINT32_MAX,1}).empty());
 auto identity=v.bytes_identity();
 auto rejected=[&](std::vector<uint8_t>b,bool crc=true){if(crc&&b.size()>=32)put(b,16,encore::crc32(b.data()+32,b.size()-32));CHECK(!data.load(b.data(),b.size(),error));CHECK(data.view().bytes_identity()==identity&&data.view().switch_count()==5);};
 auto bad=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);rejected(std::move(b));};
 CHECK(!data.load(nullptr,0,error));CHECK(!data.load_file("build/nonexistent-dungeon-switch-input",error));CHECK(data.view().bytes_identity()==identity);
 for(size_t n:{size_t(0),size_t(64),size_t(143),bytes.size()-1}){auto b=bytes;b.resize(n);rejected(std::move(b),false);}
 {auto b=bytes;b.resize(65537);rejected(std::move(b),false);}
 for(auto p:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,2},{12,uint32_t(bytes.size()+1)},{20,2},{24,0},{24,65536},{28,1},{32,0},{52,4},{56,1},{60,1}})bad(p.first,p.second);
 {auto b=bytes;b.back()^=1;rejected(std::move(b),false);}
 for(unsigned section=0;section<5;++section){auto at=64+section*16;bad(at,99);bad(at+4,UINT32_MAX);bad(at+8,0);bad(at+8,UINT32_MAX);bad(at+12,99);}
 const auto strings=u(bytes.data()+68),p=u(bytes.data()+84),s=u(bytes.data()+100),o=u(bytes.data()+116),e=u(bytes.data()+132);
 bad(p+36,0);bad(p+44,0);
 for(auto item:std::vector<std::pair<size_t,uint32_t>>{{p+8,0},{p+8,0x7fc00000},{p+12,0},{p+16,v.switch_node(0).id},{p+20,0},{p+24,0x7f800000},{p+28,1},{s,0},{s+64,v.switch_node(0).id},{s+8,0},{s+20,128},{s+24,0x7fc00000},{s+28,0x7f800000},{s+36,1},{s+40,0},{s+52,3},{s+56,1},{s+60,1},{o,v.switch_node(0).id},{o+12,0},{o+16,32768},{o+20,4},{o+24,0x7fc00000},{o+40,1},{o+44,1},{e,0},{e+4,v.switch_node(0).id},{e+4,0}})bad(item.first,item.second);
 {auto b=bytes;put(b,e+8,v.edge(0).source);put(b,e+12,v.edge(0).target);rejected(std::move(b));}
 for(uint8_t c:{uint8_t(0),uint8_t(255),uint8_t(':')}){auto b=bytes;b[strings]=c;rejected(std::move(b));}
 // Find actual canonical alignment, then reject nonzero padding. No fabricated
 // pointer or out-of-range write is used to exercise this negative path.
 bool padding=false;size_t cursor=144;
 for(unsigned i=0;i<5;++i){auto d=bytes.data()+64+i*16;auto at=u(d+4);if(at>cursor){auto b=bytes;b[cursor]=1;rejected(std::move(b));padding=true;}cursor=at+size_t(u(d+8))*u(d+12);}
 CHECK(padding);
 std::printf("Dungeon switch metadata: %u checks; actual14 nodes/9 edges, strict format/capability/source/identity/canonical data and atomic rejection; no scene execution\n",checks);
}
