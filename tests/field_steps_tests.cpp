#include "encore/field_steps.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Field steps %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/mayor.encsteps").c_str(),bytes,65536,error));FieldStepsData data;CHECK(data.load(bytes.data(),bytes.size(),error));const auto v=data.view();CHECK(v.scene_id()==3&&v.sound_count()==2&&v.area_count()==2);CHECK(v.text(v.policy().slot)=="run"&&v.text(v.policy().shadow)=="shadow"&&v.policy().default_sound==0);
 CHECK(v.text(v.sound(0).name)=="wood"&&v.text(v.sound(1).name)=="stone");CHECK(v.sound(0).id==1701&&v.sound(1).id==1702);CHECK(v.find_sound("wood")==0&&v.find_sound("unknown")==kFieldNone);CHECK(v.text(v.area(0).path)=="Stepping Sounds/Wood"&&v.area(0).exit==kFieldNone&&v.area(1).exit==0&&v.area(1).enter==1);
 CHECK(v.area(0).center.x==8&&v.area(0).center.y==12&&v.area(0).extents.x==32&&v.area(0).extents.y==28);CHECK(v.area(1).center.x==8&&v.area(1).center.y==-581&&v.area(1).extents.x==41&&v.area(1).extents.y==25);CHECK(v.area(0).layer==2048&&v.area(0).mask==1&&v.area(0).enabled);
 AudioBank bank,wrong;CHECK(bank.load_file((root+"/mayor-steps.encaudio").c_str(),error));CHECK(v.matches(bank,error));CHECK(wrong.load_file((root+"/mayor.encaudio").c_str(),error));CHECK(!v.matches(wrong,error));CHECK(!FieldStepsView{}.matches(bank,error));
 const auto policy=get(bytes,84),sounds=get(bytes,100),areas=get(bytes,116);auto reject=[&](std::vector<uint8_t>bad){sign(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().bytes_identity()==v.bytes_identity());CHECK(data.view().matches(bank,error));};
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,0},{28,1},{32,0},{52,5},{56,1},{64,2},{68,0},{72,0},{76,2},{92,32},{policy,2},{policy+4,UINT32_MAX},{policy+12,UINT32_MAX},{policy+20,1},{sounds,UINT32_MAX},{sounds+8,UINT32_MAX},{sounds+48,0},{sounds+52,0},{sounds+56+48,1701},{areas+8,2},{areas+12,2},{areas+16,0x7fc00000},{areas+24,0},{areas+32,UINT32_MAX},{areas+40,1},{areas+48,0},{areas+52,0},{areas+56,2},{areas+60,1}}){auto bad=bytes;put(bad,change.first,change.second);reject(std::move(bad));}
 {auto bad=bytes;for(unsigned i=0;i<32;++i)bad[sounds+16+i]=0;reject(std::move(bad));}
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.view().bytes_identity()==v.bytes_identity());}
 {auto bad=bytes;bad[sounds+16]^=1;sign(bad);FieldStepsData changed;CHECK(changed.load(bad.data(),bad.size(),error));CHECK(!changed.view().matches(bank,error));CHECK(v.matches(bank,error));}
 {auto bad=bytes;bad.back()^=1;CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().bytes_identity()==v.bytes_identity());}
 CHECK(!data.load(bytes.data(),65537,error));
 CHECK(!data.load(nullptr,bytes.size(),error));CHECK(data.view().bytes_identity()==v.bytes_identity());
 std::printf("Field stepping codec: %u checks; original loop/media and rectangle identities, strict version/source/negative paths; no movement/audio consumer or scene claim\n",checks);
}
