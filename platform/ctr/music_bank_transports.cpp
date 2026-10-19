#include "music_bank_transports.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {namespace {
bool fail(std::string&e,const char*why){e=why;return false;}
bool gain(float db){return std::isfinite(db)&&db>=-120&&db<=0;}
}
struct MusicBankTransports::Provider {
    uint64_t identity=0;
    upstream::MusicRegionData data;
    MusicRegionPlayer player;
    bool prepared=false,published=false,bound=false;
};
MusicBankTransports::MusicBankTransports()=default;
MusicBankTransports::~MusicBankTransports(){shutdown();}
MusicBankTransports::Provider*MusicBankTransports::find(uint64_t id){for(auto&p:providers_)if(p->identity==id)return p.get();return nullptr;}
const MusicBankTransports::Provider*MusicBankTransports::find(uint64_t id)const{for(const auto&p:providers_)if(p->identity==id)return p.get();return nullptr;}
uint32_t MusicBankTransports::provider_count()const{return uint32_t(providers_.size());}
uint64_t MusicBankTransports::preparing()const{for(const auto&p:providers_)if(!p->prepared)return p->identity;return 0;}
const upstream::MusicRegionData*MusicBankTransports::data(uint64_t id)const{const auto*p=find(id);return p&&p->prepared?&p->data:nullptr;}
const upstream::AudioBank*MusicBankTransports::bank(uint64_t id)const{const auto*p=find(id);return p&&p->prepared?&p->player.bank():nullptr;}
bool MusicBankTransports::initialize(uint32_t maximum,float volume,std::string&e){
    if(ready_||!maximum||maximum>64||!gain(volume))return fail(e,"Global music transport policy/owner is invalid");
    maximum_providers_=maximum;user_music_db_=volume;ready_=true;e.clear();return true;
}
bool MusicBankTransports::begin_prepare(uint64_t id,const char*regions,const char*bank_path,const char*root,uint32_t voices,bool available,float master,std::string&e){
    if(!ready_||!available||!id||id==upstream::MusicLoopChoice::no_player||id<=last_provider_||preparing()||providers_.size()>=maximum_providers_)return fail(e,"Global music candidate identity/capacity/NDSP owner is unavailable");
    auto candidate=std::make_unique<Provider>();candidate->identity=id;
    if(!candidate->data.load_file(regions,e)||!candidate->player.begin_prepare(bank_path,root,candidate->data,voices,available,master,e)||!candidate->player.set_user_music_db(user_music_db_))return false;
    providers_.push_back(std::move(candidate));last_provider_=id;e.clear();return true;
}
bool MusicBankTransports::begin_library(uint64_t id,const upstream::MusicLibraryBindings&library,uint32_t track,const char*root,uint32_t voices,bool available,float master,std::string&e){
    if(!ready_||!available||!id||id==upstream::MusicLoopChoice::no_player||id<=last_provider_||preparing()||providers_.size()>=maximum_providers_||!library.admitted()||!library.find(track)||!root||!voices||voices>MusicRegionPlayer::maximum_voices)return fail(e,"Music library candidate identity/binding/capacity/owner is unavailable");
    auto candidate=std::make_unique<Provider>();candidate->identity=id;upstream::AudioBank bank;
    if(!library.read_track(track,candidate->data,bank,e)||!candidate->player.begin_prepare(bank,root,candidate->data,voices,available,master,e)||!candidate->player.set_user_music_db(user_music_db_))return false;
    providers_.push_back(std::move(candidate));last_provider_=id;e.clear();return true;
}
MusicPreparationStep MusicBankTransports::prepare_step(uint64_t id,uint32_t budget,std::string&e){
    auto*p=find(id);if(!ready_||!p||p->published){e="Global music candidate is unavailable or already published";return MusicPreparationStep::Failed;}
    const auto step=p->player.prepare_step(budget,e);
    if(step==MusicPreparationStep::Failed){providers_.erase(std::remove_if(providers_.begin(),providers_.end(),[&](const auto&v){return v->identity==id;}),providers_.end());return step;}
    if(step==MusicPreparationStep::Ready){p->prepared=true;if(!p->player.set_paused(paused_,e))return MusicPreparationStep::Failed;}
    return step;
}
bool MusicBankTransports::cancel(uint64_t id,std::string&e){
    auto*p=find(id);if(!ready_||!p||p->published)return fail(e,"Only an unpublished music candidate may be cancelled");
    providers_.erase(std::remove_if(providers_.begin(),providers_.end(),[&](const auto&v){return v->identity==id;}),providers_.end());e.clear();return true;
}
bool MusicBankTransports::publish(uint64_t id,upstream::MusicBankRoutes&routes,std::string&e){
    auto*p=find(id);if(!ready_||!p||!p->prepared||p->published)return fail(e,"Global music provider is not fresh and prepared");
    if(!routes.add_provider(id,p->data,p->player.bank(),e))return false;
    p->published=true;e.clear();return true;
}
bool MusicBankTransports::check_routes(const upstream::MusicBankRoutes&routes,std::string&e)const{
    if(!ready_||!routes.ready()||!routes.owner())return fail(e,"Global music plan is not published");
    for(const auto&metadata:routes.providers()){const auto*p=find(metadata.identity);if(!p||!p->prepared||!p->published)return fail(e,"Global music plan names an unprepared transport");}
    for(const auto&p:providers_)if(p->published&&!p->player.check_routes(routes,p->identity,e))return false;
    e.clear();return true;
}
bool MusicBankTransports::sync_routes(const upstream::MusicBankRoutes&routes,std::string&e){
    if(!check_routes(routes,e))return false;
    // Every provider was checked before the first queue can change. Release all
    // obsolete leases before any provider tries to acquire a newly free channel.
    for(auto&p:providers_)if(p->published){if(!p->player.release_routes(routes,p->identity,e))return false;p->bound=true;}
    for(auto&p:providers_)if(p->published&&!p->player.sync_routes(routes,p->identity,e))return false;
    e.clear();return true;
}
bool MusicBankTransports::retire(uint64_t id,upstream::MusicBankRoutes&routes,std::string&e){
    auto*p=find(id);if(!ready_||!p||!p->published)return fail(e,"Global music retirement has no published provider");
    auto next=routes;if(!next.remove_provider(id,e))return false;
    if(p->bound&&(!p->player.check_routes(next,id,e)||!p->player.release_routes(next,id,e)||!p->player.sync_routes(next,id,e)))return false;
    providers_.erase(std::remove_if(providers_.begin(),providers_.end(),[&](const auto&v){return v->identity==id;}),providers_.end());routes=std::move(next);e.clear();return true;
}
bool MusicBankTransports::pump_streams(std::string&e){
    if(!ready_)return fail(e,"Global music transports are not initialized");
    for(auto&p:providers_)if(p->bound&&!p->player.pump_streams(e))return false;
    e.clear();return true;
}
std::vector<MusicBankFinished>MusicBankTransports::take_finished(){
    std::vector<MusicBankFinished>out;
    for(auto&p:providers_)for(const auto&event:p->player.take_finished())out.push_back({p->identity,event.player,event.generation});
    // Source AudioPlayers children retain insertion order when predecessors
    // disappear; node IDs follow that order, rather than provider grouping.
    std::stable_sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.player!=b.player?a.player<b.player:a.generation<b.generation;});return out;
}
bool MusicBankTransports::set_user_music_db(float db,std::string&e){
    if(!ready_||!gain(db))return fail(e,"Global music user volume is invalid");
    for(auto&p:providers_)if(!p->player.set_user_music_db(db))return fail(e,"Global music provider volume rejected");
    user_music_db_=db;e.clear();return true;
}
MusicPositionStep MusicBankTransports::playback_position(const upstream::MusicBankRoutes&routes,uint64_t player,uint64_t generation,double&seconds,std::string&e)const{
    if(!check_routes(routes,e))return MusicPositionStep::Failed;
    for(const auto&r:routes.routes())if(r.player==player&&r.generation==generation){const auto*p=find(r.provider);if(!p||!p->prepared||!p->published){e="Music position provider is unavailable";return MusicPositionStep::Failed;}return p->player.playback_position(routes,r.provider,player,generation,seconds,e);}
    e="Music position has no current playback route";return MusicPositionStep::Failed;
}
bool MusicBankTransports::set_paused(bool value,std::string&e){
    if(!ready_)return fail(e,"Global music transports are not initialized");
    for(auto&p:providers_)if(p->prepared&&!p->player.set_paused(value,e))return false;
    paused_=value;e.clear();return true;
}
void MusicBankTransports::shutdown(){providers_.clear();last_provider_=0;maximum_providers_=0;user_music_db_=0;ready_=paused_=false;}
}
