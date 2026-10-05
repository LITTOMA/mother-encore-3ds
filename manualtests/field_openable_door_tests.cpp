#include "encore/field_openable_door.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t openable_word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void openable_set(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void openable_crc(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t p=128;p<b.size();++p){c^=b[p];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0u);}openable_set(b,20,~c);}
}
// Deliberately manual-only. Compile this source separately; do not execute it
// during development/build/automatic CI. Trusted identity comes from the scene.
void field_openable_door_manual_parser_cases(const char*path,const FieldIdentity&id){
 std::ifstream file(path,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(file)),{});assert(b.size()>128);FieldOpenableDoorData d;std::string e;assert(d.load(b.data(),b.size(),id,e));const auto count=d.records().size();assert(count);
 for(size_t cut:{size_t(0),size_t(127),b.size()-1})assert(!d.load(b.data(),cut,id,e));
 for(size_t at:{size_t(0),size_t(8),size_t(12),size_t(16),size_t(20),size_t(24),size_t(28),size_t(32),size_t(40),size_t(60),size_t(124),b.size()-1}){auto bad=b;bad[at]^=1;assert(!d.load(bad.data(),bad.size(),id,e));assert(d.valid()&&d.records().size()==count);}
 auto utf=b;utf[132]=0xff;openable_crc(utf);assert(!d.load(utf.data(),utf.size(),id,e));
 size_t at=128;for(unsigned i=0;i<4;++i)at+=4+openable_word(b,at);at+=28;assert(openable_word(b,at)>0);at+=4;assert(openable_word(b,at)==3);at+=8;at+=4+openable_word(b,at);at+=12;assert(at+12<b.size());auto track=b;openable_set(track,at,99);openable_crc(track);assert(!d.load(track.data(),track.size(),id,e));
 FieldOpenableDoorRuntime runtime;assert(!runtime.initialize(d,{},e));assert(!runtime.create(d.records().front().id));assert(!runtime.ready(d.records().front().id));assert(!runtime.interact_item(d.records().front().id,0));
}
