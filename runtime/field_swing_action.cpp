#include "encore/field_swing_action.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool token(std::string_view s){if(s.empty()||s.size()>64)return false;for(size_t i=0;i<s.size();++i){const char c=s[i];if(!(c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(i&&c>='0'&&c<='9')))return false;}return true;}
bool skill(std::string_view s){return s.size()<=64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32&&c<127;});}
void clear_motion(SwingMotionFlags&f,uint32_t mask){if(mask&1)f.crouch=false;if(mask&2)f.idle=false;if(mask&4)f.tap_run=false;if(mask&8)f.running=false;if(mask&16)f.walking=false;}
}
const SwingIdleDirection*FieldSwingActionData::direction(uint32_t clip)const{for(const auto&r:directions_)if(r.clip==clip)return &r;return nullptr;}
bool FieldSwingActionData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,32*1024,e)&&load(b.data(),b.size(),e);}
bool FieldSwingActionData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<112||n>32*1024)return fail(e,"Swing action size rejected");
 const auto capability=word(p+20);const unsigned sections=capability==2?4:3;
 if(std::memcmp(p,"ENCSWA01",8)||word(p+8)!=1||word(p+12)!=n||(capability!=1&&capability!=2)||word(p+24)||word(p+28)||word(p+52)!=sections||word(p+56)||word(p+60)||n<64+sections*16)return fail(e,"Swing action schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};
 for(unsigned i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Swing action source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Swing action checksum rejected");
 const uint32_t strides[]={64,24,1,48},limits[]={1,8,256,1};uint32_t offsets[4]{},counts[4]{};size_t end=64+sections*16;
 for(unsigned i=0;i<sections;++i){const auto*d=p+64+i*16;offsets[i]=word(d+4);counts[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<end||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return fail(e,"Swing action directory/span rejected");
  for(size_t j=end;j<offsets[i];++j)if(p[j])return fail(e,"Swing action padding rejected");
  end=offsets[i]+counts[i]*strides[i];
 }
 if(end!=n||counts[1]!=8)return fail(e,"Swing action trailing bytes/directions rejected");
 FieldSwingActionData v;v.capability_=capability;const auto*b=p+offsets[0];for(unsigned i=0;i<8;++i)v.policy_[i]=word(b+i*4);const auto&q=v.policy_;
 if(q[0]!=1||!q[1]||q[1]>63||q[2]>31||q[3]>31||q[4]||q[5]!=2||q[6]!=1||!q[7]||q[7]>65535)return fail(e,"Swing action execution policy rejected");
 uint32_t string_end=0;for(unsigned i=0;i<4;++i){const auto first=word(b+32+i*8),size=word(b+36+i*8);if(first!=string_end||!size||size>64||first>counts[2]||size>counts[2]-first)return fail(e,"Swing action string span rejected");v.strings_[i].assign(reinterpret_cast<const char*>(p+offsets[2]+first),size);if(!token(v.strings_[i]))return fail(e,"Swing action role identity rejected");string_end+=size;}
 if(string_end!=counts[2]||v.strings_[0]==v.strings_[1])return fail(e,"Swing action duplicate/unowned identities rejected");
 std::set<uint32_t>ids;std::set<std::pair<float,float>>positions;
 for(uint32_t i=0;i<counts[1];++i){const auto*r=p+offsets[1]+i*24;SwingIdleDirection row{word(r),{real(r+4),real(r+8)},word(r+12),word(r+16)};
  if(!row.clip||row.clip>65535||!ids.insert(row.clip).second||!std::isfinite(row.direction.x)||!std::isfinite(row.direction.y)||std::abs(row.direction.x)>1||std::abs(row.direction.y)>1||(row.direction.x==0&&row.direction.y==0)||!positions.insert({row.direction.x,row.direction.y}).second||row.frame>=q[7]||row.flags>3||word(r+20))return fail(e,"Swing action direction/pose rejected");
  v.directions_.push_back(row);
 }
 if(capability==2){const auto*r=p+offsets[3];std::copy(r,r+32,v.audio_.source_sha256.begin());v.audio_.gain_db=real(r+32);v.audio_.pitch=real(r+36);v.audio_.bus=word(r+40);v.audio_.pause_mode=word(r+44);
  if(std::all_of(r,r+32,[](uint8_t value){return value==0;})||!std::isfinite(v.audio_.gain_db)||v.audio_.gain_db<-120||v.audio_.gain_db>24||!std::isfinite(v.audio_.pitch)||v.audio_.pitch<=0||v.audio_.pitch>4||v.audio_.bus!=1||v.audio_.pause_mode!=2)return fail(e,"Swing action audio identity/policy rejected");
 }
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool FieldSwingActionData::bind(const FieldSwingData&d,std::string&e)const{
 if(!valid_||!d.valid()||d.clips().size()!=directions_.size())return fail(e,"Swing action bindings unprepared");
 for(const auto&r:directions_){const auto*c=d.clip(r.clip);if(!c||c->direction.x!=r.direction.x||c->direction.y!=r.direction.y)return fail(e,"Swing action direction identity differs");}e.clear();return true;
}
bool FieldSwingAction::initialize(const FieldSwingActionData&data,const FieldSwingData&swing,Vec2 facing,std::string_view selected,std::string&e){
 if(!data.bind(swing,e))return false;
 const auto*c=swing.direction(facing);if(!c||!skill(selected))return fail(e,"Swing action initial facing/selection rejected");
 FieldSwingAction v;v.data_=data;v.swing_=swing;if(!v.player_.initialize(swing,e))return false;v.clip_=c->id;const auto*r=data.direction(c->id);v.main_frame_=r->frame;v.pose_=swing.initial();v.pose_.main=bool(r->flags&1);v.pose_.special=bool(r->flags&2);v.selected_=std::string(selected);*this=std::move(v);e.clear();return true;
}
bool FieldSwingAction::rebase(Vec2 facing,std::string&e){
 if(!data_.valid()||player_.attacking()||travel_||bat_node_)return fail(e,"Swing action motion handoff rejected");
 const auto*c=swing_.direction(facing);if(!c)return fail(e,"Swing action motion facing rejected");
 const auto*r=data_.direction(c->id);if(!r)return fail(e,"Swing action idle binding lost");
 clip_=c->id;main_frame_=r->frame;pose_.main=bool(r->flags&1);pose_.special=bool(r->flags&2);e.clear();return true;
}
bool FieldSwingAction::input(std::string_view action,bool pressed,const SwingActionContext&context,SwingMotionFlags&motion,bool&accepted,std::string&e){
 if(!data_.valid())return fail(e,"Swing action owner unprepared");
 const auto mask=data_.policy()[1];const bool blocked=((mask&1)&&context.paused)||((mask&2)&&(!context.moving||player_.attacking()))||((mask&4)&&context.entering_door)||((mask&8)&&context.climbing)||((mask&16)&&context.spinning);
 if(action!=data_.strings()[2]||!pressed||blocked){accepted=false;e.clear();return true;}
 std::string selection=context.skill_available?data_.strings()[3]:std::string();
 if((mask&32)&&!context.skill_available){selected_=std::move(selection);accepted=false;e.clear();return true;}
 const auto*c=swing_.direction(context.direction);if(!c||starts_==UINT32_MAX)return fail(e,"Swing action input facing/counter rejected");
 if(!player_.play(c->id,context.incapacitated,e))return false;
 clip_=c->id;selected_=std::move(selection);main_frame_=data_.direction(clip_)->frame;travel_=true;++starts_;clear_motion(motion,data_.policy()[2]);accepted=true;e.clear();return true;
}
bool FieldSwingAction::advance(double delta,std::string&e){
 if(!data_.valid()||!std::isfinite(delta)||delta<0||delta>60)return fail(e,"Swing action clock rejected");
 // A source travel request is committed after the outgoing node's pass. Its
 // first Bat pass has position zero and has not evaluated the new clip keys.
 if(travel_){travel_=false;bat_node_=true;e.clear();return true;}
 if(bat_node_){if(!player_.advance(delta,e))return false;pose_=player_.pose();if(!player_.playing()){bat_node_=false;const auto*r=data_.direction(clip_);pose_.main=bool(r->flags&1);pose_.special=bool(r->flags&2);main_frame_=r->frame;}}
 e.clear();return true;
}
bool FieldSwingAction::flush(SwingMotionFlags&motion,std::string&e){
 if(!data_.valid())return fail(e,"Swing action callback owner unprepared");
 const bool pending=player_.pending_callbacks()!=0;if(!player_.flush(e))return false;
 if(pending){pose_.hitbox=player_.pose().hitbox;clear_motion(motion,data_.policy()[3]);}e.clear();return true;
}
}
