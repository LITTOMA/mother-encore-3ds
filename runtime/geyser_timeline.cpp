#include "encore/geyser_timeline.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto raw=u(p);float out;std::memcpy(&out,&raw,4);return out;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1e6;}
bool safe(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t first=0;
 for(size_t n=0;n<=s.size();++n){if(n<s.size()&&(static_cast<unsigned char>(s[n])<32||static_cast<unsigned char>(s[n])>=127||s[n]==':'||s[n]=='\\'))return false;
  if(n==s.size()||s[n]=='/'){auto part=s.substr(first,n-first);if(part.empty()||part=="."||part=="..")return false;first=n+1;}}
 return true;
}
}
struct GeyserTimelineData::State {GeyserPolicy policy;std::vector<GeyserClip>clips;std::vector<GeyserKey>keys;std::vector<GeyserCue>cues;std::vector<GeyserSound>sounds;};
const GeyserPolicy&GeyserTimelineData::policy()const{static const GeyserPolicy empty;return state_?state_->policy:empty;}
const std::vector<GeyserClip>&GeyserTimelineData::clips()const{static const std::vector<GeyserClip>empty;return state_?state_->clips:empty;}
const std::vector<GeyserKey>&GeyserTimelineData::keys()const{static const std::vector<GeyserKey>empty;return state_?state_->keys:empty;}
const std::vector<GeyserCue>&GeyserTimelineData::cues()const{static const std::vector<GeyserCue>empty;return state_?state_->cues:empty;}
const std::vector<GeyserSound>&GeyserTimelineData::sounds()const{static const std::vector<GeyserSound>empty;return state_?state_->sounds:empty;}
const GeyserClip*GeyserTimelineData::clip(uint32_t id)const{for(const auto&v:clips())if(v.id==id)return &v;return nullptr;}
const GeyserSound*GeyserTimelineData::sound(uint32_t id)const{for(const auto&v:sounds())if(v.id==id)return &v;return nullptr;}
bool GeyserTimelineData::load_file(const char*path,std::string&e){if(valid()||!path||!*path)return bad(e,"Geyser fresh owner/path required");std::vector<uint8_t>b;return encore::read_file(path,b,65536,e)&&load(b.data(),b.size(),e);}
bool GeyserTimelineData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<160||size>65536)return bad(e,"Geyser fresh owner/size rejected");
 if(std::memcmp(p,"ENCGYS01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||u(p+24)||u(p+28)||u(p+52)!=6||u(p+56)||u(p+60))return bad(e,"Geyser format/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Geyser source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return bad(e,"Geyser checksum rejected");
 const uint32_t stride[]={1,192,32,16,32,32},limits[]={8192,1,2,1024,256,3};uint32_t off[6]{},count[6]{};size_t cursor=160;
 for(unsigned n=0;n<6;++n){const auto q=p+64+16*n;off[n]=u(q+4);count[n]=u(q+8);if(u(q)!=n+1||u(q+12)!=stride[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/stride[n])return bad(e,"Geyser directory span/count/stride rejected");
  for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Geyser padding rejected");cursor=off[n]+size_t(count[n])*stride[n];}
 if(cursor!=size||count[2]!=2||count[5]!=3)return bad(e,"Geyser exact section coverage rejected");auto next=std::make_shared<State>();std::vector<uint8_t>used(count[0]);
 auto text=[&](const uint8_t*q,std::string&out){const auto first=u(q),length=u(q+4);if(!length||length>512||first>count[0]||length>count[0]-first)return false;for(uint32_t n=first;n<first+length;++n)if(used[n])return false;out.assign(reinterpret_cast<const char*>(p+off[0]+first),length);if(!safe(out))return false;std::fill(used.begin()+first,used.begin()+first+length,1);return true;};
 auto&v=next->policy;const auto q=p+off[1];if(!text(q,v.source)||!text(q+8,v.script)||!text(q+16,v.slot)||!text(q+24,v.bus))return bad(e,"Geyser source/string binding rejected");
 v.spewing=u(q+32);v.reset=u(q+36);v.frames=u(q+40);v.initial_frame=u(q+44);v.layer=u(q+60);v.mask=u(q+64);v.hframes=u(q+68);v.vframes=u(q+72);
 for(unsigned at:{48u,52u,56u,76u})if(u(q+at)>1)return bad(e,"Geyser property Boolean rejected");v.monitoring=u(q+48);v.disabled=u(q+52);v.monitorable=u(q+56);v.centered=u(q+76);
 if(!v.spewing||v.spewing>65535||!v.reset||v.reset>65535||v.spewing==v.reset||!v.frames||v.frames>256||v.initial_frame>=v.frames||!v.hframes||v.hframes>256||!v.vframes||v.vframes>256||v.frames!=v.hframes*v.vframes||v.layer>0xfffff||v.mask>0xfffff)return bad(e,"Geyser policy identity/grid rejected");
 float nums[28];for(unsigned n=0;n<28;++n){nums[n]=real(q+80+4*n);if(!finite(nums[n])||(n>=23&&nums[n]!=0))return bad(e,"Geyser finite geometry/reserved rejected");}
 v.root={nums[0],nums[1]};v.sprite={nums[2],nums[3]};v.offset={nums[4],nums[5]};v.size={nums[6],nums[7]};v.shape={nums[8],nums[9]};v.extents={nums[10],nums[11]};v.rotation=nums[12];v.max_distance=nums[13];v.attenuation=nums[14];v.pitch=nums[15];v.normal_gain=nums[16];v.muted_gain=nums[17];std::copy(nums+18,nums+22,v.notify);v.panning=nums[22];
 if(v.size.x<=0||v.size.y<=0||v.extents.x<=0||v.extents.y<=0||v.notify[2]<=0||v.notify[3]<=0||v.max_distance<=0||v.attenuation<=0||v.pitch<=0||v.panning<0||v.panning>4||v.normal_gain< -100||v.normal_gain>24||v.muted_gain< -100||v.muted_gain>24)return bad(e,"Geyser geometry/audio defaults rejected");
 std::set<uint32_t>sound_ids,roles;for(uint32_t n=0;n<count[5];++n){const auto a=p+off[5]+32*n;GeyserSound s;s.id=u(a);s.length=real(a+12);s.role=u(a+16);if(!s.id||s.id==UINT32_MAX||!sound_ids.insert(s.id).second||!text(a+4,s.source)||!finite(s.length)||s.length<.001||s.length>3600||s.role<1||s.role>3||!roles.insert(s.role).second||u(a+20)||u(a+24)||u(a+28))return bad(e,"Geyser media identity/length/role rejected");next->sounds.push_back(s);}
 for(uint32_t n=0;n<count[3];++n){const auto a=p+off[3]+16*n;GeyserKey k;k.time=real(a);k.kind=u(a+4);k.value=u(a+8);k.constant=u(a+12);if(!finite(k.time)||k.time<0||k.kind<1||k.kind>3||k.value>=(k.kind==1?v.frames:2)||k.constant>1||(k.constant&&k.time!=0))return bad(e,"Geyser key opcode/value/time rejected");next->keys.push_back(k);}
 for(uint32_t n=0;n<count[4];++n){const auto a=p+off[4]+32*n;GeyserCue c;c.time=real(a);c.kind=u(a+4);c.sound=u(a+8);c.start=real(a+12);c.end=real(a+16);c.track=u(a+20);auto found=std::find_if(next->sounds.begin(),next->sounds.end(),[&](const auto&s){return s.id==c.sound;});if(!finite(c.time)||c.time<0||c.kind<1||c.kind>2||found==next->sounds.end()||!finite(c.start)||c.start<0||c.start>3600||!finite(c.end)||c.end<0||c.end>3600||c.track>31||u(a+24)||u(a+28)||(c.kind==1&&(found->role!=3||c.start||c.end))||(c.kind==2&&found->role==3))return bad(e,"Geyser cue opcode/media/range rejected");next->cues.push_back(c);}
 std::set<uint32_t>clips;uint32_t first=0,cue_first=0;
 for(uint32_t n=0;n<count[2];++n){const auto a=p+off[2]+32*n;GeyserClip c;c.id=u(a);c.duration=real(a+4);c.first=u(a+8);c.count=u(a+12);c.cue_first=u(a+16);c.cue_count=u(a+20);if((c.id!=v.spewing&&c.id!=v.reset)||!clips.insert(c.id).second||!finite(c.duration)||c.duration<.000001||c.duration>60||c.first!=first||!c.count||c.count>256||first>count[3]||c.count>count[3]-first||c.cue_first!=cue_first||c.cue_count>64||cue_first>count[4]||c.cue_count>count[4]-cue_first||u(a+24)||u(a+28))return bad(e,"Geyser clip ownership/duration rejected");
  std::set<uint32_t>constants,seen;std::pair<float,uint32_t>previous{-1,0};bool frame=false;
  for(uint32_t j=first;j<first+c.count;++j){const auto&k=next->keys[j];const auto order=std::make_pair(k.time,k.kind);if(k.time>c.duration||order<=previous||constants.count(k.kind)||(k.constant&&seen.count(k.kind)))return bad(e,"Geyser key order/constant ownership rejected");if(k.constant)constants.insert(k.kind);seen.insert(k.kind);frame|=k.kind==1;previous=order;}if(!frame)return bad(e,"Geyser missing frame track");
  previous={-1,0};uint32_t tracks[3]={UINT32_MAX,UINT32_MAX,UINT32_MAX};for(uint32_t j=cue_first;j<cue_first+c.cue_count;++j){const auto&k=next->cues[j];const auto order=std::make_pair(k.time,k.track);if(k.time>c.duration||order<=previous||(tracks[k.kind]!=UINT32_MAX&&tracks[k.kind]!=k.track)||(tracks[3-k.kind]!=UINT32_MAX&&tracks[3-k.kind]==k.track))return bad(e,"Geyser cue order/time/target ownership rejected");tracks[k.kind]=k.track;previous=order;}
  first+=c.count;cue_first+=c.cue_count;next->clips.push_back(c);
 }
 if(first!=count[3]||cue_first!=count[4]||std::find(used.begin(),used.end(),uint8_t(0))!=used.end())return bad(e,"Geyser unowned keys/cues/strings rejected");state_=std::move(next);e.clear();return true;
}
bool GeyserTimeline::owner(uint64_t epoch,uint64_t instance,std::string&e)const{return valid()&&epoch==epoch_&&instance==instance_?true:bad(e,"Geyser stale instance/epoch rejected");}
bool GeyserTimeline::prepare(const GeyserTimelineData&data,uint64_t epoch,uint64_t instance,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_||!instance)return bad(e,"Geyser fresh source/epoch/instance required");GeyserTimeline next;next.data_=data;next.epoch_=next.last_epoch_=epoch;next.instance_=instance;next.pose_.clip=data.policy().spewing;next.pose_.frame=data.policy().initial_frame;next.pose_.monitoring=data.policy().monitoring;next.pose_.disabled=data.policy().disabled;next.pose_.gain=data.policy().normal_gain;*this=std::move(next);e.clear();return true;
}
bool GeyserTimeline::manual_advance(uint64_t epoch,uint64_t instance,double delta,std::string&e){
 if(!owner(epoch,instance,e))return false;if(!std::isfinite(delta)||delta<0||std::signbit(delta)||delta>60||pose_.queued_free)return bad(e,"Geyser unsupported delta/retiring actor rejected");const auto*c=data_.clip(pose_.clip);if(!c)return bad(e,"Geyser unbound clip");auto next=*this;auto&p=next.pose_;const float old=p.time,time=std::min(c->duration,old+float(delta)),step=time-old;const bool done=time==c->duration;
 auto apply=[&](const GeyserKey&k){if(k.kind==1)p.frame=k.value;else if(k.kind==2)p.monitoring=k.value;else p.disabled=k.value;};
 if(step==0){const GeyserKey*last[4]{},*single[4]{};uint32_t counts[4]{};for(uint32_t n=c->first;n<c->first+c->count;++n){const auto&k=data_.keys()[n];++counts[k.kind];single[k.kind]=&k;if(k.time<=time)last[k.kind]=&k;}for(unsigned n=1;n<=3;++n){if(counts[n]==1)apply(*single[n]);else if(last[n])apply(*last[n]);}}
 else{for(uint32_t n=c->first;n<c->first+c->count;++n){const auto&k=data_.keys()[n];if(k.constant||(k.time>=old&&(k.time<time||(done&&k.time==time))))apply(k);}
  const GeyserCue*audio=nullptr;for(uint32_t n=c->cue_first;n<c->cue_first+c->cue_count;++n){const auto&k=data_.cues()[n];if(k.time>=old&&(k.time<time||(done&&k.time==time))){if(k.kind==1){if(p.pending_rumbles>=64)return bad(e,"Geyser deferred capacity rejected");++p.pending_rumbles;}else audio=&k;}}
  if(audio){const auto*s=data_.sound(audio->sound);if(!s)return bad(e,"Geyser audio binding rejected");if(next.commands_.size()>254)return bad(e,"Geyser command capacity rejected");next.commands_.push_back({GeyserCommandKind::SetStream,s->id,0});next.commands_.push_back({GeyserCommandKind::Play,s->id,audio->start});p.audio_cached=true;p.audio_sound=s->id;p.audio_start=time;p.audio_length=s->length&&audio->end>0?(s->length-audio->start)-audio->end:0;}
  else if(p.audio_cached&&(time<p.audio_start||(p.audio_length>0&&time-p.audio_start>p.audio_length))){if(next.commands_.size()>=256)return bad(e,"Geyser command capacity rejected");next.commands_.push_back({GeyserCommandKind::Stop,0,0});p.audio_cached=false;}
 }
 p.time=time;if(done){p.playing=false;if(old<c->duration)p.queued_free=true;}*this=std::move(next);e.clear();return true;
}
bool GeyserTimeline::advance_idle(uint64_t epoch,uint64_t instance,double delta,bool enabled,std::string&e){if(!owner(epoch,instance,e))return false;if(!std::isfinite(delta)||delta<0||std::signbit(delta)||delta>60)return bad(e,"Geyser idle delta rejected");if(!enabled||!pose_.active||!pose_.playing){e.clear();return true;}return manual_advance(epoch,instance,delta,e);}
bool GeyserTimeline::player_signal(uint64_t epoch,uint64_t instance,bool paused,std::string&e){if(!owner(epoch,instance,e))return false;pose_.active=!paused;pose_.stream_paused=paused;e.clear();return true;}
bool GeyserTimeline::tree_notification(uint64_t epoch,uint64_t instance,bool paused,bool can_process,std::string&e){if(!owner(epoch,instance,e))return false;if(!paused)pose_.stream_paused=false;else if(!can_process)pose_.stream_paused=true;e.clear();return true;}
bool GeyserTimeline::process_ui(uint64_t epoch,uint64_t instance,GeyserContext c,std::string&e){if(!owner(epoch,instance,e))return false;pose_.gain=c.battle||c.cutscene||c.game_over?data_.policy().muted_gain:data_.policy().normal_gain;e.clear();return true;}
bool GeyserTimeline::flush_deferred(uint64_t epoch,uint64_t instance,GeyserContext c,GeyserNamedVoice&voice,std::string&e){
 if(!owner(epoch,instance,e))return false;if(voice!=GeyserNamedVoice::Absent&&voice!=GeyserNamedVoice::Idle&&voice!=GeyserNamedVoice::Playing)return bad(e,"Geyser unknown named-voice state rejected");auto next=*this;auto next_voice=voice;
 if(next.pose_.pending_rumbles&&c.on_screen&&!c.battle&&!c.cutscene&&!c.game_over&&voice!=GeyserNamedVoice::Playing){const auto found=std::find_if(data_.sounds().begin(),data_.sounds().end(),[](const auto&s){return s.role==3;});if(found==data_.sounds().end()||next.commands_.size()>=256)return bad(e,"Geyser rumble command capacity/binding rejected");next.commands_.push_back({GeyserCommandKind::GlobalRumble,found->id,0});next_voice=GeyserNamedVoice::Playing;}
 next.pose_.pending_rumbles=0;*this=std::move(next);voice=next_voice;e.clear();return true;
}
bool GeyserTimeline::retire(uint64_t epoch,uint64_t instance,std::string&e){if(!owner(epoch,instance,e))return false;if(pose_.pending_rumbles||!commands_.empty())return bad(e,"Geyser pending callbacks/commands require drain before retirement");data_=GeyserTimelineData{};epoch_=instance_=0;pose_={};e.clear();return true;}
bool GeyserTimeline::inspect(uint64_t epoch,uint64_t instance,GeyserSnapshot&out,std::string&e)const{if(!owner(epoch,instance,e))return false;out=pose_;e.clear();return true;}
}
