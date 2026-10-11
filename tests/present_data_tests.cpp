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
 const char*path=argc>1?argv[1]:"romfs/data/opening.encpresent";
 PresentData data;CHECK(data.load_file(path,error));CHECK(data.view().count(PresentSection::Objects)==4);
 CHECK(data.view().count(PresentSection::Programs)==1);CHECK(data.view().count(PresentSection::Textures)==4);
 CHECK(data.view().count(PresentSection::Clips)==2);CHECK(data.view().command(0).opcode==uint32_t(PresentOpcode::BranchFlag));
 uint32_t programmes=0;for(uint32_t i=0;i<data.view().count(PresentSection::Objects);++i)if(data.view().object(i).program!=kPresentNone)++programmes;
 CHECK(programmes==1);
 std::vector<uint8_t>bytes;CHECK(encore::read_file(path,bytes,64*1024,error));
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));
 CHECK(data.valid());
 for(size_t i=0;i<bytes.size();++i){auto mutation=bytes;mutation[i]^=1;CHECK(!data.load(mutation.data(),mutation.size(),error));}
 CHECK(data.load(bytes.data(),bytes.size(),error));
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,2},{28,1},{8,0}}){
  auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 auto trailing=bytes;trailing.push_back(0);fix(trailing);CHECK(!data.load(trailing.data(),trailing.size(),error));
 CHECK(data.load(bytes.data(),bytes.size(),error));
 std::printf("PresentData: %u checks; ENCPRS01 load, object/programme counts and fail-closed mutations\n",checks);
 return 0;
}
