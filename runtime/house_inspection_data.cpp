#include "encore/house_inspection_data.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
constexpr uint32_t sections=3,header=112,max_bytes=1024*1024;
constexpr uint32_t strides[]={1,76,16};
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
float f32(const uint8_t*p){auto b=u32(p);float v;std::memcpy(&v,&b,4);return v;}
Vec2 vec(const uint8_t*p){return {f32(p),f32(p+4)};}
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=10000&&std::abs(v.y)<=10000;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=(i>=16&&i<20)?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool utf8(const uint8_t*p,size_t n){for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<0x80)continue;unsigned extra;uint32_t low;if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;if(extra>n-i)return false;while(extra--){auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
bool safe(std::string_view p){if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;for(unsigned char ch:s)if(ch<32||ch==127)return false;if(b==p.size())break;a=b+1;}return true;}
}
uint32_t HouseInspectionView::count(HouseInspectionSection s)const{auto k=uint32_t(s);return bytes_&&k>=1&&k<=sections?u32(bytes_+64+(k-1)*16+8):0;}
const uint8_t*HouseInspectionView::record(HouseInspectionSection s,uint32_t i)const{auto k=uint32_t(s);if(!bytes_||k<1||k>sections||i>=count(s))return nullptr;return bytes_+u32(bytes_+64+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view HouseInspectionView::string(uint32_t o)const{auto*p=record(HouseInspectionSection::Strings,o);if(!p)return {};auto*e=static_cast<const uint8_t*>(std::memchr(p,0,count(HouseInspectionSection::Strings)-o));return e?std::string_view(reinterpret_cast<const char*>(p),size_t(e-p)):std::string_view{};}
HouseInspectionObject HouseInspectionView::object(uint32_t i)const{auto*p=record(HouseInspectionSection::Objects,i);return p?HouseInspectionObject{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),u32(p+36),u32(p+40),vec(p+44),vec(p+52),vec(p+60),vec(p+68)}:HouseInspectionObject{};}
HouseInspectionOverride HouseInspectionView::override_dialogue(uint32_t i)const{auto*p=record(HouseInspectionSection::Overrides,i);return p?HouseInspectionOverride{u32(p),u32(p+4),u32(p+8),u32(p+12)}:HouseInspectionOverride{};}
bool HouseInspectionData::load(const uint8_t*input,size_t size,std::string&error){
 auto fail=[&](const char*s){error=s;return false;};
 if(!input||size<header||size>max_bytes)return fail("House inspection pack size rejected");
 if(std::memcmp(input,"ENCHIN01",8)||u32(input+8)!=1||u32(input+12)!=size||u32(input+20)!=sections||u32(input+24)!=1||u32(input+28)!=1)return fail("House inspection schema/capabilities/rules rejected");
 for(unsigned i=52;i<64;++i)if(input[i])return fail("House inspection reserved bytes rejected");
 bool has_pin=false;for(unsigned i=32;i<52;++i)has_pin|=input[i]!=0;if(!has_pin)return fail("House inspection provenance rejected");
 if(crc(input,size)!=u32(input+16))return fail("House inspection CRC mismatch");
 size_t end=header;
 for(uint32_t i=0;i<sections;++i){auto*d=input+64+i*16;const auto off=u32(d+4),n=u32(d+8),bytes=u32(d+12);
  if(u16(d)!=i+1||u16(d+2)!=strides[i]||uint64_t(n)*strides[i]!=bytes)return fail("House inspection directory rejected");
  if(!n){if(off||bytes)return fail("House inspection empty section rejected");continue;}
  if(off%4||off<end||off>size||bytes>size-off)return fail("House inspection section span rejected");
  for(size_t j=end;j<off;++j)if(input[j])return fail("House inspection padding rejected");end=size_t(off)+bytes;
 }
 if(end!=size)return fail("House inspection trailing bytes rejected");
 HouseInspectionView v;v.bytes_=input;v.size_=size;
 const auto pool_size=v.count(HouseInspectionSection::Strings),objects=v.count(HouseInspectionSection::Objects),overrides=v.count(HouseInspectionSection::Overrides);
 if(!pool_size||pool_size>65536||!objects||objects>256||overrides>1024)return fail("House inspection capacity rejected");
 const auto*pool=v.record(HouseInspectionSection::Strings,0);
 if(pool[0]||pool[pool_size-1]||!utf8(pool,pool_size))return fail("House inspection strings rejected");
 auto str=[&](uint32_t o){return o<pool_size&&(o==0||pool[o-1]==0)&&v.string(o).size()<=4096;};
 auto path=[&](uint32_t o){return str(o)&&safe(v.string(o));};
 auto optional=[&](uint32_t o){return str(o)&&(v.string(o).empty()||safe(v.string(o)));};
 std::set<uint32_t>ids,owned;std::set<std::string_view>paths;
 for(uint32_t i=0;i<objects;++i){const auto o=v.object(i);
  if(!o.id||!ids.insert(o.id).second||!path(o.source_path)||!paths.insert(v.string(o.source_path)).second)return fail("House inspection zero/duplicate identity rejected");
  if(!path(o.default_dialogue)||!optional(o.appear_flag)||!optional(o.disappear_flag)||o.seen_key!=0||(o.player_turn&~3u)||!o.collision_mask||o.collision_mask>0xfffff||o.first_override>overrides||o.override_count>overrides-o.first_override||(o.default_dialogue_index!=house_inspection_no_index&&o.default_dialogue_index>=1024))return fail("House inspection reference/policy rejected");
  if(!finite(o.position)||!finite(o.interact_center)||!finite(o.interact_extents)||o.interact_extents.x<=0||o.interact_extents.y<=0||o.interact_extents.x>10000||o.interact_extents.y>10000||!finite(o.prompt_offset))return fail("House inspection geometry rejected");
  std::set<std::string_view>flags;
  for(uint32_t j=0;j<o.override_count;++j){const uint32_t at=o.first_override+j;const auto r=v.override_dialogue(at);if(!owned.insert(at).second||r.object!=i||!path(r.flag)||!flags.insert(v.string(r.flag)).second||!path(r.dialogue)||(r.dialogue_index!=house_inspection_no_index&&r.dialogue_index>=1024))return fail("House inspection override binding rejected");}
 }
 if(owned.size()!=overrides)return fail("House inspection orphan overrides rejected");
 std::vector<uint8_t>next(input,input+size);bytes_.swap(next);error.clear();return true;
}
bool HouseInspectionData::load_file(const char*path,std::string&error){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);}
}
