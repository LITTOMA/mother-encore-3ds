#pragma once
#include "encore/collision.hpp"
#include "encore/field_data.hpp"
#include "encore/introduction.hpp"
#include "encore/world.hpp"
#include "encore/world_links.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Condition-evaluated TileMap cell shapes and static bodies of one bound field.
// Templates keep Godot ConvexPolygonShape2DSW point order/normals per shape.
class FieldCollision final : public MotionObstacleSource {
public:
    bool bind(FieldMapView map,const std::vector<bool>& layer_active,const std::vector<bool>& body_active,std::string& error);
    bool collect(Vec2 minimum,Vec2 maximum,std::vector<MotionObstacle>& out)const override;
private:
    FieldMapView map_;
    std::vector<Vec2> points_,normals_;
    std::vector<uint32_t> first_;
    std::vector<uint32_t> layers_,bodies_;
};

// Original Door/SceneTransition sequence for one cross-scene route: pause, one
// idle frame, door flag write, fade-in at in_speed, scene swap, settle frames,
// fade-out at out_speed with unpause at the clip's mostly-done key.
enum class SceneDoorPhase:uint8_t {Idle,AwaitIdle,FadeIn,SwapRequested,Settle,FadeOut,Done,Error};
enum class SceneDoorEventKind:uint8_t {FadeMusic,PlaySound,SetFlag,Unpause};
struct SceneDoorEvent {SceneDoorEventKind kind=SceneDoorEventKind::PlaySound;std::string_view text;float seconds=0;bool value=false;};
class SceneDoorTransition {
public:
    bool begin(const WorldLinksData& links,uint32_t route,const IntroductionData& fades,std::string& error);
    bool idle_frame(double delta,std::string& error);
    // The platform calls this after committing the destination scene.
    bool swapped(std::string& error);
    // Abort before the swap; the previous scene stays active.
    void cancel(){*this=SceneDoorTransition{};}
    SceneDoorPhase phase()const{return phase_;}
    bool active()const{return phase_!=SceneDoorPhase::Idle&&phase_!=SceneDoorPhase::Done;}
    bool covering()const{return phase_==SceneDoorPhase::FadeIn||phase_==SceneDoorPhase::SwapRequested||phase_==SceneDoorPhase::Settle||phase_==SceneDoorPhase::FadeOut;}
    const WorldRoute& route()const{return route_;}
    uint32_t route_index()const{return index_;}
    uint32_t kind()const;
    float cut()const;
    std::vector<SceneDoorEvent> take_events(){auto out=std::move(events_);events_.clear();return out;}
private:
    const IntroductionData* fades_=nullptr;WorldRoute route_;uint32_t index_=WorldLinksData::kNotFound;
    SceneDoorPhase phase_=SceneDoorPhase::Idle;double time_=0;uint32_t settle_=0;bool unpaused_=false;
    std::vector<SceneDoorEvent> events_;
};

enum class FieldPhase:uint8_t {Walking,DoorAwaitIdle,Transition,Unsupported,Error};
struct FieldNoticeView {uint32_t index=0;float distance=0;};

// AreaRoom field: TileMap world, routed and unported doors, explicit development
// boundaries and unlocked openable doors. Rendering and scene swaps are platform owned.
class FieldScene {
public:
    FieldScene()=default;
    FieldScene(const FieldScene&)=delete;
    FieldScene& operator=(const FieldScene&)=delete;
    // Validation and construction do not consume RNG or touch audio. Flags use
    // the room's flag table order (the House table, shared by stable name).
    bool prepare(RoomView room,FieldMapView map,const WorldLinksData& links,const std::vector<bool>& story_flags,
                 Vec2 position,Vec2 direction,Vec2 viewport,std::string& error);
    OpeningWorld world;
    bool before_physics(WalkInput& input);
    bool after_physics();
    bool idle_frame(double delta,bool back);
    FieldPhase phase()const{return phase_;}
    const std::string& error()const{return error_;}
    bool blocks_player()const{return phase_!=FieldPhase::Walking;}
    // Route index requested by a door contact; cleared by take_route().
    uint32_t take_route(){const auto r=route_;route_=WorldLinksData::kNotFound;return r;}
    void begin_transition(){phase_=FieldPhase::Transition;}
    bool finish_transition();
    // Rejected destination: stay paused at an explicit stop; B returns to the last safe point.
    void abort_transition(std::string message){route_=WorldLinksData::kNotFound;phase_=FieldPhase::Unsupported;error_=std::move(message);}
    bool layer_active(uint32_t i)const{return i<layer_active_.size()&&layer_active_[i];}
    bool item_active(uint32_t i)const{return i<item_active_.size()&&item_active_[i];}
    bool sprite_visible(uint32_t i)const;
    bool openable_open(uint32_t i)const{return i<openables_.size()&&openables_[i].open;}
    Vec2 camera_origin(Vec2 player,Vec2 viewport)const;
    std::vector<FieldNoticeView> nearby_notices(Vec2 center,float radius,uint32_t limit)const;
    uint32_t boundary_index()const{return boundary_;}
    std::vector<std::string_view> take_sounds(){auto out=std::move(sounds_);sounds_.clear();return out;}
    FieldMapView map()const{return map_;}
    uint32_t scene_id()const{return map_.scene_id();}
private:
    struct Openable {bool inside=false,open=false;double timer=-1;};
    bool overlaps(Vec2 center,Vec2 extents,Vec2 player)const;
    bool conditions(FieldSpan)const;
    bool fail(std::string message){phase_=FieldPhase::Error;error_=std::move(message);return false;}
    FieldMapView map_;const WorldLinksData* links_=nullptr;
    FieldCollision collision_;
    std::vector<bool> layer_active_,sprite_active_,item_active_,door_active_,boundary_active_,camera_active_,notice_active_;
    std::vector<uint8_t> door_inside_,boundary_inside_,camera_inside_;
    std::vector<Openable> openables_;
    std::vector<std::string_view> sounds_;
    FieldPhase phase_=FieldPhase::Error;std::string error_="Field not prepared";
    uint32_t route_=WorldLinksData::kNotFound,pending_door_=WorldLinksData::kNotFound,boundary_=WorldLinksData::kNotFound;
    Vec2 last_safe_position_{},last_safe_direction_{};
};
}
