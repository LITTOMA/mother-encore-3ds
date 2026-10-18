#include "encore/music_scene_graph.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*why){e=why;return false;}
bool duration(double value){return std::isfinite(value)&&value>=0&&value<=60;}
}
bool MusicSceneGraph::initialize(MusicPlayersPolicy policy,std::string&e){
    if(players_.ready())return fail(e,"Music scene graph is already initialized");
    if(!players_.initialize(policy,e))return false;
    policy_=policy;return true;
}
bool MusicSceneGraph::attach_scene(uint64_t epoch,const MusicRegionData&data,std::string&e){
    if(!players_.ready()||!data.valid()||!epoch||epoch<=epoch_||active_||
       data.silence_db()!=policy_.silence_db||data.fade_to_seconds()!=policy_.fade_to_seconds)
        return fail(e,"Music graph scene/epoch/tuning is not admissible");
    MusicRegionData retained=data;std::vector<MusicSceneArea>areas(data.regions().size());
    data_=std::move(retained);areas_=std::move(areas);registered_.clear();pending_.clear();epoch_=epoch;active_=true;e.clear();return true;
}
int MusicSceneGraph::area(std::string_view path)const{for(size_t i=0;i<data_.regions().size();++i)if(data_.regions()[i].source_path==path)return int(i);return -1;}
const std::string&MusicSceneGraph::stream(uint32_t index)const{
    const auto id=data_.regions()[index].track_id;
    for(const auto&track:data_.tracks())if(track.id==id)return track.source_path;
    // The checked MusicRegionData parser rejects missing track identities.
    static const std::string empty;return empty;
}
bool MusicSceneGraph::event(uint64_t epoch,std::string_view path,int&index,std::string&e)const{
    if(!players_.ready()||!epoch_)return fail(e,"Music scene graph has no scene");
    if(epoch!=epoch_){index=-1;e.clear();return true;}
    if(!active_)return fail(e,"Music graph scene is not active");
    index=area(path);if(index<0||areas_[size_t(index)].removed)return fail(e,"Music graph callback has no live source area");
    e.clear();return true;
}
bool MusicSceneGraph::enter(uint64_t epoch,std::string_view path,const MusicRegionContext&context,std::string&e){
    int index;if(!event(epoch,path,index,e))return false;if(index<0)return true;
    const auto&binding=data_.regions()[size_t(index)];
    // disabled controls the source CollisionShape; the callback itself only
    // checks player/UI/flags. The physical Area emitter owns disabled filtering.
    if(!context.is_player||context.in_cutscene||context.in_battle){e.clear();return true;}
    if((!binding.appear_flag.empty()||!binding.disappear_flag.empty())&&!context.flag)return fail(e,"Music graph flag context is unavailable");
    if((!binding.appear_flag.empty()&&!context.flag(binding.appear_flag))||
       (!binding.disappear_flag.empty()&&context.flag(binding.disappear_flag))){e.clear();return true;}
    auto next=*this;auto&state=next.areas_[size_t(index)];state.inside=true;
    if(!state.registered){
        state.registered=true;next.registered_.push_back(uint32_t(index));
        MusicLoopRequest request{next.stream(uint32_t(index)),next.stream(next.registered_.front()),next.registered_.front()==uint32_t(index),binding.volume_db,policy_.silence_db,binding.fadein_seconds,policy_.fade_to_seconds};
        if(!next.players_.choose_loop(request,state.player,e))return false;
    }
    *this=std::move(next);e.clear();return true;
}
bool MusicSceneGraph::exit(uint64_t epoch,std::string_view path,const MusicRegionContext&context,std::string&e){
    int index;if(!event(epoch,path,index,e))return false;if(index<0)return true;
    if(!context.is_player||!context.has_collisions||context.in_cutscene){e.clear();return true;}
    if(pending_.size()>=1024)return fail(e,"Music graph deferred callback capacity exhausted");
    areas_[size_t(index)].inside=false;areas_[size_t(index)].pending_exit=true;pending_.push_back(uint32_t(index));e.clear();return true;
}
bool MusicSceneGraph::stop(uint32_t index,double seconds,std::string&e){
    auto&state=areas_[index];state.registered=false;
    registered_.erase(std::remove(registered_.begin(),registered_.end(),index),registered_.end());
    const auto*attached=players_.find(state.player);bool shared=false;
    if(attached)for(auto other:registered_){const auto*node=players_.find(areas_[other].player);if(node&&node->stream==stream(index)){shared=true;break;}}
    if(attached&&!shared){
        if(seconds==0){if(!players_.stop(state.player,e))return false;}
        else if(!players_.fade_out(state.player,seconds,e))return false;
    }
    state.player=MusicLoopChoice::no_player;e.clear();return true;
}
bool MusicSceneGraph::stop_area(uint64_t epoch,std::string_view path,double seconds,std::string&e){
    int index;if(!event(epoch,path,index,e))return false;if(index<0)return true;
    if(!duration(seconds))return fail(e,"Music graph stop duration is invalid");
    auto next=*this;if(!next.stop(uint32_t(index),seconds,e))return false;*this=std::move(next);return true;
}
bool MusicSceneGraph::stop_on_door(uint64_t epoch,double seconds,std::string&e){
    if(!players_.ready()||!epoch_)return fail(e,"Music graph door has no scene");
    if(epoch!=epoch_){e.clear();return true;}
    if(!active_||!duration(seconds))return fail(e,"Music graph door phase/duration is invalid");
    auto next=*this;
    // SceneTransition iterates the live registration array; stop_music erases
    // its member, so the next member is skipped. Preserve that source order.
    for(size_t cursor=0;cursor<next.registered_.size();++cursor)if(!next.stop(next.registered_[cursor],seconds,e))return false;
    *this=std::move(next);e.clear();return true;
}
bool MusicSceneGraph::idle_frame(uint64_t epoch,std::string&e){
    if(!players_.ready()||!epoch_)return fail(e,"Music graph idle has no scene");
    if(epoch!=epoch_){e.clear();return true;}if(!active_)return fail(e,"Music graph idle scene is inactive");
    auto next=*this;auto pending=std::move(next.pending_);next.pending_.clear();
    for(auto index:pending){auto&state=next.areas_[index];state.pending_exit=false;if(!state.removed&&!state.inside&&!next.stop(index,next.data_.regions()[index].fadeout_seconds,e))return false;}
    *this=std::move(next);e.clear();return true;
}
bool MusicSceneGraph::tree_exit(uint64_t epoch,std::string_view path,std::string&e){
    int index;if(!event(epoch,path,index,e))return false;if(index<0)return true;
    auto next=*this;auto&state=next.areas_[size_t(index)];
    if(state.registered&&!next.stop(uint32_t(index),data_.regions()[size_t(index)].fadeout_seconds,e))return false;
    state.inside=state.registered=state.pending_exit=false;state.removed=true;
    next.pending_.erase(std::remove(next.pending_.begin(),next.pending_.end(),uint32_t(index)),next.pending_.end());
    *this=std::move(next);e.clear();return true;
}
bool MusicSceneGraph::finish_scene(uint64_t epoch,std::string&e){
    if(!players_.ready()||!epoch_)return fail(e,"Music graph finish has no scene");
    if(epoch!=epoch_){e.clear();return true;}if(!active_)return fail(e,"Music graph finish scene is inactive");
    for(const auto&state:areas_)if(!state.removed)return fail(e,"Music graph source tree exits are incomplete");
    active_=false;e.clear();return true;
}
bool MusicSceneGraph::advance(double delta,bool external,std::string&e){return players_.advance(delta,external,e);}
bool MusicSceneGraph::flush_deletions(std::string&e){return players_.flush_deletions(e);}
}
