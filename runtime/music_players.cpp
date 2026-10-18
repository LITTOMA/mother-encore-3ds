#include "encore/music_players.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
bool MusicPlayers::play_loop(uint64_t id,std::string_view source,std::string&e){return play_track(id,source,source,e);}
bool MusicPlayers::play_track(uint64_t id,std::string_view source,std::string_view loop,std::string&e){return play_track(id,source,loop,0,e);}
bool MusicPlayers::play_track(uint64_t id,std::string_view source,std::string_view loop,double start,std::string&e){
    auto*node=mutable_node(id);
    if(!ready_||!node||node->generation==UINT64_MAX||!std::isfinite(start)||start<0||start>std::numeric_limits<float>::max()||!valid_music_stream_uri(source)||(!loop.empty()&&!valid_music_stream_uri(loop)))return bad(e,"Music track identity/source/loop is invalid");
    // Source would emit an engine error when connecting the same one-shot
    // removal twice. Refuse that unsupported sequence without partial playback.
    if(loop.empty()&&node->remove_on_finished)return bad(e,"Music one-shot finished callback is already connected");
    std::string retained(source),repeat(loop);
    node->stream=std::move(retained);node->finished_loop=std::move(repeat);
    node->finished_index=loop.empty()?UINT32_MAX:uint32_t(node-nodes_.data());
    node->remove_on_finished|=loop.empty();node->playing=true;node->start_seconds=double(float(start));++node->generation;
    e.clear();return true;
}
bool MusicPlayers::signal_finished(uint64_t id,uint64_t generation,std::string&e){
    const auto*node=find(id);
    if(!ready_||!node||!generation||node->generation!=generation)return bad(e,"Music finished signal has no current playback identity");
    if(!node->finished_loop.empty()&&deferred_.size()>=1024)return bad(e,"Music deferred finished callback capacity exhausted");
    MusicPlayers next=*this;auto*target=next.mutable_node(id);target->playing=false;
    if(target->remove_on_finished){target->remove_on_finished=false;if(!next.mark_remove(id,true,e))return false;}
    if(!target->finished_loop.empty())next.deferred_.push_back({target->finished_loop,target->finished_index});
    *this=std::move(next);e.clear();return true;
}
bool MusicPlayers::dispatch_finished(std::string&e){
    if(!ready_)return bad(e,"Music players are not initialized");
    MusicPlayers next=*this;auto pending=std::move(next.deferred_);next.deferred_.clear();
    for(const auto&event:pending){
        // Original get_audio_player returns null after child-index shifts. A
        // queued callback deliberately uses that saved index, not its emitter.
        if(event.index>=next.nodes_.size())continue;
        const auto id=next.nodes_[event.index].identity;
        if(!next.play_track(id,event.stream,event.stream,e))return false;
    }
    *this=std::move(next);e.clear();return true;
}
bool MusicPlayers::stop(uint64_t id,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node)return bad(e,"Loop player stop identity is unavailable");
    node->playing=false;node->finished_loop.clear();node->finished_index=UINT32_MAX;e.clear();return true;
}
bool MusicPlayers::pause_all(std::string&e){
    if(!ready_)return bad(e,"Music pause owner is unavailable");
    MusicPlayers next=*this;bool active=false;
    if(!next.tweens_stopped_)for(const auto&node:next.nodes_)active|=node.tween.active;
    if(active){
        // Tween.seek beyond runtime applies final values without emitting its
        // completion signal. stop_all retains the tracks, reset to time zero.
        for(auto&node:next.nodes_)if(node.tween.active){node.gain_db=node.tween.to;node.tween.elapsed=0;}
        for(const auto&node:next.nodes_)if(!node.playing||node.gain_db<=next.policy_.silence_db){
            if(std::find(next.after_tween_deletions_.begin(),next.after_tween_deletions_.end(),node.identity)==next.after_tween_deletions_.end())
                next.after_tween_deletions_.push_back(node.identity);
        }
        next.tweens_stopped_=true;
    }
    for(auto&node:next.nodes_)node.stream_paused=true;
    *this=std::move(next);e.clear();return true;
}
bool MusicPlayers::resume_all(std::string&e){
    if(!ready_)return bad(e,"Music resume owner is unavailable");
    for(auto&node:nodes_)node.stream_paused=false;e.clear();return true;
}
bool MusicPlayers::set_volume(uint64_t id,float value,std::string&e){
    auto*node=mutable_node(id);if(!ready_||!node||!gain(value))return bad(e,"Loop player volume identity/value is invalid");
    node->gain_db=value;e.clear();return true;
}
bool MusicPlayers::tween(uint64_t id,float from,float to,double seconds,MusicRegionCurve curve,std::string&e){
    auto*node=mutable_node(id);float checked;
    if(!ready_||!node)return bad(e,"Loop player tween identity is unavailable");
    if(!sample_music_tween(from,to,seconds,0,curve,checked,e))return false;
    const bool restarting=tweens_stopped_;
    if(restarting){
        for(auto&other:nodes_)if(other.tween.active){other.tween.elapsed=0;other.gain_db=other.tween.from;}
        tweens_stopped_=false;
    }
    node->tween={from,to,seconds,0,curve,true};if(restarting)node->gain_db=from;e.clear();return true;
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
        if(std::find(next.after_tween_deletions_.begin(),next.after_tween_deletions_.end(),id)!=next.after_tween_deletions_.end())
            return bad(e,"Deleting a paused Tween waiter before completion is unsupported");
        const bool ensure=found->ensure_empty_on_delete;next.nodes_.erase(found);
        if(ensure&&next.nodes_.empty()){uint64_t empty;if(!next.add_player(empty,e))return false;}
    }
    *this=std::move(next);e.clear();return true;
}
bool MusicPlayers::advance(double delta,bool external,std::string&e){
    if(!ready_||!std::isfinite(delta)||delta<0||delta>1)return bad(e,"Loop player frame delta is invalid");
    MusicPlayers next=*this;bool active=false;
    if(next.tweens_stopped_){e.clear();return true;}
    for(auto&node:next.nodes_)if(node.tween.active){
        auto&t=node.tween;t.elapsed+=delta;
        if(!sample_music_tween(t.from,t.to,t.duration,t.elapsed,t.curve,node.gain_db,e))return false;
        t.active=t.elapsed<t.duration;active|=t.active;
    }
    if(!active&&!external&&!next.after_tween_deletions_.empty()){
        for(auto id:next.after_tween_deletions_)if(!next.mark_remove(id,true,e))return false;
        next.after_tween_deletions_.clear();
    }
    if(next.cleanup_waiting_&&!active&&!external){
        next.cleanup_waiting_=false;
        for(const auto&node:next.nodes_)if(!node.playing||node.gain_db<=next.policy_.silence_db)
            if(!next.mark_remove(node.identity,true,e))return false;
    }
    *this=std::move(next);e.clear();return true;
}
}
