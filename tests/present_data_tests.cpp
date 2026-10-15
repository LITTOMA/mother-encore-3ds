#include "encore/present_data.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"PresentData check failed at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
static void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
int main(int argc,char**argv){
 const char*path=argc>1?argv[1]:"romfs/world/house/opening.encpresent";
 PresentData data;CHECK(data.load_file(path,error));CHECK(data.view().count(PresentSection::Objects)==4);
 CHECK(data.view().count(PresentSection::Programs)==2);CHECK(data.view().count(PresentSection::Textures)==4);
 CHECK(data.view().count(PresentSection::Clips)==2);CHECK(data.view().command(0).opcode==uint32_t(PresentOpcode::BranchFlag));
 uint32_t programmes=0;for(uint32_t i=0;i<data.view().count(PresentSection::Objects);++i)if(data.view().object(i).program!=kPresentNone)++programmes;
 CHECK(programmes==2);
 std::vector<uint8_t>bytes;CHECK(encore::read_file(path,bytes,64*1024,error));
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));
 CHECK(data.view().valid());
 for(size_t i=0;i<bytes.size();++i){auto mutation=bytes;mutation[i]^=1;CHECK(!data.load(mutation.data(),mutation.size(),error));}
 CHECK(data.load(bytes.data(),bytes.size(),error));
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,3},{24,2},{28,1},{8,0}}){
  auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 auto downgraded=bytes;put(downgraded,20,1);fix(downgraded);CHECK(!data.load(downgraded.data(),downgraded.size(),error));
 const size_t objects_offset=64+(uint32_t(PresentSection::Objects)-1)*16;
 uint32_t map_index=0;for(uint32_t i=0;i<data.view().count(PresentSection::Objects);++i)if(data.view().object(i).policy==2&&data.view().object(i).program!=kPresentNone)map_index=i;
 auto get=[](const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;};
 put(downgraded,get(bytes,objects_offset+4)+map_index*52+8,1);fix(downgraded);
 CHECK(data.load(downgraded.data(),downgraded.size(),error)); // capability1 still admits story-only programmes
 CHECK(data.load(bytes.data(),bytes.size(),error));
 const auto directory=[&](PresentSection s){return 64+(uint32_t(s)-1)*16;};
 const auto object_base=get(bytes,directory(PresentSection::Objects)+4),program_base=get(bytes,directory(PresentSection::Programs)+4),command_base=get(bytes,directory(PresentSection::Commands)+4);
 const auto map_program=data.view().object(map_index).program,map_first=get(bytes,program_base+map_program*8);
 auto foreign=bytes;put(foreign,command_base+(map_first+3)*20+4,data.view().object(2).flag);fix(foreign);
 CHECK(!data.load(foreign.data(),foreign.size(),error));CHECK(error=="Present pack foreign object flag command rejected");
 auto duplicate=bytes;put(duplicate,object_base+52+12,data.view().object(map_index).flag);fix(duplicate);
 CHECK(!data.load(duplicate.data(),duplicate.size(),error));CHECK(error=="Present pack duplicate object flag");
 auto overlap=bytes;put(overlap,program_base+map_program*8,0);fix(overlap);
 CHECK(!data.load(overlap.data(),overlap.size(),error));CHECK(error=="Present pack overlapping programmes rejected");
 CHECK(data.view().object(map_index).program==map_program);
 auto trailing=bytes;trailing.push_back(0);fix(trailing);CHECK(!data.load(trailing.data(),trailing.size(),error));
 CHECK(data.load(bytes.data(),bytes.size(),error));
 std::printf("PresentData: %u checks; ENCPRS01 load, object/programme counts and fail-closed mutations\n",checks);
 return 0;
}
