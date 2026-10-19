#include "encore/field_swing_entry.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto b=word(p);float v;std::memcpy(&v,&b,4);return v;}
int32_t integer(const uint8_t*p){const auto b=word(p);int32_t v;std::memcpy(&v,&b,4);return v;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool facing(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1&&std::abs(v.y)<=1&&(v.x!=0||v.y!=0);}
bool token(const std::string&s){if(s.empty()||s.size()>64)return false;for(size_t i=0;i<s.size();++i){char c=s[i];if(!(c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(i&&c>='0'&&c<='9')))return false;}return true;}
}
const SwingEntryState*FieldSwingEntryData::state(uint32_t id)const{for(const auto&s:states_)if(s.id==id)return &s;return nullptr;}
const SwingEntryClip*FieldSwingEntryData::select(uint32_t id,Vec2 v)const{
 const auto*s=state(id);if(!valid_||!s||!facing(v))return nullptr;const SwingEntryClip*result=nullptr;float best=INFINITY;
 for(uint32_t i=0;i<s->count;++i){const auto&c=clips_[s->first+i];const float x=c.direction.x-v.x,y=c.direction.y-v.y,d=x*x+y*y;if(d<best){best=d;result=&c;}}return result;
}
bool FieldSwingEntryData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,64*1024,e)&&load(b.data(),b.size(),e);}
bool FieldSwingEntryData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<144||n>64*1024)return fail(e,"Swing entry size rejected");
 if(std::memcmp(p,"ENCSWE01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||word(p+24)||word(p+28)||word(p+52)!=5||word(p+56)||word(p+60))return fail(e,"Swing entry schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};for(unsigned i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Swing entry source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Swing entry checksum rejected");
 const uint32_t strides[]={48,32,40,8,1},limits[]={1,32,256,4096,2048};uint32_t offsets[5],counts[5];size_t end=144;
 for(unsigned i=0;i<5;++i){const auto*d=p+64+i*16;offsets[i]=word(d+4);counts[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<end||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return fail(e,"Swing entry directory/span rejected");
  for(size_t j=end;j<offsets[i];++j)if(p[j])return fail(e,"Swing entry padding rejected");
  end=offsets[i]+counts[i]*strides[i];
 }
 if(end!=n||counts[1]<2)return fail(e,"Swing entry trailing bytes/states rejected");
 FieldSwingEntryData v;const auto*m=p+offsets[0];const auto frames=word(m);v.idle_=word(m+4);v.attack_=word(m+8);
 if(!frames||frames>65535||!v.idle_||!v.attack_||v.idle_==v.attack_||word(m+12)!=counts[1]||word(m+16)!=counts[2]||word(m+20)!=counts[3]||word(m+24)!=counts[1]||word(m+28))return fail(e,"Swing entry policy rejected");
 v.frames_=uint16_t(frames);
 std::set<uint32_t>ids;std::set<std::string>names;uint32_t pool_end=0,first=0;
 for(uint32_t i=0;i<counts[1];++i){const auto*r=p+offsets[1]+i*32;SwingEntryState s;s.id=word(r);const auto at=word(r+4),size=word(r+8);s.next=word(r+12);s.mode=word(r+16);s.first=word(r+20);s.count=word(r+24);
  if(!s.id||s.id>65535||!ids.insert(s.id).second||at!=pool_end||!size||size>64||at>counts[4]||size>counts[4]-at||s.first!=first||s.count>8||s.count>counts[2]-first||(s.mode!=0&&s.mode!=2)||word(r+28))return fail(e,"Swing entry state ownership rejected");
  s.name.assign(reinterpret_cast<const char*>(p+offsets[4]+at),size);if(!token(s.name)||!names.insert(s.name).second)return fail(e,"Swing entry state name rejected");pool_end+=size;first+=s.count;v.states_.push_back(s);
 }
 if(pool_end!=counts[4]||first!=counts[2]||!v.state(v.idle_)||!v.state(v.attack_))return fail(e,"Swing entry unowned spans/roles rejected");
 std::set<uint32_t>motions;for(unsigned i=0;i<4;++i){v.motion_[i]=word(m+32+i*4);if(!v.state(v.motion_[i])||v.motion_[i]==v.attack_||!motions.insert(v.motion_[i]).second)return fail(e,"Swing entry motion identity rejected");}
 for(const auto&s:v.states_){
  if(s.id==v.attack_){if(s.count||s.next||s.mode)return fail(e,"Swing entry attack ownership rejected");}
  else if(!s.count||!v.state(s.next))return fail(e,"Swing entry next state rejected");
 }
 for(const auto&s:v.states_){std::set<uint32_t>visited;auto at=s.id;while(at!=v.attack_){if(!visited.insert(at).second||visited.size()>v.states_.size())return fail(e,"Swing entry cycle rejected");at=v.state(at)->next;}}
 uint32_t key_end=0;
 for(const auto&s:v.states_){std::set<std::pair<float,float>>directions;
  for(uint32_t i=0;i<s.count;++i){const auto*r=p+offsets[2]+(s.first+i)*40;SwingEntryClip c;c.state=word(r);c.ordinal=word(r+4);c.direction={real(r+8),real(r+12)};c.clip.length=real(r+16);const auto loop=word(r+20),start=word(r+24),count=word(r+28);const auto main=integer(r+32),special=integer(r+36);
   if(c.state!=s.id||c.ordinal!=i||!facing(c.direction)||!directions.insert({c.direction.x,c.direction.y}).second||!std::isfinite(c.clip.length)||c.clip.length<=0||c.clip.length>60||loop>1||start!=key_end||!count||count>16||count>counts[3]-start||main< -1||main>1||special< -1||special>1)return fail(e,"Swing entry clip rejected");
   c.clip.loop=bool(loop);c.clip.count=uint8_t(count);c.clip.main_visibility=int8_t(main);c.clip.special_visibility=int8_t(special);float previous=-1;
   for(uint32_t j=0;j<count;++j){const auto*k=p+offsets[3]+(start+j)*8;const auto time=real(k);const auto frame=word(k+4);if(!std::isfinite(time)||time<0||time>c.clip.length||time<=previous||(j==0&&time!=0)||frame>=frames)return fail(e,"Swing entry frame key rejected");previous=time;c.clip.keys[j]={time,uint16_t(frame)};}
   key_end+=count;v.clips_.push_back(c);
  }
 }
 if(key_end!=counts[3])return fail(e,"Swing entry unowned keys rejected");
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool FieldSwingEntryData::bind(const FieldSwingActionData&a,const FieldSwingData&s,std::string&e)const{
 if(!valid_||!a.bind(s,e)||frames_!=a.policy()[7]||state(idle_)->name!=a.strings()[0]||state(attack_)->name!=a.strings()[1])return fail(e,"Swing entry/action identity differs");
 if(state(idle_)->next!=attack_||state(idle_)->mode)return fail(e,"Swing entry action handoff differs");
 for(const auto&r:states_)if(r.id!=idle_&&r.next==attack_)return fail(e,"Swing entry bypasses idle handoff");
 for(const auto&r:a.directions()){const auto*c=select(idle_,r.direction);if(!c||c->clip.keys[0].frame!=r.frame||c->clip.main_visibility!=int(bool(r.flags&1))||c->clip.special_visibility!=int(bool(r.flags&2)))return fail(e,"Swing entry/idle pose differs");}e.clear();return true;
}
bool FieldSwingEntry::initialize(const FieldSwingEntryData&d,const FieldSwingActionData&a,const FieldSwingData&s,uint32_t state,Vec2 direction,float position,std::string_view selected,std::string&e){
 if(!d.bind(a,s,e))return false;
 const auto*c=d.select(state,direction);if(!c||!std::isfinite(position)||position<0||position>c->clip.length)return fail(e,"Swing entry snapshot rejected");
 FieldSwingEntry v;v.data_=d;v.state_=state;v.facing_=direction;v.clip_=uint32_t(c-d.clips().data());if(!v.action_.initialize(a,s,direction,selected,e))return false;v.playback_.sample={uint16_t(v.action_.main_frame()),v.action_.pose().main,v.action_.pose().special};
 if(!begin_clip(c->clip,d.frames(),v.playback_)||!advance_clip(c->clip,position,d.frames(),v.playback_))return fail(e,"Swing entry initial frame rejected");
 v.handed_=state==d.idle();*this=std::move(v);e.clear();return true;
}
bool FieldSwingEntry::input(std::string_view action,bool pressed,const SwingActionContext&context,SwingMotionFlags&motion,bool&accepted,std::string&e){
 if(!data_.valid())return fail(e,"Swing entry owner unprepared");
 auto candidate=action_;auto flags=motion;bool result=accepted;if(!candidate.input(action,pressed,context,flags,result,e))return false;
 if(result&&(context.direction.x!=facing_.x||context.direction.y!=facing_.y))return fail(e,"Swing entry facing snapshot changed");
 action_=std::move(candidate);motion=flags;accepted=result;if(result)requested_=true;e.clear();return true;
}
bool FieldSwingEntry::advance(double delta,std::string&e){
 if(!data_.valid()||!std::isfinite(delta)||delta<0||delta>60)return fail(e,"Swing entry clock rejected");
 if(!requested_){e.clear();return true;}
 if(handed_)return action_.advance(delta,e);
 const auto*state=data_.state(state_);if(state_==data_.idle()){if(!action_.advance(delta,e))return false;handed_=true;return true;}
 auto playback=playback_;const auto&clip=data_.clips()[clip_].clip;if(pending_&&!begin_clip(clip,data_.frames(),playback))return fail(e,"Swing entry new frame rejected");if(!advance_clip(clip,float(delta),data_.frames(),playback))return fail(e,"Swing entry frame advance rejected");
 const bool next=state->mode==0||playback.position>=clip.length;uint32_t next_clip=clip_;
 if(next){const auto*target=data_.select(state->next,facing_);if(!target)return fail(e,"Swing entry target frame lost");next_clip=uint32_t(target-data_.clips().data());playback.position=0;}
 playback_=playback;pending_=next;if(next){state_=state->next;clip_=next_clip;}e.clear();return true;
}
bool FieldSwingEntry::flush(SwingMotionFlags&motion,std::string&e){if(!data_.valid())return fail(e,"Swing entry callback owner unprepared");return action_.flush(motion,e);}
}
