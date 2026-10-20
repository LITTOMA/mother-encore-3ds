#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <vector>
namespace encore::upstream {
class SourceRandom;
// Source-bound Camera2D/Shaker subset; limits come from the external room.
// Call physics_frame after actor physics and idle_frame after dialogue timers.
// New dialogue commands are idle-born, as in the audited lamp sequence.
class CutsceneCamera {
public:
    bool initialize(const RoomView& content,Vec2 player,Vec2 viewport={400,240});
    bool relocate_player(Vec2 player);
    // Rebind only scene limits; a persistent player keeps its own camera/shakers.
    bool rebind_scene(const RoomView&,Vec2 player,Vec2 viewport);
    bool resize_viewport(Vec2 viewport);
    bool is_shaking()const{return !current().shakes.empty()||!current().recoveries.empty();}
    bool change_to_dialogue(){return change_to_actor(kRoomNoIndex,{});}
    bool change_to_actor(Vec2 target); // Legacy single-actor caller uses actor id 0.
    bool change_to_actor(uint32_t actor_id,Vec2 target);
    bool update_actor_position(uint32_t actor_id,Vec2 position);
    bool remove_actor(uint32_t actor_id);
    bool restore_player(Vec2 player);
    void pause();
    bool return_offset(Vec2 local_target,double duration);
    void resume(){current().paused=false;} // Original reset() only assigns loop locals and changes no fields.
    bool move_to(Vec2 target,double duration);
    bool shake(double magnitude,double duration,Vec2 direction);
    // Explicit source parameters; a zero direction requires the RNG-taking
    // physics overload. All gameplay values are supplied by checked resources.
    bool shake(double magnitude,double duration,Vec2 direction,double interval,double weight,bool diminish);
    bool physics_frame(Vec2 target,double delta=1.0/60.0);
    bool physics_frame(double delta=1.0/60.0);
    bool physics_frame(SourceRandom&,double delta=1.0/60.0);
    bool idle_frame(double delta);
    Vec2 center() const;
    Vec2 display_offset()const{return current().offset;}
    void update_player_position(Vec2 position){player_=position;cameras_[0].parent=position;}
    bool is_moving()const{return current().moving;}
    bool follows_actor() const { return current_index_!=0; }
    Vec2 global_position() const { return current().global; }
private:
    friend class OpeningWorld;
    bool same_state(const CutsceneCamera&)const;
    struct Shake { double timer=0,magnitude=0,reduction=0,interval=0,weight=0; int left=0,side=1; Vec2 old{},direction{},sampled{}; };
    struct Recovery { double time=0; Vec2 from{}; bool started=false; };
    struct Camera {
        Vec2 global{},local{},move_from{},move_target{},shake{},offset{},parent{},min_center{},max_center{};
        uint32_t actor_id=0;
        double move_time=0,move_duration=0;
        std::vector<Shake> shakes;
        std::vector<Recovery> recoveries;
        bool moving=false,paused=false,returning=false;
    };
    struct Binding {
        std::vector<RoomCameraArea> areas;uint32_t default_area=0;
        double return_seconds=0,shake_step=0,minimum_magnitude=0,zero_threshold=0,shake_weight=0,recovery_seconds=0;
    } binding_;
    bool bind_content(const RoomView&,bool require_same_parameters);
    bool needs_random()const;
    bool physics_step(double,SourceRandom*);
    Vec2 min_center_{},max_center_{},viewport_{};
    std::vector<Camera> cameras_{Camera{}};
    size_t current_index_=0;
    Vec2 player_{};
    bool initialized_=false;
    Camera& current() { return cameras_[current_index_]; }
    const Camera& current() const { return cameras_[current_index_]; }
    Vec2 center(const Camera& camera) const;
};
}
