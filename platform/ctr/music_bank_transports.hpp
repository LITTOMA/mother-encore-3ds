#pragma once
#include "music_region_player.hpp"
#include "encore/music_library_bindings.hpp"
#include <memory>
namespace encore::ctr {
// Physical providers for a checked global plan. The caller owns source scene
// callbacks, clocks and publication; this class never infers a MusicChanger.
// The existing AudioPlayer must own NDSP for this entire object's lifetime.
struct MusicBankFinished{uint64_t provider=0,player=0,generation=0;};
class MusicBankTransports final {
public:
    MusicBankTransports();~MusicBankTransports();
    MusicBankTransports(const MusicBankTransports&)=delete;
    MusicBankTransports&operator=(const MusicBankTransports&)=delete;
    bool initialize(uint32_t maximum_providers,float user_music_db,std::string&);
    bool begin_prepare(uint64_t,const char*regions,const char*bank,const char*root,uint32_t voices,bool ndsp_available,float master_db,std::string&);
    bool begin_library(uint64_t,const upstream::MusicLibraryBindings&,uint32_t track,const char*pcm_root,uint32_t voices,bool ndsp_available,float master_db,std::string&);
    MusicPreparationStep prepare_step(uint64_t,uint32_t byte_budget,std::string&);
    bool cancel(uint64_t,std::string&);
    bool publish(uint64_t,upstream::MusicBankRoutes&,std::string&);
    bool check_routes(const upstream::MusicBankRoutes&,std::string&)const;
    bool sync_routes(const upstream::MusicBankRoutes&,std::string&);
    bool retire(uint64_t,upstream::MusicBankRoutes&,std::string&);
    bool pump_streams(std::string&);
    std::vector<MusicBankFinished>take_finished();
    MusicPositionStep playback_position(const upstream::MusicBankRoutes&,uint64_t player,uint64_t generation,double&seconds,std::string&)const;
    bool set_user_music_db(float,std::string&);
    bool set_paused(bool,std::string&);
    // Returned metadata is borrowed until that provider is cancelled/retired.
    const upstream::MusicRegionData*data(uint64_t)const;
    const upstream::AudioBank*bank(uint64_t)const;
    uint32_t provider_count()const;
    uint64_t preparing()const;
    void shutdown();
private:
    struct Provider;
    Provider*find(uint64_t);const Provider*find(uint64_t)const;
    std::vector<std::unique_ptr<Provider>>providers_;
    uint64_t last_provider_=0;
    uint32_t maximum_providers_=0;
    float user_music_db_=0;
    bool ready_=false,paused_=false;
};
}
