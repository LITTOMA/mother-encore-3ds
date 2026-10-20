#pragma once
#include "encore/collision.hpp"
#include "encore/field_data.hpp"
#include "encore/field_story.hpp"
#include "encore/field_steps.hpp"
#include "encore/field_props.hpp"
#include "encore/field_objects.hpp"
#include "encore/field_area_contacts.hpp"
#include "encore/introduction.hpp"
#include "encore/mick_treats.hpp"
#include "encore/world.hpp"
#include "encore/world_links.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Condition-evaluated TileMap cell shapes and static bodies of one bound field.
// Templates keep Godot ConvexPolygonShape2DSW point order/normals per shape.
class FieldCollision final : public MotionObstacleSource {
public:
    bool bind(FieldMapView map,const std::vector<bool>& layer_active,const std::vector<bool>& body_active,std::string& error);
    // Source-ordered moving foot bodies. Slot zero retains Mick's legacy API.
    void set_body(Vec2 center,Vec2 extents,uint32_t slot=0,bool enabled=true);
    // One checked static owner changes its shape admission without rebuilding
    // the TileMap cells/templates or changing collision collection order.
    bool set_static_body_enabled(uint32_t,bool);
    void set_extra_enabled(bool enabled){set_excluded(enabled?UINT32_MAX:0);}
    void set_excluded(uint32_t slot){excluded_=slot;}
    bool collect(Vec2 minimum,Vec2 maximum,std::vector<MotionObstacle>& out)const override;
private:
    FieldRunLoop run_audio_;std::vector<uint8_t>step_inside_;
    FieldMapView map_;
    std::vector<Vec2> points_,normals_;
    std::vector<uint32_t> first_;
    // Derived in original order. Empty ENCMAP01 chunks have (0,0) bounds;
    // decorative cells must never be scanned by an origin collision query.
    struct Cell {Vec2 offset;uint32_t first,count;};
    struct Chunk {Vec2 minimum,maximum;uint32_t first,count;};
    std::vector<Cell> cells_;
    std::vector<Chunk> chunks_;
    std::vector<bool> bodies_;
    struct Extra {bool enabled=false;Vec2 points[4]{},normals[4]{},minimum{},maximum{};};
    std::vector<Extra> extra_;uint32_t excluded_=UINT32_MAX;
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

enum class FieldPhase:uint8_t {Walking,DoorAwaitIdle,Transition,Unsupported,MickTalk,Error};
struct FieldNoticeView {uint32_t index=0;float distance=0;};
struct FieldMickWorld;
class FieldStoryHost {
public:
    virtual ~FieldStoryHost()=default;
    // Pure admission before any cutscene, RNG, inventory or flag mutation.
    virtual bool can_start(FieldStoryView,uint32_t programme,std::string& error)=0;
};

// AreaRoom field: TileMap world, routed and unported doors, explicit development
// boundaries and unlocked openable doors. Rendering and scene swaps are platform owned.
class FieldScene : private StoryPropHost,private FieldPropHost {
public:
    FieldScene()=default;
    FieldScene(const FieldScene&)=delete;
    FieldScene& operator=(const FieldScene&)=delete;
    // Validation and construction do not consume RNG or touch audio. Flags use
    // the room's flag table order (the House table, shared by stable name).
    bool prepare(RoomView room,FieldMapView map,const WorldLinksData& links,const std::vector<bool>& story_flags,
                 Vec2 position,Vec2 direction,Vec2 viewport,std::string& error);
    // Re-evaluate FlagLandmark / CutsceneArea conditions after a live story flag write.
    // Podunk can change gave_treats in-session once Mick accepts DogTreats.
    bool refresh_conditions(std::string& error);
    bool bind_story(FieldStoryView,ItemView,FieldStoryHost&,std::string& error);
    FieldStoryView story()const{return story_;}
    // Bind Mick after prepare; host effects stay platform-owned.
    bool bind_mick(MickView,MickHost&,std::string& error);
    bool bind_npc(MickView,MickHost&,std::string& error);
    bool bind_steps(FieldStepsView,const AudioBank&,std::string_view inherited_sound,std::string&);
    bool bind_props(const FieldPropsData&,std::string&);
    bool bind_prop_contacts(const FieldPropsData&,const FieldSwingData&,FieldPropContactEffects&,std::string&);
    bool bind_objects(const FieldObjectsData&,std::string&);
    const FieldObjects&objects()const{return objects_;}
    const FieldProps&props()const{return props_;}
    std::vector<FieldPropEvent>take_prop_events(){return props_.take_events();}
    bool grow_prop(uint32_t stable_id,std::string&);
    bool hit_prop(uint32_t index,std::string&);
    bool flush_props(FieldPropHost&,std::string&);
    // Returns a checked dynamic pose only for an owned map Sprite.
    bool prop_sprite(uint32_t sprite,PropPose&)const;
    FieldRunLoop&run_audio(){return run_audio_;}
    const FieldRunLoop&run_audio()const{return run_audio_;}
    void clear_run_events(){run_audio_.clear_events();}
    bool npcs_ready()const;
    bool actor_visible(uint32_t index)const;
    ActorActionState actor_pose(uint32_t index)const;
    MickRuntime& mick(){return mick_;}
    const MickRuntime& mick()const{return mick_;}
    OpeningWorld world;
    bool before_physics(WalkInput& input);
    bool after_physics();
    bool idle_frame(double delta,bool back,bool accept=false,int choice=0);
    // Source uiManager.battle_to_ov -> CutsceneArea._stop_process.
    void battle_to_overworld();
    // The dialogue box close finishes the pause. Complete alone keeps MickTalk.
    bool finish_talk();
    bool talk_accept_latched()const{return mick_ignore_accept_;}
    // A against Mick's live interact rectangle while walking. The matching spoken tree starts.
    bool try_mick(bool accept,std::string& error);
    // Ordinary walking never activates confirm-only development boundaries.
    bool try_interaction(bool accept,std::string& error);
    bool mick_contact()const;
    bool try_telepathy(Vec2 source_ray_offset,float source_ray_length,std::string& error);
    FieldPhase phase()const{return phase_;}
    const std::string& error()const{return error_;}
    bool blocks_player()const{return phase_!=FieldPhase::Walking;}
    // Route index requested by a door contact; cleared by take_route().
    uint32_t take_route(){const auto r=route_;route_=WorldLinksData::kNotFound;return r;}
    void begin_transition(){phase_=FieldPhase::Transition;}
    bool finish_transition();
    // Checked same-scene door: keep NPCs, RNG, flags and music owners.
    bool warp_route(uint32_t route_index,std::string& error);
    // Rejected destination: stay paused at an explicit stop; B returns to the last safe point.
    void abort_transition(std::string message){route_=WorldLinksData::kNotFound;phase_=FieldPhase::Unsupported;error_=std::move(message);}
    bool layer_active(uint32_t i)const{return i<layer_active_.size()&&layer_active_[i];}
    bool item_active(uint32_t i)const;
    bool sprite_visible(uint32_t i)const;
    bool openable_open(uint32_t i)const{return i<openables_.size()&&openables_[i].open;}
    Vec2 camera_origin(Vec2 player,Vec2 viewport)const;
    bool resize_viewport(Vec2 viewport){if(!world.resize_field_viewport(viewport))return false;viewport_=viewport;return true;}
    std::vector<FieldNoticeView> nearby_notices(Vec2 center,float radius,uint32_t limit)const;
    uint32_t boundary_index()const{return boundary_;}
    std::vector<std::string_view> take_sounds(){auto out=std::move(sounds_);sounds_.clear();return out;}
    FieldMapView map()const{return map_;}
    uint32_t scene_id()const{return map_.scene_id();}
private:
    bool bind_props_owner(const FieldPropsData&,std::string&);
    bool can_call(uint32_t,std::string&)override;
    bool call(uint32_t,uint32_t,std::string&)override;
    uint32_t prop_programme(uint32_t)const;
    bool prop_callback_area(uint32_t)const;
    bool prop_contact_enabled(std::array<uint8_t,128>&,std::string&)const;
    bool begin_prop_contacts(std::string&);
    bool sample_prop_contacts(std::string&);
    bool commit_prop_hits(const std::vector<uint32_t>&,std::string&);
    bool process_prop_areas(std::string&);
    bool validate_grow(uint32_t,std::string&)const override;
    bool grow(uint32_t id,std::string&error)override{return grow_prop(id,error);}
    struct Openable {bool inside=false,open=false;double timer=-1;};
    friend struct FieldMickWorld;
    bool overlaps(Vec2 center,Vec2 extents,Vec2 player)const;
    bool circle_overlaps(Vec2 center,float radius)const;
    bool conditions(FieldSpan)const;
    void apply_mick_body();
    void apply_npc_bodies();
    bool apply_prop_bodies(std::string&);
    bool apply_object_state(std::string&);
    std::vector<bool>prop_flags()const;
    bool fail(std::string message){phase_=FieldPhase::Error;error_=std::move(message);return false;}
    FieldRunLoop run_audio_;std::vector<uint8_t>step_inside_;
    FieldMapView map_;const WorldLinksData* links_=nullptr;
    FieldCollision collision_;
    FieldProps props_;
    FieldAreaContacts prop_contacts_;const FieldSwingData*prop_action_=nullptr;FieldPropContactEffects*prop_effects_=nullptr;
    std::vector<uint8_t>prop_area_pending_;
    FieldObjects objects_;bool objects_first_idle_=false;
    std::vector<uint32_t>prop_sprites_;
    std::vector<bool>collision_layers_,collision_bodies_;bool collision_ready_=false;
    StaticMotionSolver mick_solver_;bool mick_solver_ready_=false;
    MickRuntime mick_;bool mick_bound_=false;
    uint32_t interaction_actor_=UINT32_MAX;
    struct Npc {uint32_t actor=0,item=0;MickRuntime motion;ActorActionState sprite;StaticMotionSolver solver;};
    std::vector<std::unique_ptr<Npc>> npcs_;
    Vec2 viewport_{400,240};
    // Suppress the same confirm pulse that started MickTalk from advancing text.
    bool mick_ignore_accept_=false;
    std::vector<bool> layer_active_,sprite_active_,item_active_,door_active_,boundary_active_,camera_active_,notice_active_;
    std::vector<uint8_t> door_inside_,boundary_inside_,camera_inside_;
    std::vector<Openable> openables_;
    std::vector<std::string_view> sounds_;
    FieldPhase phase_=FieldPhase::Error;std::string error_="Field not prepared";
    uint32_t route_=WorldLinksData::kNotFound,pending_door_=WorldLinksData::kNotFound,boundary_=WorldLinksData::kNotFound;
    Vec2 last_safe_position_{},last_safe_direction_{};
    FieldStoryView story_;FieldStoryHost*story_host_=nullptr;uint64_t story_revision_=0;
};
}
