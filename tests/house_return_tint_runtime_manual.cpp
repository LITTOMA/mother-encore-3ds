// Manual-only native admission negatives; caller supplies the actual House
// sources and already prepared native owners. No synthetic lifecycle/frame.
#include "../platform/ctr/house_return_tint_runtime.hpp"
#include "manual_require.hpp"
#include <algorithm>

using namespace encore::upstream;
using namespace encore::ctr;
namespace {
uint32_t tint_runtime_word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void tint_runtime_put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));}
void tint_runtime_crc(std::vector<uint8_t>&b){tint_runtime_put(b,16,0);uint32_t crc=~0u;for(const auto v:b){crc^=v;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}tint_runtime_put(b,16,~crc);}
void tint_runtime_scene(std::vector<uint8_t>&b,uint32_t scene){
  tint_runtime_put(b,52,scene);size_t at=80;
  for(uint32_t i=0;i<tint_runtime_word(b,28);++i){const auto targets=tint_runtime_word(b,at+16);
    tint_runtime_put(b,at+4,scene);at+=20;
    for(unsigned text=0;text<2;++text)at+=4+tint_runtime_word(b,at);
    for(uint32_t target=0;target<targets;++target){at+=24;for(unsigned text=0;text<3;++text)at+=4+tint_runtime_word(b,at);}
  }
}
}
void house_return_tint_runtime_manual(const std::vector<uint8_t>&bytes,
    const HouseReturnSources&sources,const HouseReturnTintInput&actual_input){
  std::string error;FieldTintData data;
  MANUAL_REQUIRE(data.load(bytes.data(),bytes.size(),error));
  MANUAL_REQUIRE(HouseReturnTintRuntime::admit_source(data,sources,error));
  auto mismatched=[&](std::vector<uint8_t>bad){tint_runtime_crc(bad);FieldTintData foreign;
    MANUAL_REQUIRE(foreign.load(bad.data(),bad.size(),error));
    MANUAL_REQUIRE(!HouseReturnTintRuntime::admit_source(foreign,sources,error));
  };
  auto wrong_pin=bytes;wrong_pin.at(32)^=1;mismatched(wrong_pin);
  auto wrong_scene=bytes;tint_runtime_scene(wrong_scene,tint_runtime_word(bytes,52)^1u);mismatched(wrong_scene);
  auto ordinal=bytes;tint_runtime_put(ordinal,88,tint_runtime_word(ordinal,88)+1);mismatched(ordinal);
  size_t first_target=100;
  first_target+=4+tint_runtime_word(bytes,first_target);
  first_target+=4+tint_runtime_word(bytes,first_target);
  auto wrong_target=bytes;tint_runtime_put(wrong_target,first_target,tint_runtime_word(bytes,first_target)^1u);mismatched(wrong_target);
  const size_t node_path=first_target+24;
  auto wrong_path=bytes;wrong_path.at(node_path+4+tint_runtime_word(bytes,node_path)-1)^=1;mismatched(wrong_path);
  const std::string script="Scripts/misc/character_tint.gd";
  const auto source=std::search(bytes.begin()+64,bytes.end(),script.begin(),script.end());
  MANUAL_REQUIRE(source!=bytes.end());
  auto wrong_hash=bytes;wrong_hash.at(size_t(source-bytes.begin())+script.size())^=1;mismatched(wrong_hash);
  HouseReturnTintRuntime native;
  auto missing=actual_input;missing.signals=nullptr;MANUAL_REQUIRE(!native.prepare(missing,error));
  missing=actual_input;missing.canvas=nullptr;MANUAL_REQUIRE(!native.prepare(missing,error));
  missing=actual_input;missing.kinematic=nullptr;MANUAL_REQUIRE(!native.prepare(missing,error));
  // A correctly parsed substitute data owner cannot bypass Sources.tint borrow.
  missing=actual_input;missing.data=&data;MANUAL_REQUIRE(!native.prepare(missing,error));
}
