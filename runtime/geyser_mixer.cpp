#include "encore/geyser_mixer.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t pcm_crc(const uint8_t*p,size_t size){uint32_t crc=UINT32_MAX;for(size_t first=0;first<size;first+=8192){const size_t end=std::min(first+8192,size);for(size_t n=first;n<end;++n){crc^=p[n];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}report_load_progress(LoadPhase::Checksum,end,size);}return crc^UINT32_MAX;}
}
bool GeyserMixer::prepare(const GeyserMediaData&data,const std::vector<GeyserPcmView>&views,std::string&e){
 if(valid()||!data.valid()||data.timeline().policy().pitch!=1||views.size()!=data.bank().count())return bad(e,"Geyser mixer fresh media/PCM/pitch rejected");GeyserMixer next;next.data_=data;next.voices_.resize(data.capacity());next.next_.resize(data.capacity());std::set<uint32_t>ids;
 for(const auto&v:views){AudioAsset a;if(!ids.insert(v.id).second||!data.bank().find(v.id,a)||!v.pcm||v.samples!=size_t(a.frames)*a.channels||pcm_crc(reinterpret_cast<const uint8_t*>(v.pcm),a.pcm_bytes)!=a.pcm_crc)return bad(e,"Geyser mixer immutable PCM dimensions/checksum rejected");next.caches_.push_back({a,v});}
 *this=std::move(next);e.clear();return true;
}
bool GeyserMixer::owner(uint64_t epoch,std::string&e)const{return valid()&&epoch&&epoch==epoch_&&!pending_?true:bad(e,"Geyser mixer stale scene owner or pending render rejected");}
size_t GeyserMixer::find(uint64_t instance)const{for(size_t n=0;n<voices_.size();++n)if(voices_[n].instance==instance)return n;return SIZE_MAX;}
size_t GeyserMixer::cache(uint32_t id)const{for(size_t n=0;n<caches_.size();++n)if(caches_[n].asset.stable_id==id)return n;return SIZE_MAX;}
bool GeyserMixer::activate(uint64_t epoch,std::string&e){if(!valid()||!epoch||epoch_||epoch<=last_epoch_||pending_)return bad(e,"Geyser mixer fresh epoch rejected");epoch_=last_epoch_=epoch;last_instance_=0;e.clear();return true;}
bool GeyserMixer::bind(uint64_t epoch,uint64_t instance,std::string&e){if(!owner(epoch,e))return false;if(!instance||instance<=last_instance_)return bad(e,"Geyser mixer source ticket must increase");const auto slot=std::find_if(voices_.begin(),voices_.end(),[](const Voice&v){return !v.instance;});if(slot==voices_.end())return bad(e,"Geyser mixer source instance capacity reached");*slot={};slot->instance=instance;slot->state.present=true;last_instance_=instance;e.clear();return true;}
bool GeyserMixer::sync(uint64_t epoch,uint64_t instance,GeyserTimeline&timeline,Vec2 position,GeyserListener listener,std::string&e){
 if(!owner(epoch,e))return false;const auto at=find(instance);GeyserSnapshot pose;if(!instance||at==SIZE_MAX||!data_.timeline().same_owner(timeline.content())||!timeline.inspect(epoch,instance,pose,e))return bad(e,"Geyser mixer source instance/timeline owner differs");const auto&commands=timeline.commands();if(commands.size()>256)return bad(e,"Geyser mixer command capacity rejected");auto next=voices_[at],global=global_;uint64_t starts=0;GeyserStereoGain gain;
 if(!geyser_spatial_gain(data_.timeline().policy(),position,listener,pose.gain,gain,e))return false;next.gain=gain;next.state.paused=pose.stream_paused;
 auto stop=[](Voice&v){v.state.playing=false;v.state.end_frame=UINT64_MAX;};
 auto play=[&](Voice&v,uint32_t id,float offset){const auto n=cache(id);uint32_t frame=0;if(n==SIZE_MAX||!music_start_frame(caches_[n].asset,offset,frame,e))return false;v.cache=n;v.state.present=true;v.state.sound=id;v.state.cursor=frame;v.state.end_frame=UINT64_MAX;v.state.playing=true;++starts;return true;};
 for(const auto&c:commands){
  const auto*s=data_.timeline().sound(c.sound);
  switch(c.kind){
   case GeyserCommandKind::SetStream:if(!s||s->role==3||c.offset!=0)return bad(e,"Geyser mixer unknown local stream");stop(next);next.cache=cache(c.sound);next.state.sound=c.sound;next.state.cursor=0;break;
   case GeyserCommandKind::Play:if(!s||s->role==3||c.sound!=next.state.sound||!std::isfinite(c.offset)||c.offset<0||c.offset>3600||!play(next,c.sound,c.offset))return bad(e,"Geyser mixer local play binding/offset rejected");break;
   case GeyserCommandKind::Stop:if(c.sound||c.offset!=0)return bad(e,"Geyser mixer stop operand rejected");stop(next);break;
   case GeyserCommandKind::GlobalRumble:if(!s||s->role!=3||c.offset!=0||global.state.playing||!play(global,c.sound,0))return bad(e,"Geyser mixer global named play rejected");global.gain={1,1};break;
   default:return bad(e,"Geyser mixer unknown command opcode rejected");
  }
 }
 if(starts>UINT64_MAX-plays_)return bad(e,"Geyser mixer play generation exhausted");voices_[at]=next;global_=global;plays_+=starts;timeline.take_commands();e.clear();return true;
}
bool GeyserMixer::user_sfx(float db,std::string&e){if(!valid()||pending_||!std::isfinite(db)||db< -120||db>24)return bad(e,"Geyser mixer user gain rejected");user_gain_=audio_linear_gain(db);e.clear();return true;}
bool GeyserMixer::global_tree_notification(bool paused,bool can_process,std::string&e){if(!valid()||pending_)return bad(e,"Geyser mixer needs prepared global owner");if(global_.state.present){if(!paused)global_.state.paused=false;else if(!can_process)global_.state.paused=true;}e.clear();return true;}
bool GeyserMixer::reserve_render(int16_t*out,uint32_t capacity,uint32_t&frames,std::string&e){
 if(!valid()||pending_||!out||!capacity||capacity>4096)return bad(e,"Geyser mixer output capacity rejected");uint32_t count=0;
 auto remaining=[&](const Voice&v){return v.state.playing&&!v.state.paused&&v.cache<caches_.size()?caches_[v.cache].asset.frames-v.state.cursor:0;};
 for(const auto&v:voices_)count=std::max(count,std::min(capacity,remaining(v)));count=std::max(count,std::min(capacity,remaining(global_)));if(count>UINT64_MAX-rendered_)return bad(e,"Geyser mixer frame generation exhausted");
 // Resolve only audible,active source spans once per block. The schema caps
 // child voices at128 plus the independent global named voice;inactive or
 // out-of-range actors still advance their source cursors below.
 struct Input {const int16_t*pcm=nullptr;uint32_t frames=0,channels=0;GeyserStereoGain gain;};std::array<Input,129>inputs{};size_t input_count=0;
 auto input=[&](const Voice&v){const auto frames=remaining(v);if(!frames||(!v.gain.left&&!v.gain.right))return;const auto&c=caches_[v.cache];inputs[input_count++]={c.view.pcm+size_t(v.state.cursor)*c.asset.channels,frames,c.asset.channels,v.gain};};for(const auto&v:voices_)input(v);input(global_);
 for(uint32_t frame=0;frame<count;++frame){float left=0,right=0;
  for(size_t n=0;n<input_count;++n){const auto&v=inputs[n];if(frame>=v.frames)continue;const auto first=size_t(frame)*v.channels;left+=float(v.pcm[first])*v.gain.left;right+=float(v.pcm[first+(v.channels==2)])*v.gain.right;}left*=user_gain_;right*=user_gain_;out[2*frame]=int16_t(std::max(-32768.f,std::min(32767.f,left)));out[2*frame+1]=int16_t(std::max(-32768.f,std::min(32767.f,right)));
 }
 std::copy(voices_.begin(),voices_.end(),next_.begin());next_global_=global_;auto advance=[&](Voice&v){const auto taken=std::min(count,remaining(v));if(!taken)return;v.state.cursor+=taken;if(v.state.cursor==caches_[v.cache].asset.frames)v.state.end_frame=rendered_+taken;};for(auto&v:next_)advance(v);advance(next_global_);pending_frames_=count;pending_=true;frames=count;e.clear();return true;
}
bool GeyserMixer::commit_render(std::string&e){if(!valid()||!pending_)return bad(e,"Geyser mixer no reserved render");voices_.swap(next_);global_=next_global_;rendered_+=pending_frames_;pending_=false;e.clear();return true;}
bool GeyserMixer::render(int16_t*out,uint32_t capacity,uint32_t&frames,std::string&e){return reserve_render(out,capacity,frames,e)&&commit_render(e);}
bool GeyserMixer::consumed(uint64_t frames,std::string&e){if(!valid()||pending_||frames<consumed_||frames>rendered_)return bad(e,"Geyser mixer unknown consumed generation rejected");consumed_=frames;auto finished=[&](Voice&v){if(v.state.playing&&v.state.end_frame!=UINT64_MAX&&v.state.end_frame<=consumed_)v.state.playing=false;};for(auto&v:voices_)finished(v);finished(global_);e.clear();return true;}
bool GeyserMixer::retire_actor(uint64_t epoch,uint64_t instance,std::string&e){if(!owner(epoch,e))return false;const auto at=find(instance);if(!instance||at==SIZE_MAX)return bad(e,"Geyser mixer retiring unknown instance");voices_[at]={};e.clear();return true;}
bool GeyserMixer::retire_scene(uint64_t epoch,std::string&e){if(!owner(epoch,e))return false;for(auto&v:voices_)v={};epoch_=0;e.clear();return true;}
bool GeyserMixer::inspect(uint64_t epoch,uint64_t instance,GeyserPlayback&out,std::string&e)const{if(!owner(epoch,e))return false;const auto at=find(instance);if(!instance||at==SIZE_MAX)return bad(e,"Geyser mixer inspecting unknown instance");out=voices_[at].state;e.clear();return true;}
GeyserNamedVoice GeyserMixer::global_voice()const{return global_.state.playing?GeyserNamedVoice::Playing:global_.state.present?GeyserNamedVoice::Idle:GeyserNamedVoice::Absent;}
}
