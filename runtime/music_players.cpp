#include "encore/music_players.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*why){e=why;return false;}
bool gain(float value){return std::isfinite(value)&&value>=-120&&value<=24;}
bool duration(double value){return std::isfinite(value)&&value>=0&&value<=60;}
}
const MusicPlayerNode*MusicPlayers::find(uint64_t id)const{for(const auto&n:nodes_)if(n.identity==id)return &n;return nullptr;}
MusicPlayerNode*MusicPlayers::mutable_node(uint64_t id){for(auto&n:nodes_)if(n.identity==id)return &n;return nullptr;}
std::vector<MusicPlayerSnapshot>MusicPlayers::snapshot()const{
    std::vector<MusicPlayerSnapshot>out;out.reserve(nodes_.size());
    for(const auto&n:nodes_)out.push_back({n.identity,n.stream,n.playing,n.gain_db});return out;
}
bool MusicPlayers::initialize(MusicPlayersPolicy policy,std::string&e){
    if(ready_||!gain(policy.silence_db)||policy.silence_db>-20||!gain(policy.empty_player_gain_db)||
       !duration(policy.fade_to_seconds)||!policy.maximum_nodes||policy.maximum_nodes>256)
        return bad(e,"Loop music player policy/owner is invalid");
    MusicPlayers next;next.owner_=std::make_shared<const MusicPlayersOwner>();next.ready_=true;next.policy_=policy;uint64_t empty;
    if(!next.add_player(empty,e))return false;*this=std::move(next);e.clear();return true;
}
bool MusicPlayers::add_player(uint64_t&id,std::string&e){
    if(!ready_||nodes_.size()>=policy_.maximum_nodes||next_identity_==MusicLoopChoice::no_player)
        return bad(e,"Loop music player node capacity/identity exhausted");
    MusicPlayerNode next;next.identity=next_identity_;next.gain_db=policy_.empty_player_gain_db;
    nodes_.push_back(std::move(next));id=next_identity_++;e.clear();return true;
}
bool MusicPlayers::play_loop(uint64_t id,std::string_view source,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node||node->generation==UINT64_MAX)return bad(e,"Loop player playback identity is unavailable");
    // Reuse the admitted URI domain without borrowing it after this call.
    MusicLoopRequest request{source,source,true,policy_.empty_player_gain_db,policy_.silence_db,0,policy_.fade_to_seconds};
    MusicLoopChoice checked;if(!choose_music_loop({{id,{},false,policy_.empty_player_gain_db}},request,checked,e))return false;
    std::string retained(source);node->stream=std::move(retained);node->playing=true;++node->generation;
    e.clear();return true;
}
bool MusicPlayers::stop(uint64_t id,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node)return bad(e,"Loop player stop identity is unavailable");
    node->playing=false;e.clear();return true;
}
bool MusicPlayers::set_volume(uint64_t id,float value,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node||!gain(value))return bad(e,"Loop player volume identity/value is invalid");
    node->gain_db=value;e.clear();return true;
}
bool MusicPlayers::tween(uint64_t id,float from,float to,double seconds,MusicRegionCurve curve,std::string&e){
    auto*node=mutable_node(id);float checked;
    if(!ready_||!node)return bad(e,"Loop player tween identity is unavailable");
    if(!sample_music_tween(from,to,seconds,0,curve,checked,e))return false;
    node->tween={from,to,seconds,0,curve,true};e.clear();return true;
}
bool MusicPlayers::fade_in(uint64_t id,float target,double seconds,std::string&e){return tween(id,policy_.silence_db,target,seconds,MusicRegionCurve::QuartOut,e);}
bool MusicPlayers::fade_to(uint64_t id,float target,double seconds,std::string&e){
    const auto*node=find(id);if(!ready_||!node||!gain(target)||!duration(seconds))return bad(e,"Loop player fadeto identity/value is invalid");
    if(node->gain_db==target){e.clear();return true;}
    return tween(id,node->gain_db,target,seconds,MusicRegionCurve::Linear,e);
}
bool MusicPlayers::fade_out(uint64_t id,double seconds,std::string&e){
    const auto*node=find(id);if(!ready_||!node||!duration(seconds))return bad(e,"Loop player fadeout identity/value is invalid");
    if(!node->playing){e.clear();return true;}
    const auto from=node->gain_db;
    if(!tween(id,from,policy_.silence_db,seconds,MusicRegionCurve::QuartIn,e))return false;
    cleanup_waiting_=true;return true;
}
bool MusicPlayers::choose_loop(const MusicLoopRequest&request,uint64_t&attached,std::string&e){
    if(!ready_||request.silence_db!=policy_.silence_db||request.fade_to_seconds!=policy_.fade_to_seconds)
        return bad(e,"Loop chooser policy differs from player owner");
    MusicLoopChoice choice;if(!choose_music_loop(snapshot(),request,choice,e))return false;
    // Input views may refer to this owner's current strings. Retain before
    // committing the staged copy, including when appending reallocates nodes.
    const std::string source(request.stream);MusicPlayers next=*this;uint64_t id=choice.player;
    if(choice.append_player){
        if(!next.add_player(id,e)||!next.set_volume(id,choice.initial_gain_db,e)||
           !next.fade_in(id,choice.target_gain_db,choice.seconds,e)||!next.play_loop(id,source,e))return false;
    }else if(choice.restart_stream){
        if(!next.play_loop(id,source,e)||!next.set_volume(id,choice.target_gain_db,e))return false;
    }else if(choice.volume==MusicLoopVolume::FadeTo){
        if(!next.fade_to(id,choice.target_gain_db,choice.seconds,e))return false;
    }
    *this=std::move(next);attached=id;e.clear();return true;
}
bool MusicPlayers::mark_remove(uint64_t id,bool ensure,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node)return bad(e,"Loop player deletion identity is unavailable");
    if(!node->pending_delete){deletions_.push_back(id);node->pending_delete=true;}
    node->ensure_empty_on_delete|=ensure;e.clear();return true;
}
bool MusicPlayers::queue_remove(uint64_t id,std::string&e){return mark_remove(id,true,e);}
bool MusicPlayers::queue_clear(std::string&e){
    if(!ready_)return bad(e,"Loop players are not initialized");
    for(auto&node:nodes_){if(!node.pending_delete){deletions_.push_back(node.identity);node.pending_delete=true;}}
    e.clear();return true;
}
bool MusicPlayers::flush_deletions(std::string&e){
    if(!ready_)return bad(e,"Loop players are not initialized");MusicPlayers next=*this;
    auto pending=std::move(next.deletions_);next.deletions_.clear();
    for(auto id:pending){
        const auto found=std::find_if(next.nodes_.begin(),next.nodes_.end(),[&](const auto&node){return node.identity==id;});
        if(found==next.nodes_.end())return bad(e,"Loop player deletion queue lost its identity");
        const bool ensure=found->ensure_empty_on_delete;next.nodes_.erase(found);
        if(ensure&&next.nodes_.empty()){uint64_t empty;if(!next.add_player(empty,e))return false;}
    }
    *this=std::move(next);e.clear();return true;
}
bool MusicPlayers::advance(double delta,bool external,std::string&e){
    if(!ready_||!std::isfinite(delta)||delta<0||delta>1)return bad(e,"Loop player frame delta is invalid");
    MusicPlayers next=*this;bool active=false;
    for(auto&node:next.nodes_)if(node.tween.active){
        auto&t=node.tween;t.elapsed+=delta;
        if(!sample_music_tween(t.from,t.to,t.duration,t.elapsed,t.curve,node.gain_db,e))return false;
        t.active=t.elapsed<t.duration;active|=t.active;
    }
    if(next.cleanup_waiting_&&!active&&!external){
        next.cleanup_waiting_=false;
        for(const auto&node:next.nodes_)if(!node.playing||node.gain_db<=next.policy_.silence_db)
            if(!next.mark_remove(node.identity,true,e))return false;
    }
    *this=std::move(next);e.clear();return true;
}
}
