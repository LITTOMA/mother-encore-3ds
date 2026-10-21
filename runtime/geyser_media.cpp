#include "encore/geyser_media.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1e6;}
}
struct GeyserMediaData::State {uint32_t rate=0,capacity=0;GeyserTimelineData timeline;AudioBank bank;};
uint32_t GeyserMediaData::sample_rate()const{return state_?state_->rate:0;}
uint32_t GeyserMediaData::capacity()const{return state_?state_->capacity:0;}
const GeyserTimelineData&GeyserMediaData::timeline()const{static const GeyserTimelineData empty;return state_?state_->timeline:empty;}
const AudioBank&GeyserMediaData::bank()const{static const AudioBank empty;return state_?state_->bank:empty;}
bool GeyserMediaData::load_file(const char*path,std::string&e){if(valid()||!path||!*path)return bad(e,"Geyser media fresh owner/path required");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,262144,e)&&load(bytes.data(),bytes.size(),e);}
bool GeyserMediaData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<128||size>262144)return bad(e,"Geyser media fresh owner/size rejected");
 if(std::memcmp(p,"ENCGMD01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||u(p+24)||u(p+28)||u(p+52)!=4||u(p+56)||u(p+60))return bad(e,"Geyser media format/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Geyser media source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return bad(e,"Geyser media checksum rejected");const uint32_t strides[]={32,1,1,36},limits[]={1,65536,65536,64};uint32_t off[4]{},count[4]{};size_t cursor=128;
 for(unsigned n=0;n<4;++n){const auto q=p+64+16*n;off[n]=u(q+4);count[n]=u(q+8);if(u(q)!=n+1||u(q+12)!=strides[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/strides[n])return bad(e,"Geyser media directory bounds/stride rejected");for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Geyser media padding rejected");cursor=off[n]+size_t(count[n])*strides[n];}
 if(cursor!=size||count[1]<160||count[2]<64)return bad(e,"Geyser media exact coverage rejected");auto next=std::make_shared<State>();next->rate=u(p+off[0]);next->capacity=u(p+off[0]+4);if(next->rate<8000||next->rate>48000||!next->capacity||next->capacity>128)return bad(e,"Geyser media rate/capacity rejected");for(unsigned n=2;n<8;++n)if(u(p+off[0]+4*n))return bad(e,"Geyser media reserved policy rejected");if(!next->timeline.load(p+off[1],count[1],e)||!next->bank.load(p+off[2],count[2],e))return false;
 const auto&sounds=next->timeline.sounds();if(count[3]!=sounds.size()||next->bank.count()!=sounds.size())return bad(e,"Geyser media join coverage rejected");std::set<uint32_t>ids;size_t total=0;
 for(uint32_t n=0;n<count[3];++n){const auto q=p+off[3]+36*n;const auto id=u(q);std::array<uint8_t,32>digest;std::copy(q+4,q+36,digest.begin());const auto&s=sounds[n];AudioAsset a;const std::string name="res://"+s.source;
  if(id!=s.id||!ids.insert(id).second||std::all_of(digest.begin(),digest.end(),[](uint8_t c){return c==0;})||!next->bank.find_source(name,digest,a)||a.stable_id!=id||a.loops()||a.loop_start||a.sample_rate!=next->rate||a.gain_db!=0||!a.pcm_bytes||a.pcm_bytes>1024*1024||total>8*1024*1024-a.pcm_bytes)return bad(e,"Geyser media source digest/identity/PCM format rejected");total+=a.pcm_bytes;
 }state_=std::move(next);e.clear();return true;
}
bool geyser_spatial_gain(const GeyserPolicy&p,Vec2 source,GeyserListener listener,float db,GeyserStereoGain&out,std::string&e){
 if(!finite(source.x)||!finite(source.y)||!finite(listener.center.x)||!finite(listener.center.y)||!finite(listener.width)||listener.width<=0||!finite(listener.global_panning)||listener.global_panning<0||listener.global_panning>4||!finite(db)||db< -120||db>24||!finite(p.max_distance)||p.max_distance<=0||!finite(p.attenuation)||p.attenuation<=0||!finite(p.panning)||p.panning<0||p.panning>4)return bad(e,"Geyser unsupported spatial listener/source policy");
 const float x=source.x-listener.center.x,y=source.y-listener.center.y;const float dist=std::sqrt(x*x+y*y);GeyserStereoGain next;
 if(dist<=p.max_distance){float multiplier=std::pow(1.f-dist/p.max_distance,p.attenuation);multiplier*=audio_linear_gain(db);float pan=std::max(-1.f,std::min(1.f,x/listener.width));pan*=p.panning*listener.global_panning*.5f;pan=std::max(0.f,std::min(1.f,pan+.5f));next.left=(1.f-pan)*multiplier;next.right=pan*multiplier;}
 out=next;e.clear();return true;
}
}
