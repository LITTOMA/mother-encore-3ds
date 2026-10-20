#include "encore/dungeon_keys.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Dungeon keys %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);DungeonKeysData data;CHECK(!data.view().valid());std::vector<uint8_t>bytes;
 CHECK(encore::read_file(argv[1],bytes,65536,error));CHECK(data.load(bytes.data(),bytes.size(),error));
 const auto view=data.view();const auto policy=view.policy();CHECK(view.scene_id()==4&&view.node_count()==6);
 CHECK(view.text(policy.region)=="Catacombs"&&view.text(policy.locked_dialogue)=="Reusable/locklocked");CHECK(policy.initial==0&&policy.minimum==0&&policy.maximum==3);
 const char*flags[]={"Catacombs/Locked Door3","Catacombs/Locked Door1","Catacombs/Locked Door2","Catacombs/Key1","Catacombs/Key2","Catacombs/Key3"};
 for(uint32_t i=0;i<6;++i){auto node=view.node(i);CHECK(node.id&&node.flags==3&&view.text(node.flag)==flags[i]);CHECK(node.kind==(i<3?DungeonKeyKind::Lock:DungeonKeyKind::Pickup));CHECK(node.delta==(i<3?-1:1));CHECK(!view.text(node.path).empty());}
 CHECK(view.node(6).id==0&&view.node(UINT32_MAX).id==0);CHECK(view.text({UINT32_MAX,1}).empty());
 const auto identity=view.bytes_identity();const auto policy_at=get(bytes,84),node_at=get(bytes,100),strings_at=get(bytes,68);
 auto reject=[&](const std::vector<uint8_t>&bad){CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().bytes_identity()==identity);CHECK(data.view().text(data.view().node(0).flag)==flags[0]);};
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,2},{12,uint32_t(bytes.size()-1)},{20,0},{20,2},{24,0},{24,65536},{28,1},{32,0},{52,2},{52,4},{56,1},{60,1},{64,0},{68,111},{72,UINT32_MAX},{76,2},{84,112},{88,2},{92,31},{100,0},{104,0},{104,129},{108,31},{policy_at+20,1},{policy_at+16,4},{policy_at+12,1},{node_at,0},{node_at+4,0},{node_at+4,3},{node_at+8,UINT32_MAX},{node_at+12,0},{node_at+16,UINT32_MAX},{node_at+24,0},{node_at+24,1},{node_at+28,2},{node_at+28,4},{node_at+32,get(bytes,node_at)},{node_at+3*32+24,UINT32_MAX}}){auto bad=bytes;put(bad,edit.first,edit.second);sign(bad);reject(bad);}
 for(unsigned char value:{static_cast<unsigned char>(0),static_cast<unsigned char>(':'),static_cast<unsigned char>(255)}){auto bad=bytes;bad[strings_at]=value;sign(bad);reject(bad);}
 {auto bad=bytes;bad.back()^=1;reject(bad);}
 {auto bad=bytes;bad[0]^=1;sign(bad);reject(bad);}
 {auto bad=bytes;bad.resize(65537);reject(bad);}
 {std::vector<uint8_t>padded;CHECK(encore::read_file(argv[2],padded,65536,error));DungeonKeysData boundary;CHECK(boundary.load(padded.data(),padded.size(),error));CHECK(boundary.view().text(boundary.view().policy().region)=="CatacombsX");const auto begin=get(padded,68)+get(padded,72),end=get(padded,84);CHECK(begin<end);for(size_t at=begin;at<end;++at){auto bad=padded;bad[at]=1;sign(bad);reject(bad);}}
 {auto bad=bytes;std::swap_ranges(bad.begin()+node_at,bad.begin()+node_at+32,bad.begin()+node_at+32);sign(bad);reject(bad);}
 for(size_t size:{size_t(0),size_t(63),size_t(111),bytes.size()-1}){std::vector<uint8_t>bad(bytes.begin(),bytes.begin()+size);reject(bad);}
 CHECK(!data.load(nullptr,bytes.size(),error));CHECK(data.view().bytes_identity()==identity);
 DungeonKeysData second;CHECK(second.load_file(argv[1],error));CHECK(second.view().text(second.view().node(5).flag)==flags[5]);
 std::printf("Dungeon keys: %u checks; candidate source metadata, strict schema/pin/identity/counter/text and failed-load preservation; no gameplay/animation/audio/save admission\n",checks);
}
