#include "music_region_service.hpp"
#include <cmath>
namespace encore::ctr {
namespace {bool fail(std::string&e,const char*m){e=m;return false;}}
struct MusicRegionService::State {
 upstream::MusicRegionData data;
 upstream::MusicRegionController controller;
 MusicRegionPlayer player;
 uint64_t external_generation=0;
};
MusicRegionService::MusicRegionService()=default;
MusicRegionService::~MusicRegionService(){shutdown();}
void MusicRegionService::shutdown(){state_.reset();phase_=MusicRegionServicePhase::Dormant;}
uint64_t MusicRegionService::scene_epoch()const{return state_?state_->controller.scene_epoch():0;}
uint32_t MusicRegionService::live_voice_count()const{uint32_t count=0;if(state_)for(const auto&v:state_->controller.voices())count+=v.allocated&&v.playing;return count;}
uint32_t MusicRegionService::submitted_voices()const{return state_?state_->player.submitted_voices():0;}
uint32_t MusicRegionService::buffer_bytes()const{return state_?state_->player.buffer_bytes():0;}
bool MusicRegionService::begin_prepare(const char*regions,const char*bank,const char*root,uint32_t capacity,const AudioPlayer&owner,std::string&e){
 if(state_)return fail(e,"Music region service already prepared; preserve the live owner");
 const auto snapshot=owner.observe_music();
 if(!snapshot.available)return fail(e,"Music region service unavailable: existing NDSP owner is not initialized");
 auto candidate=std::make_unique<State>();
 if(!candidate->data.load_file(regions,e)||!candidate->controller.initialize(candidate->data,capacity,e)||
    !candidate->player.begin_prepare(bank,root,candidate->data,capacity,snapshot.available,snapshot.master_db,e))return false;
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
bool MusicRegionService::observe(upstream::MusicRegionController&controller,uint64_t&known,const MusicObservation&s,std::string&e){
 if(!s.available)return fail(e,"Music region service lost its NDSP owner");
 if(s.generation<known)return fail(e,"Existing music owner generation regressed");
 if(s.playing){if(!s.generation)return fail(e,"Playing external music has no observed identity");if(!controller.observe_external_player({s.generation,true,true},e))return false;known=s.generation;}
 else if(known){if(!controller.observe_external_player({known,false,false},e))return false;}
 e.clear();return true;
}
bool MusicRegionService::commit_scene(uint64_t epoch,const AudioPlayer&owner,std::string&e){
 if(!state_||(phase_!=MusicRegionServicePhase::Prepared&&phase_!=MusicRegionServicePhase::Draining))return fail(e,"Music region scene is not prepared or still owns active areas");
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing)return fail(e,"Music region external DialogueMusic player is outside the mapped handoff");
 auto next=state_->controller;auto generation=state_->external_generation;
 if(!observe(next,generation,snapshot,e)||!next.attach_scene(epoch,e))return false;
 state_->controller=std::move(next);state_->external_generation=generation;phase_=MusicRegionServicePhase::Active;e.clear();return true;
}
bool MusicRegionService::area_enter(uint64_t epoch,std::string_view path,const upstream::MusicRegionContext&context,const AudioPlayer&owner,std::string&e){
 if(!state_)return fail(e,"Music region Area event has no prepared service");
 if(epoch!=scene_epoch()){e.clear();return true;}
 if(phase_!=MusicRegionServicePhase::Active)return fail(e,"Music region Area enter outside active scene");
 const auto snapshot=owner.observe_music();
 if(snapshot.dialogue_music_playing&&context.is_player&&!context.in_cutscene&&!context.in_battle)return fail(e,"Music region external DialogueMusic player is outside the mapped handoff");
 auto next=state_->controller;auto generation=state_->external_generation;
 if(!observe(next,generation,snapshot,e)||!next.enter(epoch,path,context,e))return false;
 state_->controller=std::move(next);state_->external_generation=generation;e.clear();return true;
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
 if(!observe(state_->controller,state_->external_generation,snapshot,e)||
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
