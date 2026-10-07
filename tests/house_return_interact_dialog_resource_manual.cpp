// Manual only; not registered or executed by this resource slice.
#include "encore/field_interact_dialog.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void seal(std::vector<uint8_t>&b){put(b,16,0);uint32_t c=~0u;for(auto x:b){c^=x;for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}put(b,16,~c);}
size_t first_record(const std::vector<uint8_t>&b){size_t p=64;for(unsigned i=0;i<2;++i)p+=4+get(b,p);auto n=get(b,p);p+=4;for(uint32_t i=0;i<n;++i)p+=4+get(b,p)+32;return p+4;}
}
int main(int argc,char**argv){
  assert(argc==2);std::ifstream f(argv[1],std::ios::binary);
  const std::vector<uint8_t>original{std::istreambuf_iterator<char>(f),{}};
  FieldInteractData live;std::string e;assert(live.load(original.data(),original.size(),e));
  const auto scene=live.scene_id();const auto first=live.records().front().id;
  auto rejected=[&](std::vector<uint8_t>b){assert(!live.load(b.data(),b.size(),e));assert(live.valid()&&live.scene_id()==scene&&live.records().front().id==first);};
  for(auto p:{size_t(8),size_t(20),size_t(24)}){auto b=original;put(b,p,2);seal(b);rejected(std::move(b));}
  auto b=original;b.back()^=1;rejected(std::move(b));
  b=original;b.pop_back();rejected(std::move(b));
  const auto record=first_record(original);
  b=original;put(b,record+12,32);seal(b);rejected(std::move(b)); // unknown flags
  b=original;put(b,record+16,0x7fc00000);seal(b);rejected(std::move(b)); // NaN offset
  b=original;put(b,record+8,get(b,record));seal(b);rejected(std::move(b)); // root=Prompt identity
}
