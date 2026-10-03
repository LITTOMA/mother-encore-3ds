#include "encore/cutscene_camera.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool valid_delta(double delta){return std::isfinite(delta)&&delta>=0&&delta<=1;}
}
bool CutsceneCamera::initialize(const RoomView& content,Vec2 player,Vec2 viewport){
    if(!content.valid()||!finite(player)||!finite(viewport)||viewport.x<=0||viewport.y<=0)return false;
    *this=CutsceneCamera{};content_=content;viewport_=viewport;player_=player;cameras_[0].global=player;cameras_[0].parent=player;
    uint32_t selected=content.scene().default_camera_area;
    for(uint32_t i=0;i<content.camera_area_count();++i){const auto a=content.camera_area(i);if(std::abs(player.x-a.center.x)<=a.extents.x&&std::abs(player.y-a.center.y)<=a.extents.y){selected=i;break;}}
    const auto area=content.camera_area(selected);const Vec2 half{viewport.x*0.5f,viewport.y*0.5f};
    const Vec2 extent{std::max(half.x,area.extents.x),std::max(half.y,area.extents.y)};
    min_center_={area.center.x-extent.x+half.x,area.center.y-extent.y+half.y};
    max_center_={area.center.x+extent.x-half.x,area.center.y+extent.y-half.y};
    cameras_[0].min_center=min_center_;cameras_[0].max_center=max_center_;
    initialized_=true;return true;
}
bool CutsceneCamera::relocate_player(Vec2 player){
    if(!initialized_||!finite(player))return false;
    uint32_t selected=content_.scene().default_camera_area;
    for(uint32_t i=0;i<content_.camera_area_count();++i){const auto a=content_.camera_area(i);if(std::abs(player.x-a.center.x)<=a.extents.x&&std::abs(player.y-a.center.y)<=a.extents.y){selected=i;break;}}
    const auto area=content_.camera_area(selected);const Vec2 half{viewport_.x*.5f,viewport_.y*.5f};
    const Vec2 extent{std::max(half.x,area.extents.x),std::max(half.y,area.extents.y)};
    min_center_={area.center.x-extent.x+half.x,area.center.y-extent.y+half.y};max_center_={area.center.x+extent.x-half.x,area.center.y+extent.y-half.y};
    // Same player camera survives the warp. Area enter/exit resets its local
    // camarea position; source Shaker children and offsets continue to exist.
    current_index_=0;player_=player;auto& c=current();c.local={};c.global=player;c.parent=player;c.min_center=min_center_;c.max_center=max_center_;return true;
}
Vec2 CutsceneCamera::center(const Camera& c) const {
    return {std::clamp(c.global.x,c.min_center.x,c.max_center.x)+c.offset.x,std::clamp(c.global.y,c.min_center.y,c.max_center.y)+c.offset.y};
}
Vec2 CutsceneCamera::center()const{return center(current());}
void CutsceneCamera::pause(){current().paused=true;}
bool CutsceneCamera::change_to_actor(Vec2 target){return change_to_actor(0,target);}
bool CutsceneCamera::update_actor_position(uint32_t actor_id,Vec2 position){
    if(!initialized_||!finite(position))return false;
    for(size_t i=1;i<cameras_.size();++i)if(cameras_[i].actor_id==actor_id){cameras_[i].parent=position;return true;}
    Camera camera;camera.actor_id=actor_id;camera.parent=position;camera.global=position;
    cameras_.push_back(camera);return true;
}
bool CutsceneCamera::remove_actor(uint32_t actor_id){
    if(!initialized_)return false;
    for(size_t i=1;i<cameras_.size();++i)if(cameras_[i].actor_id==actor_id){
        if(i==current_index_)return false;
        cameras_.erase(cameras_.begin()+i);if(i<current_index_)--current_index_;return true;
    }
    return false;
}
bool CutsceneCamera::change_to_actor(uint32_t actor_id,Vec2 target){
    if(!initialized_||!finite(target))return false;
    const Vec2 start=center(),minimum=current().min_center,maximum=current().max_center;
    if(!update_actor_position(actor_id,target))return false;
    for(size_t i=1;i<cameras_.size();++i)if(cameras_[i].actor_id==actor_id){current_index_=i;break;}
    auto& c=current();c.global=start;c.local={start.x-target.x,start.y-target.y};c.min_center=minimum;c.max_center=maximum;
    // set_current changes the position and inherited limits only. This actor's
    // own tween and Shaker children remain alive, including while non-current.
    return true;
}
bool CutsceneCamera::restore_player(Vec2 player){
    if(!initialized_||!finite(player))return false;
    const Vec2 start=center(),minimum=current().min_center,maximum=current().max_center;player_=player;current_index_=0;
    auto& c=current();c.parent=player;c.global=start;c.local={start.x-player.x,start.y-player.y};c.min_center=minimum;c.max_center=maximum;
    // actor.update_npcs restores a different camera, then return_camera(.5)
    // tweens LOCAL position to that player's _camarea_offset (zero here).
    c.move_from=c.local;c.move_target={};c.move_time=0;c.move_duration=content_.rule_f64(RoomRuleKey::CameraReturnSeconds);
    c.moving=true;c.paused=false;c.returning=true;return true;
}
bool CutsceneCamera::return_offset(Vec2 local_target,double duration){
    if(!initialized_||!finite(local_target)||!std::isfinite(duration)||duration<=0)return false;
    auto& c=current();c.move_from=c.local;c.move_target=local_target;c.move_time=0;c.move_duration=duration;
    c.moving=true;c.paused=false;c.returning=true;return true;
}
bool CutsceneCamera::move_to(Vec2 target,double duration){
    if(!initialized_||!finite(target)||!std::isfinite(duration)||duration<0)return false;
    auto& c=current();c.move_from=center();c.move_target=target;c.move_time=0;
    c.move_duration=duration;c.moving=duration>0;c.paused=false;c.returning=false;
    if(!c.moving){c.global=target;const auto parent=c.parent;c.local={target.x-parent.x,target.y-parent.y};}
    return true;
}
bool CutsceneCamera::shake(double magnitude,double duration,Vec2 direction){
    const double step=content_.rule_f64(RoomRuleKey::CameraShakeStepSeconds);
    if(!initialized_||!std::isfinite(duration)||duration<step||duration>60||!std::isfinite(magnitude)||magnitude<0||!finite(direction)||(direction.x==1&&direction.y==1))return false;
    auto& c=current();if(c.shakes.size()>=16)return false;
    Shake s;s.left=int(duration/step);s.magnitude=magnitude;s.reduction=magnitude/s.left;s.old=c.shake;s.direction=direction;c.shakes.push_back(s);return true;
}
bool CutsceneCamera::physics_frame(Vec2 target,double delta){
    if(!initialized_||!finite(target)||!valid_delta(delta))return false;
    if(current_index_!=0)current().parent=target;
    return physics_frame(delta);
}
bool CutsceneCamera::physics_frame(double delta){
    if(!initialized_||!valid_delta(delta))return false;
    // Godot3 dispatches Node callbacks with its float real_t delta; retaining
    // double 1/60 instead shifts exact .02-interval crossings and diminution.
    delta=double(float(delta));
    for(auto& c:cameras_){
        const auto parent=c.parent;
        c.global={parent.x+c.local.x,parent.y+c.local.y};
        // Camera script runs before child Shaker nodes. Their newly computed
        // _shake_offset becomes the visible Camera2D.offset NEXT physics pass.
        c.offset=c.shake;
        for(size_t i=0;i<c.shakes.size();){
            auto& s=c.shakes[i];
            if(s.left>0){
                s.timer+=delta;
                if(s.timer>=content_.rule_f64(RoomRuleKey::CameraShakeStepSeconds)){
                    --s.left;s.side=-s.side;s.timer-=content_.rule_f64(RoomRuleKey::CameraShakeStepSeconds);
                    double magnitude=std::max(s.magnitude,content_.rule_f64(RoomRuleKey::CameraMinimumMagnitude));
                    if(s.magnitude<=content_.rule_f64(RoomRuleKey::CameraZeroMagnitudeThreshold)&&s.side==-1)magnitude=0;
                    const Vec2 value=s.left>1?Vec2{s.old.x+s.direction.x*float(s.side*magnitude),s.old.y+s.direction.y*float(s.side*magnitude)}:s.old;
                    if(s.magnitude<=content_.rule_f64(RoomRuleKey::CameraMinimumMagnitude))c.shake=value;
                    else c.shake={c.shake.x+(value.x-c.shake.x)*float(content_.rule_f64(RoomRuleKey::CameraShakeWeight)),c.shake.y+(value.y-c.shake.y)*float(content_.rule_f64(RoomRuleKey::CameraShakeWeight))};
                }
                // Original Shaker diminishes every physics frame, including
                // frames that do not cross its interval.
                s.magnitude-=s.reduction;++i;
            }else{
                c.shake=s.old;c.recoveries.push_back({});c.shakes.erase(c.shakes.begin()+i);
            }
        }
    }
    return true;
}
bool CutsceneCamera::idle_frame(double delta){
    if(!initialized_||!valid_delta(delta))return false;
    delta=double(float(delta));
    for(auto& c:cameras_){
        for(size_t i=0;i<c.recoveries.size();){
            auto& r=c.recoveries[i];
            // PropertyTweener captures its start value when first processed,
            // after any other Shaker child writes during the same physics pass.
            if(!r.started){r.from=c.shake;r.started=true;}
            r.time=std::min(r.time+delta,content_.rule_f64(RoomRuleKey::CameraRecoverySeconds));
            const float left=float(1-r.time/content_.rule_f64(RoomRuleKey::CameraRecoverySeconds));c.shake={r.from.x*left,r.from.y*left};
            if(r.time>=content_.rule_f64(RoomRuleKey::CameraRecoverySeconds))c.recoveries.erase(c.recoveries.begin()+i);else ++i;
        }
        if(c.moving&&!c.paused){
            c.move_time=std::min(c.move_time+delta,c.move_duration);
            const double t=c.move_time/c.move_duration;
            const float eased=float(std::sin(t*1.57079632679489661923));
            Vec2 value={c.move_from.x+(c.move_target.x-c.move_from.x)*eased,c.move_from.y+(c.move_target.y-c.move_from.y)*eased};
            const auto parent=c.parent;
            if(c.returning){c.local=value;c.global={parent.x+value.x,parent.y+value.y};}
            else{c.global=value;c.local={value.x-parent.x,value.y-parent.y};}
            if(c.move_time>=c.move_duration)c.moving=false;
        }
    }
    return true;
}
}
