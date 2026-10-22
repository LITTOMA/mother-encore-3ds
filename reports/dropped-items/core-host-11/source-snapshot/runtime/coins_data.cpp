#include "encore/coins.hpp"
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
float real(const uint8_t*p){auto u=word(p);float v;std::memcpy(&v,&u,4);return v;}
double scalar(const uint8_t*p){uint64_t u=word(p)|uint64_t(word(p+4))<<32;double v;std::memcpy(&v,&u,8);return v;}
bool finite(double v){return std::isfinite(v)&&std::abs(v)<=1e6;}
bool path(const std::string&s){if(s.empty()||s.front()=='/'||s.find(':')!=std::string::npos)return false;size_t begin=0;for(size_t n=0;n<=s.size();++n)if(n==s.size()||s[n]=='/'){auto p=s.substr(begin,n-begin);if(p.empty()||p=="."||p=="..")return false;begin=n+1;}return true;}
}
const CoinClip*CoinsData::clip(uint32_t id)const{for(const auto&c:clips_)if(c.id==id)return &c;return nullptr;}
bool CoinsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,128*1024,e)&&load(b.data(),b.size(),e);}
bool CoinsData::load(const uint8_t*p,size_t size,std::string&e){
 if(!p||size<208||size>128*1024)return fail(e,"Coin resource size");
 if(std::memcmp(p,"ENCCOIN1",8)||word(p+8)!=1||word(p+12)!=size||word(p+20)!=1||word(p+24)||word(p+28)||word(p+52)!=9||word(p+56)||word(p+60)||encore::crc32(p+32,size-32)!=word(p+16))return fail(e,"Coin header/version/capability/checksum");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return fail(e,"Coin source pin");}
 const uint32_t strides[]={112,96,24,24,8,24,48,24,1},limits[]={1,1,64,2048,4096,1024,1,1,8192};uint32_t offsets[9]{},counts[9]{};size_t cursor=208;
 for(unsigned i=0;i<9;++i){const auto*d=p+64+i*16;offsets[i]=word(d+4);counts[i]=word(d+8);if(word(d)!=i+1||word(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<cursor||offsets[i]>size||counts[i]>(size-offsets[i])/strides[i])return fail(e,"Coin section identity/span");for(size_t n=cursor;n<offsets[i];++n)if(p[n])return fail(e,"Coin section padding");cursor=offsets[i]+counts[i]*strides[i];}
 if(cursor!=size||counts[8]<2)return fail(e,"Coin trailing/pool bytes");
 CoinsData v;const auto*pr=p+offsets[0];for(unsigned n=0;n<9;++n){v.policy_.scalar[n]=scalar(pr+8*n);if(!finite(v.policy_.scalar[n]))return fail(e,"Coin policy scalar");}const auto&x=v.policy_.scalar;float y[10];for(unsigned n=0;n<10;++n){y[n]=real(pr+72+4*n);if(!finite(y[n]))return fail(e,"Coin policy real");}
 if(x[0]<=0||x[0]>x[1]||x[2]<=0||x[3]<=0||x[3]>x[4]||x[5]<=0||x[6]<0||x[6]>x[7]||x[8]<=0||y[0]<=0||y[1]<=0||y[2]<0||y[2]>100||y[3]<=0||y[3]>1||y[4]<=0||y[5]<0)return fail(e,"Coin policy domain");
 v.policy_.body_radius=y[0];v.policy_.pickup_radius=y[1];v.policy_.margin=y[2];v.policy_.delta=y[3];v.policy_.sparkle_fps=y[4];v.policy_.panning=y[5];v.policy_.sprite_offset={y[6],y[7]};v.policy_.sparkle_offset={y[8],y[9]};
 std::map<uint32_t,std::string>strings;std::set<std::string>distinct;const auto*pool=p+offsets[8];if(pool[0]||pool[counts[8]-1])return fail(e,"Coin pool boundary");
 for(uint32_t n=0;n<counts[8];){const auto begin=n;std::string s;while(n<counts[8]&&pool[n]){auto c=pool[n++];if(c<32||c>=127||c=='\\'||s.size()>=512)return fail(e,"Coin pool text");s+=char(c);}if(n==counts[8]||(!begin&&!s.empty())||(begin&&s.empty())||!distinct.insert(s).second)return fail(e,"Coin canonical pool");strings.emplace(begin,std::move(s));++n;}
 std::set<uint32_t>used{0};auto string=[&](uint32_t at,std::string&out,bool is_path=true){auto found=strings.find(at);if(found==strings.end()||found->second.empty()||(is_path&&!path(found->second)))return false;used.insert(at);out=found->second;return true;};
 const auto*mr=p+offsets[1];uint32_t m[24];for(unsigned n=0;n<24;++n)m[n]=word(mr+4*n);
 if(!m[0]||m[0]>1000000||m[1]>=1u<<20||m[2]>=1u<<20||m[3]>=1u<<20||m[4]>=1u<<20||!m[5]||m[5]>65535||m[6]>=m[5]||m[11]!=counts[4]||m[12]!=counts[5]||m[13]>=m[12]||m[14]>1||m[15]>1||!m[16]||m[16]>1024||m[17]>=20||m[23])return fail(e,"Coin metadata domain");
 auto&info=v.info_;info.cash=m[0];info.body_layer=m[1];info.body_mask=m[2];info.pickup_layer=m[3];info.pickup_mask=m[4];info.frames=m[5];info.initial_frame=m[6];info.flipping=m[7];info.grounded=m[8];info.collect=m[9];info.reset=m[10];info.direction_count=m[11];info.sparkle_count=m[12];info.sparkle_initial=m[13];info.sparkle_playing=m[14];info.sparkle_loop=m[15];info.default_coins=m[16];info.pickup_area_bit=m[17];
 if(!string(m[18],info.drop_audio)||!string(m[19],info.collect_audio)||!string(m[20],info.voice,false)||!string(m[21],info.scene)||!string(m[22],info.script))return fail(e,"Coin metadata source strings");
 const auto*ar=p+offsets[7];uint32_t asset_refs[6];for(unsigned n=0;n<6;++n){asset_refs[n]=word(ar+4*n);if(!string(asset_refs[n],v.assets_[n]))return fail(e,"Coin source asset reference");}
 uint32_t first=0;std::set<uint32_t>ids;for(uint32_t n=0;n<counts[2];++n){const auto*r=p+offsets[2]+24*n;CoinClip c;c.id=word(r);c.duration=real(r+4);c.loop=word(r+8);const auto start=word(r+12),count=word(r+16);if(!c.id||c.id>65535||!ids.insert(c.id).second||!std::isfinite(c.duration)||c.duration<=0||c.duration>60||word(r+8)>1||start!=first||!count||count>counts[3]-first||word(r+20))return fail(e,"Coin clip identity/span");std::pair<uint32_t,float>previous{0,-1};
  for(uint32_t i=0;i<count;++i){const auto*k=p+offsets[3]+24*(start+i);const auto target=word(k);CoinKey key;key.target=CoinProperty(target);key.time=real(k+4);key.value=scalar(k+8);key.update=word(k+16);const auto order=std::make_pair(target,key.time);if((target!=1&&target!=2)||!std::isfinite(key.time)||key.time<0||key.time>c.duration||!std::isfinite(key.value)||key.update!=(target==1?1u:0u)||word(k+20)||order<=previous||(target==1?(key.value<0||key.value>=info.frames||key.value!=std::floor(key.value)):(key.value<0||key.value>1)))return fail(e,"Coin animation key domain/order");previous=order;c.keys.push_back(key);}
  first+=count;v.clips_.push_back(std::move(c));
 }
 if(first!=counts[3]||std::set<uint32_t>{m[7],m[8],m[9],m[10]}.size()!=4||ids!=std::set<uint32_t>{m[7],m[8],m[9],m[10]})return fail(e,"Coin unowned animation roles/keys");
 for(uint32_t n=0;n<counts[4];++n){const auto*r=p+offsets[4]+8*n;Vec2 point{real(r),real(r+4)};if(!std::isfinite(point.x)||!std::isfinite(point.y)||std::abs(double(point.x)*point.x+double(point.y)*point.y-1)>=1e-5)return fail(e,"Coin direction domain");v.directions_.push_back(point);}
 for(uint32_t n=0;n<counts[5];++n){const auto*r=p+offsets[5]+24*n;std::string texture;if(word(r)!=n||word(r+4)!=asset_refs[1]||!string(word(r+4),texture))return fail(e,"Coin atlas identity");std::array<float,4>rect;for(unsigned k=0;k<4;++k){rect[k]=real(r+8+4*k);if(!std::isfinite(rect[k])||rect[k]<0||rect[k]>65535)return fail(e,"Coin atlas bounds");}if(rect[2]<=0||rect[3]<=0)return fail(e,"Coin atlas extents");v.atlas_.push_back(rect);}
 const auto*cr=p+offsets[6];for(unsigned n=0;n<8;++n){v.colors_[n]=real(cr+4*n);if(!std::isfinite(v.colors_[n])||v.colors_[n]<0||v.colors_[n]>1)return fail(e,"Coin material color");}for(unsigned n=0;n<2;++n){v.modifiers_[n]=scalar(cr+32+8*n);if(!std::isfinite(v.modifiers_[n])||v.modifiers_[n]<0||v.modifiers_[n]>1)return fail(e,"Coin material modifier");}
 if(used.size()!=strings.size())return fail(e,"Coin unowned source string");if(generation_==UINT64_MAX)return fail(e,"Coin data generation exhausted");v.generation_=generation_+1;v.valid_=true;*this=std::move(v);e.clear();return true;
}
}
