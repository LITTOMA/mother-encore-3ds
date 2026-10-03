#pragma once
#include "encore/animation.hpp"
#include "encore/cutscene_camera.hpp"
#include "encore/movement.hpp"
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace encore::upstream {
class OpeningWorld;
class PersistentPlayerOwner;

// One player and camera object survive scene ownership changes. Runtime scene
// bindings may retain this state while preparing, but only the active binding
// may write it. Room/profile data borrowed by the camera must outlive this owner.
class PersistentPlayerState final {
public:
    PersistentPlayerState(const PersistentPlayerState&)=delete;
    PersistentPlayerState& operator=(const PersistentPlayerState&)=delete;
    PersistentPlayerState(PersistentPlayerState&&)=delete;
    PersistentPlayerState& operator=(PersistentPlayerState&&)=delete;
    const WalkState& player()const{return player_;}
    AnimationSample animation()const{return playback_.sample;}
    const CutsceneCamera& camera()const{return camera_;}
    std::string_view party_leader()const{return party_leader_;}
    bool paused()const{return house_paused_;}
    bool battle_visible()const{return return_player_visible_;}
private:
    PersistentPlayerState()=default;
    ~PersistentPlayerState()=default;
    size_t references_=1;
    WalkState player_{};
    AnimationPlayback playback_{0,{0,true,false}};
    CutsceneCamera camera_;
    std::string party_leader_;
    uint32_t clip_=kRoomNoIndex;
    bool return_player_visible_=false,house_paused_=false;
    bool initialized_=false;
    friend class PersistentPlayerOwner;
    friend class OpeningWorld;
};

// Intrusive strong reference for the single game thread. Copying a prepared
// scene's reference allocates nothing. Creating the owner uses checked nothrow
// allocation rather than an additional shared_ptr control-block allocation.
class PersistentPlayerOwner final {
public:
    PersistentPlayerOwner()=default;
    static PersistentPlayerOwner create(){return PersistentPlayerOwner(new(std::nothrow) PersistentPlayerState);}
    PersistentPlayerOwner(const PersistentPlayerOwner& other):state_(other.state_){retain();}
    PersistentPlayerOwner(PersistentPlayerOwner&& other)noexcept:state_(std::exchange(other.state_,nullptr)){}
    PersistentPlayerOwner& operator=(const PersistentPlayerOwner& other){
        if(this!=&other){PersistentPlayerOwner next(other);swap(next);}return *this;
    }
    PersistentPlayerOwner& operator=(PersistentPlayerOwner&& other)noexcept{
        if(this!=&other){release();state_=std::exchange(other.state_,nullptr);}return *this;
    }
    ~PersistentPlayerOwner(){release();}
    explicit operator bool()const{return state_!=nullptr;}
    const PersistentPlayerState* get()const{return state_;}
    void swap(PersistentPlayerOwner& other)noexcept{std::swap(state_,other.state_);}
private:
    explicit PersistentPlayerOwner(PersistentPlayerState* state):state_(state){}
    void retain(){if(state_)++state_->references_;}
    void release(){if(state_&&!--state_->references_)delete state_;state_=nullptr;}
    PersistentPlayerState* state_=nullptr;
    friend class OpeningWorld;
};

// Explicitly distinguishes staged retention from New Game / LOAD construction.
struct RetainPersistentPlayer final {};
}
