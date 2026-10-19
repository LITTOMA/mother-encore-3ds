#pragma once
#include "encore/music_players.hpp"
namespace encore::upstream {
struct MusicRoutingPolicy {
    uint32_t maximum_providers=0,maximum_voices=0;
    float master_db=0,silence_db=0;
};
struct MusicBankProvider {
    uint64_t identity=0;
    MusicRegionData regions;
    AudioBank bank;
};
struct MusicPlayerStart {
    uint64_t player=0,generation=0,provider=0;
};
struct MusicPlaybackRoute {
    uint64_t player=0,generation=0,provider=0;
    uint32_t track_id=0,slot=0;
    float gain_db=0;
    bool stream_paused=false;
    double start_seconds=0;
};
// Checked metadata ownership and stable logical voice slots. This does not
// open PCM, allocate DSP memory, stop/start hardware or infer external starts.
// A caller stages this alongside a copied MusicSceneGraph before publication.
class MusicBankRoutes final {
public:
    bool initialize(MusicRoutingPolicy,std::string&);
    bool add_provider(uint64_t,const MusicRegionData&,const AudioBank&,std::string&);
    bool remove_provider(uint64_t,std::string&);
    bool reconcile(const MusicPlayers&,const std::vector<MusicPlayerStart>&,std::string&);
    const MusicBankProvider*provider(uint64_t)const;
    const std::vector<MusicBankProvider>&providers()const{return providers_;}
    const std::vector<MusicPlaybackRoute>&routes()const{return routes_;}
    bool ready()const{return ready_;}
    std::shared_ptr<const MusicPlayersOwner>owner()const{return owner_;}
private:
    std::shared_ptr<const MusicPlayersOwner>owner_;
    MusicRoutingPolicy policy_;
    std::vector<MusicBankProvider>providers_;
    std::vector<MusicPlaybackRoute>routes_;
    uint64_t last_provider_=0;
    bool ready_=false;
};
}
