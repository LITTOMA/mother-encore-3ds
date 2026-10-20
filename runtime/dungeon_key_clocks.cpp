#include "encore/dungeon_key_clocks.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
namespace encore::upstream {namespace {
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float f(const uint8_t*p){uint32_t raw=u(p);float v;std::memcpy(&v,&raw,4);return v;}
bool reject(std::string&e,const char*s){e=s;return false;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1000000;}
bool path(std::string_view s){
 if(s.empty()||s.size()>512)return false;size_t start=0;
 for(size_t i=0;i<=s.size();++i){if(i==s.size()||s[i]=='/'){auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;start=i+1;}else if(s[i]<32||s[i]>126||s[i]=='\\'||s[i]==':')return false;}
 return true;
}
}
struct DungeonKeyClocksData::State {uint32_t scene=0;DungeonKeysData metadata;std::vector<KeyClockProfile>profiles;std::vector<KeyClockClip>clips;std::vector<KeyClockAudio>audios;};
uint32_t DungeonKeyClocksData::scene()const{return state_?state_->scene:0;}
DungeonKeysView DungeonKeyClocksData::metadata()const{return state_?state_->metadata.view():DungeonKeysView{};}
const std::vector<KeyClockProfile>&DungeonKeyClocksData::profiles()const{static const std::vector<KeyClockProfile>empty;return state_?state_->profiles:empty;}
const std::vector<KeyClockClip>&DungeonKeyClocksData::clips()const{static const std::vector<KeyClockClip>empty;return state_?state_->clips:empty;}
const std::vector<KeyClockAudio>&DungeonKeyClocksData::audios()const{static const std::vector<KeyClockAudio>empty;return state_?state_->audios:empty;}
const KeyClockProfile*DungeonKeyClocksData::profile(uint32_t id)const{for(const auto&p:profiles())if(p.id==id)return &p;return nullptr;}
const KeyClockClip*DungeonKeyClocksData::clip(uint32_t id)const{for(const auto&c:clips())if(c.id==id)return &c;return nullptr;}
bool DungeonKeyClocksData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,128*1024,e)&&load(b.data(),b.size(),e);}
bool DungeonKeyClocksData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<176||n>128*1024)return reject(e,"Key clock size rejected");
 if(std::memcmp(p,"ENCKLC01",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+24)>=65536||u(p+28)||u(p+52)!=7||u(p+56)||u(p+60))return reject(e,"Key clock schema/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return reject(e,"Key clock source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return reject(e,"Key clock checksum rejected");
 const uint32_t strides[]={1,1,64,32,24,32,80},limits[]={65536,65536,16,64,256,4096,16};uint32_t offsets[7],counts[7];size_t cursor=176;
 for(unsigned i=0;i<7;++i){const auto*r=p+64+i*16;offsets[i]=u(r+4);counts[i]=u(r+8);const size_t aligned=(cursor+3)&~size_t(3);
  if(u(r)!=i+1||u(r+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]!=aligned||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return reject(e,"Key clock section directory/span rejected");
  for(size_t j=cursor;j<aligned;++j)if(p[j])return reject(e,"Key clock section padding rejected");cursor=offsets[i]+size_t(counts[i])*strides[i];
 }
 if(cursor!=n)return reject(e,"Key clock trailing data rejected");
 auto next=std::make_shared<State>();next->scene=u(p+24);
 if(!next->metadata.load(p+offsets[1],counts[1],e)||next->metadata.view().scene_id()!=next->scene)return reject(e,"Key clock nested key policy rejected");
 std::map<std::string,std::pair<uint32_t,uint32_t>>strings;uint32_t owned=0;
 auto text=[&](uint32_t at,uint32_t length,std::string&out){
  if(!length||length>512||at>counts[0]||length>counts[0]-at)return false;
  std::string value(reinterpret_cast<const char*>(p+offsets[0]+at),length);if(value.compare(0,6,"res://")||!path(std::string_view(value).substr(6)))return false;
  const auto found=strings.find(value);if(found!=strings.end()){if(found->second!=std::make_pair(at,length))return false;}
  else{if(at!=owned)return false;strings[value]={at,length};owned+=length;}out=std::move(value);return true;
 };
 std::set<uint32_t>pids,kinds;
 for(uint32_t i=0;i<counts[2];++i){const auto*r=p+offsets[2]+i*64;KeyClockProfile row;row.id=u(r);const auto kind=u(r+4);row.kind=KeyClockKind(kind);row.frames=u(r+8);row.frame=u(r+12);row.mask=u(r+16);row.position={f(r+20),f(r+24)};for(unsigned j=0;j<4;++j)row.roles[j]=u(r+28+j*4);row.child=u(r+44);row.diagnostics=u(r+56);
  if(!row.id||row.id>65535||!pids.insert(row.id).second||kind<1||kind>3||!kinds.insert(kind).second||!row.frames||row.frames>65535||row.frame>=row.frames||row.mask>7||((row.mask&2)&&kind==2)||((row.mask&4)&&kind!=2)||u(r+60)||!finite(row.position.x)||!finite(row.position.y)||row.diagnostics>7||((row.diagnostics&4)&&(kind!=1||(row.diagnostics&3))))return reject(e,"Key clock profile identity/pose rejected");
  if((kind==2&&(!row.roles[2]||!row.roles[3]))||(kind==3&&!row.roles[2])||(kind==2?(!row.child||row.child>65535):row.child!=0)||!text(u(r+48),u(r+52),row.source))return reject(e,"Key clock profile roles/source rejected");
  next->profiles.push_back(std::move(row));
 }
 for(uint32_t i=0;i<next->metadata.view().node_count();++i)if(!kinds.count(uint32_t(next->metadata.view().node(i).kind)))return reject(e,"Key clock method profile coverage rejected");
 auto find_profile=[&](uint32_t id)->const KeyClockProfile*{for(const auto&v:next->profiles)if(v.id==id)return &v;return nullptr;};
 for(const auto&v:next->profiles)if(v.child&&(!find_profile(v.child)||find_profile(v.child)->kind!=KeyClockKind::Child))return reject(e,"Key clock child binding rejected");
 uint32_t tracks=0,keys=0;std::set<uint32_t>cids,roles;
 for(const auto&v:next->profiles)for(auto id:v.roles)if(id&&!roles.insert(id).second)return reject(e,"Key clock duplicate role identity rejected");
 for(uint32_t i=0;i<counts[3];++i){const auto*r=p+offsets[3]+i*32;KeyClockClip c;c.id=u(r);c.profile=u(r+4);const uint32_t role=u(r+8),first=u(r+16),count=u(r+20);c.role=KeyClockRole(role);c.duration=f(r+12);const auto*profile=find_profile(c.profile);
  if(!c.id||c.id>65535||!cids.insert(c.id).second||!profile||role<1||role>4||profile->roles[role-1]!=c.id||!std::isfinite(c.duration)||c.duration<=0||c.duration>60||first!=tracks||!count||count>16||count>counts[4]-tracks||u(r+24)||u(r+28))return reject(e,"Key clock animation identity/span rejected");
  std::set<uint32_t>targets;
  for(uint32_t j=0;j<count;++j){const auto*t=p+offsets[4]+size_t(first+j)*24;const uint32_t kind=u(t),mode=u(t+4),kfirst=u(t+12),kcount=u(t+16);KeyClockTrack track;track.target=KeyClockTarget(kind);track.mode=mode;track.index=u(t+8);
   if(kind<1||kind>8||!targets.insert(kind).second||track.index!=j||mode>2||kfirst!=keys||!kcount||kcount>256||kcount>counts[5]-keys||u(t+20)||(mode==2)!=(kind>=7)||(mode==0&&kind!=3&&kcount!=1)||(kind==3&&mode!=0)||((kind==5||kind>=7)&&profile->kind!=KeyClockKind::Door)||(kind==4&&profile->kind==KeyClockKind::Door))return reject(e,"Key clock track owner/interpolation/span rejected");
   float previous=-1;
   for(uint32_t k=0;k<kcount;++k){const auto*v=p+offsets[5]+size_t(kfirst+k)*32;KeyClockKey key;key.time=f(v);key.ease=f(v+4);key.value=u(v+8);key.position={f(v+12),f(v+16)};
    if(!std::isfinite(key.time)||key.time<0||key.time>c.duration||key.time<=previous||!std::isfinite(key.ease)||key.ease<=0||key.ease>64||!finite(key.position.x)||!finite(key.position.y)||u(v+20)||u(v+24)||u(v+28)||(kind!=3&&(key.position.x!=0||key.position.y!=0||key.ease!=1))||(kind==1&&key.value>=profile->frames)||((kind==2||kind==4||kind==5)&&key.value>1)||(kind==6&&key.value!=1)||((kind==3||kind>=7)&&key.value)||(mode==0&&kind!=3&&key.time!=0))return reject(e,"Key clock key value/time rejected");
    previous=key.time;track.keys.push_back(key);
   }
   keys+=kcount;c.tracks.push_back(std::move(track));
  }
  tracks+=count;next->clips.push_back(std::move(c));
 }
 if(tracks!=counts[4]||keys!=counts[5]||cids!=roles)return reject(e,"Key clock unowned animation records rejected");
 std::set<uint32_t>audio_profiles;
 for(uint32_t i=0;i<counts[6];++i){const auto*r=p+offsets[6]+i*80;KeyClockAudio a;a.profile=u(r);std::copy(r+12,r+44,a.sha.begin());a.gain=f(r+44);a.pitch=f(r+48);a.bus=u(r+52);const auto*profile=find_profile(a.profile);bool digest=false;for(auto b:a.sha)digest|=b!=0;
  if(!profile||profile->kind==KeyClockKind::Door||!audio_profiles.insert(a.profile).second||!digest||!std::isfinite(a.gain)||a.gain< -80||a.gain>24||!std::isfinite(a.pitch)||a.pitch<.01f||a.pitch>8||a.bus!=1||u(r+56)||u(r+60)||u(r+64)||u(r+68)||u(r+72)||u(r+76)||!text(u(r+4),u(r+8),a.source))return reject(e,"Key clock audio source metadata rejected");next->audios.push_back(std::move(a));
 }
 for(const auto&profile:next->profiles)if(profile.kind!=KeyClockKind::Door&&!audio_profiles.count(profile.id))return reject(e,"Key clock missing audio owner rejected");
 if(owned!=counts[0])return reject(e,"Key clock unowned text rejected");
 state_=std::move(next);e.clear();return true;
}
bool DungeonKeyClockPlayer::initialize(const DungeonKeyClocksData&d,uint32_t profile,std::string&e){
 const auto*p=d.profile(profile);if(!d.valid()||!p)return reject(e,"Key clock player profile rejected");
 DungeonKeyClockPlayer next;next.data_=d;next.profile_=profile;next.pose_={p->frame,p->position,bool(p->mask&1),bool(p->mask&2),bool(p->mask&4),false};*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClockPlayer::play(uint32_t clip,std::string&e){
 const auto*c=data_.clip(clip);if(!data_.valid()||!c||c->profile!=profile_)return reject(e,"Key clock play identity rejected");
 if(clip_!=clip||time_==c->duration){time_=0;cursors_.assign(c->tracks.size(),0);}clip_=clip;playing_=true;e.clear();return true;
}
bool DungeonKeyClockPlayer::write_audio(bool value,std::string&e){
 const auto*p=data_.profile(profile_);if(!p||p->kind==KeyClockKind::Door)return reject(e,"Key clock audio owner rejected");
 auto next=*this;next.pose_.audio=value;if(!next.emit(KeyClockEventKind::Audio,value,e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClockPlayer::audio_finished(std::string&e){
 const auto*p=data_.profile(profile_);if(!p||p->kind==KeyClockKind::Door)return reject(e,"Key clock audio completion owner rejected");pose_.audio=false;e.clear();return true;
}
bool DungeonKeyClockPlayer::emit(KeyClockEventKind kind,uint32_t value,std::string&e){if(events_.size()>=1024)return reject(e,"Key clock event capacity rejected");events_.push_back({kind,profile_,clip_,value});return true;}
bool DungeonKeyClockPlayer::apply(const KeyClockTrack&t,const KeyClockKey&key,std::string&e){
 switch(t.target){
 case KeyClockTarget::Frame:pose_.frame=key.value;break;
 case KeyClockTarget::Visible:pose_.visible=key.value;break;
 case KeyClockTarget::Position:pose_.position=key.position;break;
 case KeyClockTarget::Audio:pose_.audio=key.value;return emit(KeyClockEventKind::Audio,key.value,e);
 case KeyClockTarget::ChildVisible:pose_.child_visible=key.value;break;
 case KeyClockTarget::RootParent:return emit(KeyClockEventKind::RootParent,key.value,e);
 case KeyClockTarget::OpenBody:return emit(KeyClockEventKind::OpenBody,0,e);
 case KeyClockTarget::ChildPlay:return emit(KeyClockEventKind::ChildPlay,0,e);
 }return true;
}
bool DungeonKeyClockPlayer::advance(double delta,std::string&e){
 if(!data_.valid()||!std::isfinite(delta)||delta<0||delta>60)return reject(e,"Key clock delta rejected");
 auto next=*this;if(!next.step(delta,e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClockPlayer::step(double delta,std::string&e){
 if(!playing_)return true;const auto*c=data_.clip(clip_);const float next=std::min(c->duration,time_+float(delta));const bool done=next>=c->duration;
 for(size_t i=0;i<c->tracks.size();++i){const auto&t=c->tracks[i];
  if(t.target==KeyClockTarget::Position){
   const auto&ks=t.keys;size_t index=0;while(index+1<ks.size()&&ks[index+1].time<=next)++index;auto key=ks[index];
   if(index+1<ks.size()&&next>key.time){const auto&end=ks[index+1];const float fraction=std::clamp((next-key.time)/(end.time-key.time),0.f,1.f);const float weight=key.ease<1?1.f-std::pow(1.f-fraction,1.f/key.ease):std::pow(fraction,key.ease);key.position={key.position.x+(end.position.x-key.position.x)*weight,key.position.y+(end.position.y-key.position.y)*weight};}
   if(!apply(t,key,e))return false;continue;
  }
  if(t.mode==0){if(!apply(t,t.keys[0],e))return false;continue;}
  if(next==time_){
   if(t.mode==2)continue;const KeyClockKey*selected=nullptr;for(const auto&k:t.keys)if(k.time<=next)selected=&k;if(t.keys.size()==1)selected=&t.keys[0];if(selected&&!apply(t,*selected,e))return false;
   while(cursors_[i]<t.keys.size()&&t.keys[cursors_[i]].time<=next)++cursors_[i];
  }else{
   for(;cursors_[i]<t.keys.size();++cursors_[i]){const auto&k=t.keys[cursors_[i]];if(k.time>next||(!done&&k.time==next))break;if(!apply(t,k,e))return false;}
  }
 }
 time_=next;if(done){playing_=false;const auto*p=data_.profile(profile_);
  if((p->diagnostics&1)&&!emit(KeyClockEventKind::MissingHandler,0,e))return false;
  if((p->diagnostics&2)&&!emit(KeyClockEventKind::InvalidSignal,uint32_t(c->role),e))return false;
  if(p->diagnostics&4){pose_.queued=true;if(!emit(KeyClockEventKind::DeleteQueued,0,e))return false;}
  if(!emit(KeyClockEventKind::Completed,0,e))return false;
 }return true;
}
bool DungeonKeyClocks::validate(uint64_t epoch,std::string&e)const{if(!epoch_||epoch!=epoch_)return reject(e,"Key clocks scene epoch rejected");return true;}
bool DungeonKeyClocks::owner(uint64_t epoch,uint32_t id,size_t&index,bool live,std::string&e)const{
 if(!validate(epoch,e))return false;for(size_t i=0;i<owners_.size();++i)if(owners_[i].id==id){DungeonKeyState s;if(!core_.state(core_.handle(id),s,e))return false;if(live&&!s.alive)return reject(e,"Key clocks owner is deleted");index=i;return true;}return reject(e,"Key clocks owner identity rejected");
}
bool DungeonKeyClocks::publish(size_t i,const DungeonKeyEvent&value,std::string&e){if(events_.size()>=4096)return reject(e,"Key clocks output capacity rejected");DungeonKeyClockEvent event;event.source=DungeonKeyClockEventSource::Method;event.epoch=epoch_;event.owner=owners_[i].id;event.method=value;events_.push_back(event);return true;}
bool DungeonKeyClocks::publish(size_t i,const KeyClockEvent&value,std::string&e){if(events_.size()>=4096)return reject(e,"Key clocks output capacity rejected");DungeonKeyClockEvent event;event.source=DungeonKeyClockEventSource::Clock;event.epoch=epoch_;event.owner=owners_[i].id;event.clock=value;events_.push_back(event);return true;}
bool DungeonKeyClocks::methods(std::string&e){
 // Source notifications retain their original snapshots and order. Translate
 // only the two admitted animation/audio requests; all other events remain
 // visible to the world owner instead of being discarded.
 auto events=core_.events();core_.clear_events();
 for(const auto&event:events){size_t i;if(!owner(epoch_,event.target.id,i,false,e)||!publish(i,event,e))return false;auto&node=owners_[i];
  if(event.kind==DungeonKeyEventKind::PickupAudioRequested){if(!node.root.write_audio(true,e)||!clocks(i,node.root,e))return false;}
  else if(event.kind==DungeonKeyEventKind::LockAnimationRequested){const auto*p=data_.profile(node.root.profile_id());const auto role=event.state.requested_animation==DungeonLockAnimation::Opening?KeyClockRole::Open:KeyClockRole::Opened;const auto id=p->roles[uint32_t(role)-1];if(!id||!node.root.play(id,e))return reject(e,"Key clocks original animation role rejected");}
 }
 return true;
}
bool DungeonKeyClocks::clocks(size_t i,DungeonKeyClockPlayer&player,std::string&e){
 const auto events=player.events();player.clear_events();
 for(const auto&event:events){if(!publish(i,event,e))return false;
  if(event.kind==KeyClockEventKind::OpenBody||event.kind==KeyClockEventKind::ChildPlay){if(pending_.size()>=512)return reject(e,"Key clocks deferred capacity rejected");pending_.push_back({i,event.kind});}
  else if(event.kind==KeyClockEventKind::DeleteQueued){if(!core_.pickup_finished(core_.handle(owners_[i].id),e)||!methods(e))return false;}
 }
 return true;
}
bool DungeonKeyClocks::prepare(const DungeonKeyClocksData&d,uint64_t epoch,int32_t count,const std::vector<DungeonKeyFlagValue>&flags,std::string&e){
 if(!d.valid()||!epoch||epoch<=last_epoch_)return reject(e,"Key clocks preparation epoch/resource rejected");DungeonKeyClocks next;next.data_=d;next.epoch_=next.last_epoch_=epoch;const auto metadata=d.metadata();
 if(!next.core_.prepare(metadata,d.scene(),metadata.text(metadata.policy().region),count,flags,e))return false;
 for(uint32_t i=0;i<metadata.node_count();++i){const auto n=metadata.node(i);const auto kind=n.kind==DungeonKeyKind::Pickup?KeyClockKind::Key:KeyClockKind::Door;const KeyClockProfile*p=nullptr;for(const auto&v:d.profiles())if(v.kind==kind)p=&v;if(!p)return reject(e,"Key clocks method profile is absent");
  Owner node;node.id=n.id;node.has_child=p->child!=0;if(!node.root.initialize(d,p->id,e)||(node.has_child&&!node.child.initialize(d,p->child,e)))return false;next.owners_.push_back(std::move(node));
 }
 if(!next.methods(e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClocks::state(uint64_t epoch,uint32_t id,DungeonKeyClockView&out,std::string&e)const{
 size_t i;if(!owner(epoch,id,i,false,e))return false;DungeonKeyClockView next;if(!core_.state(core_.handle(id),next.object,e))return false;const auto&node=owners_[i];next.root=node.root.pose();next.root_clip=node.root.assigned();next.root_time=node.root.position();next.root_playing=node.root.playing();next.has_child=node.has_child;if(next.has_child){next.child=node.child.pose();next.child_clip=node.child.assigned();next.child_time=node.child.position();next.child_playing=node.child.playing();}out=next;e.clear();return true;
}
bool DungeonKeyClocks::flag(uint64_t epoch,uint32_t id,bool&out,std::string&e)const{size_t i;return owner(epoch,id,i,false,e)&&core_.flag(core_.handle(id),out,e);}
bool DungeonKeyClocks::pickup(uint64_t epoch,uint32_t id,bool current,bool paused,std::string&e){
 size_t i;if(!owner(epoch,id,i,true,e))return false;auto next=*this;if(!next.core_.pickup_body_entered(next.core_.handle(id),current,paused,next.owners_[i].root.pose().audio,e)||!next.methods(e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClocks::interact(uint64_t epoch,uint32_t id,std::string&e){size_t i;if(!owner(epoch,id,i,true,e))return false;auto next=*this;if(!next.core_.lock_interact(next.core_.handle(id),e)||!next.methods(e))return false;*this=std::move(next);e.clear();return true;}
bool DungeonKeyClocks::play_animation(uint64_t epoch,uint32_t id,KeyClockRole role,std::string&e){
 size_t i;if(!owner(epoch,id,i,true,e))return false;const auto*p=data_.profile(owners_[i].root.profile_id());const auto r=uint32_t(role);if(r<1||r>4||!p->roles[r-1])return reject(e,"Key clocks animation role is absent");return owners_[i].root.play(p->roles[r-1],e);
}
bool DungeonKeyClocks::audio_finished(uint64_t epoch,uint32_t id,std::string&e){
 size_t i;if(!owner(epoch,id,i,true,e))return false;auto next=*this;if(!next.owners_[i].root.audio_finished(e)||!next.core_.pickup_finished(next.core_.handle(id),e)||!next.methods(e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClocks::advance(uint64_t epoch,double delta,std::string&e){
 if(!validate(epoch,e)||!std::isfinite(delta)||delta<0||delta>60)return reject(e,"Key clocks scene delta/epoch rejected");auto next=*this;
 for(size_t i=0;i<next.owners_.size();++i){auto&node=next.owners_[i];DungeonKeyState s;if(!next.core_.state(next.core_.handle(node.id),s,e))return false;if(!s.alive)continue;
  if(node.has_child&&(!node.child.advance(delta,e)||!next.clocks(i,node.child,e)))return false;
  if(!node.root.advance(delta,e)||!next.clocks(i,node.root,e))return false;
 }
 *this=std::move(next);e.clear();return true;
}
bool DungeonKeyClocks::idle_frame(uint64_t epoch,std::string&e){
 if(!validate(epoch,e))return false;auto next=*this;
 for(const auto&pending:next.pending_){auto&node=next.owners_[pending.owner];DungeonKeyState s;if(!next.core_.state(next.core_.handle(node.id),s,e))return false;if(!s.alive)continue;
  if(pending.kind==KeyClockEventKind::OpenBody){if(!next.core_.lock_open_method(next.core_.handle(node.id),e)||!next.methods(e))return false;}
  else{const auto*p=next.data_.profile(node.child.profile_id());if(!node.has_child||!p||!node.child.play(p->roles[uint32_t(KeyClockRole::Open)-1],e))return reject(e,"Key clocks deferred child target rejected");}
 }
 next.pending_.clear();if(!next.core_.end_scene_frame(e)||!next.methods(e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonKeyClocks::retire(uint64_t epoch,std::string&e){
 if(!validate(epoch,e))return false;auto next=*this;
 for(size_t i=0;i<next.owners_.size();++i){auto&node=next.owners_[i];for(auto*player:{&node.root,&node.child})if(player->profile_id()&&player->pose().audio){if(!player->write_audio(false,e)||!next.clocks(i,*player,e))return false;}}
 next.pending_.clear();next.epoch_=0;*this=std::move(next);e.clear();return true;
}
}
