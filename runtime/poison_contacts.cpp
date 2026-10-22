#include "encore/poison_contacts.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float scalar(const uint8_t*p){const auto bits=word(p);float value;std::memcpy(&value,&bits,4);return value;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e6f&&std::abs(p.y)<=1e6f;}
bool safe(const std::string&s){if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t first=0;for(size_t n=0;n<=s.size();++n){if(n<s.size()&&(static_cast<unsigned char>(s[n])<32||static_cast<unsigned char>(s[n])>=127||s[n]==':'||s[n]=='\\'))return false;if(n==s.size()||s[n]=='/'){const auto part=s.substr(first,n-first);if(part.empty()||part=="."||part=="..")return false;first=n+1;}}return true;}
double cross(Vec2 a,Vec2 b,Vec2 c){return(double(b.x)-a.x)*(double(c.y)-b.y)-(double(b.y)-a.y)*(double(c.x)-b.x);}
double area2(const Vec2*p,size_t n){double result=0;for(size_t i=0;i<n;++i){const auto a=p[i],b=p[(i+1)%n];result+=double(a.x)*b.y-double(a.y)*b.x;}return std::abs(result);}
bool convex(const Vec2*p,size_t n){
 if(!p||n<3||n>64)return false;bool positive=false,negative=false;
 for(size_t i=0;i<n;++i){if(!finite(p[i]))return false;for(size_t j=0;j<i;++j)if(p[i].x==p[j].x&&p[i].y==p[j].y)return false;const auto value=cross(p[i],p[(i+1)%n],p[(i+2)%n]);positive|=value>0;negative|=value<0;}
 return area2(p,n)>0&&!(positive&&negative);
}
// Ideal SAT with inclusive touching,matching the existing stable convex-query
// profile. This is not a reproduction of a physics server's margin/latency.
bool overlaps(const Vec2*a,size_t na,const Vec2*b,size_t nb){
 auto separated=[&](const Vec2*edges,size_t n){for(size_t i=0;i<n;++i){const auto first=edges[i],last=edges[(i+1)%n];const double x=double(last.y)-first.y,y=double(first.x)-last.x;double amin=0,amax=0,bmin=0,bmax=0;for(size_t j=0;j<na;++j){const double v=double(a[j].x)*x+double(a[j].y)*y;if(!j)amin=amax=v;else{amin=std::min(amin,v);amax=std::max(amax,v);}}for(size_t j=0;j<nb;++j){const double v=double(b[j].x)*x+double(b[j].y)*y;if(!j)bmin=bmax=v;else{bmin=std::min(bmin,v);bmax=std::max(bmax,v);}}if(amax<bmin||bmax<amin)return true;}return false;};
 return !separated(a,na)&&!separated(b,nb);
}
}
struct PoisonContactsData::State {PoisonHazardsData poison;std::string source,player;uint32_t layer=0,mask=0;std::vector<Vec2>body,points;std::vector<PoisonContactArea>areas;std::vector<PoisonContactPart>parts;};
const PoisonHazardsData&PoisonContactsData::poison()const{static const PoisonHazardsData empty;return state_?state_->poison:empty;}
const std::vector<Vec2>&PoisonContactsData::body()const{static const std::vector<Vec2>empty;return state_?state_->body:empty;}
const std::vector<Vec2>&PoisonContactsData::points()const{static const std::vector<Vec2>empty;return state_?state_->points:empty;}
const std::vector<PoisonContactArea>&PoisonContactsData::areas()const{static const std::vector<PoisonContactArea>empty;return state_?state_->areas:empty;}
const std::vector<PoisonContactPart>&PoisonContactsData::parts()const{static const std::vector<PoisonContactPart>empty;return state_?state_->parts:empty;}
uint32_t PoisonContactsData::layer()const{return state_?state_->layer:0;}uint32_t PoisonContactsData::mask()const{return state_?state_->mask:0;}
bool PoisonContactsData::load_file(const char*path,std::string&e){if(valid()||!path||!*path)return bad(e,"Poison contacts fresh owner/path rejected");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,1048576,e)&&load(bytes.data(),bytes.size(),e);}
bool PoisonContactsData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<160||size>1048576)return bad(e,"Poison contacts fresh owner/size rejected");if(std::memcmp(p,"ENCPCN01",8)||word(p+8)!=1||word(p+12)!=size||word(p+20)!=1||!word(p+24)||word(p+24)>65535||word(p+28)||word(p+52)!=6||word(p+56)||word(p+60))return bad(e,"Poison contacts format/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Poison contacts source pin rejected");}if(encore::crc32(p+32,size-32)!=word(p+16))return bad(e,"Poison contacts checksum rejected");
 const uint32_t strides[]={1,32,1,16,16,8},limits[]={1536,1,262144,128,4096,16448};uint32_t off[6]{},count[6]{};size_t cursor=160;
 for(unsigned n=0;n<6;++n){const auto q=p+64+16*n;off[n]=word(q+4);count[n]=word(q+8);if(word(q)!=n+1||word(q+12)!=strides[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/strides[n])return bad(e,"Poison contacts section span/count/stride rejected");for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Poison contacts padding rejected");cursor=off[n]+size_t(count[n])*strides[n];}if(cursor!=size)return bad(e,"Poison contacts exact section coverage rejected");
 auto next=std::make_shared<State>();std::vector<uint8_t>used(count[0]);auto text=[&](const uint8_t*q,std::string&out){const auto first=word(q),length=word(q+4);if(!length||length>512||first>count[0]||length>count[0]-first)return false;for(uint32_t n=first;n<first+length;++n)if(used[n])return false;out.assign(reinterpret_cast<const char*>(p+off[0]+first),length);if(!safe(out))return false;std::fill(used.begin()+first,used.begin()+first+length,1);return true;};const auto q=p+off[1];const auto body_count=word(q+28);
 if(!text(q,next->source)||!text(q+8,next->player)||std::find(used.begin(),used.end(),uint8_t(0))!=used.end()||word(q+16)>0xfffff||word(q+20)>0xfffff||word(q+24)||body_count<3||body_count>64||body_count>count[5])return bad(e,"Poison contacts string/body ownership rejected");next->layer=word(q+16);next->mask=word(q+20);
 if(!next->poison.load(p+off[2],count[2],e))return false;if(next->poison.policy().scene!=word(p+24)||next->poison.policy().source!=next->source||count[3]!=next->poison.areas().size())return bad(e,"Poison contacts nested source/scene/area mismatch");
 for(uint32_t n=0;n<count[5];++n){Vec2 point{scalar(p+off[5]+8*n),scalar(p+off[5]+8*n+4)};if(!finite(point))return bad(e,"Poison contacts point domain rejected");next->points.push_back(point);}next->body.assign(next->points.begin(),next->points.begin()+body_count);if(!convex(next->body.data(),next->body.size()))return bad(e,"Poison contacts nonconvex body rejected");
 uint32_t part_cursor=0,point_cursor=body_count;
 for(uint32_t n=0;n<count[3];++n){const auto raw=p+off[3]+16*n;PoisonContactArea area{word(raw),word(raw+4),word(raw+8)};const auto&source=next->poison.areas()[n];if(area.id!=source.id||area.first!=part_cursor||!area.count||area.count>64||area.count>count[4]-part_cursor||word(raw+12))return bad(e,"Poison contacts area part binding rejected");const auto&shape=next->poison.shapes()[source.first];std::vector<Vec2>source_points;std::vector<uint8_t>covered(shape.count);for(uint32_t i=0;i<shape.count;++i){const auto v=next->poison.points()[shape.first+i];source_points.push_back({v.x+shape.position.x,v.y+shape.position.y});}double total=0;
  for(uint32_t i=0;i<area.count;++i){const auto r=p+off[4]+16*part_cursor;PoisonContactPart part{word(r),word(r+4),word(r+8)};if(part.area!=n||part.first!=point_cursor||part.count<3||part.count>64||part.count>count[5]-point_cursor||word(r+12)||!convex(next->points.data()+part.first,part.count))return bad(e,"Poison contacts convex ownership/span rejected");
   for(uint32_t j=0;j<part.count;++j){const auto point=next->points[part.first+j];bool found=false;for(uint32_t k=0;k<shape.count;++k)if(point.x==source_points[k].x&&point.y==source_points[k].y){covered[k]=1;found=true;}if(!found)return bad(e,"Poison contacts foreign source vertex rejected");}total+=area2(next->points.data()+part.first,part.count);next->parts.push_back(part);point_cursor+=part.count;++part_cursor;
  }const double original=area2(source_points.data(),source_points.size());if(std::find(covered.begin(),covered.end(),uint8_t(0))!=covered.end()||std::abs(total-original)>std::max(1e-6,original*1e-12))return bad(e,"Poison contacts source partition coverage rejected");next->areas.push_back(area);
 }
 if(part_cursor!=count[4]||point_cursor!=count[5])return bad(e,"Poison contacts orphan geometry rejected");state_=std::move(next);e.clear();return true;
}
bool PoisonPlayerContacts::prepare(const PoisonContactsData&data,uint64_t epoch,uint64_t instance,FieldStepBody body,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_||!instance||(body!=FieldStepBody::Player&&body!=FieldStepBody::Other))return bad(e,"Poison contacts fresh body/source/epoch rejected");PoisonPlayerContacts next;next.data_=data;next.epoch_=next.last_epoch_=epoch;next.instance_=instance;next.body_=body;std::vector<uint32_t>ids;for(const auto&area:data.areas())ids.push_back(area.id);if(!next.pairs_.initialize(ids,e))return false;*this=std::move(next);e.clear();return true;
}
bool PoisonPlayerContacts::owner(uint64_t epoch,const PoisonBodySample&body,std::string&e)const{return valid()&&epoch==epoch_&&finite(body.position)&&body.layer<=0xfffff&&body.mask<=0xfffff?true:bad(e,"Poison contacts stale owner/body sample rejected");}
void PoisonPlayerContacts::enabled(const PoisonBodySample&body,std::array<uint8_t,128>&out)const{for(size_t n=0;n<data_.areas().size();++n){const auto&area=data_.poison().areas()[n];out[n]=uint8_t(body.enabled&&(area.bits&1)&&((area.layer&body.mask)||(area.mask&body.layer)));}}
bool PoisonPlayerContacts::begin(uint64_t epoch,const PoisonBodySample&body,bool paused,WorldDamageActor&actor,SessionCharacter&character,FieldRunLoop&run,FieldStepShadowState*shadow,std::vector<WorldDamageEvent>&out,std::string&e){
 if(!owner(epoch,body,e))return false;if(!out.empty()||!data_.poison().matches(run.content(),e)||!actor.validates(epoch,instance_,character,e))return bad(e,"Poison contacts borrowed owner/output rejected");auto next=*this;std::array<uint8_t,128>bits{};enabled(body,bits);FieldAreaEvents contacts;if(!next.pairs_.begin(bits.data(),data_.areas().size(),contacts,e))return false;if(contacts.count>1)return bad(e,"Poison simultaneous area transitions are outside the audited profile");std::vector<WorldDamageEvent>events;next.events_.clear();
 // At most one audited callback is delivered. Allocate its pair record first,
 // then use the existing atomic damage boundary directly;unrelated Character
 // storage and source no-op branches retain their actual caller ownership.
 if(contacts.count){const auto&event=contacts.values[0];next.events_.push_back({epoch,instance_,event.owner,event.entered});if(!deliver_poison_player_damage(data_.poison(),PoisonContactKind::Area,event.owner,event.entered,body_,paused,epoch,instance_,actor,character,run,shadow,events,e))return false;}
 *this=std::move(next);out=std::move(events);e.clear();return true;
}
bool PoisonPlayerContacts::end(uint64_t epoch,const PoisonBodySample&body,std::string&e){
 if(!owner(epoch,body,e))return false;std::array<uint8_t,128>bits{},overlap{};enabled(body,bits);std::vector<Vec2>polygon;for(auto point:data_.body()){Vec2 world{point.x+body.position.x,point.y+body.position.y};if(!finite(world))return bad(e,"Poison contacts translated body domain rejected");polygon.push_back(world);}if(!convex(polygon.data(),polygon.size()))return bad(e,"Poison contacts translated body degenerates");
 for(size_t n=0;n<data_.areas().size();++n){const auto&area=data_.areas()[n];for(uint32_t j=0;j<area.count&&!overlap[n];++j){const auto&part=data_.parts()[area.first+j];overlap[n]=uint8_t(overlaps(polygon.data(),polygon.size(),data_.points().data()+part.first,part.count));}}
 auto next=pairs_;if(!next.end(bits.data(),overlap.data(),data_.areas().size(),e))return false;pairs_=std::move(next);e.clear();return true;
}
bool PoisonPlayerContacts::retire(uint64_t epoch,std::string&e){if(!valid()||epoch!=epoch_||pairs_.sampling())return bad(e,"Poison contacts stale/unfinished retirement rejected");for(size_t n=0;n<data_.areas().size();++n)if(pairs_.inside(n))return bad(e,"Poison contacts active pair requires source exit before retirement");data_={};pairs_={};events_.clear();epoch_=instance_=0;e.clear();return true;}
}
