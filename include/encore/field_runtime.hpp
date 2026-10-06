#pragma once
#include "encore/field_data.hpp"
#include "encore/source_random.hpp"
#include <string>
#include <vector>

namespace encore::upstream {
struct FieldGrassDraw {
    uint32_t stable_id=0,texture_index=0;
    uint64_t instance_id=0;
    uint16_t frame=0;
    Vec2 position{},sprite_offset{};
    float scale_y=1;
    bool flip_h=false;
};
// Source-owned body identities are explicit; physics supplies genuine Area2D
// enter/exit notifications and current global X. Camera visibility supplies
// VisibilityNotifier2D enter/exit notifications. Neither event is guessed from
// an arbitrary player-radius approximation by this consumer.
class FieldRuntime {
public:
 const FieldData*data()const{return data_;}
    bool prepare_grass_slice(const FieldData&,std::string&);
    bool activate_scene(std::string&) const;
    bool execute_grass_ready(uint32_t ready_ordinal,SourceRandom&,std::string&);
    bool screen_entered(uint32_t stable_id,std::string&);
    bool screen_exited(uint32_t stable_id,std::string&);
    uint64_t grass_instance(uint32_t stable_id) const;
    bool body_entered(uint64_t instance_id,uint32_t body_id,float global_x,bool grass_visible,std::string&);
    bool body_exited(uint64_t instance_id,uint32_t body_id,std::string&);
    bool set_body_global_x(uint32_t body_id,float global_x,std::string&);
    bool physics_tick(float delta,std::string&);
    bool idle_tick(double delta,std::string&);
    void flush_deferred();
    void grass_draws(std::vector<FieldGrassDraw>&) const;
    void clear();
    uint32_t ready_grass_count() const { return next_ready_; }
private:
    struct Body { uint32_t stable_id=0;float x=0; };
    struct Tween { double time=0,duration=0;bool deactivate=false; };
    struct GrassState {
        uint64_t instance_id=0;
        bool ready=false,alive=false,flip=false,animation_active=false,physics_active=false;
        bool timer_running=false;
        uint32_t texture_index=0;uint16_t frame=0,desired_frame=0;
        double timer_left=0;
        float scale_y=1;
        std::vector<Body> bodies;
        std::vector<Tween> tweens;
    };
    bool index(uint32_t,size_t&,std::string&) const;
    bool instance(uint64_t,size_t&,GrassState*&,std::string&);
    void physics_step(uint32_t,GrassState&);
    void idle_step(uint32_t,GrassState&,double);
    struct Retired { uint32_t index=0;GrassState state; };
    const FieldData* data_=nullptr;
    std::vector<GrassState> grass_;
    std::vector<Retired> deferred_;
    uint32_t next_ready_=0;
    uint64_t next_instance_=1;
};
}
