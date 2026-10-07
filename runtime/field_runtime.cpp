#include "encore/field_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::upstream {
void FieldRuntime::clear() { data_=nullptr;grass_.clear();deferred_.clear();native_pending_.clear();next_ready_=0;next_instance_=1; }
bool FieldRuntime::prepare_grass_slice(const FieldData& data,std::string& error) {
    if(!data.valid()){error="Field: grass resource not loaded";return false;}
    std::vector<GrassState> next(data.grass_count());
    grass_=std::move(next);deferred_.clear();native_pending_.clear();data_=&data;next_ready_=0;next_instance_=1;error.clear();return true;
}
bool FieldRuntime::activate_scene(std::string& error) const {
    if(!data_){error="Field: not prepared";return false;}
    if(!data_->scene_admitted()){error="Field: full scene mechanisms pending";return false;}
    error.clear();return true;
}
bool FieldRuntime::index(uint32_t id,size_t& out,std::string& error) const {
    if(!data_){error="Field: not prepared";return false;}
    for(size_t i=0;i<grass_.size();++i)if(data_->grass(uint32_t(i)).stable_id==id){out=i;return true;}
    error="Field: unknown grass identity";return false;
}
bool FieldRuntime::execute_grass_ready(uint32_t ordinal,SourceRandom& random,std::string& error) {
    if(!data_||next_ready_>=grass_.size()||data_->grass(next_ready_).ready_ordinal!=ordinal){error="Field: grass Ready order rejected";return false;}
    const auto instance=data_->grass(next_ready_);const auto profile=data_->profile(instance.profile_index);
    // This mutates the shared global stream exactly as the original spawner:
    // seed(node.name.hash()), randi()%grass_types, then randi()%2.
    random.seed(instance.seed);auto& state=grass_[next_ready_];
    state.texture_index=profile.texture_first+random.randi()%instance.grass_types;
    state.flip=random.randi()%2==1;state.frame=state.desired_frame=profile.frames[0];state.ready=true;
    ++next_ready_;error.clear();return true;
}
bool FieldRuntime::screen_entered(uint32_t id,std::string& error) {
    size_t i=0;if(!index(id,i,error))return false;auto& s=grass_[i];
    if(!s.ready){error="Field: visibility before grass Ready";return false;}
    if(!s.alive){if(!next_instance_){error="Field: grass generation overflow";return false;}auto texture=s.texture_index;bool flip=s.flip;s=GrassState{};s.ready=s.alive=true;s.instance_id=next_instance_++;s.texture_index=texture;s.flip=flip;s.frame=s.desired_frame=data_->profile(data_->grass(uint32_t(i)).profile_index).frames[0];}
    error.clear();return true;
}
bool FieldRuntime::screen_exited(uint32_t id,std::string& error) {
    size_t i=0;if(!index(id,i,error))return false;auto& s=grass_[i];
    if(!s.ready){error="Field: visibility before grass Ready";return false;}
    // Upstream clears currentGrass immediately, but queue_free keeps the old
    // Area alive until the delete boundary. A second entry may spawn another
    // Area in that interval; instance handles disambiguate their body signals.
    if(s.alive){if(deferred_.size()>=8192){error="Field: deferred grass capacity exceeded";return false;}auto texture=s.texture_index;bool flip=s.flip;deferred_.push_back({uint32_t(i),std::move(s)});s=GrassState{};s.ready=true;s.texture_index=texture;s.flip=flip;s.frame=data_->profile(data_->grass(uint32_t(i)).profile_index).frames[0];}
    error.clear();return true;
}
uint64_t FieldRuntime::grass_instance(uint32_t id) const {
    if(!data_)return 0;
    for(uint32_t i=0;i<grass_.size();++i)if(data_->grass(i).stable_id==id)return grass_[i].instance_id;
    return 0;
}
bool FieldRuntime::instance(uint64_t id,size_t& i,GrassState*& state,std::string& error) {
    if(id&&data_){for(i=0;i<grass_.size();++i)if(grass_[i].instance_id==id){state=&grass_[i];return true;}
        for(auto& r:deferred_){if(r.state.instance_id==id){i=r.index;state=&r.state;return true;}}
        for(auto& r:native_pending_){if(r.state.instance_id==id){i=r.index;state=&r.state;return true;}}}
    error="Field: stale/unknown grass instance";return false;
}
bool FieldRuntime::body_entered(uint64_t instance_id,uint64_t body_id,float x,bool visible,std::string& error) {
    size_t i=0;GrassState* state=nullptr;if(!instance(instance_id,i,state,error))return false;auto& s=*state;
    if(!body_id||!std::isfinite(x)||std::fabs(x)>1000000||!s.alive){error="Field: invalid grass body entry";return false;}
    if(!visible){error.clear();return true;}
    if(std::any_of(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.stable_id==body_id;})){error="Field: duplicate grass body entry";return false;}
    if(s.bodies.size()>=256){error="Field: grass overlap capacity exceeded";return false;}
    if(!s.native_owned&&s.timer_left<=0&&s.tweens.size()>=256){error="Field: grass tween capacity exceeded";return false;}
    s.bodies.push_back({body_id,x});s.animation_active=true;if(s.bodies.size()==1)s.physics_active=true;
    if(!s.native_owned&&s.timer_left<=0){const auto p=data_->profile(data_->grass(uint32_t(i)).profile_index);s.tweens.push_back({0,p.enter_tween,false});s.scale_y=p.squash;}
    error.clear();return true;
}
bool FieldRuntime::body_exited(uint64_t instance_id,uint64_t body_id,std::string& error,bool timer_exists) {
    size_t i=0;GrassState* state=nullptr;if(!instance(instance_id,i,state,error))return false;auto& s=*state;
    if(!body_id||!s.alive){error="Field: invalid grass body exit";return false;}
    auto it=std::find_if(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.stable_id==body_id;});if(it!=s.bodies.end())s.bodies.erase(it);
    if(s.bodies.empty()){s.physics_active=false;if(timer_exists){s.timer_running=true;s.timer_left=data_->profile(data_->grass(uint32_t(i)).profile_index).idle_delay;}}
    error.clear();return true;
}
bool FieldRuntime::set_body_global_x(uint64_t id,float x,std::string& error) {
    if(!data_||!id||!std::isfinite(x)||std::fabs(x)>1000000){error="Field: invalid body position";return false;}
    for(auto& s:grass_)for(auto& b:s.bodies)if(b.stable_id==id)b.x=x;
    for(auto& r:deferred_)for(auto& b:r.state.bodies)if(b.stable_id==id)b.x=x;
    error.clear();return true;
}
bool FieldRuntime::physics_tick(float delta,std::string& error) {
    if(!data_||!std::isfinite(delta)||delta<0||delta>1){error="Field: invalid grass physics delta";return false;}
    for(uint32_t i=0;i<grass_.size();++i)if(!grass_[i].native_owned)physics_step(i,grass_[i]);
    for(auto& r:deferred_)if(!r.state.native_owned)physics_step(r.index,r.state);
    error.clear();return true;
}
void FieldRuntime::physics_step(uint32_t i,GrassState& s) {
    if(!s.alive)return;
    const auto g=data_->grass(i);const auto p=data_->profile(g.profile_index);const Vec2 position=s.native_owned?s.native_world:g.position;
        // AnimationTree Ruffle uses nearest-point BlendSpace2D. Source integer
        // truncation is applied only after summing floats in body entry order.
        if(s.physics_active&&!s.bodies.empty()&&s.animation_active){float sum=0;for(const auto& b:s.bodies)sum+=b.x;const auto average=int32_t(sum/float(s.bodies.size()));
            const float denominator=float(data_->texture(s.texture_index).width)/p.blend_divisor;
            float blend=(position.x-float(average))/denominator;
            const float blend_y=s.flip?0:position.y/denominator;
            if(s.flip)blend=-blend;
            // Godot finds the first strictly nearer point; Middle is declared
            // first, then Left, then Right, so it owns equal-distance ties.
            uint16_t frame=p.frames[0];float distance=std::numeric_limits<float>::max();
            for(unsigned point=0;point<p.blend_points.size();++point){const float dx=p.blend_points[point].x-blend,dy=p.blend_points[point].y-blend_y;
                const float candidate=dx*dx+dy*dy;if(candidate<distance){distance=candidate;frame=p.blend_frames[point];}}
            s.desired_frame=frame;
        }
}
bool FieldRuntime::idle_tick(double delta,std::string& error) {
    if(!data_||!std::isfinite(delta)||delta<0||delta>1){error="Field: invalid grass idle delta";return false;}
    auto full=[&](const GrassState& s){return !s.native_owned&&s.alive&&s.timer_running&&s.timer_left-delta<0&&s.tweens.size()>=256;};
    for(const auto& s:grass_)if(full(s)){error="Field: grass timeout tween capacity exceeded";return false;}
    for(const auto& r:deferred_)if(full(r.state)){error="Field: grass timeout tween capacity exceeded";return false;}
    for(uint32_t i=0;i<grass_.size();++i)if(!grass_[i].native_owned)idle_step(i,grass_[i],delta);
    for(auto& r:deferred_)if(!r.state.native_owned)idle_step(r.index,r.state,delta);
    error.clear();return true;
}
void FieldRuntime::idle_step(uint32_t i,GrassState& s,double delta) {
    if(!s.alive)return;
    const auto p=data_->profile(data_->grass(i).profile_index);
    // Default AnimationTree processing is idle. Physics updates its requested
    // blend/state; frame output applies at the following idle tree evaluation.
    if(s.animation_active)s.frame=s.desired_frame;
        // Timer and create_tween use the original idle clock; they must not
        // consume physics catch-up ticks. SceneTree tweens advance after Timer
        // timeout processing, so a newly created timeout tween sees this delta.
        if(s.timer_running){s.timer_left-=delta;if(s.timer_left<0){s.timer_running=false;s.timer_left=0;s.desired_frame=p.frames[0];s.tweens.push_back({0,p.exit_tween,true});s.scale_y=p.squash;}}
        // Concurrent source-created tweens remain separate. Re-entry must not
        // cancel an older timeout coroutine's later AnimationTree deactivation.
        for(auto& t:s.tweens){t.time=std::min(t.duration,t.time+delta);const double ratio=t.time/t.duration;
            s.scale_y=p.squash+(1-p.squash)*float(ratio);
            if(t.time>=t.duration&&t.deactivate)s.animation_active=false;}
        s.tweens.erase(std::remove_if(s.tweens.begin(),s.tweens.end(),[](const Tween& t){return t.time>=t.duration;}),s.tweens.end());
}
bool FieldRuntime::bind_native_instance(uint32_t stable,uint64_t actual,Vec2 world,std::string&e){
 size_t i=0;if(!index(stable,i,e))return false;auto&s=grass_[i];
 if(!actual||!s.ready||s.alive||!std::isfinite(world.x)||!std::isfinite(world.y)){e="Grass actual native instance/publication invalid";return false;}
 for(const auto&other:grass_)if(other.instance_id==actual){e="Grass actual instance already owned";return false;}
 for(const auto&other:deferred_)if(other.state.instance_id==actual){e="Grass queued actual instance already owned";return false;}
 if(native_pending_.size()>=8192){e="Grass pending native constructor capacity";return false;}GrassState candidate;candidate.ready=candidate.alive=candidate.native_owned=true;candidate.instance_id=actual;candidate.native_world=world;candidate.texture_index=s.texture_index;candidate.flip=s.flip;candidate.frame=candidate.desired_frame=data_->profile(data_->grass(uint32_t(i)).profile_index).frames[0];native_pending_.push_back({uint32_t(i),std::move(candidate)});e.clear();return true;
}
bool FieldRuntime::publish_native_instance(uint32_t stable,uint64_t id,std::string&e){size_t i=0;if(!index(stable,i,e))return false;for(auto it=native_pending_.begin();it!=native_pending_.end();++it)if(it->state.instance_id==id&&it->index==i&&!grass_[i].alive){grass_[i]=std::move(it->state);native_pending_.erase(it);return true;}e="Grass currentGrass publication source cursor rejected";return false;}
bool FieldRuntime::native_position(uint64_t id,Vec2 world,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned||!std::isfinite(world.x)||!std::isfinite(world.y)){e="Grass native world position invalid";return false;}s->native_world=world;return true;}
bool FieldRuntime::native_pose(uint64_t id,FieldGrassDraw&out,bool&physics,bool&graph,bool&timer,double&left,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned){e="Grass native owner missing";return false;}const auto g=data_->grass(uint32_t(i));const auto p=data_->profile(g.profile_index);out={g.stable_id,s->texture_index,id,s->frame,s->native_world,p.sprite_offset,s->scale_y,s->flip};physics=s->physics_active;graph=s->animation_active;timer=s->timer_running;left=s->timer_left;return true;}
bool FieldRuntime::native_physics(uint64_t id,float dt,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned||!std::isfinite(dt)||dt<0||dt>1){e="Grass native physics clock invalid";return false;}physics_step(uint32_t(i),*s);return true;}
bool FieldRuntime::native_graph(uint64_t id,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned){e="Grass native graph owner missing";return false;}if(s->animation_active)s->frame=s->desired_frame;return true;}
bool FieldRuntime::native_timer(uint64_t id,double dt,bool&timeout,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned||!std::isfinite(dt)||dt<0||dt>1){e="Grass native Timer clock invalid";return false;}timeout=false;if(s->timer_running){s->timer_left-=dt;if(s->timer_left<0){s->timer_left=0;s->timer_running=false;s->desired_frame=data_->profile(data_->grass(uint32_t(i)).profile_index).frames[0];timeout=true;}}return true;}
bool FieldRuntime::native_scale(uint64_t id,float value,bool deactivate,std::string&e){size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned||!std::isfinite(value)||value<=0){e="Grass native Tween value invalid";return false;}s->scale_y=value;if(deactivate)s->animation_active=false;return true;}
bool FieldRuntime::retire_native_instance(uint64_t id,std::string&e){for(auto it=deferred_.begin();it!=deferred_.end();++it)if(it->state.instance_id==id&&it->state.native_owned){deferred_.erase(it);return true;}size_t i=0;GrassState*s=nullptr;if(!instance(id,i,s,e))return false;if(!s->native_owned){e="Grass retired native owner missing";return false;}const auto texture=s->texture_index;const bool flip=s->flip;*s=GrassState{};s->ready=true;s->texture_index=texture;s->flip=flip;return true;}
void FieldRuntime::flush_deferred() { deferred_.erase(std::remove_if(deferred_.begin(),deferred_.end(),[](const Retired&r){return !r.state.native_owned;}),deferred_.end()); }
void FieldRuntime::grass_draws(std::vector<FieldGrassDraw>& output) const {
    output.clear();if(!data_)return;
    for(uint32_t i=0;i<grass_.size();++i){const auto& s=grass_[i];if(!s.alive)continue;const auto g=data_->grass(i);const auto p=data_->profile(g.profile_index);
        output.push_back({g.stable_id,s.texture_index,s.instance_id,s.frame,s.native_owned?s.native_world:g.position,p.sprite_offset,s.scale_y,s.flip});}
    for(const auto& r:deferred_){const auto& s=r.state;const auto g=data_->grass(r.index);const auto p=data_->profile(g.profile_index);output.push_back({g.stable_id,s.texture_index,s.instance_id,s.frame,s.native_owned?s.native_world:g.position,p.sprite_offset,s.scale_y,s.flip});}
}
}
