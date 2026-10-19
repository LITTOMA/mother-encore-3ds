#include "encore/prop_clips.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto bits=u32(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*s){e=s;return false;}
}
const PropClip*PropClipsData::clip(uint32_t id)const{for(const auto&c:clips_)if(c.id==id)return &c;return nullptr;}
bool PropClipsData::load_file(const char*path,std::string&e){std::vector<uint8_t>b;return encore::read_file(path,b,64*1024,e)&&load(b.data(),b.size(),e);}
bool PropClipsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<112||n>64*1024)return fail(e,"Prop clips size rejected");
 if(std::memcmp(p,"ENCPCL01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)||u32(p+28)||u32(p+52)!=3||u32(p+56)||u32(p+60))return fail(e,"Prop clips schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Prop clips source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u32(p+16))return fail(e,"Prop clips checksum rejected");
 const uint32_t strides[]={16,24,12},limits[]={1,32,4096};uint32_t offsets[3],counts[3];size_t cursor=112;
 for(unsigned i=0;i<3;++i){const auto*d=p+64+i*16;offsets[i]=u32(d+4);counts[i]=u32(d+8);
  if(u32(d)!=i+1||u32(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<cursor||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return fail(e,"Prop clips directory/span rejected");
  for(size_t k=cursor;k<offsets[i];++k)if(p[k])return fail(e,"Prop clips padding rejected");
  cursor=offsets[i]+counts[i]*strides[i];
 }
 if(cursor!=n)return fail(e,"Prop clips trailing bytes rejected");
 PropClipsData v;const auto*policy=p+offsets[0];v.frames_=u32(policy);v.initial_.frame=u32(policy+4);const auto flags=u32(policy+8);
 if(!v.frames_||v.frames_>65535||v.initial_.frame>=v.frames_||flags>15||u32(policy+12))return fail(e,"Prop clips initial state rejected");
 v.initial_.visible=flags&1;v.initial_.body=flags&2;v.initial_.hitbox=flags&4;v.initial_.interaction=flags&8;
 for(uint32_t i=0;i<counts[2];++i){const auto*k=p+offsets[2]+i*12;const auto kind=u32(k+4),value=u32(k+8);const auto time=real(k);
  if(!std::isfinite(time)||time<0||kind<1||kind>5||(kind==1?value>=v.frames_:value>1))return fail(e,"Prop key time/track/value rejected");
  v.keys_.push_back({time,PropTrack(kind),value});
 }
 std::set<uint32_t>ids;uint32_t first=0;
 for(uint32_t i=0;i<counts[1];++i){const auto*c=p+offsets[1]+i*24;PropClip row{u32(c),real(c+4),u32(c+8),u32(c+12)};
  if(!row.id||row.id>65535||!ids.insert(row.id).second||!std::isfinite(row.duration)||row.duration<=0||row.duration>60||row.first!=first||!row.count||row.count>counts[2]-first||u32(c+16)||u32(c+20))return fail(e,"Prop clip identity/duration/span rejected");
  float time=-1;uint32_t track=0;
  for(uint32_t j=0;j<row.count;++j){const auto&key=v.keys_[row.first+j];const auto kind=uint32_t(key.track);
   if(key.time>row.duration||key.time<time||(key.time==time&&kind<=track))return fail(e,"Prop clip key order/range rejected");
   time=key.time;track=kind;
  }
  first+=row.count;v.clips_.push_back(row);
 }
 if(first!=counts[2])return fail(e,"Prop clip keys are unowned");
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool PropClipPlayer::initialize(const PropClipsData&d,std::string&e){if(!d.valid())return fail(e,"Prop player needs checked data");PropClipPlayer v;v.data_=d;v.pose_=d.initial();*this=std::move(v);e.clear();return true;}
bool PropClipPlayer::play(uint32_t id,std::string&e){if(!data_.valid()||!data_.clip(id))return fail(e,"Prop player clip is unbound");id_=id;time_=0;key_=0;playing_=true;e.clear();return true;}
bool PropClipPlayer::advance(double delta,std::string&e){
 if(!data_.valid()||!std::isfinite(delta)||delta<0||delta>60)return fail(e,"Prop player delta rejected");
 if(!playing_){e.clear();return true;}
 const auto*c=data_.clip(id_);if(!c)return fail(e,"Prop player lost clip");
 const double next=std::min(double(c->duration),time_+delta);const bool done=next>=c->duration;
 if(done&&completed_.size()>=64)return fail(e,"Prop completion queue capacity");
 for(;key_<c->count;++key_){const auto&key=data_.keys()[c->first+key_];if(key.time>next)break;
  switch(key.track){case PropTrack::Frame:pose_.frame=key.value;break;case PropTrack::Visible:pose_.visible=key.value!=0;break;case PropTrack::BodyDisabled:pose_.body=!key.value;break;case PropTrack::HitboxDisabled:pose_.hitbox=!key.value;break;case PropTrack::InteractionDisabled:pose_.interaction=!key.value;break;}
 }
 time_=next;if(done){playing_=false;completed_.push_back(id_);}e.clear();return true;
}
}
