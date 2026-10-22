#include "encore/dropped_items.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){auto bits=word(p);float value;std::memcpy(&value,&bits,4);return value;}
double scalar(const uint8_t*p){const uint64_t bits=uint64_t(word(p))|(uint64_t(word(p+4))<<32);double value;std::memcpy(&value,&bits,8);return value;}
bool finite(double v){return std::isfinite(v)&&std::abs(v)<=1e6;}
bool path(const std::string&s){
 if(s.empty()||s.front()=='/'||s.find(':')!=std::string::npos)return false;
 size_t begin=0;for(size_t n=0;n<=s.size();++n)if(n==s.size()||s[n]=='/'){
  const auto part=s.substr(begin,n-begin);if(part.empty()||part=="."||part=="..")return false;begin=n+1;
 }return true;
}
uint32_t stable(const std::string&prefix,const std::string&s){uint32_t result=2166136261u;for(unsigned char c:prefix+s)result=(result^c)*16777619u;return result;}
}
const DroppedTemplate*DroppedItemsData::item(uint32_t id)const{for(const auto&v:templates_)if(v.id==id)return &v;return nullptr;}
const DroppedOwner*DroppedItemsData::owner(uint32_t id)const{for(const auto&v:owners_)if(v.id==id)return &v;return nullptr;}
const DroppedClip*DroppedItemsData::clip(DroppedClipRole role)const{for(const auto&v:clips_)if(v.role==role)return &v;return nullptr;}
bool DroppedItemsData::load_file(const char*path,std::string&e){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,128*1024,e)&&load(bytes.data(),bytes.size(),e);}
bool DroppedItemsData::load(const uint8_t*p,size_t size,std::string&e){
 constexpr size_t sections=8,header=64+16*sections;
 if(!p||size<header||size>128*1024)return fail(e,"Dropped resource size");
 const auto scene=word(p+24);
 if(std::memcmp(p,"ENCDRP01",8)||word(p+8)!=1||word(p+12)!=size||word(p+20)!=1||!scene||scene>65535||word(p+28)||word(p+52)!=sections||word(p+56)||word(p+60)||encore::crc32(p+32,size-32)!=word(p+16))return fail(e,"Dropped resource header/version/capability/checksum");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){const auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return fail(e,"Dropped resource source pin");}
 const uint32_t strides[]={80,12,32,40,24,28,12,1},limits[]={1,uint32_t(DroppedScalar::Count),256,4096,2,2,4096,65536};
 uint32_t offsets[sections]{},counts[sections]{};size_t cursor=header;
 for(size_t n=0;n<sections;++n){
  const auto*d=p+64+16*n;offsets[n]=word(d+4);counts[n]=word(d+8);
  if(word(d)!=n+1||word(d+12)!=strides[n]||!counts[n]||counts[n]>limits[n]||offsets[n]%4||offsets[n]<cursor||offsets[n]>size||counts[n]>(size-offsets[n])/strides[n])return fail(e,"Dropped resource directory/span");
  for(size_t at=cursor;at<offsets[n];++at)if(p[at])return fail(e,"Dropped resource padding");
  cursor=offsets[n]+counts[n]*strides[n];
 }
 if(cursor!=size||counts[1]!=uint32_t(DroppedScalar::Count)||counts[4]!=2||counts[5]!=2||counts[7]<2)return fail(e,"Dropped resource section coverage/trailing bytes");
 DroppedItemsData next;const auto*meta=p+offsets[0];next.info_.scene=scene;next.info_.capacity=word(meta+36);next.info_.default_bits=word(meta+72);
 if(word(meta)!=scene||!next.info_.capacity||next.info_.capacity>4096||word(meta+56)>1||next.info_.default_bits>3||word(meta+76))return fail(e,"Dropped resource metadata");
 for(unsigned n=0;n<4;++n)if(word(meta+40+4*n)>=(1u<<20))return fail(e,"Dropped resource collision masks");
 next.info_.body_layer=word(meta+40);next.info_.body_mask=word(meta+44);next.info_.interaction_layer=word(meta+48);next.info_.interaction_mask=word(meta+52);next.info_.monitoring=word(meta+56)!=0;
 std::map<uint32_t,std::string>strings;std::set<std::string>distinct;const auto*pool=p+offsets[7];
 if(pool[0]||pool[counts[7]-1])return fail(e,"Dropped resource pool boundary");
 for(uint32_t at=0;at<counts[7];){
  const auto begin=at;std::string value;
  while(at<counts[7]&&pool[at]){const auto c=pool[at++];if(c<32||c>=127||c=='\\'||value.size()>=512)return fail(e,"Dropped resource pool text");value+=char(c);}
  if(at==counts[7]||(begin?!path(value):!value.empty())||!distinct.insert(value).second)return fail(e,"Dropped resource canonical source path");
  strings.emplace(begin,std::move(value));++at;
 }
 std::set<uint32_t>used{0};const auto string=[&](uint32_t at,std::string&out){const auto found=strings.find(at);if(found==strings.end()||found->second.empty())return false;used.insert(at);out=found->second;return true;};
 std::string*source_fields[]={&next.info_.source,&next.info_.name,&next.info_.prefab,&next.info_.script,&next.info_.holder,&next.info_.flag_script,&next.info_.inventory,&next.info_.item};
 for(unsigned n=0;n<8;++n)if(!string(word(meta+4+4*n),*source_fields[n]))return fail(e,"Dropped resource source strings");
 if(!string(word(meta+60),next.info_.success_dialogue)||!string(word(meta+64),next.info_.full_dialogue)||!string(word(meta+68),next.info_.empty_dialogue))return fail(e,"Dropped resource dialogue strings");
 for(uint32_t n=0;n<counts[1];++n){const auto*r=p+offsets[1]+12*n;const auto value=scalar(r+4);if(word(r)!=n+1||!finite(value))return fail(e,"Dropped resource scalar role/value");next.policy_[n]=value;}
 const auto&v=next.policy_;
 for(unsigned n:{0u,2u,9u,10u,11u,12u,13u,19u,20u})if(v[n]<=0)return fail(e,"Dropped resource positive scalar");
 for(unsigned n:{1u,3u,4u,5u,6u,14u,15u,16u,17u,18u})if(v[n]<0)return fail(e,"Dropped resource nonnegative scalar");
 if(v[17]>v[18]||v[20]>1024||v[20]!=std::floor(v[20]))return fail(e,"Dropped resource sparkle domain");
 const auto curve=[](double transition,double ease){
  return (transition==double(DroppedTransition::Quart)&&ease==double(DroppedEase::Out))||
         (transition==double(DroppedTransition::Quad)&&ease==double(DroppedEase::In))||
         (transition==double(DroppedTransition::Elastic)&&ease==double(DroppedEase::Out));
 };
 for(size_t n:{size_t(DroppedScalar::CollectTransition),size_t(DroppedScalar::ScaleTransition),size_t(DroppedScalar::RevertTransition)})if(!curve(v[n],v[n+1]))return fail(e,"Dropped resource unsupported interpolation capability");
 std::set<uint32_t>ids;std::set<std::string>names;
 for(uint32_t n=0;n<counts[2];++n){
  const auto*r=p+offsets[2]+32*n;DroppedTemplate t;t.id=word(r);t.doses=word(r+8);t.key_item=word(r+12)!=0;t.width=word(r+20);t.height=word(r+24);
  if(!string(word(r+4),t.item)||!string(word(r+16),t.texture))return fail(e,"Dropped resource template strings");
  if(!t.id||t.id==UINT32_MAX||t.id!=stable("dropped-template:",t.item)||!ids.insert(t.id).second||!names.insert(t.item).second||!t.doses||t.doses>65535||word(r+12)>1||!t.width||t.width>1024||!t.height||t.height>1024||word(r+28))return fail(e,"Dropped resource template domain");
  next.templates_.push_back(std::move(t));
 }
 std::set<uint32_t>owner_ids;std::set<std::string>paths;uint32_t previous=0;
 for(uint32_t n=0;n<counts[3];++n){
  const auto*r=p+offsets[3]+40*n;DroppedOwner o;o.id=word(r);o.order=word(r+4);o.template_id=word(r+20);o.bits=word(r+28);o.position={real(r+32),real(r+36)};
  if(!string(word(r+8),o.path)||!string(word(r+12),o.name)||!string(word(r+16),o.parent)||!string(word(r+24),o.flag))return fail(e,"Dropped resource owner strings");
  if(!o.id||o.id==UINT32_MAX||o.id!=stable("dropped-item:",o.path)||!owner_ids.insert(o.id).second||!paths.insert(o.path).second||o.order<=previous||o.order>65535||o.path!=o.parent+"/"+o.name||o.flag!=next.info_.name+"/"+o.name||!next.item(o.template_id)||o.bits>7||!o.object_flag()||!finite(o.position.x)||!finite(o.position.y))return fail(e,"Dropped resource owner domain/binding");
  previous=o.order;next.owners_.push_back(std::move(o));
 }
 for(uint32_t n=0;n<2;++n){const auto*r=p+offsets[4]+24*n;if(word(r)!=n+1||!string(word(r+20),next.shape_paths_[n])||next.shape_paths_[n].find('/')==std::string::npos)return fail(e,"Dropped resource shape role/path");for(unsigned j=0;j<4;++j){const auto value=real(r+4+4*j);if(!finite(value)||(j>=2&&value<=0))return fail(e,"Dropped resource shape domain");next.shapes_[n][j]=value;}}
 if(next.shape_paths_[0]==next.shape_paths_[1])return fail(e,"Dropped resource duplicate shape binding");
 uint32_t first=0;std::set<uint32_t>clip_ids;std::set<std::string>clip_names;
 for(uint32_t n=0;n<2;++n){
  const auto*r=p+offsets[5]+28*n;DroppedClip c;c.id=word(r);c.role=DroppedClipRole(word(r+4));const auto begin=word(r+12),count=word(r+16);c.loop=word(r+20)!=0;c.duration=real(r+24);
  if(!string(word(r+8),c.name))return fail(e,"Dropped resource clip string");
  if(word(r+4)!=n+1||c.id!=stable("dropped-clip:",c.name)||!clip_ids.insert(c.id).second||!clip_names.insert(c.name).second||begin!=first||!count||count>1024||count>counts[6]-first||word(r+20)>1||!std::isfinite(c.duration)||c.duration<=0||c.duration>60)return fail(e,"Dropped resource clip domain");
  float time=-1;
  for(uint32_t j=0;j<count;++j){const auto*k=p+offsets[6]+12*(begin+j);DroppedClipKey key;key.time=real(k);key.value=word(k+4)!=0;key.update=word(k+8);if(!std::isfinite(key.time)||key.time<0||key.time<=time||key.time>c.duration||word(k+4)>1||key.update>1)return fail(e,"Dropped resource visibility key domain");time=key.time;c.keys.push_back(key);}
  first+=count;next.clips_.push_back(std::move(c));
 }
 if(first!=counts[6]||used.size()!=strings.size())return fail(e,"Dropped resource unowned keys/strings");
 if(generation_==UINT64_MAX)return fail(e,"Dropped resource generation exhausted");
 next.generation_=generation_+1;next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool DroppedItemsData::compatible(const BreakablesData&b,std::string&e)const{
 if(!valid_||!b.valid()||info_.scene!=b.scene()||info_.source!=b.policy().source||info_.name!=b.policy().name)return fail(e,"Dropped companion scene identity");
 size_t count=0;for(const auto&c:b.contents())if(c.script==info_.script){++count;bool found=false;for(const auto&o:owners_)if(o.path==c.path&&o.parent==c.parent&&c.source==info_.prefab)found=true;if(!found)return fail(e,"Dropped companion content binding");}
 if(count!=owners_.size())return fail(e,"Dropped companion complete coverage");e.clear();return true;
}
}
