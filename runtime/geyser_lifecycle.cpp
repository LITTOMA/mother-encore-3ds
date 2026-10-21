#include "encore/geyser_lifecycle.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool safe(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t first=0;
 for(size_t n=0;n<=s.size();++n){
  if(n<s.size()&&(static_cast<unsigned char>(s[n])<32||static_cast<unsigned char>(s[n])>=127||s[n]==':'||s[n]=='\\'))return false;
  if(n==s.size()||s[n]=='/'){auto part=s.substr(first,n-first);if(part.empty()||part=="."||part=="..")return false;first=n+1;}
 }return true;
}
}
struct GeyserLifecycleData::State {PoisonHazardsData poison;GeyserTimelineData geyser;std::string source,prefab,parent;};
const PoisonHazardsData&GeyserLifecycleData::poison()const{static const PoisonHazardsData empty;return state_?state_->poison:empty;}
const GeyserTimelineData&GeyserLifecycleData::geyser()const{static const GeyserTimelineData empty;return state_?state_->geyser:empty;}
const std::string&GeyserLifecycleData::parent_path()const{static const std::string empty;return state_?state_->parent:empty;}
bool GeyserLifecycleData::load_file(const char*path,std::string&e){
 if(valid()||!path||!*path)return bad(e,"Geyser lifecycle fresh owner/path required");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,524288,e)&&load(bytes.data(),bytes.size(),e);
}
bool GeyserLifecycleData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<128||size>524288)return bad(e,"Geyser lifecycle fresh owner/size rejected");
 if(std::memcmp(p,"ENCGLF01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||!u(p+24)||u(p+24)>65535||u(p+28)||u(p+52)!=4||u(p+56)||u(p+60))return bad(e,"Geyser lifecycle format/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Geyser lifecycle source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return bad(e,"Geyser lifecycle checksum rejected");
 const uint32_t strides[]={1,32,1,1},limits[]={1536,1,262144,65536};uint32_t off[4]{},count[4]{};size_t cursor=128;
 for(unsigned n=0;n<4;++n){const auto q=p+64+16*n;off[n]=u(q+4);count[n]=u(q+8);if(u(q)!=n+1||u(q+12)!=strides[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/strides[n])return bad(e,"Geyser lifecycle section span/count/stride rejected");for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Geyser lifecycle padding rejected");cursor=off[n]+size_t(count[n])*strides[n];}
 if(cursor!=size)return bad(e,"Geyser lifecycle exact section coverage required");
 auto next=std::make_shared<State>();std::vector<uint8_t>used(count[0]);
 auto text=[&](const uint8_t*q,std::string&out){const auto first=u(q),length=u(q+4);if(!length||length>512||first>count[0]||length>count[0]-first)return false;for(uint32_t n=first;n<first+length;++n)if(used[n])return false;out.assign(reinterpret_cast<const char*>(p+off[0]+first),length);if(!safe(out))return false;std::fill(used.begin()+first,used.begin()+first+length,1);return true;};
 const auto q=p+off[1];if(!text(q,next->source)||!text(q+8,next->prefab)||!text(q+16,next->parent)||u(q+24)||u(q+28)||std::find(used.begin(),used.end(),uint8_t(0))!=used.end())return bad(e,"Geyser lifecycle source/parent/ownership rejected");
 if(!next->poison.load(p+off[2],count[2],e)||!next->geyser.load(p+off[3],count[3],e))return false;
 if(next->poison.policy().scene!=u(p+24)||next->poison.policy().source!=next->source||next->geyser.policy().source!=next->prefab)return bad(e,"Geyser lifecycle nested source/scene mismatch");
 state_=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::owner(uint64_t epoch,std::string&e)const{return valid()&&epoch==epoch_?true:bad(e,"Geyser lifecycle stale epoch rejected");}
bool GeyserLifecycle::prepare(const GeyserLifecycleData&data,uint64_t epoch,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_)return bad(e,"Geyser lifecycle fresh source/epoch required");GeyserLifecycle next;next.data_=data;next.epoch_=next.last_epoch_=epoch;*this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::receive_spawns(uint64_t epoch,PoisonHoleTimers&timers,std::string&e){
 if(!owner(epoch,e))return false;
 if(!timers.valid()||timers.epoch()!=epoch||!data_.poison().same_owner(timers.content())||timers.requests().size()>128-instances_.size())return bad(e,"Geyser lifecycle Timer owner/epoch/capacity rejected");
 auto next=*this;
 for(const auto&request:timers.requests()){
  if(request.epoch!=epoch||next.last_ticket_==UINT64_MAX||request.ticket!=next.last_ticket_+1)return bad(e,"Geyser lifecycle source ticket replay/gap rejected");
  const auto&holes=data_.poison().holes();auto found=std::find_if(holes.begin(),holes.end(),[&](const auto&h){return h.id==request.hole;});
  if(found==holes.end())return bad(e,"Geyser lifecycle unknown source hole");const auto offset=data_.poison().policy().spawn_offset;
  if(request.position.x!=found->position.x+offset.x||request.position.y!=found->position.y+offset.y)return bad(e,"Geyser lifecycle source spawn position rejected");
  Instance instance;instance.source=request;next.instances_.push_back(std::move(instance));next.last_ticket_=request.ticket;
 }
 *this=std::move(next);timers.take_requests();e.clear();return true;
}
bool GeyserLifecycle::player_signal(uint64_t epoch,PoisonHoleTimers&timers,bool paused,std::string&e){
 if(!owner(epoch,e))return false;if(!timers.valid()||timers.epoch()!=epoch||!data_.poison().same_owner(timers.content()))return bad(e,"Geyser lifecycle player signal Timer binding rejected");
 auto next=*this;auto clocks=timers;
 for(const auto&hole:data_.poison().holes())if(!clocks.pause(epoch,hole.id,paused,e))return false;
 for(auto&instance:next.instances_)if(instance.actor.valid()&&!instance.actor.player_signal(epoch,instance.source.ticket,paused,e))return false;
 *this=std::move(next);timers=std::move(clocks);e.clear();return true;
}
bool GeyserLifecycle::tree_notification(uint64_t epoch,bool paused,bool can_process,std::string&e){
 if(!owner(epoch,e))return false;auto next=*this;
 for(auto&instance:next.instances_)if(instance.actor.valid()&&!instance.actor.tree_notification(epoch,instance.source.ticket,paused,can_process,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::drain(Instance&instance,std::string&e){
 for(const auto&command:instance.actor.commands()){
  if(commands_.size()>=32768)return bad(e,"Geyser lifecycle command capacity rejected");
  commands_.push_back({epoch_,instance.source.ticket,command});
 }instance.actor.take_commands();return true;
}
bool GeyserLifecycle::advance_animations_idle(uint64_t epoch,double delta,bool enabled,std::string&e){
 if(!owner(epoch,e))return false;if(!std::isfinite(delta)||delta<0||std::signbit(delta)||delta>60)return bad(e,"Geyser lifecycle idle delta rejected");auto next=*this;
 for(auto&instance:next.instances_){
  if(!instance.actor.valid())continue;GeyserSnapshot pose;if(!instance.actor.inspect(epoch,instance.source.ticket,pose,e))return false;
  if(pose.queued_free)return bad(e,"Geyser lifecycle previous idle deletion must retire first");
  if(!instance.actor.advance_idle(epoch,instance.source.ticket,delta,enabled,e))return false;
  if(!next.drain(instance,e))return false;
 }*this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::process_ui(uint64_t epoch,bool enabled,GeyserContext context,std::string&e){
 if(!owner(epoch,e))return false;if(!enabled){e.clear();return true;}auto next=*this;
 for(auto&instance:next.instances_)if(instance.actor.valid()&&!instance.actor.process_ui(epoch,instance.source.ticket,context,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::advance_idle(uint64_t epoch,double delta,bool enabled,GeyserContext context,std::string&e){
 auto next=*this;if(!next.advance_animations_idle(epoch,delta,enabled,e)||!next.process_ui(epoch,enabled,context,e))return false;*this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::flush_additions(uint64_t epoch,std::string&e){
 if(!owner(epoch,e))return false;auto next=*this;
 // The original ready plays and connects. It does not sample earlier player
 // pause signals or synthesize a PAUSED notification for an already paused tree.
 for(auto&instance:next.instances_)if(!instance.actor.valid()&&!instance.actor.prepare(data_.geyser(),epoch,instance.source.ticket,e))return false;
 *this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::flush_methods(uint64_t epoch,const std::vector<GeyserDeferredContext>&contexts,GeyserNamedVoice&voice,std::string&e){
 if(!owner(epoch,e))return false;
 if(voice!=GeyserNamedVoice::Absent&&voice!=GeyserNamedVoice::Idle&&voice!=GeyserNamedVoice::Playing)return bad(e,"Geyser lifecycle unknown named voice rejected");
 size_t ready=0;for(const auto&instance:instances_)ready+=instance.actor.valid();if(contexts.size()!=ready)return bad(e,"Geyser lifecycle exact current context set required");auto next=*this;auto next_voice=voice;size_t index=0;
 for(auto&instance:next.instances_){if(!instance.actor.valid())continue;const auto&entry=contexts[index++];if(entry.instance!=instance.source.ticket)return bad(e,"Geyser lifecycle deferred instance order/ownership rejected");
  if(!instance.actor.flush_deferred(epoch,entry.instance,entry.context,next_voice,e)||!next.drain(instance,e))return false;
 }*this=std::move(next);voice=next_voice;e.clear();return true;
}
bool GeyserLifecycle::take_commands(uint64_t epoch,std::vector<GeyserInstanceCommand>&out,std::string&e){
 if(!owner(epoch,e))return false;if(!out.empty())return bad(e,"Geyser lifecycle fresh command output required");out=std::move(commands_);commands_.clear();e.clear();return true;
}
bool GeyserLifecycle::retire_queued(uint64_t epoch,std::vector<PoisonSpawnRequest>&out,std::string&e){
 if(!owner(epoch,e))return false;if(!out.empty())return bad(e,"Geyser lifecycle fresh retirement output required");auto next=*this;std::vector<PoisonSpawnRequest>retired;
 for(auto it=next.instances_.begin();it!=next.instances_.end();){
  GeyserSnapshot pose;if(!it->actor.valid()){++it;continue;}if(!it->actor.inspect(epoch,it->source.ticket,pose,e))return false;if(!pose.queued_free){++it;continue;}
  if(std::any_of(commands_.begin(),commands_.end(),[&](const auto&command){return command.instance==it->source.ticket;}))return bad(e,"Geyser lifecycle queued instance commands require delivery before retirement");
  if(!it->actor.retire(epoch,it->source.ticket,e))return false;retired.push_back(it->source);it=next.instances_.erase(it);
 }*this=std::move(next);out=std::move(retired);e.clear();return true;
}
bool GeyserLifecycle::retire_scene(uint64_t epoch,std::string&e){
 if(!owner(epoch,e))return false;if(!commands_.empty())return bad(e,"Geyser lifecycle scene commands require drain before retirement");auto next=*this;
 for(auto&instance:next.instances_)if(instance.actor.valid()&&!instance.actor.retire(epoch,instance.source.ticket,e))return false;
 next.instances_.clear();next.data_={};next.epoch_=0;*this=std::move(next);e.clear();return true;
}
bool GeyserLifecycle::inspect(uint64_t epoch,uint64_t ticket,GeyserInstanceSnapshot&out,std::string&e)const{
 if(!owner(epoch,e))return false;auto found=std::find_if(instances_.begin(),instances_.end(),[&](const auto&instance){return instance.source.ticket==ticket;});if(found==instances_.end())return bad(e,"Geyser lifecycle stale or unknown instance");GeyserInstanceSnapshot next;next.source=found->source;
 if(found->actor.valid()){GeyserSnapshot pose;if(!found->actor.inspect(epoch,ticket,pose,e))return false;next.pose=pose;}out=std::move(next);e.clear();return true;
}
std::vector<uint64_t>GeyserLifecycle::instances()const{std::vector<uint64_t>out;for(const auto&instance:instances_)out.push_back(instance.source.ticket);return out;}
}
