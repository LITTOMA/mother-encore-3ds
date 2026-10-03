#include "encore/world_effect_data.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstring>
namespace encore::upstream {
namespace {
constexpr uint32_t header=128,limit=128*1024;
constexpr uint16_t strides[]={92,24,4,1};
// Reviewed compatibility identity, not content or gameplay tuning.
constexpr uint8_t pin[]={0x7d,0x92,0x46,0x60,0x0f,0xff,0xe5,0x18,0x40,0x8f,0x58,0x30,0xd4,0x84,0x86,0x35,0x01,0x90,0x05,0xa3};
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
float f32(const uint8_t*p){uint32_t bits=u32(p);float f;std::memcpy(&f,&bits,4);return f;}
WorldEffectColor color(const uint8_t*p){return {f32(p),f32(p+4),f32(p+8),f32(p+12)};}
bool bound(float f,float lo,float hi){return std::isfinite(f)&&f>=lo&&f<=hi;}
bool color_ok(WorldEffectColor c){return bound(c.r,0,1)&&bound(c.g,0,1)&&bound(c.b,0,1)&&bound(c.a,0,1);}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=(i>=16&&i<20)?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
}
uint32_t WorldEffectView::count(uint32_t section)const{return bytes_&&section<4?u32(bytes_+64+section*16+8):0;}
const uint8_t*WorldEffectView::record(uint32_t section,uint32_t index)const{
 if(section>=4||index>=count(section))return nullptr;
 const size_t offset=u32(bytes_+64+section*16+4),stride=strides[section];
 if(offset>size_||index>(size_-offset)/stride)return nullptr;
 const size_t pos=offset+size_t(index)*stride;return stride<=size_-pos?bytes_+pos:nullptr;
}
WorldEffectSettings WorldEffectView::settings()const{
 WorldEffectSettings s;const auto*p=record(0,0);if(!p)return s;
 s.texture_width=u32(p);s.texture_height=u32(p+4);
 s.source_width=f32(p+16);s.source_height=f32(p+20);s.appear_duration=f32(p+24);s.disappear_duration=f32(p+28);s.cycle_duration=f32(p+32);s.opacity=f32(p+36);
 s.move_x=f32(p+40);s.move_y=f32(p+44);s.amplitude_y=f32(p+48);s.frequency_y=f32(p+52);s.speed_y=f32(p+56);s.translation_ping_pong_y=f32(p+60);s.amplitude_ping_pong_y=f32(p+64);s.move_divisor=f32(p+68);s.pixel_snap_uv_epsilon=f32(p+72);s.initial_modulate=color(p+76);return s;
}
uint32_t WorldEffectView::key_count()const{return count(1);}
WorldEffectKey WorldEffectView::key(uint32_t i)const{const auto*p=record(1,i);return p?WorldEffectKey{f32(p),f32(p+4),color(p+8)}:WorldEffectKey{};}
uint32_t WorldEffectView::palette_count()const{return count(2);}
uint32_t WorldEffectView::palette(uint32_t i)const{const auto*p=record(2,i);return p?u32(p):0;}
const uint8_t*WorldEffectView::texels()const{return record(3,0);}
bool WorldEffectData::load(const uint8_t*in,size_t size,std::string&error){
 const auto fail=[&](const char*s){error=s;return false;};
 if(!in||size<header||size>limit)return fail("World effect pack size rejected");
 if(std::memcmp(in,"ENCWFX01",8)||u32(in+8)!=1||u32(in+12)!=size||u32(in+20)!=4||u32(in+24)!=1||u32(in+28)!=1||std::memcmp(in+32,pin,20))return fail("World effect schema/capability/pin rejected");
 for(size_t i=52;i<64;++i)if(in[i])return fail("World effect reserved bytes set");
 if(crc(in,size)!=u32(in+16))return fail("World effect CRC mismatch");
 size_t end=header;
 for(uint32_t i=0;i<4;++i){const auto*p=in+64+i*16;const uint32_t off=u32(p+4),n=u32(p+8),amount=u32(p+12);
  if(u16(p)!=i+1||u16(p+2)!=strides[i]||!n||uint64_t(n)*strides[i]!=amount||off%4||off<end||off>size||amount>size-off)return fail("World effect section rejected");
  for(size_t j=end;j<off;++j)if(in[j])return fail("World effect padding rejected");
  end=size_t(off)+amount;
 }
 if(end!=size)return fail("World effect trailing bytes rejected");
 WorldEffectView v;v.bytes_=in;v.size_=size;
 if(v.count(0)!=1||v.count(1)>256||v.count(2)>256)return fail("World effect section capacity rejected");
 const auto*p=v.record(0,0);const auto s=v.settings();
 if(!s.texture_width||s.texture_width>256||!s.texture_height||s.texture_height>256||u32(p+8)!=3||u32(p+12)||v.count(3)!=uint64_t(s.texture_width)*s.texture_height)return fail("World effect image/sampler rejected");
 for(unsigned i=0;i<19;++i)if(!std::isfinite(f32(p+16+i*4)))return fail("Nonfinite world effect setting");
 if(!bound(s.source_width,1,1024)||!bound(s.source_height,1,1024)||!bound(s.appear_duration,0.000001f,120)||!bound(s.disappear_duration,0.000001f,120)||!bound(s.cycle_duration,0.000001f,120)||!bound(s.opacity,0,1)||!bound(s.move_divisor,0.000001f,100)||!bound(s.pixel_snap_uv_epsilon,0,.001f)||!color_ok(s.initial_modulate))return fail("World effect settings rejected");
 for(unsigned i=6;i<13;++i)if(!bound(f32(p+16+i*4),-1000,1000))return fail("World effect shader bound rejected");
 float prev=-1;
 for(uint32_t i=0;i<v.key_count();++i){const auto k=v.key(i);if(!bound(k.time,0,s.cycle_duration)||k.time>=s.cycle_duration||k.time<=prev||k.ease!=1||!color_ok(k.color)||(i==0&&k.time!=0))return fail("World effect animation key rejected");prev=k.time;}
 for(uint32_t i=0;i<v.count(3);++i)if(v.texels()[i]>=v.palette_count())return fail("World effect palette index rejected");
 bytes_.assign(in,in+size);error.clear();return true;
}
bool WorldEffectData::load_file(const char*path,std::string&error){std::vector<uint8_t>b;if(!encore::read_file(path,b,limit,error))return false;return load(b.data(),b.size(),error);}
}
