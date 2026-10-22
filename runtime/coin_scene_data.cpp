#include "encore/coin_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){auto bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool path(const std::string&s){if(s.empty()||s.front()=='/'||s.find(':')!=std::string::npos)return false;size_t begin=0;for(size_t n=0;n<=s.size();++n)if(n==s.size()||s[n]=='/'){const auto part=s.substr(begin,n-begin);if(part.empty()||part=="."||part=="..")return false;begin=n+1;}return true;}
uint32_t stable(const std::string&path){uint32_t result=2166136261u;for(unsigned char c:std::string("coin-factory:")+path)result=(result^c)*16777619u;return result;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1e6;}
}
const CoinFactoryBinding*CoinSceneData::owner(uint32_t id)const{for(const auto&v:bindings_)if(v.owner==id)return &v;return nullptr;}
bool CoinSceneData::load_file(const char*path,std::string&e){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,128*1024,e)&&load(bytes.data(),bytes.size(),e);}
bool CoinSceneData::load(const uint8_t*p,size_t size,std::string&e){
 if(!p||size<112||size>128*1024)return fail(e,"Coin scene resource size");
 const auto scene=word(p+24);if(std::memcmp(p,"ENCCF001",8)||word(p+8)!=1||word(p+12)!=size||word(p+20)!=1||!scene||scene>65535||word(p+28)||word(p+52)!=3||word(p+56)||word(p+60)||encore::crc32(p+32,size-32)!=word(p+16))return fail(e,"Coin scene header/version/capability/checksum");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return fail(e,"Coin scene source pin");}
 const uint32_t stride[]={32,64,1},limit[]={1,4096,65536};uint32_t offset[3]{},count[3]{};size_t cursor=112;
 for(unsigned n=0;n<3;++n){const auto*d=p+64+16*n;offset[n]=word(d+4);count[n]=word(d+8);if(word(d)!=n+1||word(d+12)!=stride[n]||!count[n]||count[n]>limit[n]||offset[n]%4||offset[n]<cursor||offset[n]>size||count[n]>(size-offset[n])/stride[n])return fail(e,"Coin scene directory/span");for(size_t i=cursor;i<offset[n];++i)if(p[i])return fail(e,"Coin scene nonzero padding");cursor=offset[n]+count[n]*stride[n];}
 if(cursor!=size||count[2]<2)return fail(e,"Coin scene trailing/pool bytes");const auto*meta=p+offset[0];if(word(meta)!=scene||word(meta+12)!=1)return fail(e,"Coin scene coordinate contract");for(unsigned n=0;n<4;++n)if(!std::isfinite(real(meta+16+4*n))||real(meta+16+4*n)!=0)return fail(e,"Coin scene unsupported origin");
 CoinSceneData v;v.scene_=scene;std::map<uint32_t,std::string>strings;std::set<std::string>distinct;const auto*pool=p+offset[2];if(pool[0]||pool[count[2]-1])return fail(e,"Coin scene pool boundary");
 for(uint32_t n=0;n<count[2];){const auto begin=n;std::string value;while(n<count[2]&&pool[n]){const auto c=pool[n++];if(c<32||c>=127||c=='\\'||value.size()>=512)return fail(e,"Coin scene pool text");value+=char(c);}if(n==count[2]||(begin?!path(value):!value.empty())||!distinct.insert(value).second)return fail(e,"Coin scene canonical source path");strings.emplace(begin,std::move(value));++n;}
 std::set<uint32_t>used{0};auto string=[&](uint32_t at,std::string&out){const auto found=strings.find(at);if(found==strings.end()||found->second.empty())return false;used.insert(at);out=found->second;return true;};if(!string(word(meta+4),v.source_)||!string(word(meta+8),v.sink_))return fail(e,"Coin scene metadata strings");
 std::set<uint32_t>ids,owners;std::set<std::string>paths;uint32_t previous=0;
 for(uint32_t n=0;n<count[1];++n){const auto*r=p+offset[1]+64*n;CoinFactoryBinding row;row.id=word(r);row.owner=word(r+4);row.order=word(r+8);row.owner_order=word(r+12);row.default_count=word(r+36);if(!row.id||row.id==UINT32_MAX||!ids.insert(row.id).second||!row.owner||row.owner==UINT32_MAX||!owners.insert(row.owner).second||row.order<=previous||row.order>65535||!row.owner_order||row.owner_order>=row.order||!row.default_count||row.default_count>1024||word(r+44))return fail(e,"Coin scene instance domain");previous=row.order;
  if(!string(word(r+16),row.path)||!string(word(r+20),row.parent)||!string(word(r+24),row.source)||!string(word(r+28),row.owner_path)||!string(word(r+32),row.sink)||!string(word(r+40),row.reparent))return fail(e,"Coin scene instance strings");
  const auto slash=row.path.find_last_of('/');if(!paths.insert(row.path).second||slash==std::string::npos||row.path.substr(0,slash)!=row.parent||row.parent!=row.owner_path||row.sink!=v.sink_||row.id!=stable(row.path))return fail(e,"Coin scene ownership/stable identity");
  row.local={real(r+48),real(r+52)};row.position={real(r+56),real(r+60)};if(!finite(row.local.x)||!finite(row.local.y)||!finite(row.position.x)||!finite(row.position.y))return fail(e,"Coin scene coordinate domain");v.bindings_.push_back(std::move(row));
 }
 if(used.size()!=strings.size())return fail(e,"Coin scene unowned source strings");if(generation_==UINT64_MAX)return fail(e,"Coin scene data generation exhausted");v.generation_=generation_+1;v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool CoinSceneData::compatible(const BreakablesData&b,const CoinsData&c,std::string&e)const{
 if(!valid_||!b.valid()||!c.valid()||scene_!=b.scene()||source_!=b.policy().source||sink_!=c.assets()[5])return fail(e,"Coin scene companion source/scene mismatch");size_t references=0;for(const auto&o:b.owners())if(!o.spawner.empty())++references;if(references!=bindings_.size())return fail(e,"Coin scene incomplete factory companions");
 for(const auto&v:bindings_){const auto*o=b.owner(v.owner);if(!o||o->spawner!=v.path||o->path!=v.owner_path||o->new_parent!=v.reparent||o->content!=UINT32_MAX||v.source!=c.assets()[3]||v.default_count!=c.info().default_coins||float(o->position.x+v.local.x)!=v.position.x||float(o->position.y+v.local.y)!=v.position.y)return fail(e,"Coin scene actual owner/factory binding mismatch");}
 e.clear();return true;
}
}
