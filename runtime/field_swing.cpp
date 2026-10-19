#include "encore/field_swing.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){uint32_t bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s){return !s.empty()&&s.size()<=1024&&s[0]!='/'&&s.find(':')==std::string::npos&&s.find('\\')==std::string::npos&&s.find("..") ==std::string::npos&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32&&c<127;});}
}
const SwingClip*FieldSwingData::clip(uint32_t id)const{for(const auto&c:clips_)if(c.id==id)return &c;return nullptr;}
const SwingClip*FieldSwingData::direction(Vec2 v)const{
 if(!valid_||!std::isfinite(v.x)||!std::isfinite(v.y)||std::abs(v.x)>1||std::abs(v.y)>1||(v.x==0&&v.y==0))return nullptr;
 const SwingClip*result=nullptr;float best=INFINITY;for(const auto&c:clips_){const float x=c.direction.x-v.x,y=c.direction.y-v.y,d=x*x+y*y;if(d<best){best=d;result=&c;}}return result;
}
bool FieldSwingData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,64*1024,e)&&load(b.data(),b.size(),e);}
bool FieldSwingData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<192||n>64*1024)return fail(e,"Swing resource size rejected");
 if(std::memcmp(p,"ENCSWG01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||word(p+24)||word(p+28)||word(p+52)!=4||word(p+56)||word(p+60))return fail(e,"Swing schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};for(unsigned i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Swing source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Swing checksum rejected");
 const uint32_t stride[]={128,24,16,1},limit[]={1,8,2048,4096};uint32_t offset[4],count[4];size_t at=128;
 for(unsigned i=0;i<4;++i){const auto*d=p+64+i*16;offset[i]=word(d+4);count[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=stride[i]||!count[i]||count[i]>limit[i]||offset[i]%4||offset[i]<at||offset[i]>n||count[i]>(n-offset[i])/stride[i])return fail(e,"Swing directory/span rejected");
  for(size_t j=at;j<offset[i];++j)if(p[j])return fail(e,"Swing padding rejected");
  at=offset[i]+count[i]*stride[i];
 }
 if(at!=n||count[1]!=8)return fail(e,"Swing trailing bytes/directions rejected");
 FieldSwingData v;const auto*b=p+offset[0];const auto frames=word(b),initial=word(b+4),flags=word(b+8);
 if(!frames||frames>65535||initial>=frames||flags>15||word(b+12)||word(b+124))return fail(e,"Swing initial policy rejected");
 for(unsigned i=0;i<19;++i){const float x=real(b+16+i*4);if(!std::isfinite(x)||std::abs(x)>100000)return fail(e,"Swing finite policy rejected");v.policy_[i]=x;}
 if(v.policy_[0]<=0||v.policy_[0]>4||v.policy_[1]<=0||v.policy_[1]>4||v.policy_[13]<=0||v.policy_[14]<=0)return fail(e,"Swing speed/capsule rejected");
 for(unsigned i=15;i<19;++i)if(v.policy_[i]<0||v.policy_[i]>10)return fail(e,"Swing vibration rejected");
 uint32_t previous=0;for(unsigned i=0;i<4;++i){auto start=word(b+92+i*8),length=word(b+96+i*8);if(start!=previous||!length||length>count[3]-start)return fail(e,"Swing string span rejected");v.strings_[i].assign(reinterpret_cast<const char*>(p+offset[3]+start),length);if(!path(v.strings_[i]))return fail(e,"Swing string identity rejected");previous+=length;}
 if(previous!=count[3])return fail(e,"Swing unowned strings rejected");
 v.initial_={initial,v.policy_[6]*float(180.0/3.14159265358979323846),bool(flags&1),bool(flags&2),bool(flags&4),bool(flags&8)};
 for(uint32_t i=0;i<count[2];++i){const auto*k=p+offset[2]+i*16;const float time=real(k),value=real(k+8);const auto kind=word(k+4),order=word(k+12);
  if(!std::isfinite(time)||time<0||!std::isfinite(value)||kind<1||kind>8||order>=8||(kind==1?(value<0||value>=frames||std::floor(value)!=value):kind==2?std::abs(value)>360:kind==6||kind==8?value!=0:(value!=0&&value!=1)))return fail(e,"Swing key type/value rejected");
  v.keys_.push_back({time,SwingTrack(kind),value,order});
 }
 uint32_t first=0;std::set<uint32_t>ids;std::set<std::pair<float,float>>directions;
 for(uint32_t i=0;i<count[1];++i){const auto*c=p+offset[1]+i*24;SwingClip row{word(c),{real(c+4),real(c+8)},real(c+12),word(c+16),word(c+20)};
  if(!row.id||row.id>65535||!ids.insert(row.id).second||!std::isfinite(row.direction.x)||!std::isfinite(row.direction.y)||std::abs(row.direction.x)>1||std::abs(row.direction.y)>1||(row.direction.x==0&&row.direction.y==0)||!directions.insert({row.direction.x,row.direction.y}).second||!std::isfinite(row.duration)||row.duration<=0||row.duration>10||row.first!=first||!row.count||row.count>count[2]-first)return fail(e,"Swing clip identity/direction/span rejected");
  float time=-1;uint32_t order=0,mask=0,callbacks=0;std::array<int32_t,8>orders;orders.fill(-1);std::set<std::pair<float,uint32_t>>seen;
  for(uint32_t j=0;j<row.count;++j){const auto&k=v.keys_[first+j];auto kind=uint32_t(k.kind);
   if(k.time>row.duration||k.time<time||(k.time==time&&k.order<=order)||!seen.insert({k.time,kind}).second||(orders[kind-1]>=0&&orders[kind-1]!=int32_t(k.order)))return fail(e,"Swing key order/ownership rejected");
   if(k.kind==SwingTrack::Finished){if(k.time!=row.duration)return fail(e,"Swing deferred finish time rejected");++callbacks;}
   orders[kind-1]=int32_t(k.order);mask|=1u<<(kind-1);time=k.time;order=k.order;
  }
  std::set<int32_t>used(orders.begin(),orders.end());if(mask!=255||callbacks!=1||used.size()!=8)return fail(e,"Swing typed tracks incomplete");
  first+=row.count;v.clips_.push_back(row);
 }
 if(first!=count[2])return fail(e,"Swing unowned keys rejected");
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool FieldSwingPlayer::initialize(const FieldSwingData&d,std::string&e){if(!d.valid())return fail(e,"Swing player needs checked data");FieldSwingPlayer v;v.data_=d;v.pose_=d.initial();*this=std::move(v);e.clear();return true;}
bool FieldSwingPlayer::play(uint32_t id,bool incap,std::string&e){if(!data_.valid()||!data_.clip(id))return fail(e,"Swing animation is not bound");id_=id;time_=0;key_=0;rate_=data_.policy()[incap?1:0];playing_=attack_=true;e.clear();return true;}
bool FieldSwingPlayer::advance(double delta,std::string&e){
 if(!data_.valid()||!std::isfinite(delta)||delta<0||delta>60)return fail(e,"Swing delta rejected");
 if(!playing_){e.clear();return true;}
 const auto*c=data_.clip(id_);if(!c)return fail(e,"Swing clip lost");const float next=std::min(c->duration,time_+float(delta)*rate_);const bool done=next>=c->duration;
 if(done&&pending_>=64)return fail(e,"Swing callback capacity");
 for(;key_<c->count;++key_){const auto&k=data_.keys()[c->first+key_];if(k.time>next||(!done&&next!=time_&&k.time==next))break;
  switch(k.kind){case SwingTrack::Frame:pose_.frame=uint32_t(k.value);break;case SwingTrack::PivotDegrees:pose_.pivot=k.value;break;case SwingTrack::HitboxDisabled:pose_.hitbox=!bool(k.value);break;case SwingTrack::MainVisible:pose_.main=bool(k.value);break;case SwingTrack::SpecialVisible:pose_.special=bool(k.value);break;case SwingTrack::Texture:break;case SwingTrack::BatBehind:pose_.behind=bool(k.value);break;case SwingTrack::Finished:++pending_;break;}
 }
 time_=next;if(done)playing_=false;e.clear();return true;
}
bool FieldSwingPlayer::flush(std::string&e){if(!data_.valid())return fail(e,"Swing player is not initialized");if(pending_){pose_.hitbox=false;attack_=false;pending_=0;}e.clear();return true;}
}
