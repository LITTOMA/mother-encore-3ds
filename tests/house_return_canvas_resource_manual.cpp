// Manual v2 parser driver. Not run by this change. Link the shared Canvas
// reader and UTF-8 core, then pass the actual house-return.enccanvas path.
// Runtime Control owner/digest cross-binding must additionally be exercised
// through the real House native owner; a successful parser is not Ready.
#include "encore/field_canvas_art.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
using namespace encore::upstream;
static uint32_t get(const std::vector<uint8_t>&b,size_t p){assert(p+4<=b.size());return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
static void put(std::vector<uint8_t>&b,size_t p,uint32_t v){assert(p+4<=b.size());for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(i*8));}
static uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
struct Cursor{const std::vector<uint8_t>&b;size_t at=128;uint32_t u(){auto v=get(b,at);at+=4;return v;}void skip(size_t n){assert(at+n<=b.size());at+=n;}void text(){skip(u());}void hash(){skip(32);}};
struct Sites{size_t page=0,second_page=0,animated_frame=0,atlas=0,uniform_type=0,uniform_value=0,control_owner=0,control_digest=0;};
static Sites sites(const std::vector<uint8_t>&b){
 Cursor c{b};Sites s;c.text();c.skip(12);c.hash();auto textures=c.u();
 for(uint32_t i=0;i<textures;++i){c.skip(12);c.text();c.hash();auto pages=c.u();for(uint32_t j=0;j<pages;++j){if(!s.page)s.page=c.at;if(j==1&&!s.second_page)s.second_page=c.at;c.skip(28);c.text();c.hash();c.hash();}}
 c.skip(20);c.text();c.text();c.hash();c.hash();auto records=c.u();
 for(uint32_t i=0;i<records;++i){auto base=c.at;c.u();auto kind=c.u();c.skip(12);c.u();if(kind==2&&!s.animated_frame)s.animated_frame=base+24;c.skip(32+16);c.text();c.text();c.text();c.hash();c.skip(32);c.text();c.text();auto animations=c.u();
  for(uint32_t j=0;j<animations;++j){c.text();c.skip(8);auto frames=c.u();for(uint32_t k=0;k<frames;++k){c.u();if(!s.atlas)s.atlas=c.at;c.skip(36);c.text();}}
  if(c.u()){c.skip(8);c.text();c.hash();auto uniforms=c.u();for(uint32_t j=0;j<uniforms;++j){c.text();if(!s.uniform_type){s.uniform_type=c.at;s.uniform_value=c.at+12;}c.skip(32);}}
 }
 assert(c.u());c.skip(8);s.control_owner=c.at;c.u();c.text();c.text();c.text();c.hash();s.control_digest=c.at;c.hash();return s;
}
int main(int argc,char**argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);assert(f);std::vector<uint8_t>raw{std::istreambuf_iterator<char>(f),{}};assert(raw.size()>=128&&get(raw,8)==2);
 FieldIdentity identity;identity.scene_id=get(raw,36);std::copy_n(raw.begin()+40,20,identity.upstream_commit.begin());std::copy_n(raw.begin()+60,32,identity.source_sha256.begin());
 FieldCanvasArtData data;std::string e;assert(data.load(raw.data(),raw.size(),identity,e));assert(data.format()==2&&!data.scene_admitted());auto s=sites(raw);
 auto reject=[&](std::vector<uint8_t>b,bool repair=true){if(repair){put(b,16,uint32_t(b.size()));put(b,20,crc(b.data()+128,b.size()-128));}assert(!data.load(b.data(),b.size(),identity,e));assert(!e.empty());assert(data.valid()&&data.format()==2);};
 auto mutate=[&](size_t p,uint32_t v){auto b=raw;put(b,p,v);reject(std::move(b));};
 auto short_pack=raw;short_pack.pop_back();reject(std::move(short_pack));
 mutate(8,3);mutate(28,1);mutate(32,2);mutate(124,1);
 assert(s.page&&s.second_page&&s.animated_frame&&s.atlas&&s.uniform_type&&s.control_owner&&s.control_digest);
 mutate(s.page+12,0); // uncovered page
 mutate(s.page+4,0xffffffffu); // source PNG bounds
 auto overlap=raw;put(overlap,s.second_page+4,get(overlap,s.page+4));put(overlap,s.second_page+8,get(overlap,s.page+8));reject(std::move(overlap));
 mutate(s.animated_frame,0xffffffffu);mutate(s.atlas,0x49742400u); // finite million-pixel atlas origin
 mutate(s.uniform_type,6);mutate(s.uniform_value,0x7fc00000u); // unknown uniform / NaN
 mutate(s.control_owner,0);auto digest=raw;std::fill_n(digest.begin()+s.control_digest,32,0);reject(std::move(digest));
 // For real native owner integration: change a nonzero native property digest
 // or actual owner attachment while preserving source inventory/CRC. Parser
 // admission alone is insufficient; admit_control/control_snapshot must reject
 // the mismatch before any visible Label delegate is emitted. Also omit the
 // owner entirely and require collect() to fail on visible Control boundaries.
}
