// Invoke only from an actual loaded full-House fixture owner; never fabricate
// source/Ready receipts. This file is not registered or run by this slice.
#include "../platform/ctr/house_return_interact_dialog.hpp"
#include <cassert>
using namespace encore::upstream;
using namespace encore::ctr;
namespace {
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,0);uint32_t c=~0u;for(auto x:b){c^=x;for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}put(b,16,~c);}
size_t record(const std::vector<uint8_t>&b){size_t p=64;for(unsigned i=0;i<2;++i)p+=4+get(b,p);auto n=get(b,p);p+=4;for(uint32_t i=0;i<n;++i)p+=4+get(b,p)+32;return p+4;}
}
void house_interact_source_negatives(const HouseReturnSources&actual_sources,
                                     const std::vector<uint8_t>&original){
  std::string e;FieldInteractData live;
  assert(HouseReturnInteractDialog::load(original.data(),original.size(),actual_sources,live,e));
  const auto first=live.records().front().id;const auto start=record(original);
  auto reject=[&](std::vector<uint8_t>b){
    seal(b);FieldInteractData parsed;assert(parsed.load(b.data(),b.size(),e));
    assert(!HouseReturnInteractDialog::admit_source(parsed,actual_sources,e));
    assert(!HouseReturnInteractDialog::load(b.data(),b.size(),actual_sources,live,e));
    assert(live.valid()&&live.records().front().id==first);
  };
  auto b=original;put(b,28,live.scene_id()^1);reject(std::move(b));
  b=original;put(b,start,UINT32_MAX);reject(std::move(b));
  b=original;put(b,start+4,get(b,start+4)+1);reject(std::move(b)); // wrong actual Ready ordinal
  b=original;put(b,start+8,UINT32_MAX);reject(std::move(b)); // unrelated onready Prompt
  b=original;put(b,start+12,get(b,start+12)^8);reject(std::move(b)); // native initial visibility
}
