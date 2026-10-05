#include "encore/items_data.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
constexpr uint32_t section_count=11,header_size=240,max_bytes=1024*1024;
constexpr uint16_t strides[]={1,16,48,16,60,84,20,24,20,24,16};
// Compatibility identity, not game content. Changing it requires source review.
constexpr uint8_t reviewed_pin[]={0x7d,0x92,0x46,0x60,0x0f,0xff,0xe5,0x18,0x40,0x8f,0x58,0x30,0xd4,0x84,0x86,0x35,0x01,0x90,0x05,0xa3};
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int32_t i32(const uint8_t*p){uint32_t u=u32(p);int32_t x;std::memcpy(&x,&u,4);return x;}
float f32(const uint8_t*p){uint32_t u=u32(p);float x;std::memcpy(&x,&u,4);return x;}
BattleValue value(const uint8_t*p){return {f32(p),f32(p+4),f32(p+8),f32(p+12)};}
bool bounded(float f,float lo,float hi){return std::isfinite(f)&&f>=lo&&f<=hi;}
bool bounded(BattleValue v,float lo,float hi){return bounded(v.x,lo,hi)&&bounded(v.y,lo,hi)&&bounded(v.z,lo,hi)&&bounded(v.w,lo,hi);}
bool integral(float x){return std::isfinite(x)&&std::floor(x)==x;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=(i>=16&&i<20)?0:p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
bool utf8(const uint8_t*p,size_t n){
 for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<0x80)continue;unsigned extra;uint32_t low;
  if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;
  if(extra>n-i)return false;
  while(extra--){auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}
  if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;
 }return true;
}
bool safe_path(std::string_view s){
 if(s.empty())return false;
 for(unsigned char c:s)if(c<32||c==127||c==':'||c=='\\')return false;
 size_t begin=0;for(;;){auto end=s.find('/',begin);if(end==s.npos)end=s.size();auto part=s.substr(begin,end-begin);if(part.empty()||part=="."||part=="..")return false;if(end==s.size())break;begin=end+1;}return true;
}
}
uint32_t ItemView::count(ItemSection s)const{
 auto k=uint32_t(s);return bytes_&&size_>=header_size&&k>=1&&k<=section_count?u32(bytes_+64+(k-1)*16+8):0;
}
const uint8_t*ItemView::record(ItemSection s,uint32_t i)const{
 auto k=uint32_t(s);if(!bytes_||k<1||k>section_count||i>=count(s))return nullptr;
 auto*d=bytes_+64+(k-1)*16;const size_t off=u32(d+4),stride=strides[k-1];
 if(off>size_||size_t(i)>(size_-off)/stride)return nullptr;
 const size_t at=off+size_t(i)*stride;return stride<=size_-at?bytes_+at:nullptr;
}
std::string_view ItemView::string(uint32_t offset)const{
 const auto*p=record(ItemSection::Strings,offset);if(!p||(offset&&p[-1]))return {};
 const auto*end=static_cast<const uint8_t*>(std::memchr(p,0,count(ItemSection::Strings)-offset));
 return end?std::string_view(reinterpret_cast<const char*>(p),size_t(end-p)):std::string_view{};
}
ItemMetadata ItemView::metadata()const{auto*p=record(ItemSection::Metadata,0);return p?ItemMetadata{u32(p),u32(p+4),u32(p+8),u32(p+12)}:ItemMetadata{};}
ItemDefinition ItemView::definition(uint32_t i)const{
 ItemDefinition d;auto*p=record(ItemSection::Definitions,i);if(p)d={u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),i32(p+24),i32(p+28),i32(p+32),i32(p+36),u32(p+40),u32(p+44)};return d;
}
ItemInstance ItemView::initial_instance(uint32_t i)const{auto*p=record(ItemSection::Instances,i);return p?ItemInstance{u32(p),u32(p+4),u32(p+8),u32(p+12)}:ItemInstance{};}
BattleResource ItemView::resource(uint32_t i)const{
 BattleResource r;auto*p=record(ItemSection::Resources,i);if(p){r.id=u32(p);r.path=u32(p+4);r.kind=u32(p+8);r.width=u32(p+12);r.height=u32(p+16);r.columns=u32(p+20);r.rows=u32(p+24);std::memcpy(r.sha256,p+28,32);}return r;
}
ItemLayout ItemView::layout(uint32_t i)const{
 ItemLayout r;auto*p=record(ItemSection::Layouts,i);if(p){r.id=u32(p);r.role=u32(p+4);r.parent=u32(p+8);r.kind=u32(p+12);r.resource=u32(p+16);r.frame=u32(p+20);r.flags=u32(p+24);r.anchor={f32(p+28),f32(p+32)};r.rect=value(p+36);r.color=value(p+52);for(unsigned j=0;j<4;++j)r.patch[j]=u32(p+68+j*4);}return r;
}
BattleValue ItemView::parameter(ItemParameter parameter)const{for(uint32_t i=0;i<count(ItemSection::Parameters);++i){auto*p=record(ItemSection::Parameters,i);if(p&&u32(p)==uint32_t(parameter))return value(p+4);}return {};}
ItemClip ItemView::clip(uint32_t i)const{auto*p=record(ItemSection::Clips,i);return p?ItemClip{u32(p),u32(p+4),u32(p+8),u32(p+12),f32(p+16),u32(p+20)}:ItemClip{};}
ItemTrack ItemView::track(uint32_t i)const{auto*p=record(ItemSection::Tracks,i);return p?ItemTrack{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:ItemTrack{};}
ItemKey ItemView::key(uint32_t i)const{auto*p=record(ItemSection::Keys,i);return p?ItemKey{f32(p),f32(p+4),value(p+8)}:ItemKey{};}
ItemSound ItemView::sound(uint32_t i)const{auto*p=record(ItemSection::Sounds,i);return p?ItemSound{u32(p),u32(p+4),u32(p+8),u32(p+12)}:ItemSound{};}
uint32_t ItemView::clip_for(ItemClipRole role)const{for(uint32_t i=0;i<count(ItemSection::Clips);++i)if(clip(i).role==uint32_t(role))return i;return item_no_index;}
uint32_t ItemView::layout_for(ItemLayoutRole role)const{for(uint32_t i=0;i<count(ItemSection::Layouts);++i)if(layout(i).role==uint32_t(role))return i;return item_no_index;}
ItemSound ItemView::sound_for(ItemSoundEvent event)const{for(uint32_t i=0;i<count(ItemSection::Sounds);++i)if(sound(i).event==uint32_t(event))return sound(i);return {};}

bool ItemData::load(const uint8_t*input,size_t size,std::string&error){
 auto fail=[&](const char*message){error=message;return false;};
 if(!input||size<header_size||size>max_bytes)return fail("Items pack size rejected");
 const auto version=u32(input+8),caps=u32(input+24);
 if(std::memcmp(input,"ENCITM01",8)||!((version==1&&caps==1)||(version==2&&caps==2))||u32(input+12)!=size||u32(input+20)!=section_count||u32(input+28)!=1||std::memcmp(input+32,reviewed_pin,20))return fail("Items schema/capability/pin rejected");
 for(unsigned i=52;i<64;++i)if(input[i])return fail("Items reserved header bytes set");
 if(crc(input,size)!=u32(input+16))return fail("Items checksum mismatch");
 size_t end=header_size;
 for(uint32_t k=1;k<=section_count;++k){
  const auto*d=input+64+(k-1)*16;uint32_t off=u32(d+4),n=u32(d+8),amount=u32(d+12);
  if(u16(d)!=k||u16(d+2)!=strides[k-1]||uint64_t(n)*strides[k-1]!=amount)return fail("Items directory rejected");
  if(!n){if(off||amount)return fail("Items empty section rejected");continue;}
  if(off%4||off<end||off>size||amount>size-off)return fail("Items section span rejected");
  for(size_t j=end;j<off;++j)if(input[j])return fail("Items section padding rejected");
  end=size_t(off)+amount;
 }
 if(end!=size)return fail("Items trailing bytes rejected");
 ItemView v;v.bytes_=input;v.size_=size;
 auto n=[&](ItemSection s){return v.count(s);};
 if(!n(ItemSection::Strings)||n(ItemSection::Strings)>65536||n(ItemSection::Metadata)!=1||!n(ItemSection::Definitions)||n(ItemSection::Definitions)>256||n(ItemSection::Instances)>64||!n(ItemSection::Resources)||n(ItemSection::Resources)>64||!n(ItemSection::Layouts)||n(ItemSection::Layouts)>128||n(ItemSection::Parameters)!=uint32_t(ItemParameter::Count)-1||n(ItemSection::Clips)!=uint32_t(ItemClipRole::Count)-1||!n(ItemSection::Tracks)||n(ItemSection::Tracks)>256||!n(ItemSection::Keys)||n(ItemSection::Keys)>2048||n(ItemSection::Sounds)!=uint32_t(ItemSoundEvent::Count)-1)return fail("Items section capacity rejected");
 const auto*pool=v.record(ItemSection::Strings,0);uint32_t pool_size=n(ItemSection::Strings);
 if(pool[0]||pool[pool_size-1]||!utf8(pool,pool_size))return fail("Items UTF-8 strings rejected");
 size_t length=0;for(uint32_t i=0;i<pool_size;++i){if(!pool[i])length=0;else if(++length>4096)return fail("Items string length rejected");}
 auto str=[&](uint32_t offset){return offset<pool_size&&(!offset||pool[offset-1]==0);};
 for(auto s:{ItemSection::Definitions,ItemSection::Instances,ItemSection::Resources,ItemSection::Layouts,ItemSection::Clips}){
  std::set<uint32_t>ids;for(uint32_t i=0;i<n(s);++i){auto id=u32(v.record(s,i));if(!id||!ids.insert(id).second)return fail("Items duplicate/zero ID");}
 }
 auto m=v.metadata();if(!m.capacity||m.capacity>64||!m.owner||m.flags||m.reserved||n(ItemSection::Instances)>m.capacity)return fail("Items metadata/capacity rejected");
 std::set<std::string_view>resource_paths;
 for(uint32_t i=0;i<n(ItemSection::Resources);++i){auto r=v.resource(i);
  if(!str(r.path)||!safe_path(v.string(r.path))||r.kind<1||r.kind>3||!r.width||r.width>8192||!r.height||r.height>8192||!r.columns||r.columns>256||!r.rows||r.rows>256||(r.kind==1&&(r.width%r.columns||r.height%r.rows))||std::all_of(r.sha256,r.sha256+32,[](uint8_t x){return x==0;}))return fail("Items resource rejected");
  if(!resource_paths.insert(v.string(r.path)).second)return fail("Items duplicate resource path rejected");
 }
 std::set<std::string_view>sources;
 for(uint32_t i=0;i<n(ItemSection::Definitions);++i){auto d=v.definition(i);
  if(!str(d.source)||!safe_path(v.string(d.source))||!sources.insert(v.string(d.source)).second||!str(d.name)||v.string(d.name).empty()||!str(d.description)||v.string(d.description).empty())return fail("Items definition strings rejected");
  if((d.icon!=item_no_index&&d.icon>=n(ItemSection::Resources))||(d.flags&~(caps==2?3u:1u))||d.can_use>1||d.heal_hp< -65535||d.heal_hp>65535||d.heal_pp< -65535||d.heal_pp>65535||d.max_hp_boost< -65535||d.max_hp_boost>65535||d.max_pp_boost< -65535||d.max_pp_boost>65535||((d.flags&1u)?d.equipment_slot>=4:d.equipment_slot!=item_no_index))return fail("Items definition rejected");
  if((d.flags&uint32_t(ItemDefinitionFlag::RichDescription))&&(d.can_use||(d.flags&1u)||d.heal_hp||d.heal_pp||d.max_hp_boost||d.max_pp_boost))return fail("Items unsupported rich/action projection must stay disabled");
  if(d.icon!=item_no_index&&v.resource(d.icon).kind!=1)return fail("Items icon resource rejected");
 }
 std::set<uint32_t>equipped;
 for(uint32_t i=0;i<n(ItemSection::Instances);++i){auto a=v.initial_instance(i);
  if(a.definition>=n(ItemSection::Definitions)||a.equipped>1||!a.doses||a.doses>65535)return fail("Items instance rejected");
  if(a.equipped){auto d=v.definition(a.definition);if(!(d.flags&1u)||!equipped.insert(d.equipment_slot).second)return fail("Items equipped instance rejected");}
 }
 for(uint32_t i=0;i<n(ItemSection::Layouts);++i){auto l=v.layout(i);
  if(l.role<1||l.role>uint32_t(ItemLayoutRole::Hint)||(l.parent!=item_no_index&&l.parent>=i)||l.kind<1||l.kind>uint32_t(ItemDrawKind::Text)||(l.flags&~15u)||((l.flags&4u)&&l.kind!=uint32_t(ItemDrawKind::Sprite))||((l.flags&8u)&&l.kind==uint32_t(ItemDrawKind::Container))||!bounded(l.anchor.x,0,1)||!bounded(l.anchor.y,0,1)||!bounded(l.rect,-8192,8192)||l.rect.z<0||l.rect.w<0||!bounded(l.color,0,1))return fail("Items layout geometry/parent rejected");
  for(auto x:l.patch)if(x>8192)return fail("Items patch range rejected");
  if(l.kind==uint32_t(ItemDrawKind::Sprite)||l.kind==uint32_t(ItemDrawKind::NinePatch)){
   if(l.resource>=n(ItemSection::Resources))return fail("Items layout resource rejected");
   auto r=v.resource(l.resource);if(r.kind!=1||l.frame>=uint64_t(r.columns)*r.rows||l.rect.z<=0||l.rect.w<=0)return fail("Items sprite layout rejected");
   if(l.kind==uint32_t(ItemDrawKind::NinePatch)&&(l.patch[0]+l.patch[2]>r.width/r.columns||l.patch[1]+l.patch[3]>r.height/r.rows))return fail("Items patch margins rejected");
  }else if(l.resource!=item_no_index&&(l.kind!=uint32_t(ItemDrawKind::Text)||l.resource>=n(ItemSection::Resources)))return fail("Items unused layout resource rejected");
  if(l.kind!=uint32_t(ItemDrawKind::NinePatch)&&(l.patch[0]||l.patch[1]||l.patch[2]||l.patch[3]))return fail("Items unused patch rejected");
 }
 uint32_t grids=0,infos=0;bool cursor=false;
 for(uint32_t i=0;i<n(ItemSection::Layouts);++i){auto l=v.layout(i);grids+=l.role==uint32_t(ItemLayoutRole::Grid);infos+=l.role==uint32_t(ItemLayoutRole::InfoPanel);cursor|=l.role==uint32_t(ItemLayoutRole::Cursor)&&l.kind==uint32_t(ItemDrawKind::Sprite);}
 if(grids!=1||infos!=1||!cursor||v.layout_for(ItemLayoutRole::Panel)==item_no_index||v.layout_for(ItemLayoutRole::ItemLabel)==item_no_index||v.layout_for(ItemLayoutRole::Description)==item_no_index)return fail("Items required layout binding rejected");
 std::set<uint32_t>params;
 for(uint32_t i=0;i<n(ItemSection::Parameters);++i){auto*p=v.record(ItemSection::Parameters,i);uint32_t k=u32(p);auto q=value(p+4);
  if(!k||k>=uint32_t(ItemParameter::Count)||!params.insert(k).second||!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z)||!std::isfinite(q.w))return fail("Items parameter rejected");
  if(k==1||k==2||k==4){if(q.x<=0||q.x>8192||q.y<=0||q.y>8192||q.z||q.w)return fail("Items dimensions rejected");}
  else if(k==3){if(!integral(q.x)||!integral(q.y)||q.x<1||q.x>64||q.y<1||q.y>64||!bounded(q.z,0,8192)||!bounded(q.w,0,8192)||q.z==0||q.w==0)return fail("Items grid rejected");}
  else if(k>=8&&k<=10){if(!bounded(q,0,1))return fail("Items parameter color rejected");}
  else if(k==11){for(unsigned j=4;j<20;j+=4){float f=f32(p+j);if(!integral(f)||double(f)<0||double(f)>double(UINT32_MAX))return fail("Items input binding rejected");}if(q.z<=0||double(q.z)>double(0x80000000u)||(uint32_t(q.z)&(uint32_t(q.z)-1u))||q.w)return fail("Items input mask rejected");}
  else if(k==12){if(q.y<=0||q.x<q.y||q.x>10||q.z||q.w)return fail("Items input repeat timing rejected");}
  else{
   if(!bounded(q,-8192,8192))return fail("Items parameter range rejected");
   if(k==5&&(q.z<=0||q.w<=0))return fail("Items cursor dimensions rejected");
   if(k==6&&(q.x<=0||q.x>120||q.y<=0||q.y>120||q.z<=0||q.z>120||q.w))return fail("Items cursor timing rejected");
   if(k==7&&(q.x<=0||q.x>120||q.y<0||q.z||q.w))return fail("Items info motion rejected");
  }
 }
 std::set<uint32_t>roles,owned_tracks,owned_keys;
 for(uint32_t i=0;i<n(ItemSection::Clips);++i){auto c=v.clip(i);
  if(!c.role||c.role>=uint32_t(ItemClipRole::Count)||!roles.insert(c.role).second||!c.track_count||c.first_track>n(ItemSection::Tracks)||c.track_count>n(ItemSection::Tracks)-c.first_track||!bounded(c.duration,0,120)||c.duration==0||c.loop>1)return fail("Items clip rejected");
  std::set<std::pair<uint32_t,uint32_t>>targets;
  for(uint32_t j=0;j<c.track_count;++j){uint32_t ti=c.first_track+j;auto t=v.track(ti);
   if(!owned_tracks.insert(ti).second||t.target>=n(ItemSection::Layouts)||!t.property||t.property>uint32_t(ItemProperty::Offset)||t.interpolation>1||!t.key_count||t.first_key>n(ItemSection::Keys)||t.key_count>n(ItemSection::Keys)-t.first_key||!targets.insert({t.target,t.property}).second)return fail("Items track/ownership rejected");
   float previous=-1;
   for(uint32_t a=0;a<t.key_count;++a){uint32_t ki=t.first_key+a;auto k=v.key(ki);
    if(!owned_keys.insert(ki).second||!bounded(k.time,0,c.duration)||k.time<=previous||!bounded(k.ease,-100,100)||!bounded(k.value,-8192,8192))return fail("Items key/ownership rejected");
    previous=k.time;
    if((t.property==uint32_t(ItemProperty::Alpha)||t.property==uint32_t(ItemProperty::Visible))&&(!bounded(k.value.x,0,1)||(t.property==uint32_t(ItemProperty::Visible)&&k.value.x!=0&&k.value.x!=1)))return fail("Items animated alpha/visibility rejected");
    if(t.property==uint32_t(ItemProperty::Scale)&&(k.value.x<=0||k.value.x>16||k.value.y<=0||k.value.y>16))return fail("Items animated scale rejected");
    if(t.property==uint32_t(ItemProperty::Rect)&&(k.value.z<0||k.value.w<0))return fail("Items animated rectangle rejected");
    if(t.property==uint32_t(ItemProperty::Frame)){auto l=v.layout(t.target);auto r=v.resource(l.resource);if((l.kind!=uint32_t(ItemDrawKind::Sprite)&&l.kind!=uint32_t(ItemDrawKind::NinePatch))||t.interpolation!=1||!integral(k.value.x)||k.value.x<0||k.value.x>=float(uint64_t(r.columns)*r.rows))return fail("Items animated frame rejected");}
   }
  }
 }
 if(owned_tracks.size()!=n(ItemSection::Tracks)||owned_keys.size()!=n(ItemSection::Keys))return fail("Items orphan animation rejected");
 std::set<uint32_t>events;
 for(uint32_t i=0;i<n(ItemSection::Sounds);++i){auto s=v.sound(i);auto path=v.string(s.path);
  if(!s.event||s.event>=uint32_t(ItemSoundEvent::Count)||!events.insert(s.event).second||!str(s.path)||!s.audio_id||s.flags||(!safe_path(path)&&!(path.substr(0,6)=="res://"&&safe_path(path.substr(6)))))return fail("Items sound rejected");
 }
 std::vector<uint8_t>copy(input,input+size);bytes_.swap(copy);error.clear();return true;
}
bool ItemData::load_file(const char*path,std::string&error){std::vector<uint8_t>bytes;if(!encore::read_file(path,bytes,max_bytes,error))return false;return load(bytes.data(),bytes.size(),error);}
}
