#pragma once
#include "encore/persistent_player.hpp"
namespace encore::upstream {
// Move-only checked publication token. It pins both identities and contains
// copied execution state; it never borrows a world pointer or room buffer.
class ScenePlayerTransfer final {
public:
    ScenePlayerTransfer()=default;
    ScenePlayerTransfer(ScenePlayerTransfer&&)=default;
    ScenePlayerTransfer& operator=(ScenePlayerTransfer&&)=default;
    ScenePlayerTransfer(const ScenePlayerTransfer&)=delete;
    ScenePlayerTransfer& operator=(const ScenePlayerTransfer&)=delete;
    bool prepared()const{return bool(source_)&&bool(destination_);}
private:
    PersistentPlayerOwner source_,destination_;
    WalkState before_,destination_before_,player_;
    CutsceneCamera before_camera_,destination_camera_,camera_;
    AnimationPlayback before_playback_{},destination_playback_{},playback_{};
    uint32_t before_clip_=0,destination_clip_=0,clip_=0;
    uint64_t source_physics_=0,source_idle_=0,destination_physics_=0,destination_idle_=0;
    uint64_t source_story_=0,destination_story_=0;
    uint64_t source_action_=0,destination_action_=0;
    FieldPlayerTree action_;
    const uint8_t*source_content_=nullptr,*destination_content_=nullptr;
    std::string leader_;
    friend class OpeningWorld;
};
}
