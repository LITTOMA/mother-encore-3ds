#include "music_region_service.hpp"
#include <cmath>
#include <algorithm>
namespace encore::ctr {
namespace {bool fail(std::string&e,const char*m){e=m;return false;}}
struct MusicRegionService::State {
 upstream::MusicRegionData data;
 upstream::MusicRegionController controller;
 MusicRegionPlayer player;
  MusicObservation external_owner;
 size_t consumed_history=0,history_size=0;uint32_t history_crc=0;std::string history_scene;
};
MusicRegionService::MusicRegionService()=default;
MusicRegionService::~MusicRegionService(){shutdown();}
void MusicRegionService::shutdown(){state_.reset();phase_=MusicRegionServicePhase::Dormant;}
uint64_t MusicRegionService::scene_epoch()const{return state_?state_->controller.scene_epoch():0;}
uint32_t MusicRegionService::live_voice_count()const{uint32_t count=0;if(state_)for(const auto&v:state_->controller.voices())count+=v.allocated&&v.playing;return count;}
uint32_t MusicRegionService::submitted_voices()const{return state_?state_->player.submitted_voices():0;}
uint32_t MusicRegionService::buffer_bytes()const{return state_?state_->player.buffer_bytes():0;}
bool MusicRegionService::registered_regions(std::vector<uint64_t>&out,std::string&e)const{
 if(!state_||phase_!=MusicRegionServicePhase::Active)return fail(e,"Source musicChangers lacks its active scene owner");
 std::vector<uint64_t>next;
 for(auto i:state_->controller.registered_indices()){
  if(i>=state_->data.regions().size()||!state_->controller.regions()[i].registered)
   return fail(e,"Source musicChangers registration differs from live controller");
  next.push_back(state_->data.regions()[i].id);
 }
 out=std::move(next);e.clear();return true;
}
bool MusicRegionService::source_players(const AudioPlayer&owner,std::vector<MusicSourcePlayer>&out,std::string&e)const{
 if(!state_||(phase_!=MusicRegionServicePhase::Active&&phase_!=MusicRegionServicePhase::Draining&&phase_!=MusicRegionServicePhase::Prepared))return fail(e,"Music source graph lacks its played controller");
 const auto actual=owner.observe_music();const auto external=state_->controller.external_child();
 if(!actual.available||actual.player_identity!=state_->external_owner.player_identity||actual.present!=external.present||actual.playing!=external.playing)return fail(e,"Music source child observation precedes actual backend observation");
 std::vector<MusicSourcePlayer>next;
 if(external.present)next.push_back({1,actual.asset_id,external.generation,state_->controller.external_child_order(),actual.playing,actual.tweening,actual.volume_db});
 for(const auto&v:state_->controller.voices())if(v.allocated)next.push_back({2,v.track_id,v.generation,v.order,v.playing,v.tweening,v.gain_db});
 std::sort(next.begin(),next.end(),[](const auto&a,const auto&b){return a.order<b.order;});
 for(size_t i=0;i<next.size();++i)if(!next[i].player_identity||!next[i].order||(i&&next[i-1].order==next[i].order))return fail(e,"Music actual source child identity/order duplicate");
 out.swap(next);e.clear();return true;
}
bool MusicRegionService::begin_prepare(const char*regions,const char*bank,const char*root,uint32_t capacity,const AudioPlayer&owner,std::string&e){
 upstream::MusicRegionData data;if(!data.load_file(regions,e))return false;
 return begin_prepare(data,bank,root,capacity,owner,e);
}
const upstream::MusicRegionData*MusicRegionService::content()const{return state_?&state_->data:nullptr;}
bool MusicRegionService::begin_prepare(const upstream::MusicRegionData&data,const char*bank,const char*root,uint32_t capacity,const AudioPlayer&owner,std::string&e){
 if(state_)return fail(e,"Music region service already prepared; preserve the live owner");
 const auto snapshot=owner.observe_music();
 if(!owner.device().available())return fail(e,"Music region service unavailable: existing audio device is not initialized");
 if(!data.valid())return fail(e,"Music region preparation needs actual loaded source data");
 auto candidate=std::make_unique<State>();candidate->data=data;
 if(!candidate->controller.initialize(candidate->data,capacity,e)||
    !candidate->player.begin_prepare(bank,root,candidate->data,capacity,owner.device(),snapshot.master_db,e))return false;
 state_=std::move(candidate);phase_=MusicRegionServicePhase::Preparing;e.clear();return true;
}
uint64_t MusicRegionService::prepared_pcm_bytes()const{return state_?state_->player.prepared_pcm_bytes():0;}
uint64_t MusicRegionService::total_pcm_bytes()const{return state_?state_->player.total_pcm_bytes():0;}
MusicPreparationStep MusicRegionService::prepare_step(uint32_t budget,std::string&e){
 if(phase_==MusicRegionServicePhase::Prepared){e.clear();return MusicPreparationStep::Ready;}
 if(!state_||phase_!=MusicRegionServicePhase::Preparing){e="Music region service is not preparing";return MusicPreparationStep::Failed;}
 const auto result=state_->player.prepare_step(budget,e);
 if(result==MusicPreparationStep::Ready)phase_=MusicRegionServicePhase::Prepared;
 else if(result==MusicPreparationStep::Failed)shutdown();
 return result;
}
bool MusicRegionService::cancel_preparation(std::string&e){if(phase_!=MusicRegionServicePhase::Dormant&&phase_!=MusicRegionServicePhase::Preparing&&phase_!=MusicRegionServicePhase::Prepared)return fail(e,"Cannot cancel committed region music; deliver source exits");shutdown();e.clear();return true;}
bool MusicRegionService::observe(upstream::MusicRegionController&controller,MusicObservation&known,const MusicObservation&s,std::string&e){
 if(!s.available)return fail(e,"Music region service lost its NDSP owner");
  if(s.generation<known.generation||s.player_identity<known.player_identity||s.retired_player_identity<known.retired_player_identity)return fail(e,"Existing music owner sequence regressed");
  if(s.retired_player_identity>s.player_identity||
     (!s.player_identity&&(s.present||s.generation||s.retired_player_identity))||
     (s.player_identity&&!s.generation)||
     (s.present&&(!s.generation||s.retired_player_identity!=s.player_identity-1))||
     (!s.present&&s.player_identity!=s.retired_player_identity)||
     (s.playing&&(!s.present||!s.asset_id))||(s.tweening&&!s.playing))
    return fail(e,"Invalid bounded Music player lifecycle snapshot");
  const bool replaced=s.player_identity!=known.player_identity;
  if(replaced&&known.player_identity){
    // A later successful start is not proof that the old source instance died.
    // Only AudioPlayer's explicit retirement receipt permits remove -> create.
    // This single lane mints an identity only after retiring its predecessor.
    // The high-water mark proves skipped instances are dead; Prepared scenes
    // need not observe every intervening title replay to retain live order.
    if(s.generation<=known.generation||
       s.retired_player_identity<known.player_identity)
      return fail(e,"Unobserved bounded Music player creation/removal history");
  }
  if(known.present&&(replaced||!s.present))
    if(!controller.observe_external_player({known.player_identity,false,false},e))return false;
  if(s.present){
    if(!replaced&&!known.present)return fail(e,"Retired bounded Music identity cannot be reused");
    // Controller generation is its source-player identity. Playback sequence
    // deliberately stays outside the child graph, so stream replacement never
    // manufactures an additional child or clears a genuine ambiguity.
    if(!controller.observe_external_player({s.player_identity,true,s.playing},e))return false;
  }
  known=s;
 e.clear();return true;
}
bool MusicRegionService::commit_scene(uint64_t epoch,const AudioPlayer&owner,std::string&e){
 if(!state_||(phase_!=MusicRegionServicePhase::Prepared&&phase_!=MusicRegionServicePhase::Draining))return fail(e,"Music region scene is not prepared or still owns active areas");
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing)return fail(e,"Music region external DialogueMusic player is outside the mapped handoff");
 auto next=state_->controller;auto generation=state_->external_owner;
 if(!observe(next,generation,snapshot,e)||!next.attach_scene(epoch,e))return false;
 state_->controller=std::move(next);state_->external_owner=generation;state_->consumed_history=state_->history_size=state_->history_crc=0;state_->history_scene.clear();phase_=MusicRegionServicePhase::Active;e.clear();return true;
}
bool MusicRegionService::handoff_scene(MusicRegionService&candidate,uint64_t epoch,const AudioPlayer&owner,std::string&e){
 if(&candidate==this||!state_||phase_!=MusicRegionServicePhase::Draining||!candidate.state_||candidate.phase_!=MusicRegionServicePhase::Prepared)return fail(e,"Music source handoff needs preceding exited scene and detached prepared candidate");
 if(state_->controller.capacity()!=candidate.state_->controller.capacity()||!state_->controller.can_handoff(candidate.state_->data,epoch,e))return false;
 auto next=state_->controller;auto known=state_->external_owner;
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing)return fail(e,"Music source handoff has unmapped DialogueMusic child");
 if(!observe(next,known,snapshot,e)||!state_->player.adopt_prepared_tracks(candidate.state_->player,e))return false;
 state_->data=candidate.state_->data;
 if(!next.handoff(state_->data,epoch,e))return false;
 state_->controller=std::move(next);state_->external_owner=known;
 state_->consumed_history=state_->history_size=state_->history_crc=0;state_->history_scene.clear();
 phase_=MusicRegionServicePhase::Active;candidate.shutdown();e.clear();return true;
}
bool MusicRegionService::bind_room_history(upstream::RoomView room,std::string&e){
 if(!state_||phase_!=MusicRegionServicePhase::Active||!room.valid()||room.byte_size()<128||state_->history_size)return fail(e,"Region music history owner rejected");
 auto word=[](const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;};
 if(word(room.bytes()+32)!=8||word(room.bytes()+36)!=9)return fail(e,"Region music history requires reviewed Room capability9");
 state_->history_crc=word(room.bytes()+52);state_->history_size=room.byte_size();state_->history_scene=room.string(room.scene().source_scene_string);state_->consumed_history=0;e.clear();return true;
}
bool MusicRegionService::consume_room_history(upstream::RoomView room,const std::vector<upstream::OpeningAudioRequest>&requests,std::string&e){
 if(!state_||(phase_!=MusicRegionServicePhase::Active&&phase_!=MusicRegionServicePhase::Draining)||!room.valid()||room.byte_size()!=state_->history_size||state_->consumed_history>requests.size()||room.string(room.scene().source_scene_string)!=state_->history_scene)return fail(e,"Region music history changed or has no checked owner");
 const auto*p=room.bytes()+52;const auto crc=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;if(crc!=state_->history_crc)return fail(e,"Region music history resource fingerprint changed");
 for(size_t i=state_->consumed_history;i<requests.size();++i){const auto&r=requests[i];
  if(r.kind==upstream::AudioRequestKind::FadeMusic){
   if(r.resource_index!=upstream::kRoomNoIndex||r.gain_db!=0||!std::isfinite(r.duration)||r.duration<0||r.duration>60)return fail(e,"Unreviewed source global music fade request");
   bool source=false;for(uint32_t c=0;c<room.command_count();++c){const auto command=room.command(c);if(command.opcode==uint16_t(upstream::DialogueActionKind::MusicFadeOut)&&command.phrase==r.phrase&&command.duration==r.duration&&command.value==0){source=true;break;}}
   if(!source)return fail(e,"Indexed region fade has no matching checked source command");
  }else if(r.kind!=upstream::AudioRequestKind::PlayMusic&&r.kind!=upstream::AudioRequestKind::PlayEffect&&r.kind!=upstream::AudioRequestKind::PlayDialogueMusic&&r.kind!=upstream::AudioRequestKind::StopMusicResource&&r.kind!=upstream::AudioRequestKind::FadeInMusic)return fail(e,"Unknown Room audio history operation");
 }
 state_->consumed_history=requests.size();e.clear();return true;
}
bool MusicRegionService::route_room_fade(upstream::RoomView room,const upstream::OpeningAudioRequest&r,AudioPlayer&owner,std::string&e){
 if(!state_||!room.valid()||room.byte_size()!=state_->history_size||room.string(room.scene().source_scene_string)!=state_->history_scene||(phase_!=MusicRegionServicePhase::Active&&phase_!=MusicRegionServicePhase::Draining)||r.kind!=upstream::AudioRequestKind::FadeMusic||r.resource_index!=upstream::kRoomNoIndex||r.gain_db!=0||!std::isfinite(r.duration)||r.duration<0||r.duration>60)return fail(e,"Unbound indexed source music fade");
 const auto*p=room.bytes()+52;const auto crc=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;if(crc!=state_->history_crc)return fail(e,"Indexed source music room changed");
 bool source=false;for(uint32_t c=0;c<room.command_count();++c){const auto command=room.command(c);if(command.opcode==uint16_t(upstream::DialogueActionKind::MusicFadeOut)&&command.phrase==r.phrase&&command.duration==r.duration&&command.value==0){source=true;break;}}if(!source)return fail(e,"Indexed fade does not match a reviewed source command");
 const auto snapshot=owner.observe_music();if(snapshot.dialogue_music_playing)return fail(e,"Indexed source fade has unmapped external DialogueMusic child order");
 auto next=state_->controller;auto generation=state_->external_owner;bool external=false;
 if(!observe(next,generation,snapshot,e)||!next.fade_index_zero(r.duration,external,e))return false;
 if(external&&!owner.fade_music(r.duration,e))return false;
 state_->controller=std::move(next);state_->external_owner=generation;e.clear();return true;
}
bool MusicRegionService::area_enter(uint64_t epoch,std::string_view path,const upstream::MusicRegionContext&context,const AudioPlayer&owner,std::string&e){
 if(!state_)return fail(e,"Music region Area event has no prepared service");
 if(epoch!=scene_epoch()){e.clear();return true;}
 if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region Area enter outside active scene");
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing&&context.is_player&&!context.in_cutscene&&!context.in_battle)return fail(e,"Music region external DialogueMusic player is outside the mapped handoff");
 auto next=state_->controller;auto generation=state_->external_owner;
 if(!observe(next,generation,snapshot,e)||!next.enter(epoch,path,context,e))return false;
 state_->controller=std::move(next);state_->external_owner=generation;e.clear();return true;
}
bool MusicRegionService::play_explicit(uint64_t epoch,std::string_view path,const AudioPlayer&owner,std::string&e){
 if(!state_)return fail(e,"Explicit music region play has no prepared service");
 if(epoch!=scene_epoch()){e.clear();return true;}
 if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Explicit music region play outside active scene");
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing)return fail(e,"Music region external DialogueMusic player is outside the mapped handoff");
 auto next=state_->controller;auto generation=state_->external_owner;
 if(!observe(next,generation,snapshot,e)||!next.play_explicit(epoch,path,e))return false;
 state_->controller=std::move(next);state_->external_owner=generation;e.clear();return true;
}
bool MusicRegionService::stop_explicit(uint64_t epoch,std::string_view path,double fade,std::string&e){
 if(!state_)return fail(e,"Explicit music region stop has no prepared service");
 if(epoch!=scene_epoch()){e.clear();return true;}
 if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Explicit music region stop outside active scene");
 return state_->controller.stop_explicit(epoch,path,fade,e);
}
bool MusicRegionService::area_exit(uint64_t epoch,std::string_view path,const upstream::MusicRegionContext&context,std::string&e){if(!state_)return fail(e,"Music region Area event has no prepared service");if(epoch!=scene_epoch()){e.clear();return true;}if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region Area exit outside active scene");return state_->controller.exit(epoch,path,context,e);}
bool MusicRegionService::idle_frame(uint64_t epoch,std::string&e){if(!state_)return fail(e,"Music region idle event has no prepared service");if(epoch!=scene_epoch()){e.clear();return true;}if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region idle event outside active scene");return state_->controller.idle_frame(epoch,e);}
bool MusicRegionService::tree_exit(uint64_t epoch,std::string_view path,std::string&e){if(!state_)return fail(e,"Music region tree event has no prepared service");if(epoch!=scene_epoch()){e.clear();return true;}if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region tree exit outside active scene");return state_->controller.tree_exit(epoch,path,e);}
bool MusicRegionService::finish_scene(uint64_t epoch,std::string&e){
 if(!state_)return fail(e,"Music region finish has no prepared service");
 if(epoch!=scene_epoch()){e.clear();return true;}
 if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region scene is not active");
 for(const auto&area:state_->controller.regions())if(area.inside||area.registered||area.pending_exit)return fail(e,"Music region source tree exits are incomplete");
 phase_=MusicRegionServicePhase::Draining;e.clear();return true;
}
bool MusicRegionService::update(double delta,const AudioPlayer&owner,std::string&e){
 if(phase_==MusicRegionServicePhase::Dormant||phase_==MusicRegionServicePhase::Preparing||phase_==MusicRegionServicePhase::Prepared){e.clear();return true;}
 if(!std::isfinite(delta)||delta<0)return fail(e,"Invalid music region service delta");
 const auto snapshot=owner.observe_music();
 if(!observe(state_->controller,state_->external_owner,snapshot,e)||
    !state_->controller.advance(delta,snapshot.any_music_tweening,e)||!state_->player.sync(state_->controller,e))return false;
 if(phase_==MusicRegionServicePhase::Draining){bool allocated=false;for(const auto&v:state_->controller.voices())allocated|=v.allocated;if(!allocated)phase_=MusicRegionServicePhase::Prepared;}
 e.clear();return true;
}
MusicPreparationStep pump_region_music_preparation(MusicRegionService&service,AudioPlayer&owner,uint32_t budget,std::string&e){
 if(service.phase()!=MusicRegionServicePhase::Preparing)return service.prepare_step(budget,e);
 if(!owner.pump_streams(e)){std::string ignored;service.cancel_preparation(ignored);return MusicPreparationStep::Failed;}
 const auto result=service.prepare_step(budget,e);const std::string preparation_error=e;
 std::string pump_error;if(!owner.pump_streams(pump_error)){std::string ignored;service.cancel_preparation(ignored);e=pump_error;return MusicPreparationStep::Failed;}
 e=preparation_error;return result;
}

}
