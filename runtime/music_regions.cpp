#include "encore/music_regions.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*m){e=m;return false;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
float f32(const uint8_t*p){uint32_t u=u32(p);float f;std::memcpy(&f,&u,4);return f;}
bool finite(float v,float a,float b){return std::isfinite(v)&&v>=a&&v<=b;}
bool path(std::string_view p){if(p.empty()||p.front()=='/'||p.back()=='/')return false;size_t start=0;while(start<p.size()){const auto end=p.find('/',start);auto part=p.substr(start,end==p.npos?p.size()-start:end-start);if(part.empty()||part=="."||part=="..")return false;for(char c:part)if(c=='\\'||c==':'||c<32||c>126)return false;if(end==p.npos)break;start=end+1;}return true;}
struct Reader{const uint8_t*p;size_t n,pos=64;bool ok=true;
 uint32_t u(){if(pos>n||n-pos<4){ok=false;return 0;}auto v=u32(p+pos);pos+=4;return v;}
 float f(){uint32_t v=u();float out;std::memcpy(&out,&v,4);return out;}
 std::array<uint8_t,32> hash(){std::array<uint8_t,32>v{};if(pos>n||n-pos<32){ok=false;return v;}std::copy_n(p+pos,32,v.begin());pos+=32;bool nonzero=false;for(auto b:v)nonzero|=b!=0;if(!nonzero)ok=false;return v;}
 std::string text(){auto len=u();if(len>1024||pos>n||len>n-pos){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p+pos),len);pos+=len;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
};}
bool MusicRegionData::load(const uint8_t*b,size_t n,std::string&e){
 if(!b||n<64||n>65536)return fail(e,"Music region size out of bounds");
 if(std::memcmp(b,"ENCMUS01",8)||u32(b+8)!=1)return fail(e,"Unsupported music region version");
 if(u32(b+12)!=n)return fail(e,"Music region size mismatch");
 for(size_t i=40;i<64;++i)if(b[i])return fail(e,"Music region reserved header");
 const auto rc=u32(b+20),tc=u32(b+24),sc=u32(b+28);
 if(!rc||rc>64||!tc||tc>16||sc<rc||sc>rc*16)return fail(e,"Music region counts out of bounds");
 if(!finite(f32(b+32),-120,-20)||!finite(f32(b+36),0,60))return fail(e,"Music region tuning invalid");
 const uint8_t zero[4]{};uint32_t crc=crc32_update(0xffffffffu,b,16);crc=crc32_update(crc,zero,4);crc=crc32_update(crc,b+20,n-20)^0xffffffffu;
 if(crc!=u32(b+16))return fail(e,"Music region checksum mismatch");
 Reader r{b,n};MusicRegionData next;next.silence_db_=f32(b+32);next.fade_to_seconds_=f32(b+36);
 std::set<uint32_t>tracks,regions;std::set<std::string>paths,shape_paths;
 for(uint32_t i=0;i<tc;++i){MusicRegionTrack t;t.id=r.u();t.source_sha=r.hash();t.import_sha=r.hash();t.source_path=r.text();if(!r.ok||!t.id||!tracks.insert(t.id).second||t.source_path.substr(0,6)!="res://"||!path(std::string_view(t.source_path).substr(6))||!paths.insert(t.source_path).second)return fail(e,"Invalid music track identity");next.tracks_.push_back(std::move(t));}
 paths.clear();uint32_t shapes=0;
 for(uint32_t i=0;i<rc;++i){MusicRegionBinding v;v.id=r.u();v.track_id=r.u();auto disabled=r.u();v.disabled=disabled!=0;v.volume_db=r.f();v.fadein_seconds=r.f();v.fadeout_seconds=r.f();v.source_path=r.text();v.appear_flag=r.text();v.disappear_flag=r.text();const auto count=r.u();
  if(!r.ok||!v.id||!regions.insert(v.id).second||!tracks.count(v.track_id)||disabled>1||!finite(v.volume_db,-120,24)||!finite(v.fadein_seconds,0,60)||!finite(v.fadeout_seconds,0,60)||!path(v.source_path)||!paths.insert(v.source_path).second||!count||count>16)return fail(e,"Invalid music region binding");
  for(uint32_t j=0;j<count;++j){auto s=r.text();if(!r.ok||!path(s)||s.compare(0,v.source_path.size()+1,v.source_path+"/")!=0||!shape_paths.insert(s).second)return fail(e,"Invalid music region shape binding");v.shape_paths.push_back(std::move(s));}shapes+=count;next.regions_.push_back(std::move(v));
 }
 if(!r.ok||r.pos!=n||shapes!=sc)return fail(e,"Music region trailing or missing data");
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool MusicRegionData::load_file(const char*p,std::string&e){FILE*f=p?std::fopen(p,"rb"):nullptr;if(!f)return fail(e,"Cannot open music region pack");std::vector<uint8_t>b(65537);const auto n=std::fread(b.data(),1,b.size(),f);bool bad=std::ferror(f);std::fclose(f);if(bad)return fail(e,"Cannot read music region pack");return load(b.data(),n,e);}
bool MusicRegionData::matches(const AudioBank&bank,std::string&e)const{
 if(!valid_||bank.silence_db()!=silence_db_)return fail(e,"Music region/audio bank tuning mismatch");
 for(const auto&t:tracks_){AudioAsset a;if(!bank.find(t.id,a)||a.source_sha256!=t.source_sha||a.source_path!=t.source_path||!a.loops())return fail(e,"Music region/audio source identity or loop mismatch");}e.clear();return true;
}
bool MusicRegionData::select_assets(const AudioBank &bank,
                                    float expected_master_db,
                                    uint32_t max_tracks,
                                    std::vector<AudioAsset> &out,
                                    std::string &error) const {
  if (!std::isfinite(expected_master_db) ||
      !std::isfinite(bank.master_db()) ||
      expected_master_db != bank.master_db())
    return fail(error, "Music region bank/master differs");
  if (!max_tracks || tracks_.empty() || tracks_.size() > max_tracks)
    return fail(error, "Music region selected track capacity exceeded");
  if (!matches(bank, error))
    return false;
  std::vector<AudioAsset> selected;
  selected.reserve(tracks_.size());
  for (const auto &track : tracks_) {
    AudioAsset asset;
    // matches has checked the same immutable bank, but retain an explicit
    // lookup guard so a failed projection can never publish a partial list.
    if (!bank.find(track.id, asset))
      return fail(error, "Music region selected track absent from bank");
    selected.push_back(asset);
  }
  out.swap(selected);
  error.clear();
  return true;
}
bool MusicRegionController::initialize(const MusicRegionData&data,uint32_t capacity,std::string&e){if(!data.valid()||!capacity||capacity>maximum_voices)return fail(e,"Invalid music region controller capacity/data");MusicRegionController n;n.data_=&data;n.voices_.resize(capacity);n.states_.resize(data.regions().size());*this=std::move(n);e.clear();return true;}
bool MusicRegionController::attach_scene(uint64_t epoch,std::string&e){if(!data_||!epoch||epoch<=epoch_)return fail(e,"Invalid music scene epoch");for(auto&s:states_)if(s.registered||s.inside||s.pending_exit)return fail(e,"Previous music scene has not exited");epoch_=epoch;states_.assign(states_.size(),{});registered_.clear();pending_exits_.clear();e.clear();return true;}
int MusicRegionController::region(std::string_view p)const{if(data_)for(size_t i=0;i<data_->regions().size();++i)if(data_->regions()[i].source_path==p)return int(i);return-1;}
int MusicRegionController::latest()const{int result=-1;uint64_t order=0;for(size_t i=0;i<voices_.size();++i)if(voices_[i].allocated&&voices_[i].order>order){result=int(i);order=voices_[i].order;}return result;}
int MusicRegionController::song(uint32_t track)const{int result=-1;uint64_t order=UINT64_MAX;for(size_t i=0;i<voices_.size();++i)if(voices_[i].allocated&&voices_[i].track_id==track&&voices_[i].order<order){result=int(i);order=voices_[i].order;}return result;}
int MusicRegionController::allocate(std::string&e){for(size_t i=0;i<voices_.size();++i)if(!voices_[i].allocated){voices_[i]={};voices_[i].allocated=true;voices_[i].order=++next_order_;return int(i);}fail(e,"Music region voice capacity exhausted; existing audio preserved");return-1;}
void MusicRegionController::tween(int slot,float target,double seconds,MusicRegionCurve curve){auto&v=voices_[slot];v.start_db=v.gain_db;v.target_db=target;v.elapsed=0;v.duration=seconds;v.curve=curve;v.tweening=seconds>0;if(!v.tweening)v.gain_db=target;}
bool MusicRegionController::observe_external_player(MusicExternalPlayer player,std::string&e){
 if(!data_||(!player.present&&player.playing)||(!player.generation&&(player.present||player.playing))||player.generation<external_.generation)return fail(e,"Invalid or stale external music player identity");
 if(player.generation>external_.generation){if(!player.present)return fail(e,"Unknown external music player removal");if(external_.present)external_history_ambiguous_=true;external_order_=++next_order_;}
 else if(player.present&&!external_.present)return fail(e,"Removed external music identity cannot be reused");
 external_=player;e.clear();return true;
}
bool MusicRegionController::play(uint32_t i,std::string&e){
 const auto&binding=data_->regions()[i];auto&s=states_[i];s.inside=true;
 if(!s.registered){s.registered=true;registered_.push_back(i);}
 const int region_latest=latest();
 const bool external_latest=external_.present&&(region_latest<0||external_order_>voices_[region_latest].order);
 int slot=external_latest?-1:region_latest;bool same=false;
 if(registered_.front()!=i){const auto&first=data_->regions()[registered_.front()];int matching=song(binding.track_id);same=first.track_id==binding.track_id||matching>=0;if(matching>=0)slot=matching;}
 else same=slot>=0&&voices_[slot].track_id==binding.track_id;
 const bool latest_playing=external_latest?external_.playing:(region_latest>=0&&voices_[region_latest].playing);
 const bool new_fade=!same&&latest_playing;
 if(new_fade||slot<0){slot=allocate(e);if(slot<0)return false;}
 auto&v=voices_[slot];s.voice=slot;
 if(new_fade){v.track_id=binding.track_id;v.playing=true;v.generation=++next_generation_;v.gain_db=data_->silence_db();tween(slot,binding.volume_db,binding.fadein_seconds,MusicRegionCurve::QuartOut);}
 else if(v.playing&&v.gain_db>data_->silence_db()){
  // Source fadeto does nothing when the instantaneous property already equals
  // its target, including leaving any prior tween alive in that edge case.
  if(v.gain_db!=binding.volume_db)tween(slot,binding.volume_db,data_->fade_to_seconds(),MusicRegionCurve::Linear);
 }else{v.track_id=binding.track_id;v.playing=true;v.generation=++next_generation_;v.gain_db=binding.volume_db;/* source set_volume does not remove old tween */}
 e.clear();return true;
}
bool MusicRegionController::enter(uint64_t epoch,std::string_view p,const MusicRegionContext&c,std::string&e){
 if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}int i=region(p);if(i<0)return fail(e,"Unbound music area enter");
 const auto&b=data_->regions()[i];if(!c.is_player||c.in_cutscene||c.in_battle||b.disabled){e.clear();return true;}
 if((!b.appear_flag.empty()||!b.disappear_flag.empty())&&!c.flag)return fail(e,"Music region flag context unavailable");
 if((!b.appear_flag.empty()&&!c.flag(b.appear_flag))||(!b.disappear_flag.empty()&&c.flag(b.disappear_flag))){e.clear();return true;}
 // Transactional guard: exhausted capability must not alter old ownership,
 // queued exits, other voices or source event order.
 auto candidate=*this;auto&s=candidate.states_[size_t(i)];s.inside=true;
 if(!s.registered&&!candidate.play(uint32_t(i),e))return false;
 *this=std::move(candidate);e.clear();return true;
}
bool MusicRegionController::play_explicit(uint64_t epoch,std::string_view p,std::string&e){
 if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}const int i=region(p);if(i<0)return fail(e,"Unbound explicit music region play");
 auto next=*this;next.states_[size_t(i)].inside=true;
 if(!next.play(uint32_t(i),e))return false;
 *this=std::move(next);e.clear();return true;
}
bool MusicRegionController::stop_explicit(uint64_t epoch,std::string_view p,double fade,std::string&e){
 if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}const int i=region(p);if(i<0)return fail(e,"Unbound explicit music region stop");
 if(!std::isfinite(fade)||fade<0||fade>60)return fail(e,"Invalid explicit music region fade");
 // stop_music removes registration and attached-player ownership. It does
 // not change player_inside or discard a pending check_player_and_stop.
 stop(uint32_t(i),fade);e.clear();return true;
}
bool MusicRegionController::fade_index_zero(double duration,bool&external_target,std::string&e){
 if(!data_||!epoch_||!std::isfinite(duration)||duration<0||duration>60||external_history_ambiguous_)return fail(e,"Indexed music fade has invalid duration or unmapped external child history");
 int first=-1;uint64_t order=UINT64_MAX;for(size_t i=0;i<voices_.size();++i)if(voices_[i].allocated&&voices_[i].order<order){first=int(i);order=voices_[i].order;}
 external_target=external_.present&&(first<0||external_order_<order);
 if(!external_target&&first>=0&&voices_[first].playing){tween(first,data_->silence_db(),duration,MusicRegionCurve::QuartIn);cleanup_waiting_=true;}
 // With no live child, source _add_at_zero leaves an idle empty player. Its
 // music_fadeout_obj is a checked no-op, not an arbitrary ignored operation.
 e.clear();return true;
}
bool MusicRegionController::exit(uint64_t epoch,std::string_view p,const MusicRegionContext&c,std::string&e){if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}int i=region(p);if(i<0)return fail(e,"Unbound music area exit");if(c.is_player&&c.has_collisions&&!c.in_cutscene){states_[i].inside=false;states_[i].pending_exit=true;pending_exits_.push_back(uint32_t(i));}e.clear();return true;}
void MusicRegionController::stop(uint32_t i,double fade){auto&s=states_[i];s.registered=false;registered_.erase(std::remove(registered_.begin(),registered_.end(),i),registered_.end());if(s.voice<0)return;
 const int slot=s.voice;const auto track=data_->regions()[i].track_id;bool shared=false;
 for(auto j:registered_)if(states_[j].voice>=0){const auto&v=voices_[states_[j].voice];if(v.allocated&&v.track_id==track){shared=true;break;}}
 if(voices_[slot].allocated&&!shared){if(fade==0)voices_[slot].playing=false;else if(voices_[slot].playing){tween(slot,data_->silence_db(),fade,MusicRegionCurve::QuartIn);cleanup_waiting_=true;}}
 s.voice=-1;
}
bool MusicRegionController::tree_exit(uint64_t epoch,std::string_view p,std::string&e){if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}int i=region(p);if(i<0)return fail(e,"Unbound music area tree exit");auto&s=states_[i];if(s.registered)stop(uint32_t(i),data_->regions()[i].fadeout_seconds);s.inside=false;s.pending_exit=false;pending_exits_.erase(std::remove(pending_exits_.begin(),pending_exits_.end(),uint32_t(i)),pending_exits_.end());e.clear();return true;}
bool MusicRegionController::idle_frame(uint64_t epoch,std::string&e){if(!data_||!epoch_)return fail(e,"Music region controller has no scene");
 if(epoch!=epoch_){e.clear();return true;}auto pending=std::move(pending_exits_);pending_exits_.clear();for(auto i:pending){auto&s=states_[i];s.pending_exit=false;if(!s.inside)stop(i,data_->regions()[i].fadeout_seconds);}e.clear();return true;}
void MusicRegionController::detach_dead(int slot){for(auto&s:states_)if(s.voice==slot)s.voice=-1;voices_[slot]={};}
bool MusicRegionController::advance(double delta,bool external_tween_active,std::string&e){
 if(!data_||!std::isfinite(delta)||delta<0)return fail(e,"Invalid music region delta/data");
 bool active=external_tween_active;
 for(auto&v:voices_)if(v.allocated&&v.tweening){v.elapsed=std::min(v.duration,v.elapsed+delta);double t=v.elapsed/v.duration;double w=t;if(v.curve==MusicRegionCurve::QuartIn)w=t*t*t*t;else if(v.curve==MusicRegionCurve::QuartOut){double a=1-t;w=1-a*a*a*a;}v.gain_db=float(v.start_db+(v.target_db-v.start_db)*w);v.tweening=v.elapsed<v.duration;active|=v.tweening;}
 // Original _remove_all_unplaying runs after tween_all_completed, not when an
 // individual voice's fade ends. Reentry may restart a still-attached silent one.
 if(cleanup_waiting_&&!active){for(size_t i=0;i<voices_.size();++i)if(voices_[i].allocated&&(!voices_[i].playing||voices_[i].gain_db<=data_->silence_db()))detach_dead(int(i));cleanup_waiting_=false;}
 e.clear();return true;
}
}
