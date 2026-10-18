#pragma once
#include "encore/music_players.hpp"
namespace encore::upstream {
struct MusicSceneArea {
    uint64_t player=MusicLoopChoice::no_player;
    bool inside=false,registered=false,pending_exit=false,removed=false;
};
// One scene's source callbacks over the persistent, global loop-player list.
// Scene binding data is owned, so old Room/Field resource buffers may be freed.
// No geometry, media/bank routing, non-loop or diegetic playback is inferred.
class MusicSceneGraph final {
public:
    bool initialize(MusicPlayersPolicy,std::string&);
    bool attach_scene(uint64_t,const MusicRegionData&,std::string&);
    bool enter(uint64_t,std::string_view,const MusicRegionContext&,std::string&);
    bool exit(uint64_t,std::string_view,const MusicRegionContext&,std::string&);
    bool idle_frame(uint64_t,std::string&);
    bool tree_exit(uint64_t,std::string_view,std::string&);
    bool stop_area(uint64_t,std::string_view,double,std::string&);
    bool stop_on_door(uint64_t,double,std::string&);
    bool finish_scene(uint64_t,std::string&);
    bool advance(double delta,bool external_tween_active,std::string&);
    bool flush_deletions(std::string&);
    MusicPlayers&players(){return players_;}
    const MusicPlayers&players()const{return players_;}
    const MusicRegionData&data()const{return data_;}
    const std::vector<MusicSceneArea>&areas()const{return areas_;}
    const std::vector<uint32_t>&registered()const{return registered_;}
    uint64_t scene_epoch()const{return epoch_;}
    bool active()const{return active_;}
private:
    int area(std::string_view)const;
    const std::string&stream(uint32_t)const;
    bool event(uint64_t,std::string_view,int&,std::string&)const;
    bool stop(uint32_t,double,std::string&);
    MusicPlayers players_;
    MusicPlayersPolicy policy_;
    MusicRegionData data_;
    std::vector<MusicSceneArea>areas_;
    std::vector<uint32_t>registered_,pending_;
    uint64_t epoch_=0;
    bool active_=false;
};
}
