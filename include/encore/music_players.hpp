#pragma once
#include "encore/music_player_choice.hpp"
#include <memory>
namespace encore::upstream {
struct MusicPlayersOwner final {};
struct MusicPlayersPolicy {
    float silence_db=0,empty_player_gain_db=0;
    double fade_to_seconds=0;
    uint32_t maximum_nodes=0;
};
struct MusicPlayerTween {
    float from=0,to=0;
    double duration=0,elapsed=0;
    MusicRegionCurve curve=MusicRegionCurve::Linear;
    bool active=false;
};
struct MusicPlayerNode {
    uint64_t identity=0,generation=0;
    std::string stream;
    float gain_db=0;
    MusicPlayerTween tween;
    bool playing=false,pending_delete=false,ensure_empty_on_delete=false;
};
// Portable loop-player property/lifetime state. Media admission and waveform
// ownership stay outside this class; no scene, resource buffer or DSP pointer
// is borrowed. This is not the complete audioManager finished/name/bus API.
class MusicPlayers final {
public:
    bool initialize(MusicPlayersPolicy,std::string&);
    bool ready()const{return ready_;}
    std::shared_ptr<const MusicPlayersOwner>owner()const{return owner_;}
    bool add_player(uint64_t&identity,std::string&);
    bool play_loop(uint64_t,std::string_view,std::string&);
    bool stop(uint64_t,std::string&);
    bool set_volume(uint64_t,float,std::string&);
    bool fade_in(uint64_t,float,double,std::string&);
    bool fade_to(uint64_t,float,double,std::string&);
    bool fade_out(uint64_t,double,std::string&);
    bool choose_loop(const MusicLoopRequest&,uint64_t&attached,std::string&);
    bool queue_remove(uint64_t,std::string&);
    bool queue_clear(std::string&);
    // Source queue_free is deferred: pending nodes remain addressable until
    // the owning frame reaches its deletion boundary.
    bool flush_deletions(std::string&);
    bool advance(double delta,bool external_tween_active,std::string&);
    const std::vector<MusicPlayerNode>&nodes()const{return nodes_;}
    const MusicPlayerNode*find(uint64_t)const;
    std::vector<MusicPlayerSnapshot>snapshot()const;
    bool cleanup_waiting()const{return cleanup_waiting_;}
private:
    MusicPlayerNode*mutable_node(uint64_t);
    bool tween(uint64_t,float,float,double,MusicRegionCurve,std::string&);
    bool mark_remove(uint64_t,bool,std::string&);
    std::shared_ptr<const MusicPlayersOwner>owner_;
    MusicPlayersPolicy policy_;
    std::vector<MusicPlayerNode>nodes_;
    std::vector<uint64_t>deletions_;
    uint64_t next_identity_=0;
    bool ready_=false,cleanup_waiting_=false;
};
}
