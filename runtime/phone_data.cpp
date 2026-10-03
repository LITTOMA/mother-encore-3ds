#include "encore/phone_data.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
constexpr uint32_t sections=8,header=192,max_bytes=1024*1024;
constexpr uint32_t strides[]={1,60,152,44,12,12,8,8};
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
float f32(const uint8_t*p){auto b=u32(p);float v;std::memcpy(&v,&b,4);return v;}
double f64(const uint8_t*p){uint64_t b=uint64_t(u32(p))|(uint64_t(u32(p+4))<<32);double v;std::memcpy(&v,&b,8);return v;}
Vec2 vec(const uint8_t*p){return {f32(p),f32(p+4)};}
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1000000&&std::abs(v.y)<=1000000;}
bool positive(Vec2 v){return finite(v)&&v.x>0&&v.y>0;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=(i>=16&&i<20)?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool utf8(const uint8_t*p,size_t n){for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<0x80)continue;unsigned extra;uint32_t low;if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;if(extra>n-i)return false;while(extra--){auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
bool safe(std::string_view p){if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;if(b==p.size())break;a=b+1;}return true;}
}
uint32_t PhoneView::count(PhoneSection s)const{auto k=uint32_t(s);return bytes_&&k>=1&&k<=sections?u32(bytes_+64+(k-1)*16+8):0;}
const uint8_t*PhoneView::record(PhoneSection s,uint32_t i)const{auto k=uint32_t(s);if(!bytes_||k<1||k>sections||i>=count(s))return nullptr;return bytes_+u32(bytes_+64+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view PhoneView::string(uint32_t o)const{auto*p=record(PhoneSection::Strings,o);if(!p)return {};auto*e=static_cast<const uint8_t*>(std::memchr(p,0,count(PhoneSection::Strings)-o));return e?std::string_view(reinterpret_cast<const char*>(p),size_t(e-p)):std::string_view{};}
PhoneResource PhoneView::resource(uint32_t i)const{PhoneResource r;auto*p=record(PhoneSection::Resources,i);if(p){r={u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),{}};std::memcpy(r.sha256,p+28,32);}return r;}
PhoneObject PhoneView::object(uint32_t i)const{auto*p=record(PhoneSection::Objects,i);return p?PhoneObject{
 u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),u32(p+36),u32(p+40),u32(p+44),u32(p+48),u32(p+52),u32(p+56),u32(p+60),u32(p+64),u32(p+68),
 vec(p+72),vec(p+80),vec(p+88),vec(p+96),vec(p+104),vec(p+112),vec(p+120),vec(p+128),vec(p+136),vec(p+144)}:PhoneObject{};}
PhoneClip PhoneView::clip(uint32_t i)const{auto*p=record(PhoneSection::Clips,i);return p?PhoneClip{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),f64(p+36)}:PhoneClip{};}
PhoneFrameKey PhoneView::frame_key(uint32_t i)const{auto*p=record(PhoneSection::FrameKeys,i);return p?PhoneFrameKey{f64(p),u32(p+8)}:PhoneFrameKey{};}
PhoneSoundKey PhoneView::sound_key(uint32_t i)const{auto*p=record(PhoneSection::SoundKeys,i);return p?PhoneSoundKey{f64(p),u32(p+8)}:PhoneSoundKey{};}
PhoneFlagRef PhoneView::flag_ref(uint32_t i)const{auto*p=record(PhoneSection::FlagRefs,i);return p?PhoneFlagRef{u32(p),u32(p+4)}:PhoneFlagRef{};}
PhoneDispatch PhoneView::dispatch(uint32_t i)const{auto*p=record(PhoneSection::Dispatch,i);return p?PhoneDispatch{u32(p),u32(p+4)}:PhoneDispatch{};}

bool PhoneData::load(const uint8_t*input,size_t size,std::string&error){
 auto fail=[&](const char*s){error=s;return false;};
 if(!input||size<header||size>max_bytes)return fail("Phone pack size rejected");
 if(std::memcmp(input,"ENCPHN01",8)||u32(input+8)!=1||u32(input+12)!=size||u32(input+20)!=sections||u32(input+24)!=1||u32(input+28)!=1)return fail("Phone schema/capabilities/rules rejected");
 for(unsigned i=52;i<64;++i)if(input[i])return fail("Phone reserved bytes rejected");
 if(crc(input,size)!=u32(input+16))return fail("Phone CRC mismatch");
 size_t end=header;
 for(uint32_t i=0;i<sections;++i){auto*d=input+64+i*16;auto off=u32(d+4),n=u32(d+8),bytes=u32(d+12);
  if(u16(d)!=i+1||u16(d+2)!=strides[i]||uint64_t(n)*strides[i]!=bytes)return fail("Phone directory rejected");
  if(!n){if(off||bytes)return fail("Phone empty section rejected");continue;}
  if(off%4||off<end||off>size||bytes>size-off)return fail("Phone section span rejected");
  for(size_t j=end;j<off;++j)if(input[j])return fail("Phone padding rejected");
  end=size_t(off)+bytes;
 }
 if(end!=size)return fail("Phone trailing bytes rejected");
 PhoneView v;v.bytes_=input;v.size_=size;
 auto count=[&](PhoneSection s){return v.count(s);};
 auto capacity=[&](PhoneSection s,uint32_t lo,uint32_t hi){return count(s)>=lo&&count(s)<=hi;};
 if(!capacity(PhoneSection::Strings,1,65536)||!capacity(PhoneSection::Resources,1,16)||!capacity(PhoneSection::Objects,1,16)||!capacity(PhoneSection::Clips,2,32)||!capacity(PhoneSection::FrameKeys,2,1024)||!capacity(PhoneSection::SoundKeys,1,1024)||!capacity(PhoneSection::FlagRefs,1,128)||!capacity(PhoneSection::Dispatch,1,128))return fail("Phone capacity rejected");
 auto*pool=v.record(PhoneSection::Strings,0);auto pool_size=count(PhoneSection::Strings);
 if(pool[0]||pool[pool_size-1]||!utf8(pool,pool_size))return fail("Phone strings rejected");
 auto str=[&](uint32_t o){return o<pool_size&&(o==0||pool[o-1]==0)&&v.string(o).size()<=4096;};
 auto path=[&](uint32_t o){return str(o)&&safe(v.string(o));};
 auto span=[&](uint32_t first,uint32_t n,PhoneSection s){return first<=count(s)&&n<=count(s)-first;};
 for(auto s:{PhoneSection::Resources,PhoneSection::Objects,PhoneSection::Clips,PhoneSection::FlagRefs}){
  std::set<uint32_t>ids;for(uint32_t i=0;i<count(s);++i){auto id=u32(v.record(s,i));if(!id||!ids.insert(id).second)return fail("Phone zero/duplicate identity");}
 }
 for(uint32_t i=0;i<count(PhoneSection::Resources);++i){auto r=v.resource(i);
  if(!path(r.path)||r.kind!=1||!r.width||!r.height||r.width>1024||r.height>1024||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows)return fail("Phone texture rejected");
  bool has_hash=false;for(auto b:r.sha256)has_hash|=b!=0;if(!has_hash)return fail("Phone texture hash rejected");
 }
 std::set<uint32_t>owned_frames,owned_sounds,owned_clips,owned_dispatch,used_flags,used_resources;
 for(uint32_t i=0;i<count(PhoneSection::Clips);++i){auto c=v.clip(i);
  if((c.role!=uint32_t(PhoneClipRole::Idle)&&c.role!=uint32_t(PhoneClipRole::Ring))||!c.frame_count||!span(c.first_frame,c.frame_count,PhoneSection::FrameKeys)||!span(c.first_sound,c.sound_count,PhoneSection::SoundKeys)||c.loop>1||!std::isfinite(c.length)||c.length<=0||c.length>120||c.frame_track!=0||(c.sound_count?c.sound_track!=1:c.sound_track!=phone_no_index))return fail("Phone clip rejected");
  if((c.role==uint32_t(PhoneClipRole::Idle)&&(c.loop||c.sound_count||c.frame_count!=1))||(c.role==uint32_t(PhoneClipRole::Ring)&&(!c.loop||!c.sound_count)))return fail("Phone clip role policy rejected");
  double previous=-1;
  for(uint32_t j=0;j<c.frame_count;++j){auto k=v.frame_key(c.first_frame+j);if(!owned_frames.insert(c.first_frame+j).second||!std::isfinite(k.time)||k.time<0||k.time>=c.length||k.time<=previous||(j==0&&k.time!=0))return fail("Phone frame key rejected");previous=k.time;}
  previous=-1;
  for(uint32_t j=0;j<c.sound_count;++j){auto k=v.sound_key(c.first_sound+j);if(!owned_sounds.insert(c.first_sound+j).second||!std::isfinite(k.time)||k.time<0||k.time>=c.length||k.time<=previous||!path(k.resource))return fail("Phone sound key rejected");previous=k.time;}
 }
 std::set<std::string_view>flag_identities,object_paths;
 for(uint32_t i=0;i<count(PhoneSection::FlagRefs);++i){auto f=v.flag_ref(i);if(!path(f.identity)||!flag_identities.insert(v.string(f.identity)).second)return fail("Phone flag reference rejected");}
 for(uint32_t i=0;i<count(PhoneSection::Dispatch);++i){auto d=v.dispatch(i);if(d.flag_ref>=count(PhoneSection::FlagRefs)||!path(d.program))return fail("Phone dispatch rejected");used_flags.insert(d.flag_ref);}
 for(uint32_t i=0;i<count(PhoneSection::Objects);++i){auto o=v.object(i);
  if(!path(o.source_path)||o.resource>=count(PhoneSection::Resources)||!path(o.default_program)||!span(o.first_dispatch,o.dispatch_count,PhoneSection::Dispatch)||!o.dispatch_count||o.idle_clip>=count(PhoneSection::Clips)||o.ring_clip>=count(PhoneSection::Clips)||v.clip(o.idle_clip).role!=uint32_t(PhoneClipRole::Idle)||v.clip(o.ring_clip).role!=uint32_t(PhoneClipRole::Ring)||!path(o.ring_sound)||!path(o.hangup_sound)||!path(o.audio_bus)||!str(o.phone_location)||(o.policy&~31u)||!(o.policy&uint32_t(PhonePolicy::Free))||!o.collision_layer||o.collision_layer>0xfffff||o.collision_mask>0xfffff||!o.interaction_layer||o.interaction_layer>0xfffff||o.interaction_mask>0xfffff)return fail("Phone object reference/policy rejected");
  if(!finite(o.position)||!finite(o.sprite_center)||!finite(o.interact_center)||!positive(o.interact_extents)||!finite(o.interact_source_offset)||!positive(o.interact_source_extents)||!positive(o.interact_source_scale)||!finite(o.collider_center)||!positive(o.collider_extents)||!finite(o.audio_center))return fail("Phone geometry rejected");
  if(!object_paths.insert(v.string(o.source_path)).second||o.interact_center.x!=float(o.position.x+o.interact_source_offset.x)||o.interact_center.y!=float(o.position.y+o.interact_source_offset.y)||o.interact_extents.x!=float(o.interact_source_extents.x*o.interact_source_scale.x)||o.interact_extents.y!=float(o.interact_source_extents.y*o.interact_source_scale.y))return fail("Phone source transform consistency rejected");
  auto r=v.resource(o.resource);uint64_t frames=uint64_t(r.columns)*r.rows;
  if(o.initial_frame>=frames||v.frame_key(v.clip(o.idle_clip).first_frame).frame!=o.initial_frame)return fail("Phone initial frame rejected");
  used_resources.insert(o.resource);
  for(auto ci:{o.idle_clip,o.ring_clip}){if(!owned_clips.insert(ci).second)return fail("Phone shared clip ownership rejected");auto c=v.clip(ci);
   for(uint32_t j=0;j<c.frame_count;++j)if(v.frame_key(c.first_frame+j).frame>=frames)return fail("Phone atlas frame rejected");
   for(uint32_t j=0;j<c.sound_count;++j)if(v.sound_key(c.first_sound+j).resource!=o.ring_sound)return fail("Phone ring stream/key mismatch");
  }
  for(uint32_t j=0;j<o.dispatch_count;++j)if(!owned_dispatch.insert(o.first_dispatch+j).second)return fail("Phone dispatch ownership rejected");
 }
 if(owned_frames.size()!=count(PhoneSection::FrameKeys)||owned_sounds.size()!=count(PhoneSection::SoundKeys)||owned_clips.size()!=count(PhoneSection::Clips)||owned_dispatch.size()!=count(PhoneSection::Dispatch)||used_flags.size()!=count(PhoneSection::FlagRefs)||used_resources.size()!=count(PhoneSection::Resources))return fail("Phone orphan content rejected");
 std::vector<uint8_t>next(input,input+size);bytes_.swap(next);error.clear();return true;
}
bool PhoneData::load_file(const char*path,std::string&error){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);}
}
