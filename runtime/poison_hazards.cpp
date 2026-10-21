#include "encore/poison_hazards.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
int32_t i32(const uint8_t*p){const auto raw=u(p);int32_t v;std::memcpy(&v,&raw,4);return v;}
float f32(const uint8_t*p){const auto raw=u(p);float v;std::memcpy(&v,&raw,4);return v;}
double f64(const uint8_t*p){const uint64_t raw=uint64_t(u(p))|uint64_t(u(p+4))<<32;double v;std::memcpy(&v,&raw,8);return v;}
Vec2 vec(const uint8_t*p){return{f32(p),f32(p+4)};}
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1e6&&std::abs(v.y)<=1e6;}
bool path(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t start=0;
 for(size_t at=0;at<=s.size();++at){
  if(at<s.size()&&(static_cast<unsigned char>(s[at])<32||static_cast<unsigned char>(s[at])>=127||s[at]==':'||s[at]=='\\'))return false;
  if(at==s.size()||s[at]=='/'){const auto part=s.substr(start,at-start);if(part.empty()||part=="."||part=="..")return false;start=at+1;}
 }return true;
}
double cross(Vec2 a,Vec2 b,Vec2 c){return(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);}
bool simple(const std::vector<Vec2>&points,uint32_t first,uint32_t count){
 for(uint32_t i=0;i<count;++i){const auto a=points[first+i],b=points[first+(i+1)%count];
  for(uint32_t j=i+1;j<count;++j){if(j==i+1||(i==0&&j==count-1))continue;const auto c=points[first+j],d=points[first+(j+1)%count];
   const bool boxes=std::max(std::min(a.x,b.x),std::min(c.x,d.x))<=std::min(std::max(a.x,b.x),std::max(c.x,d.x))&&std::max(std::min(a.y,b.y),std::min(c.y,d.y))<=std::min(std::max(a.y,b.y),std::max(c.y,d.y));
   if(boxes&&cross(a,b,c)*cross(a,b,d)<=0&&cross(c,d,a)*cross(c,d,b)<=0)return false;
  }
 }return true;
}
}
struct PoisonHazardsData::State {PoisonPolicy policy;std::vector<PoisonHole>holes;std::vector<PoisonArea>areas;std::vector<PoisonShape>shapes;std::vector<Vec2>points;};
const PoisonPolicy&PoisonHazardsData::policy()const{static const PoisonPolicy empty;return state_?state_->policy:empty;}
const std::vector<PoisonHole>&PoisonHazardsData::holes()const{static const std::vector<PoisonHole>empty;return state_?state_->holes:empty;}
const std::vector<PoisonArea>&PoisonHazardsData::areas()const{static const std::vector<PoisonArea>empty;return state_?state_->areas:empty;}
const std::vector<PoisonShape>&PoisonHazardsData::shapes()const{static const std::vector<PoisonShape>empty;return state_?state_->shapes:empty;}
const std::vector<Vec2>&PoisonHazardsData::points()const{static const std::vector<Vec2>empty;return state_?state_->points:empty;}
bool PoisonHazardsData::load_file(const char*name,std::string&e){
 if(valid()||!name||!*name)return bad(e,"Poison fresh owner/path required");std::vector<uint8_t>bytes;return encore::read_file(name,bytes,262144,e)&&load(bytes.data(),bytes.size(),e);
}
bool PoisonHazardsData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<160||size>262144)return bad(e,"Poison size/fresh owner rejected");
 if(std::memcmp(p,"ENCPSN01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||!u(p+24)||u(p+24)>65535||u(p+28)||u(p+52)!=6||u(p+56)||u(p+60))return bad(e,"Poison format/capability/source scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Poison source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return bad(e,"Poison checksum rejected");
 const uint32_t strides[]={1,64,48,40,32,8},limits[]={65536,1,128,128,256,16384};uint32_t off[6]{},count[6]{};size_t cursor=160;
 for(unsigned n=0;n<6;++n){const auto q=p+64+n*16;off[n]=u(q+4);count[n]=u(q+8);
  if(u(q)!=n+1||u(q+12)!=strides[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/strides[n])return bad(e,"Poison directory span/stride/count rejected");
  for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Poison nonzero padding rejected");cursor=off[n]+size_t(count[n])*strides[n];
 }if(cursor!=size||count[4]!=2*count[3]||count[5]<6)return bad(e,"Poison shape coverage/trailing data rejected");
 auto next=std::make_shared<State>();std::vector<uint8_t>used(count[0]);
 auto text=[&](const uint8_t*q,std::string&out){const auto first=u(q),length=u(q+4);if(!length||length>512||first>count[0]||length>count[0]-first)return false;
  for(uint32_t n=first;n<first+length;++n)if(used[n])return false;out.assign(reinterpret_cast<const char*>(p+off[0]+first),length);if(!path(out))return false;std::fill(used.begin()+first,used.begin()+first+length,1);return true;
 };
 auto&q=next->policy;const auto policy=p+off[1];q.scene=u(p+24);
 if(!text(policy,q.source)||!text(policy+8,q.name)||!text(policy+16,q.status)||q.source.rfind("Maps/",0)||q.source.size()<5||q.source.substr(q.source.size()-5)!=".tscn"||q.status.size()>64||q.status.front()<'a'||q.status.front()>'z')return bad(e,"Poison policy source/status rejected");
 for(char c:q.status)if((c<'a'||c>'z')&&c!='_')return bad(e,"Poison status identity rejected");
 q.area_damage=i32(policy+24);q.area_variance=i32(policy+28);q.geyser_damage=i32(policy+32);q.geyser_variance=i32(policy+36);q.hit=vec(policy+40);q.spawn_offset=vec(policy+48);
 for(unsigned n=0;n<4;++n)if(i32(policy+24+n*4)<0||i32(policy+24+n*4)>1000000)return bad(e,"Poison damage policy rejected");
 if(!finite(q.hit)||!finite(q.spawn_offset)||u(policy+56)||u(policy+60))return bad(e,"Poison policy vector/reserved rejected");std::set<uint32_t>ids;std::set<std::string>paths;
 for(uint32_t n=0;n<count[2];++n){const auto raw=p+off[2]+size_t(n)*48;PoisonHole hole;hole.id=u(raw);hole.position=vec(raw+12);hole.interval=f64(raw+20);hole.delay=f64(raw+28);
  if(!hole.id||hole.id==UINT32_MAX||!ids.insert(hole.id).second||!text(raw+4,hole.path)||!paths.insert(hole.path).second||!finite(hole.position)||!std::isfinite(hole.interval)||hole.interval<.001||hole.interval>3600||!std::isfinite(hole.delay)||hole.delay<0||hole.delay>3600||(hole.delay!=0&&float(hole.delay)<=0)||u(raw+36)||u(raw+40)!=1||u(raw+44))return bad(e,"Poison hole identity/Timer mode rejected");next->holes.push_back(std::move(hole));
 }
 for(uint32_t n=0;n<count[3];++n){const auto raw=p+off[3]+size_t(n)*40;PoisonArea area;area.id=u(raw);area.step=u(raw+12);area.layer=u(raw+16);area.mask=u(raw+20);area.bits=u(raw+24);area.first=u(raw+28);area.count=u(raw+32);
  if(!area.id||area.id==UINT32_MAX||!ids.insert(area.id).second||!text(raw+4,area.path)||!paths.insert(area.path).second||!area.step||area.step==UINT32_MAX||area.layer>0xfffff||area.mask>0xfffff||area.bits>3||area.first!=2*n||area.count!=2||u(raw+36))return bad(e,"Poison area identity/filter/shape owner rejected");next->areas.push_back(std::move(area));
 }
 for(uint32_t n=0;n<count[5];++n){auto v=vec(p+off[5]+size_t(n)*8);if(!finite(v))return bad(e,"Poison polygon finite point rejected");next->points.push_back(v);}
 uint32_t first=0;
 for(uint32_t n=0;n<count[4];++n){const auto raw=p+off[4]+size_t(n)*32;PoisonShape shape;shape.owner=u(raw);shape.role=u(raw+4);shape.first=u(raw+8);shape.count=u(raw+12);shape.position=vec(raw+16);shape.layer=u(raw+24);shape.mask=u(raw+28);
  if(shape.owner!=n/2||shape.role!=n%2+1||shape.first!=first||shape.count<3||shape.count>128||first>count[5]||shape.count>count[5]-first||!finite(shape.position)||shape.layer>0xfffff||shape.mask>0xfffff)return bad(e,"Poison polygon span/ownership/filter rejected");
  const auto&area=next->areas[shape.owner];if(shape.role==1&&(shape.layer!=area.layer||shape.mask!=area.mask))return bad(e,"Poison trigger filter differs");double signed_area=0;
  for(uint32_t j=0;j<shape.count;++j){const auto a=next->points[first+j],b=next->points[first+(j+1)%shape.count];signed_area+=double(a.x)*b.y-double(b.x)*a.y;
   for(uint32_t k=0;k<j;++k){const auto prior=next->points[first+k];if(a.x==prior.x&&a.y==prior.y)return bad(e,"Poison repeated polygon point rejected");}
  }if(signed_area==0||!simple(next->points,first,shape.count))return bad(e,"Poison degenerate/self-intersecting polygon rejected");first+=shape.count;next->shapes.push_back(shape);
 }
 if(first!=count[5]||std::find(used.begin(),used.end(),uint8_t(0))!=used.end())return bad(e,"Poison unowned points/text rejected");state_=std::move(next);e.clear();return true;
}
bool PoisonHazardsData::matches(FieldStepsView steps,std::string&e)const{
 if(!valid()||!steps.valid()||steps.capability()!=2||steps.scene_id()!=policy().scene)return bad(e,"Poison stepping scene/capability rejected");
 for(const auto&area:areas()){unsigned found=0;for(uint32_t n=0;n<steps.area_count();++n)found+=steps.area_id(n)==area.step;if(found!=1)return bad(e,"Poison stepping target rejected");}e.clear();return true;
}
bool apply_poison_callback(const PoisonHazardsData&data,PoisonContactKind kind,uint32_t owner,bool entered,FieldStepBody body,bool paused,PoisonDamageState&damage,FieldRunLoop&run,FieldStepShadowState*shadow,std::string&e){
 if(!data.matches(run.content(),e))return false;
 if((kind!=PoisonContactKind::Area&&kind!=PoisonContactKind::Geyser)||(body!=FieldStepBody::Player&&body!=FieldStepBody::Party&&body!=FieldStepBody::Other))return bad(e,"Poison callback kind/body rejected");
 const PoisonArea*area=nullptr;if(kind==PoisonContactKind::Area){for(const auto&row:data.areas())if(row.id==owner)area=&row;if(!area)return bad(e,"Poison callback source area rejected");}else if(owner)return bad(e,"Poison manual geyser callback owner rejected");
 if(body==FieldStepBody::Other||(entered&&paused)){e.clear();return true;}
 auto next=damage;auto next_run=run;FieldStepShadowState next_shadow;if(shadow)next_shadow=*shadow;
 if(entered){if(next.status_requests.size()>=256)return bad(e,"Poison status request capacity rejected");next.continuous=true;next.damage=area?data.policy().area_damage:data.policy().geyser_damage;next.variance=area?data.policy().area_variance:data.policy().geyser_variance;next.status_requests.push_back(data.policy().status);}else next.continuous=false;
 if(area){uint32_t index=0;while(index<run.content().area_count()&&run.content().area_id(index)!=area->step)++index;
  if(!apply_step_callback(next_run,shadow?&next_shadow:nullptr,index,entered,body,e))return false;
 }damage=std::move(next);run=std::move(next_run);if(shadow)*shadow=next_shadow;e.clear();return true;
}
size_t PoisonHoleTimers::find(uint32_t id)const{for(size_t n=0;n<data_.holes().size();++n)if(data_.holes()[n].id==id)return n;return SIZE_MAX;}
bool PoisonHoleTimers::prepare(const PoisonHazardsData&data,uint64_t epoch,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_)return bad(e,"Poison hole fresh source/epoch rejected");PoisonHoleTimers next;next.data_=data;next.epoch_=next.last_epoch_=epoch;
 for(const auto&hole:data.holes()){const auto wait=float(hole.delay!=0?hole.delay:hole.interval);next.clocks_.push_back({double(wait),wait,false});}*this=std::move(next);e.clear();return true;
}
bool PoisonHoleTimers::pause(uint64_t epoch,uint32_t id,bool value,std::string&e){const auto index=find(id);if(!valid()||epoch!=epoch_||index==SIZE_MAX)return bad(e,"Poison hole pause owner/epoch rejected");clocks_[index].paused=value;e.clear();return true;}
bool PoisonHoleTimers::inspect(uint64_t epoch,uint32_t id,PoisonHoleClock&out,std::string&e)const{const auto index=find(id);if(!valid()||epoch!=epoch_||index==SIZE_MAX)return bad(e,"Poison hole observation owner/epoch rejected");out=clocks_[index];e.clear();return true;}
bool PoisonHoleTimers::advance_idle(uint64_t epoch,double delta,bool process,std::string&e){
 if(!valid()||epoch!=epoch_||!std::isfinite(delta)||delta<0||delta>3600)return bad(e,"Poison hole clock epoch/delta rejected");if(!process){e.clear();return true;}
 auto next=*this;
 for(size_t n=0;n<next.clocks_.size();++n)if(!next.advance_one(n,delta,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool PoisonHoleTimers::advance_one(size_t n,double delta,std::string&e){
 auto&clock=clocks_[n];if(clock.paused)return true;clock.remaining-=delta;if(!std::isfinite(clock.remaining)||clock.remaining< -1e9)return bad(e,"Poison hole remaining clock capacity rejected");
  if(clock.remaining<0){
   if(requests_.size()>=4096||ticket_==UINT64_MAX)return bad(e,"Poison spawn queue/ticket capacity rejected");
   // Native Timer rearms using its OLD binary32 wait before emitting timeout.
   // The original handler changes wait_time only after that rearm.
   clock.remaining+=clock.wait;clock.wait=float(data_.holes()[n].interval);const auto&hole=data_.holes()[n];const auto offset=data_.policy().spawn_offset;
   requests_.push_back({epoch_,++ticket_,hole.id,{hole.position.x+offset.x,hole.position.y+offset.y}});
  }
 return true;
}
bool PoisonHoleTimers::advance_hole_idle(uint64_t epoch,uint32_t id,double delta,bool process,std::string&e){
 const auto n=find(id);if(!valid()||epoch!=epoch_||n==SIZE_MAX||!std::isfinite(delta)||delta<0||delta>3600)return bad(e,"Poison selected Timer owner/epoch/delta rejected");if(!process){e.clear();return true;}auto next=*this;if(!next.advance_one(n,delta,e))return false;*this=std::move(next);e.clear();return true;
}
bool PoisonHoleTimers::retire(uint64_t epoch,std::string&e){if(!valid()||epoch!=epoch_)return bad(e,"Poison hole retirement epoch rejected");data_={};epoch_=0;clocks_.clear();requests_.clear();e.clear();return true;}
}
