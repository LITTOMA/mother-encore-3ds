#include "encore/world.hpp"
#include "encore/opening_trigger.hpp"
#include "encore/field_swing_resources.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
unsigned direction_index(Vec2 d){if(d.y>0)return d.x<0?4:(d.x>0?5:0);if(d.y<0)return d.x<0?6:(d.x>0?7:3);return d.x<0?1:2;}
SwingMotionFlags swing_flags(const WalkState&p){return {p.crouch,false,p.tap_run,p.running,p.walking};}
void apply_swing_flags(WalkState&p,const SwingMotionFlags&f){p.crouch=f.crouch;p.tap_run=f.tap_run;p.running=f.running;p.walking=f.walking;}
}
OpeningWorld::OpeningWorld():OpeningWorld(PersistentPlayerOwner::create()){}
OpeningWorld::OpeningWorld(PersistentPlayerOwner owner):persistent_player_(std::move(owner)){
    // A shared owner may only enter through read-only staged retention.
    player_read_only_=persistent_player_&&(persistent_player_.state_->references_!=1||persistent_player_.state_->initialized_);
    if(!persistent_player_)error_="Persistent player owner allocation failed";
}
OpeningWorld::OpeningWorld(RetainPersistentPlayer,const OpeningWorld& active):persistent_player_(active.persistent_player_),player_read_only_(true){
    error_=persistent_player_?"Retained player requires a scene commit binding":"Persistent player owner is absent";
}
bool OpeningWorld::bind_story_skills(StorySkillHost&host){
 if(!content_)return false;std::string error;
 for(uint32_t i=0;i<content_.command_count();++i){const auto c=content_.command(i);
  if(c.opcode==uint16_t(DialogueActionKind::LearnSkill)&&!host.validate_skill(content_.string(c.target_index),error))return false;}
 story_skills_=&host;return true;
}
bool OpeningWorld::bind_story_key_items(StoryKeyItemHost&host,std::string&error){
    if(!healthy_||story_key_items_){error="Story key items need one initialized scene owner";return false;}
    for(uint32_t i=0;i<content_.command_count();++i){const auto c=content_.command(i);if(c.opcode==uint16_t(DialogueActionKind::GrantKeyItem)&&!host.validate_key_item(content_.string(c.target_index),uint32_t(c.value),error))return false;}
    story_key_items_=&host;error.clear();return true;
}
bool OpeningWorld::bind_story_objects(StoryObjectHost&host,std::string&error){
    if(!healthy_||story_objects_){error="Story objects need one initialized scene owner";return false;}
    for(uint32_t i=0;i<content_.command_count();++i){const auto c=content_.command(i);if(c.opcode==uint16_t(DialogueActionKind::QueueFreeStoryObject)&&!host.validate_story_object(content_.scene().stable_id,content_.string(c.target_index),error))return false;}
    story_objects_=&host;error.clear();return true;
}
bool OpeningWorld::bind_story_props(StoryPropHost&host,std::string&error){
    if(!healthy_||story_props_){error="Story props need one initialized scene owner";return false;}
    for(uint32_t i=0;i<content_.binding_count();++i){const auto b=content_.binding(i);if(b.kind==uint16_t(RoomBindingKind::FieldPropGrow)&&!host.validate_grow(b.target_index,error))return false;}
    story_props_=&host;error.clear();return true;
}
bool OpeningWorld::validate_story_props(uint32_t programme,std::string&error)const{
    if(!healthy_||programme>=content_.program_count()){error="Story programme unavailable";return false;}
    const auto p=content_.program(programme);
    for(uint32_t i=0;i<p.command_count;++i){const auto c=content_.command(p.first_command+i);
        if(c.opcode==uint16_t(DialogueActionKind::QueueFreeStoryObject)){if(!story_objects_){error="Story object owner is unbound";return false;}const auto path=content_.string(c.target_index);if(!story_objects_->validate_story_object(content_.scene().stable_id,path,error)||!story_objects_->can_queue_free(path,error))return false;}
        if(c.opcode==uint16_t(DialogueActionKind::GrantKeyItem)){if(!story_key_items_){error="Story key item owner is unbound";return false;}if(!story_key_items_->validate_key_item(content_.string(c.target_index),uint32_t(c.value),error))return false;}
        if(c.opcode!=uint16_t(DialogueActionKind::CallObjectDeferred))continue;
        const auto b=content_.binding(c.target_index);if(b.kind!=uint16_t(RoomBindingKind::FieldPropGrow))continue;
        if(!story_props_){error="Story field prop owner is unbound";return false;}
        if(!story_props_->validate_grow(b.target_index,error))return false;
    }
    error.clear();return true;
}
bool OpeningWorld::has_cutscene_actors() const {return std::find(actor_bound_.begin(),actor_bound_.end(),uint8_t(1))!=actor_bound_.end();}
uint32_t OpeningWorld::walk_clip(MotionAnimation animation,Vec2 direction) const {
    const auto scene=content_.scene();const auto profile=content_.actor_profile(content_.actor_instance(scene.player_instance_index).profile_index);
    const auto d=direction_index(direction);
    for(uint32_t i=0;i<profile.animation_binding_count;++i){const auto b=content_.animation_binding(profile.animation_binding_first+i);if(b.motion_state==uint8_t(animation)&&b.direction==d)return b.clip_index;}
    return kRoomNoIndex;
}
bool OpeningWorld::initialize(const RoomView&content,Vec2 viewport){
    if(!content.valid())return false;const auto scene=content.scene();
    return initialize_state(content,viewport,nullptr,nullptr,scene.spawn,scene.start_direction);
}
bool OpeningWorld::initialize_restored(const RoomView&content,const std::vector<bool>&story_flags,const std::vector<bool>&reviewed_mutations,Vec2 position,Vec2 direction,Vec2 viewport){
    const float x=std::abs(direction.x),y=std::abs(direction.y);
    const bool cardinal=(x==0&&y==1)||(x==1&&y==0);
    const bool diagonal=x==y&&(x==1||std::abs(x-std::sqrt(.5f))<1e-6f);
    if(!content.valid()||story_flags.size()!=content.flag_count()||reviewed_mutations.size()!=content.flag_count()||!std::isfinite(position.x)||!std::isfinite(position.y)||(!cardinal&&!diagonal))return false;
    return initialize_state(content,viewport,&story_flags,&reviewed_mutations,position,direction);
}
bool OpeningWorld::initialize_state(const RoomView& content,Vec2 viewport,const std::vector<bool>*story_flags,const std::vector<bool>*reviewed_mutations,Vec2 position,Vec2 direction) {
    if(!persistent_player_){error_="Persistent player owner allocation failed";return false;}
    if(player_read_only_){error_="Retained player cannot be reset by New Game or LOAD initialization";return false;}
    healthy_=initialized_=false;error_="Invalid scene content";
    if(!content.valid())return false;
    content_=content;story_objects_=nullptr;story_key_items_=nullptr;story_props_=nullptr;story_skills_=nullptr;walk_audio_=nullptr;overlay_={};overlay_clip_=kRoomNoIndex;overlay_time_=0;choice_group_=kRoomNoIndex;save_requested_=storage_requested_=false;persistent_player_.state_->party_leader_.clear();story_talker_={};story_talking_=false;effects_.clear();story_hides_.clear();scene_calls_.clear();flagged_bodies_.clear();area_music_resource_=kRoomNoIndex;pending_dialogue_id_=kRoomNoIndex;viewport_=viewport;battle_accepted_=false;persistent_player_.state_->return_player_visible_=false;persistent_player_.state_->house_paused_=false;last_idle_delta_=0;
    const auto scene=content.scene();
    if(!flags_.initialize(content))return false;
    if(story_flags){for(uint32_t i=0;i<story_flags->size();++i)if(!flags_.set_story_flag(i,(*story_flags)[i],false).applied)return false;}
    else for(uint32_t i=0;i<scene.initial_flag_count;++i){const auto flag=content.initial_flag(scene.initial_flag_first+i);if(!flags_.set_story_flag(flag.flag_index,flag.value).applied)return false;}
    dialogue_=DialoguePlayer{};battle_=OpeningBattleRequest{};audio_.clear();trace_.clear();deferred_.clear();boundaries_.clear();room_shakers_.clear();random_=nullptr;
    actors_.assign(content.actor_instance_count(),ActorActionState{});
    actor_visible_.assign(actors_.size(),0);actor_restore_.assign(actors_.size(),0);
    actor_binding_revisions_.assign(actors_.size(),0);
    actor_bound_.assign(actors_.size(),0);actor_erased_.assign(actors_.size(),0);actor_persistent_.assign(actors_.size(),0);
    inside_trigger_.assign(content.trigger_count(),0);trigger_pending_.assign(content.trigger_count(),0);
    stage_=OpeningStage::Walking;cutscene_done_=restore_pending_=overworld_music_=false;
    pending_actor_=talker_=kRoomNoActor;program_index_=follow_actor_=kRoomNoIndex;physics_tick_=idle_frame_=0;
    if(++generation_==0)++generation_;
    std::vector<uint32_t> active;active.reserve(content.polygon_count());
    for(uint32_t i=0;i<content.polygon_count();++i){bool enabled=false;if(!body_active(content.polygon(i).body_id,flags_,enabled,reviewed_mutations))return fail("Unmapped body activation rule");if(enabled)active.push_back(i);}
    active_polygons_=active;polygon_offsets_.assign(content.polygon_count(),{});erased_bodies_.clear();
    if(!solver_.configure_room(content,active))return fail("Unsupported collision geometry");
    persistent_player_.state_->action_=FieldPlayerTree{};persistent_player_.state_->action_resources_=nullptr;persistent_player_.state_->action_event_count_=0;persistent_player_.state_->action_revision_=0;
    persistent_player_.state_->player_=WalkState{};persistent_player_.state_->player_.position=position;persistent_player_.state_->player_.direction=direction;
    persistent_player_.state_->player_.speed=content.rule_f32(RoomRuleKey::WalkSpeed);persistent_player_.state_->player_.animation=story_flags?MotionAnimation::Idle:MotionAnimation(scene.initial_motion_state);
    for(uint32_t i=0;i<actors_.size();++i){
        const auto instance=content.actor_instance(i);actor_visible_[i]=(instance.flags&1)!=0;
        const bool controlled=i==scene.player_instance_index;
        if(!initialize_actor(actors_[i],content,instance.profile_index,controlled?position:instance.position,controlled?direction:instance.direction))return fail("Invalid actor profile");
        if(instance.initial_clip!=kRoomNoIndex){actors_[i].clip_index=instance.initial_clip;actors_[i].frame=content.key(content.clip(instance.initial_clip).first_key).frame;}
    }
    persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
    if(!content.frame_clip(persistent_player_.state_->clip_,clip))return fail("Missing initial motion animation");
    persistent_player_.state_->playback_={0,{scene.initial_frame,true,false}};
    if(!begin_clip(clip,content.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_))return fail("Invalid initial motion animation");
    if(!persistent_player_.state_->camera_.initialize(content_,persistent_player_.state_->player_.position,viewport_))return false;
    for(uint32_t i=0;i<content.binding_count();++i)if(content.binding(i).kind==uint16_t(RoomBindingKind::DeferredFlagBodyDeletion))flagged_bodies_.push_back({i,{}});
    error_="";healthy_=initialized_=true;persistent_player_.state_->initialized_=true;return refresh_scene_rules();
}
bool OpeningWorld::bind_walk_audio(WalkAudioSink&sink){
 if(!healthy_||walk_audio_||!sink.walk_audio_ready())return false;walk_audio_=&sink;return true;
}
bool OpeningWorld::bind_player_action(const FieldSwingResources&resources,std::string&error){
 const auto reject=[&](const char*why){error=why;return false;};
 if(!healthy_||player_read_only_||!resources.valid()||persistent_player_.state_->action_.ready()||stage_!=OpeningStage::Walking||has_cutscene_actors())return reject("Player action binding needs a fresh live motion owner");
 const auto&data=resources.entry();if(data.capability()!=2)return reject("Player action binding requires the complete source graph");
 // Check each actual room motion binding, rather than attaching this player's
 // graph to another actor merely because its initial frame happens to fit.
 const auto scene=content_.scene();const auto profile=content_.actor_profile(content_.actor_instance(scene.player_instance_index).profile_index);
 uint32_t expected_bindings=0;for(auto id:data.motion_states())expected_bindings+=data.state(id)->count;
 if(profile.execution_kind!=1)return reject("Player graph requires the audited Player profile");
 uint32_t normal_bindings=0;
 for(uint32_t i=0;i<profile.animation_binding_count;++i){const auto binding=content_.animation_binding(profile.animation_binding_first+i);
  // Room role 4 is the separate cutscene Actor's walk, checked by RoomData
  // and the Actor owner. It is not a Player AnimationTree motion branch.
  if(binding.motion_state==4)continue;
  if(binding.motion_state>=data.motion_states().size())return reject("Player room motion role is unsupported");++normal_bindings;
  const auto room_clip=content_.clip(binding.clip_index);FrameClip old;if(!content_.frame_clip(binding.clip_index,old))return reject("Player room motion clip unavailable");
  const auto*state=data.state(data.motion_states()[binding.motion_state]);const SwingEntryClip*clip=nullptr;
  for(uint32_t j=0;j<state->count;++j){const auto&candidate=data.clips()[state->first+j];if(direction_index(candidate.direction)==binding.direction){if(clip)return reject("Player graph room direction is ambiguous");clip=&candidate;}}
  if(!clip)return reject("Player graph/room direction is missing");
  uint32_t reachable=0;for(uint32_t k=0;k<old.count;++k)if(old.keys[k].time<=old.length)++reachable;
  if(room_clip.frame_count!=data.frames()||old.length!=clip->clip.length||old.loop!=clip->clip.loop||reachable!=clip->clip.count){error="Player graph/room clip differs: motion "+std::to_string(binding.motion_state)+" direction "+std::to_string(binding.direction)+" frames "+std::to_string(room_clip.frame_count)+"/"+std::to_string(data.frames())+" length "+std::to_string(old.length)+"/"+std::to_string(clip->clip.length)+" loop "+std::to_string(old.loop)+"/"+std::to_string(clip->clip.loop)+" reachable keys "+std::to_string(reachable)+"/"+std::to_string(clip->clip.count);return false;}
  // The original Idle Right retains six typed frame keys beyond its 0.1s
  // loop. Neither executor can reach those keys; Entry's source proof records
  // them explicitly. Compare the complete reachable curve, never a prefix.
  uint32_t key=0;for(uint32_t k=0;k<old.count;++k)if(old.keys[k].time<=old.length){if(old.keys[k].time!=clip->clip.keys[key].time||old.keys[k].frame!=clip->clip.keys[key].frame)return reject("Player graph/room frame keys differ");++key;}
 }
 if(normal_bindings!=expected_bindings)return reject("Player graph/room motion coverage differs");
 auto&owner=*persistent_player_.state_;if(owner.action_revision_==UINT64_MAX)return reject("Player action revision exhausted");
 FieldPlayerTree candidate;const auto state=data.motion_states()[uint8_t(owner.player_.animation)];
 const auto*initial=data.select(state,owner.player_.direction);const auto position=initial&&initial->clip.loop&&owner.playback_.position==initial->clip.length?0:owner.playback_.position;
 if(!candidate.initialize(data,resources.action(),resources.swing(),state,owner.player_.direction,position,"",error))return false;
 if(owner.house_paused_){auto flags=swing_flags(owner.player_);if(!candidate.pause(false,true,false,flags,error))return false;}
 owner.action_=std::move(candidate);owner.action_resources_=&resources;++owner.action_revision_;error.clear();return true;
}
bool OpeningWorld::player_action_event(PlayerActionEvent event){auto&owner=*persistent_player_.state_;if(owner.action_event_count_>=owner.action_events_.size())return fail("Player action event queue exhausted");owner.action_events_[owner.action_event_count_++]=event;return true;}
std::vector<PlayerActionEvent> OpeningWorld::take_player_action_events(){
 if(!healthy_||player_read_only_||!persistent_player_)return {};
 auto&owner=*persistent_player_.state_;std::vector<PlayerActionEvent>out(owner.action_events_.begin(),owner.action_events_.begin()+owner.action_event_count_);owner.action_event_count_=0;return out;
}
bool OpeningWorld::player_action_input(std::string_view name,bool pressed,bool available,bool incap,bool entering,bool&accepted,std::string&error){
 if(!healthy_||player_read_only_||!persistent_player_||!persistent_player_.state_->action_.ready()){error="Player action owner unavailable";return false;}
 auto&owner=*persistent_player_.state_;if(owner.action_revision_==UINT64_MAX||owner.action_event_count_>=owner.action_events_.size()){error="Player action revision/event budget exhausted";return false;}
 SwingActionContext context;context.direction=owner.player_.direction;context.moving=stage_==OpeningStage::Walking&&!actor_bound_[content_.scene().player_instance_index];context.paused=owner.house_paused_;context.entering_door=entering;context.skill_available=available;context.incapacitated=incap;
 auto flags=swing_flags(owner.player_);if(!owner.action_.input(name,pressed,context,flags,accepted,error))return false;
 ++owner.action_revision_;apply_swing_flags(owner.player_,flags);
 if(accepted){WalkAudioCalls calls;calls.count=1;calls.calls[0]=WalkAudioCall::SetRunningFalse;if(!emit_walk_audio(calls)||!player_action_event(PlayerActionEvent::Start))return false;}
 error.clear();return true;
}
bool OpeningWorld::player_action_pause(bool stop,bool idle){
 auto&owner=*persistent_player_.state_;if(!owner.action_.ready())return true;
 if(owner.action_revision_==UINT64_MAX||owner.action_event_count_>=owner.action_events_.size())return fail("Player action pause budget exhausted");
 auto flags=swing_flags(owner.player_);std::string why;if(!owner.action_.pause(stop,idle,owner.action_.incapacitated(),flags,why))return fail("Player action pause rejected");apply_swing_flags(owner.player_,flags);++owner.action_revision_;return player_action_event(PlayerActionEvent::Pause);
}
bool OpeningWorld::player_action_resume(){
 auto&owner=*persistent_player_.state_;if(!owner.action_.ready())return true;
 if(owner.action_revision_==UINT64_MAX||owner.action_event_count_>=owner.action_events_.size())return fail("Player action resume budget exhausted");
 std::string why;if(!owner.action_.unpause(why))return fail("Player action resume rejected");++owner.action_revision_;return player_action_event(PlayerActionEvent::Resume);
}
bool OpeningWorld::player_action_direction(bool idle){
 auto&owner=*persistent_player_.state_;if(!owner.action_.ready())return true;
 if(owner.action_revision_==UINT64_MAX)return fail("Player action direction revision exhausted");
 std::string why;const auto target=owner.action_.incapacitated()?owner.action_resources_->entry().incapacitated_idle():owner.action_resources_->entry().idle();
 if(!(idle?owner.action_.set_idle_direction(owner.player_.direction,why):owner.action_.set_direction(owner.player_.direction,why))||(idle&&!owner.action_.travel(target,why)))return fail("Player action direction rejected");++owner.action_revision_;return true;
}
bool OpeningWorld::prepare_scene_player(OpeningWorld& source,Vec2 position,Vec2 direction,ScenePlayerTransfer& result,std::string& error){
    const auto reject=[&](const char*why){error=why;return false;};
    if(this==&source||!healthy_||!source.healthy_||player_read_only_||source.player_read_only_||
       !persistent_player_||!source.persistent_player_||persistent_player_.state_->references_!=1||
       stage_!=OpeningStage::Walking||source.stage_!=OpeningStage::Walking||
       !house_paused()||!source.house_paused()||has_cutscene_actors()||source.has_cutscene_actors()||
       restore_pending_||source.restore_pending_||battle_player_visible()||source.battle_player_visible())
        return reject("Scene player handoff requires independent prepared and paused scene bindings");
    const auto old=source.content_;const auto next=content_;
    const auto a=old.scene(),b=next.scene();
    const auto pa=old.actor_profile(old.actor_instance(a.player_instance_index).profile_index);
    const auto pb=next.actor_profile(next.actor_instance(b.player_instance_index).profile_index);
    const auto same_vector=[](Vec2 x,Vec2 y){return x.x==y.x&&x.y==y.y;};
    const auto same_resource=[&](uint32_t x,uint32_t y){
        if(x==kRoomNoIndex||y==kRoomNoIndex)return x==y;
        const auto r=old.resource(x),s=next.resource(y);
        return r.stable_id==s.stable_id&&old.string(r.path_string)==next.string(s.path_string)&&r.sha256==s.sha256&&
            r.width==s.width&&r.height==s.height&&r.columns==s.columns&&r.rows==s.rows&&r.kind==s.kind&&r.flags==s.flags;
    };
    const auto same_clip=[&](uint32_t x,uint32_t y){
        if(x==kRoomNoIndex||y==kRoomNoIndex)return x==y;
        const auto r=old.clip(x),s=next.clip(y);
        if(r.stable_id!=s.stable_id||r.length!=s.length||r.key_count!=s.key_count||r.flags!=s.flags||r.visibility!=s.visibility||r.frame_count!=s.frame_count||r.channel!=s.channel)return false;
        for(uint32_t i=0;i<r.key_count;++i){const auto u=old.key(r.first_key+i),v=next.key(s.first_key+i);if(u.time!=v.time||u.frame!=v.frame)return false;}
        return true;
    };
    if(a.rule_profile_id!=b.rule_profile_id||old.rule_count()!=next.rule_count()||a.actor_hull_count!=b.actor_hull_count||
       pa.stable_id!=pb.stable_id||pa.execution_kind!=pb.execution_kind||pa.flags!=pb.flags||
       pa.initial_frame!=pb.initial_frame||pa.emote_initial_frame!=pb.emote_initial_frame||
       !same_vector(pa.sprite_position,pb.sprite_position)||!same_vector(pa.sprite_offset,pb.sprite_offset)||
       !same_vector(pa.emote_offset,pb.emote_offset)||!same_vector(pa.shadow_offset,pb.shadow_offset)||
       pa.direction_count!=pb.direction_count||pa.animation_binding_count!=pb.animation_binding_count||
       !same_resource(pa.primary_resource,pb.primary_resource)||!same_resource(pa.shadow_resource,pb.shadow_resource)||!same_resource(pa.emote_resource,pb.emote_resource)||
       !same_clip(pa.idle_clip,pb.idle_clip)||!same_clip(pa.emote_clip,pb.emote_clip)||
       source.persistent_player_.state_->party_leader_!=persistent_player_.state_->party_leader_)
        return reject("Scene player identity or profile differs from the persistent source");
    for(uint32_t i=0;i<old.rule_count();++i){
        const auto r=old.rule(i),s=next.rule(r.key);
        if(r.scalar_type!=s.scalar_type||r.f32!=s.f32||r.f64!=s.f64||r.u32!=s.u32)return reject("Scene player rule parameters differ");
    }
    for(uint32_t i=0;i<a.actor_hull_count;++i)if(!same_vector(old.vec2(a.actor_hull_first+i),next.vec2(b.actor_hull_first+i)))return reject("Scene player local collider differs");
    for(uint32_t i=0;i<pa.direction_count;++i)if(old.direction_frame(pa.direction_first+i)!=next.direction_frame(pb.direction_first+i))return reject("Scene player direction frames differ");
    for(uint32_t i=0;i<pa.animation_binding_count;++i){
        const auto r=old.animation_binding(pa.animation_binding_first+i);uint32_t match=kRoomNoIndex;
        for(uint32_t j=0;j<pb.animation_binding_count;++j){const auto s=next.animation_binding(pb.animation_binding_first+j);if(r.motion_state==s.motion_state&&r.direction==s.direction){match=s.clip_index;break;}}
        if(match==kRoomNoIndex||!same_clip(r.clip_index,match))return reject("Scene player animation bindings differ");
    }
    auto& owner=*source.persistent_player_.state_;WalkState player;
    if(!relocate_walk(owner.player_,position,direction,player))return reject("Scene player transport pose rejected");
    auto camera=owner.camera_;
    if(!camera.rebind_scene(next,position,viewport_))return reject("Scene player camera binding rejected");
    const auto selected=walk_clip(player.animation,player.direction);FrameClip clip;
    auto playback=owner.playback_;
    if(!next.frame_clip(selected,clip)||!sample_clip(clip,playback.position,next.clip(selected).frame_count,playback.sample))return reject("Scene player animation transport rejected");
    ScenePlayerTransfer prepared;
    prepared.before_=owner.player_;prepared.player_=player;prepared.before_camera_=owner.camera_;prepared.camera_=std::move(camera);
    prepared.before_playback_=owner.playback_;prepared.playback_=playback;prepared.before_clip_=owner.clip_;prepared.clip_=selected;
    prepared.destination_before_=persistent_player_.state_->player_;prepared.destination_camera_=persistent_player_.state_->camera_;
    prepared.destination_playback_=persistent_player_.state_->playback_;prepared.destination_clip_=persistent_player_.state_->clip_;
    prepared.source_physics_=source.physics_tick_;prepared.source_idle_=source.idle_frame_;prepared.source_story_=source.story_revision_;
    prepared.source_action_=owner.action_revision_;prepared.destination_action_=persistent_player_.state_->action_revision_;prepared.action_=owner.action_;
    if(owner.action_.ready()){
        if(owner.action_revision_==UINT64_MAX||!prepared.action_.set_direction(player.direction,error))return false;
        if(persistent_player_.state_->action_.ready()&&persistent_player_.state_->action_resources_!=owner.action_resources_)return reject("Scene player action resource owner differs");
    }else if(persistent_player_.state_->action_.ready())return reject("Scene player action owner cannot be introduced during transfer");
    prepared.destination_physics_=physics_tick_;prepared.destination_idle_=idle_frame_;prepared.destination_story_=story_revision_;
    prepared.source_content_=old.bytes();prepared.destination_content_=next.bytes();prepared.leader_=owner.party_leader_;
    prepared.source_=source.persistent_player_;prepared.destination_=persistent_player_;
    result=std::move(prepared);error.clear();return true;
}
namespace {
bool same_walk(const WalkState&a,const WalkState&b){
    return a.position.x==b.position.x&&a.position.y==b.position.y&&a.direction.x==b.direction.x&&a.direction.y==b.direction.y&&
        a.velocity.x==b.velocity.x&&a.velocity.y==b.velocity.y&&a.speed==b.speed&&a.crouch==b.crouch&&a.tap_run==b.tap_run&&
        a.running==b.running&&a.substantial==b.substantial&&a.walking==b.walking&&a.previous_toggle==b.previous_toggle&&a.animation==b.animation&&a.moved_signals==b.moved_signals;
}
bool same_playback(const AnimationPlayback&a,const AnimationPlayback&b){return a.position==b.position&&a.sample.frame==b.sample.frame&&a.sample.main_visible==b.sample.main_visible&&a.sample.special_visible==b.sample.special_visible;}
}
bool OpeningWorld::commit_scene_player(OpeningWorld&source,ScenePlayerTransfer&&prepared,std::string&error){
    const auto reject=[&](){error="Prepared scene player transfer is absent, stale or belongs to another scene";return false;};
    if(this==&source||!prepared.prepared()||!healthy_||!source.healthy_||player_read_only_||source.player_read_only_||
       persistent_player_.get()!=prepared.destination_.get()||source.persistent_player_.get()!=prepared.source_.get()||
       persistent_player_.state_->references_!=2||!house_paused()||!source.house_paused()||
       stage_!=OpeningStage::Walking||source.stage_!=OpeningStage::Walking||has_cutscene_actors()||source.has_cutscene_actors()||
       restore_pending_||source.restore_pending_||battle_player_visible()||source.battle_player_visible()||
       content_.bytes()!=prepared.destination_content_||source.content_.bytes()!=prepared.source_content_||
       physics_tick_!=prepared.destination_physics_||idle_frame_!=prepared.destination_idle_||story_revision_!=prepared.destination_story_||
       source.physics_tick_!=prepared.source_physics_||source.idle_frame_!=prepared.source_idle_||source.story_revision_!=prepared.source_story_)return reject();
    auto&owner=*source.persistent_player_.state_;auto&destination=*persistent_player_.state_;
    if(owner.party_leader_!=prepared.leader_||destination.party_leader_!=prepared.leader_||
       owner.action_revision_!=prepared.source_action_||destination.action_revision_!=prepared.destination_action_||
       !same_walk(owner.player_,prepared.before_)||!same_walk(destination.player_,prepared.destination_before_)||
       !same_playback(owner.playback_,prepared.before_playback_)||!same_playback(destination.playback_,prepared.destination_playback_)||
       owner.clip_!=prepared.before_clip_||destination.clip_!=prepared.destination_clip_||
       !owner.camera_.same_state(prepared.before_camera_)||!destination.camera_.same_state(prepared.destination_camera_))return reject();
    // No allocation or other fallible work remains. Move the retained handle,
    // never the Player object. An empty token cannot be published a second time.
    owner.player_=prepared.player_;owner.camera_=std::move(prepared.camera_);owner.playback_=prepared.playback_;owner.clip_=prepared.clip_;
    if(owner.action_.ready()){owner.action_=std::move(prepared.action_);++owner.action_revision_;}
    persistent_player_=std::move(prepared.source_);prepared.destination_={};
    source.healthy_=false;source.player_read_only_=true;source.walk_audio_=nullptr;source.error_="Scene player binding retired";
    error.clear();return true;
}
bool OpeningWorld::adopt_scene_player(OpeningWorld&source,Vec2 position,Vec2 direction,std::string&error){
    ScenePlayerTransfer prepared;return prepare_scene_player(source,position,direction,prepared,error)&&commit_scene_player(source,std::move(prepared),error);
}
bool OpeningWorld::emit_walk_audio(const WalkAudioCalls&calls){
 if(!walk_audio_)return true;std::string why;
 return walk_audio_->apply_walk_calls(calls,why)||fail("Player run-loop rejected source calls");
}
bool OpeningWorld::advance(WalkInput input) {
    if(!healthy_)return false;
    ++physics_tick_;
    // Input edges expire even while story/battle owns the player. A button
    // held through unpause must not become a new movement toggle press.
    if(stage_!=OpeningStage::Walking)persistent_player_.state_->player_.previous_toggle=input.toggle;
    if(stage_==OpeningStage::BattleRequested){if(!battle_accepted_)return true;if(persistent_player_.state_->return_player_visible_&&!actor_physics_step(actors_[content_.scene().player_instance_index]))return fail("Return actor physics rejected");for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!persistent_player_.state_->camera_.update_actor_position(i,actors_[i].position))return fail("Actor camera position rejected");return persistent_player_.state_->camera_.physics_frame(1.0/60.0);}
    if(stage_==OpeningStage::ScriptRunning){
        // An unreplaced paused Player still executes MOVE's stop branch; an
        // Actor replacement disables the original Player's physics callback.
        if(walk_audio_&&!actor_bound_[content_.scene().player_instance_index]){WalkAudioCalls calls;calls.count=1;calls.calls[0]=WalkAudioCall::StopIfPlaying;if(!emit_walk_audio(calls))return false;}
        for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!actor_physics_step(actors_[i]))return fail("Actor physics rejected");
        for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!persistent_player_.state_->camera_.update_actor_position(i,actors_[i].position))return fail("Actor camera position rejected");
        if(!persistent_player_.state_->camera_.physics_frame(1.0/60.0))return fail("Camera physics rejected");
        return true;
    }
    // Area signals describe the previous completed physics motion, before this
    // tick's player callback. The idle phase later pauses/starts the program.
    for(uint32_t i=0;i<content_.trigger_count();++i){
        const bool overlap=first_trigger_overlap(content_,i,persistent_player_.state_->player_.position);
        if(overlap&&!inside_trigger_[i])trigger_pending_[i]=1;
        if(!overlap)trigger_pending_[i]=0;
        inside_trigger_[i]=overlap;
    }
    input.paused=input.paused||persistent_player_.state_->house_paused_;
    auto&owner=*persistent_player_.state_;
    if(owner.action_.ready()&&owner.action_.attacking()){
        // ATTACK has no movement or recovery query in the source. Keep its
        // velocity/speed snapshot and still process the independent Areas.
        owner.player_.previous_toggle=input.toggle;owner.player_.running=false;
        WalkAudioCalls calls;calls.count=1;calls.calls[0]=WalkAudioCall::SetRunningFalse;
        return emit_walk_audio(calls)&&owner.camera_.physics_frame(1.0/60.0);
    }
    const bool update_blend=input.x||input.y||persistent_player_.state_->player_.tap_run;
    WalkAudioCalls calls;auto player=persistent_player_.state_->player_;if(!advance_walk(player,input,solver_,content_,&calls))return fail("Movement rejected");
    const uint32_t selected=walk_clip(player.animation,player.direction);FrameClip clip;
    if(!content_.frame_clip(selected,clip))return fail("Missing motion animation binding");
    auto playback=persistent_player_.state_->playback_;
    if(selected!=persistent_player_.state_->clip_&&!begin_clip(clip,content_.clip(selected).frame_count,playback))return fail("Motion animation rejected");
    if(!input.paused&&!input.entering_door&&!advance_clip(clip,1.0f/60.0f,content_.clip(selected).frame_count,playback))return fail("Motion animation update rejected");
    if(!emit_walk_audio(calls))return false;
    persistent_player_.state_->player_=player;persistent_player_.state_->playback_=playback;persistent_player_.state_->clip_=selected;
    if(owner.action_.ready()&&!input.paused&&!input.entering_door){
        if(update_blend&&!player_action_direction())return false;std::string why;
        if(!owner.action_.travel(owner.action_resources_->entry().motion_states()[uint8_t(player.animation)],why))return fail("Player motion graph request rejected");
    }
    persistent_player_.state_->camera_.update_player_position(persistent_player_.state_->player_.position);return persistent_player_.state_->camera_.physics_frame(1.0/60.0);
}
bool OpeningWorld::begin_house_program(uint32_t program_index,uint32_t original_npc){
    if(!healthy_||stage_!=OpeningStage::Walking||program_index>=content_.program_count()||dialogue_.active())return false;
    std::string admission;if(!validate_story_props(program_index,admission))return false;
    dialogue_=DialoguePlayer{};battle_=OpeningBattleRequest{};battle_accepted_=false;persistent_player_.state_->return_player_visible_=false;
    cutscene_done_=restore_pending_=false;choice_group_=kRoomNoIndex;save_requested_=storage_requested_=false;story_hides_.clear();pending_dialogue_id_=kRoomNoIndex;pending_actor_=talker_=kRoomNoActor;
    if(++generation_==0)++generation_;
    story_talking_=false;story_talker_=original_npc==kRoomNoIndex?DialogueTalker{}:DialogueTalker{DialogueTalkerKind::OriginalNpc,original_npc};
    program_index_=program_index;persistent_player_.state_->house_paused_=false;
    if(!dialogue_.start(content_,program_index_,*this,generation_))return fail(dialogue_.error());
    return flush_deferred();
}
bool OpeningWorld::begin_battle_continuation(std::string_view source_path){
    if(!healthy_||stage_!=OpeningStage::BattleRequested||!battle_accepted_||!persistent_player_.state_->return_player_visible_||source_path.empty()||content_.string(battle_.win_cutscene_string)!=source_path)return false;
    uint32_t program=kRoomNoIndex;
    for(uint32_t i=0;i<content_.program_count();++i)if(content_.string(content_.program(i).source_path_string)==source_path){program=i;break;}
    if(program==kRoomNoIndex||content_.command(content_.program(program).first_command+content_.program(program).command_count-1).opcode!=uint16_t(DialogueActionKind::DialogueDone))return false;
    const auto controlled=content_.scene().player_instance_index;persistent_player_.state_->player_.position=actors_[controlled].position;persistent_player_.state_->player_.direction=actors_[controlled].direction;
    if(!player_action_direction())return false;
    persistent_player_.state_->camera_.resume();stage_=OpeningStage::Walking;error_="";
    return begin_house_program(program);
}
bool OpeningWorld::refresh_scene_rules(){
    if(!healthy_)return false;
    for(auto& bound:flagged_bodies_){
        const auto rule=content_.binding(bound.binding);
        if(bound.state.in_tree&&flags_.story_flag(rule.auxiliary_index)==(rule.value!=0))bound.state.queued_for_deletion=true;
    }return true;
}
bool OpeningWorld::end_scene_frame(){
    if(!healthy_)return false;
    auto&owner=*persistent_player_.state_;if(owner.action_.ready()){
        if(owner.action_revision_==UINT64_MAX)return fail("Player action callback revision exhausted");
        auto flags=swing_flags(owner.player_);std::string why;if(!owner.action_.flush(flags,why))return fail("Player action callback rejected");apply_swing_flags(owner.player_,flags);++owner.action_revision_;
    }
    for(auto&bound:flagged_bodies_)if(bound.state.in_tree&&bound.state.queued_for_deletion){
        const auto body=content_.binding(bound.binding).target_index;
        if(!set_body_enabled(body,false))return fail("Deferred flag-body deletion rejected");
        if(std::find(erased_bodies_.begin(),erased_bodies_.end(),body)==erased_bodies_.end())erased_bodies_.push_back(body);
        flush_flag_landmark_deletion(bound.state);
    }return true;
}
bool OpeningWorld::write_story_flag(uint32_t index,bool value,bool emit){
    if(!healthy_)return false;const auto result=flags_.set_story_flag(index,value,emit);if(result.flags_updated)++story_revision_;
    return result.applied&&(!result.flags_updated||refresh_scene_rules());
}
bool OpeningWorld::request_sound(uint32_t resource_index){
    if(!healthy_||resource_index>=content_.resource_count()||content_.resource(resource_index).kind!=2)return false;
    audio_.push_back({AudioRequestKind::PlayEffect,resource_index,0,phrase()});return true;
}
bool OpeningWorld::set_story_talking(bool talking){
    if(!healthy_)return false;
    if(story_talker_.kind==DialogueTalkerKind::Actor&&(story_talker_.index>=actors_.size()||!actor_set_talking(actors_[story_talker_.index],talking)))return false;
    story_talking_=talking;return true;
}
bool OpeningWorld::branch_condition(const DialogueAction& action,bool& matched){
    if(action.kind==DialogueActionKind::BranchFlag){if(action.target_index>=content_.flag_count())return false;matched=flags_.story_flag(content_.string(content_.flag(action.target_index).name_string))==(action.value!=0);return true;}
    if(action.kind==DialogueActionKind::BranchLeader){if(persistent_player_.state_->party_leader_.empty()||action.target_index>=content_.string_count())return false;matched=persistent_player_.state_->party_leader_==content_.string(action.target_index);return true;}
    return false;
}
bool OpeningWorld::choose_story_option(uint32_t pc,uint32_t generation){
    if(!healthy_||!story_choices_waiting()||generation!=dialogue_.generation()||pc>=content_.program(program_index_).command_count)return false;
    pending_dialogue_id_=kRoomNoIndex;choice_group_=kRoomNoIndex;
    if(!dialogue_.choices_selected(pc,generation,*this))return fail("Story choice callback rejected");
    return flush_deferred();
}
bool OpeningWorld::close_story_submenu(uint32_t generation){
    if(!healthy_||!story_submenu_waiting())return false;
    if(!dialogue_.submenu_closed(generation,*this))return fail("Story submenu callback rejected");
    return flush_deferred();
}
bool OpeningWorld::finish_story_dialogue(bool automatic){
    if(!healthy_||pending_dialogue_id_==kRoomNoIndex)return false;
    pending_dialogue_id_=kRoomNoIndex;
    if(!dialogue_.dialogue_finished(*this,automatic))return fail(dialogue_.error());
    return flush_deferred();
}
bool OpeningWorld::complete_actor_restore(uint32_t index,uint64_t revision){if(!healthy_||index>=actors_.size()||!revision||revision!=actor_binding_revisions_[index]||!actor_restore_[index])return false;actor_restore_[index]=0;actor_bound_[index]=0;actor_persistent_[index]=0;persistent_player_.state_->camera_.remove_actor(index);return true;}
bool OpeningWorld::accept_battle_entry(){if(!healthy_||!battle_.requested||battle_accepted_)return false;battle_accepted_=true;battle_original_direction_=persistent_player_.state_->player_.direction;return true;}
bool OpeningWorld::pause_for_house(bool stop_running){
    if(!healthy_||stage_!=OpeningStage::Walking)return false;
    if(!player_action_pause(stop_running,true))return false;
    persistent_player_.state_->house_paused_=true;persistent_player_.state_->player_.crouch=persistent_player_.state_->player_.walking=false;if(!player_action().ready())persistent_player_.state_->player_.velocity={};
    if(stop_running||!persistent_player_.state_->player_.tap_run){persistent_player_.state_->player_.tap_run=persistent_player_.state_->player_.running=false;WalkAudioCalls calls;calls.count=1;calls.calls[0]=WalkAudioCall::SetRunningFalse;if(!emit_walk_audio(calls))return false;}
    persistent_player_.state_->player_.animation=MotionAnimation::Idle;persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
    return content_.frame_clip(persistent_player_.state_->clip_,clip)&&begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_);
}
bool OpeningWorld::unpause_from_house(){if(!healthy_||stage_!=OpeningStage::Walking||!player_action_resume())return false;persistent_player_.state_->house_paused_=false;if(persistent_player_.state_->player_.running){WalkAudioCalls calls;calls.count=1;calls.calls[0]=WalkAudioCall::SetRunningTrue;if(!emit_walk_audio(calls))return false;}return true;}
bool OpeningWorld::resize_field_viewport(Vec2 viewport){if(!healthy_||stage_!=OpeningStage::Walking||!persistent_player_.state_->house_paused_||!persistent_player_.state_->camera_.resize_viewport(viewport))return false;viewport_=viewport;return true;}
bool OpeningWorld::set_house_direction(Vec2 direction){
    if(!healthy_||stage_!=OpeningStage::Walking)return false;
    if(direction.x==0&&direction.y==0)return true;
    const float x=std::abs(direction.x),y=std::abs(direction.y);const bool grid=(x==0||x==1)&&(y==0||y==1);const bool diagonal=x==y&&std::abs(x-std::sqrt(.5f))<1e-6f;
    if(!std::isfinite(direction.x)||!std::isfinite(direction.y)||(!grid&&!diagonal))return false;
    persistent_player_.state_->player_.direction=direction;persistent_player_.state_->player_.animation=MotionAnimation::Idle;persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
    if(!player_action_direction())return false;
    return content_.frame_clip(persistent_player_.state_->clip_,clip)&&begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_);
}
bool OpeningWorld::face_interaction(Vec2 target){
    if(!healthy_||stage_!=OpeningStage::Walking||!std::isfinite(target.x)||!std::isfinite(target.y))return false;
    const auto position=persistent_player_.state_->player_.position;const Vec2 delta{target.x-position.x,target.y-position.y};
    const float length=std::hypot(delta.x,delta.y);if(!std::isfinite(length))return false;if(length==0)return true;
    // Player._turn_to overrides the unconstrained party_object method.
    // Both source axes are enabled here; ties choose the vertical axis.
    persistent_player_.state_->player_.direction=std::abs(delta.x)>std::abs(delta.y)?Vec2{delta.x>0?1.f:-1.f,0}:Vec2{0,delta.y>0?1.f:-1.f};persistent_player_.state_->player_.animation=MotionAnimation::Idle;
    persistent_player_.state_->clip_=walk_clip(MotionAnimation::Idle,persistent_player_.state_->player_.direction);FrameClip clip;
    if(!player_action_direction(true))return false;
    return content_.frame_clip(persistent_player_.state_->clip_,clip)&&begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_);
}
bool OpeningWorld::warp_same_scene(Vec2 position,Vec2 direction){
    if(!healthy_||stage_!=OpeningStage::Walking||!std::isfinite(position.x)||!std::isfinite(position.y)||!set_house_direction(direction))return false;
    persistent_player_.state_->player_.position=position;persistent_player_.state_->player_.velocity={};
    return persistent_player_.state_->camera_.relocate_player(position);
}
bool OpeningWorld::script_move(Vec2 position,Vec2 direction,bool walking){
    if(!healthy_||stage_!=OpeningStage::Walking||!std::isfinite(position.x)||!std::isfinite(position.y))return false;
    const float x=std::abs(direction.x),y=std::abs(direction.y);
    if(!std::isfinite(direction.x)||!std::isfinite(direction.y)||!((x==0||x==1)&&(y==0||y==1))||(x==0&&y==0))return false;
    auto& p=persistent_player_.state_->player_;
    const auto anim=walking?MotionAnimation::Walk:MotionAnimation::Idle;
    const bool changed=p.direction.x!=direction.x||p.direction.y!=direction.y||p.animation!=anim;
    p.position=position;p.velocity={};p.direction=direction;p.animation=anim;p.walking=walking;p.running=p.crouch=false;
    if(!player_action_direction())return false;
    if(player_action().ready()){std::string why;if(!persistent_player_.state_->action_.travel(persistent_player_.state_->action_resources_->entry().motion_states()[uint8_t(anim)],why))return fail("Scripted player graph motion rejected");}
    if(changed){
        persistent_player_.state_->clip_=walk_clip(anim,direction);FrameClip clip;
        if(!content_.frame_clip(persistent_player_.state_->clip_,clip)||!begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_))return false;
    }
    if(walking){FrameClip clip;const auto selected=persistent_player_.state_->clip_;
        if(!content_.frame_clip(selected,clip)||!advance_clip(clip,1.f/60.f,content_.clip(selected).frame_count,persistent_player_.state_->playback_))return false;}
    return persistent_player_.state_->camera_.relocate_player(position);
}
bool OpeningWorld::set_initial_actor_pose(uint32_t index,Vec2 position,Vec2 direction){
    if(physics_tick_||idle_frame_)return false;
    return sync_unbound_actor_pose(index,position,direction);
}
bool OpeningWorld::sync_unbound_actor_pose(uint32_t index,Vec2 position,Vec2 direction){
    if(!healthy_||stage_!=OpeningStage::Walking||index>=actors_.size()||index==content_.scene().player_instance_index||actor_bound_[index]||!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(direction.x)||!std::isfinite(direction.y)||(!direction.x&&!direction.y))return false;
    auto&actor=actors_[index];actor.position=position;actor.direction=direction;return true;
}
bool OpeningWorld::body_enabled(uint32_t body_id)const{
    for(auto i:active_polygons_)if(content_.polygon(i).body_id==body_id)return true;
    return false;
}
bool OpeningWorld::set_body_enabled(uint32_t body_id,bool enabled){
    if(!healthy_)return false;
    bool known=false;for(uint32_t i=0;i<content_.polygon_count();++i)if(content_.polygon(i).body_id==body_id){known=true;break;}
    // A dynamic field actor owns a checked BodyRule without a static polygon.
    // Its collision adapter observes instance visibility after battle erasure.
    if(!known)for(uint32_t i=0;i<content_.body_rule_count();++i)if(content_.body_rule(i).body_id==body_id){known=true;break;}
    if(!known)return false;
    if(body_enabled(body_id)==enabled)return true;
    auto active=active_polygons_;
    if(enabled){for(uint32_t i=0;i<content_.polygon_count();++i)if(content_.polygon(i).body_id==body_id)active.push_back(i);std::sort(active.begin(),active.end());}
    else active.erase(std::remove_if(active.begin(),active.end(),[&](uint32_t i){return content_.polygon(i).body_id==body_id;}),active.end());
    if(!solver_.configure_room(content_,active,polygon_offsets_))return false;
    active_polygons_=std::move(active);return true;
}
bool OpeningWorld::body_visible(uint32_t body_id)const{
    if(std::find(erased_bodies_.begin(),erased_bodies_.end(),body_id)!=erased_bodies_.end())return false;
    for(uint32_t i=0;i<content_.body_rule_count();++i){const auto rule=content_.body_rule(i);if(rule.body_id==body_id)return rule.initially_enabled;}
    return false;
}
bool OpeningWorld::set_body_offset(uint32_t body_id,Vec2 delta){
    if(!healthy_||!std::isfinite(delta.x)||!std::isfinite(delta.y)||std::abs(delta.x)>1000000||std::abs(delta.y)>1000000)return false;
    auto offsets=polygon_offsets_;bool found=false,changed=false;
    for(uint32_t i=0;i<content_.polygon_count();++i)if(content_.polygon(i).body_id==body_id){found=true;changed|=offsets[i].x!=delta.x||offsets[i].y!=delta.y;offsets[i]=delta;}
    if(!found)return false;
    if(!changed)return true;
    if(!solver_.configure_room(content_,active_polygons_,offsets))return false;
    polygon_offsets_=std::move(offsets);return true;
}
bool OpeningWorld::erase_battle_actor(uint32_t body_id){
    if(!healthy_||!battle_accepted_||battle_.actor_index>=actors_.size()||!set_body_enabled(body_id,false))return false;
    erased_bodies_.push_back(body_id);actor_visible_[battle_.actor_index]=0;actor_bound_[battle_.actor_index]=0;actor_persistent_[battle_.actor_index]=0;return true;
}
bool OpeningWorld::set_battle_story_flag(){return healthy_&&battle_accepted_&&write_story_flag(battle_.win_flag_index,true);}
bool OpeningWorld::set_story_flag(std::string_view name,bool value,bool emit){if(!healthy_)return false;const auto result=flags_.set_story_flag(name,value,emit);if(result.flags_updated)++story_revision_;return result.applied&&(!result.flags_updated||refresh_scene_rules());}
bool OpeningWorld::start_battle_return_camera(Vec2 target,double duration){return healthy_&&battle_accepted_&&persistent_player_.state_->camera_.return_offset(target,duration)&&persistent_player_.state_->camera_.idle_frame(last_idle_delta_);}
bool OpeningWorld::start_area_music(uint32_t resource,double gain,double fadein){
 if(!healthy_||resource>=content_.resource_count()||content_.resource(resource).kind!=uint16_t(RoomResourceKind::AudioRequestOnly)||!std::isfinite(gain)||gain<-120||gain>24||!std::isfinite(fadein)||fadein<0||fadein>60)return false;
 area_music_resource_=resource;audio_.push_back({AudioRequestKind::FadeInMusic,resource,fadein,0,float(gain)});return true;
}
bool OpeningWorld::stop_area_music(){
    if(!healthy_)return false;
    if(area_music_resource_!=kRoomNoIndex){audio_.push_back({AudioRequestKind::StopMusicResource,area_music_resource_,0,dialogue_.phrase()});area_music_resource_=kRoomNoIndex;}
    return true;
}
bool OpeningWorld::resume_battle_camera(){if(!healthy_||!battle_accepted_)return false;persistent_player_.state_->camera_.resume();return true;}
bool OpeningWorld::land_battle_player(Vec2 direction,uint16_t frame){
    if(!healthy_||!battle_accepted_)return false;
    const auto index=content_.scene().player_instance_index;
    auto& actor=actors_[index];if(!initialize_actor(actor,content_,content_.actor_instance(index).profile_index,persistent_player_.state_->player_.position,direction))return false;
    actor.frame=frame;actor_bound_[index]=1;persistent_player_.state_->return_player_visible_=true;return true;
}
bool OpeningWorld::rotate_battle_player(double interval){
    if(!healthy_||!battle_accepted_||!persistent_player_.state_->return_player_visible_)return false;
    return actor_turn_to(actors_[content_.scene().player_instance_index],battle_original_direction_,interval);
}
bool OpeningWorld::finish_battle_return(){
    if(!healthy_||!battle_accepted_||!persistent_player_.state_->return_player_visible_||!flags_.story_flag(battle_.win_flag_index))return false;
    const auto index=content_.scene().player_instance_index;persistent_player_.state_->player_.direction=actors_[index].direction;
    persistent_player_.state_->player_.animation=MotionAnimation::Idle;persistent_player_.state_->player_.velocity={};persistent_player_.state_->player_.walking=persistent_player_.state_->player_.running=persistent_player_.state_->player_.crouch=false;
    actor_bound_[index]=0;persistent_player_.state_->return_player_visible_=false;battle_accepted_=false;battle_.requested=false;battle_.queued=false;
    stage_=OpeningStage::Walking;error_="";
    if(!player_action_direction()||!player_action_resume())return false;
    persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
    return content_.frame_clip(persistent_player_.state_->clip_,clip)&&begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_);
}
bool OpeningWorld::fail(const char* message){healthy_=false;stage_=OpeningStage::Error;error_=message;dialogue_.cancel();return false;}
bool OpeningWorld::trigger_conditions(uint32_t index) const {
    const auto trigger=content_.trigger(index);
    for(uint32_t i=0;i<trigger.condition_count;++i){const auto condition=content_.condition(trigger.condition_first+i);if(flags_.story_flag(condition.flag_index)!=condition.expected_value)return false;}
    return true;
}
bool OpeningWorld::idle_frame(double delta) {
    if(!healthy_)return false;
    if(!std::isfinite(delta)||delta<0||delta>double(0.1f))return fail("Unsupported idle delta");
    if(persistent_player_.state_->action_.ready()){
        if(persistent_player_.state_->action_revision_==UINT64_MAX)return fail("Player action animation revision exhausted");
        std::string why;if(!persistent_player_.state_->action_.advance(delta,why))return fail("Player action animation rejected");++persistent_player_.state_->action_revision_;
    }
    if(delta==0)return true;
    last_idle_delta_=double(float(delta));
    if(stage_==OpeningStage::BattleRequested){if(!battle_accepted_)return true;if(restore_pending_){actor_bound_[content_.scene().player_instance_index]=0;restore_pending_=false;}if(persistent_player_.state_->return_player_visible_&&!actor_scene_timers(actors_[content_.scene().player_instance_index],delta))return fail("Return actor animation rejected");return persistent_player_.state_->camera_.idle_frame(delta)&&process_room_shakers(delta);}
    delta=double(float(delta));++idle_frame_;
    if(restore_pending_){actor_bound_[content_.scene().player_instance_index]=0;restore_pending_=false;}
    const bool was_active=dialogue_.active();
    if(was_active&&!dialogue_.idle_begin(*this))return fail(dialogue_.error());
    if(stage_==OpeningStage::Walking){
        for(uint32_t i=0;i<trigger_pending_.size();++i)if(trigger_pending_[i]){
            trigger_pending_[i]=0;
            if(trigger_conditions(i)){const auto target=content_.trigger(i).program_index;std::string admission;if(!validate_story_props(target,admission))return fail("Story object host admission rejected");program_index_=target;if(!dialogue_.start(content_,program_index_,*this,generation_))return fail(dialogue_.error());break;}
        }
    }
    const uint32_t controlled=content_.scene().player_instance_index;
    for(uint32_t i=0;i<actors_.size();++i)if(!actor_erased_[i]&&(i!=controlled||actor_bound_[i]))if(!actor_idle_animations(actors_[i],delta))return fail("Actor animation rejected");
    if(was_active&&!dialogue_.idle_process(delta,*this))return fail(dialogue_.error());
    if(!flush_deferred())return false;
    for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!actor_scene_timers(actors_[i],delta))return fail("Actor timer rejected");
    if(stage_==OpeningStage::ScriptRunning&&!persistent_player_.state_->camera_.idle_frame(delta))return fail("Camera idle rejected");
    for(auto& b:boundaries_){b.remaining-=delta;if(b.remaining<0&&stage_!=OpeningStage::BattleRequested)return fail("Deferred binding reached unsupported execution boundary");}
    if(stage_==OpeningStage::Walking&&!persistent_player_.state_->camera_.idle_frame(delta))return fail("Walking camera idle rejected");
    if(overlay_clip_!=kRoomNoIndex){const auto clip=content_.clip(overlay_clip_);overlay_time_+=delta;
        auto first=content_.key(clip.first_key);float alpha=float(first.frame)/65534.f;
        for(uint32_t i=1;i<clip.key_count;++i){const auto next=content_.key(clip.first_key+i);
            if(overlay_time_<=next.time){const float t=std::clamp(float((overlay_time_-first.time)/(next.time-first.time)),0.f,1.f);alpha=(float(first.frame)+(float(next.frame)-first.frame)*t)/65534.f;break;}
            first=next;alpha=float(first.frame)/65534.f;}
        overlay_.alpha=alpha;if(overlay_time_>=clip.length)overlay_clip_=kRoomNoIndex;
    }
    return process_room_shakers(delta);
}
bool OpeningWorld::vibrate_room(uint32_t binding_index){
    if(stage_==OpeningStage::BattleRequested)return true;
    if(!random_)return fail("Room shake requires the shared random stream");
    const auto binding=content_.binding(binding_index);
    if(!persistent_player_.state_->camera_.shake(binding.value,content_.rule_f64(RoomRuleKey::RoomShakeLengthSeconds),{float(content_.rule_f64(RoomRuleKey::RoomShakeDirectionX)),float(content_.rule_f64(RoomRuleKey::RoomShakeDirectionY))}))return fail("Room camera shake rejected");
    audio_.push_back({AudioRequestKind::PlayEffect,binding.target_index,0,dialogue_.phrase()});return true;
}
bool OpeningWorld::process_room_shakers(double delta){
    delta=double(float(delta));
    for(auto& shaker:room_shakers_){
        shaker.remaining=shaker.delayed?double(float(shaker.remaining-delta)):shaker.remaining-delta;
        if(shaker.remaining>=0)continue;
        const bool was_delayed=shaker.delayed;
        if(was_delayed){shaker.delayed=false;shaker.wait_time=content_.rule_f64(RoomRuleKey::RoomShakeWaitSeconds);}
        // Timer rearms in double BEFORE its timeout signal. Changing wait_time
        // in the callback affects the following rearm, not this countdown.
        else shaker.remaining+=shaker.wait_time;
        if(!vibrate_room(shaker.binding))return false;
        if(stage_!=OpeningStage::BattleRequested){const auto wait=content_.rule_f64(RoomRuleKey::RoomShakeWaitSeconds),margin=content_.rule_f64(RoomRuleKey::RoomShakeWaitMarginSeconds);shaker.wait_time=double(float(random_->rand_range(wait-margin,wait+margin)));}
        if(was_delayed)shaker.remaining=shaker.wait_time;
    }
    return true;
}
bool OpeningWorld::flush_deferred() {
    for(size_t n=0;n<actors_.size()&&pending_actor_!=kRoomNoActor;++n){
        const auto who=pending_actor_;pending_actor_=kRoomNoActor;actor_bound_[who]=1;
        if(!dialogue_.actor_ready(who,generation_,*this))return fail(dialogue_.error());
    }
    if(pending_actor_!=kRoomNoActor)return fail("Deferred actor binding budget exceeded");
    for(const auto& action:deferred_){
        const auto binding=content_.binding(action.target_index);
        if(binding.kind==uint16_t(RoomBindingKind::PlayMusicRequest)){area_music_resource_=binding.target_index;audio_.push_back({AudioRequestKind::PlayMusic,binding.target_index,binding.duration,action.phrase});}
        else if(binding.kind==uint16_t(RoomBindingKind::PeriodicCameraShake))room_shakers_.push_back({action.target_index,binding.duration,0,true});
        else if(binding.kind==uint16_t(RoomBindingKind::StopRoomShaker))room_shakers_.erase(std::remove_if(room_shakers_.begin(),room_shakers_.end(),[&](const RoomShaker&s){return s.binding==binding.target_index;}),room_shakers_.end());
        else if(binding.kind==uint16_t(RoomBindingKind::StopMusicResource)){if(area_music_resource_==binding.target_index&&!stop_area_music())return false;}
        else if(binding.kind==uint16_t(RoomBindingKind::WorldEffectAppear)||binding.kind==uint16_t(RoomBindingKind::WorldEffectDisappear)){
            uint64_t mask=0;for(uint32_t i=0;i<actor_bound_.size();++i)if(actor_bound_[i])mask|=uint64_t(1)<<i;
            effects_.push_back({binding.kind==uint16_t(RoomBindingKind::WorldEffectAppear),binding.target_index,persistent_player_.state_->camera_.center(),mask,story_talker_.kind==DialogueTalkerKind::OriginalNpc?story_talker_.index:kRoomNoIndex});
        }
        else if(binding.kind==uint16_t(RoomBindingKind::StartPhoneRing)){if(scene_calls_.size()>=64)return fail("Scene call budget exceeded");scene_calls_.push_back({binding.target_index});}
        else if(binding.kind==uint16_t(RoomBindingKind::DelayedUnsupportedBoundary))boundaries_.push_back({action.target_index,binding.duration});
        else if(binding.kind==uint16_t(RoomBindingKind::FieldPropGrow)){std::string why;if(!story_props_||!story_props_->grow(binding.target_index,why))return fail("Deferred field prop grow rejected");}
        else return fail("Unsupported object binding kind");
    }
    deferred_.clear();return true;
}
bool OpeningWorld::apply(const DialogueAction& a) {
    if(trace_.size()>=65535)return false;
    trace_.push_back({a,physics_tick_,idle_frame_});
    auto* actor=a.actor<actors_.size()?&actors_[a.actor]:nullptr;
    using K=DialogueActionKind;
    switch(a.kind){
    case K::BeginCutscene:
        if(!player_action_pause(false,true))return false;
        stage_=OpeningStage::ScriptRunning;persistent_player_.state_->player_.crouch=persistent_player_.state_->player_.walking=false;if(!persistent_player_.state_->player_.tap_run)persistent_player_.state_->player_.running=false;
        persistent_player_.state_->player_.animation=MotionAnimation::Idle;return true;
    case K::BindActor:{
        if(!actor||actor_erased_[a.actor]||pending_actor_!=kRoomNoActor||actor_binding_revisions_[a.actor]==std::numeric_limits<uint64_t>::max())return false;
        const auto instance=content_.actor_instance(a.actor);const bool controlled=instance.binding_kind==1;
        const auto position=controlled?persistent_player_.state_->player_.position:actor->position,direction=controlled?persistent_player_.state_->player_.direction:actor->direction;
        if(!initialize_actor(*actor,content_,instance.profile_index,position,direction))return false;
        ++actor_binding_revisions_[a.actor];actor_restore_[a.actor]=0;
        if(controlled)restore_pending_=false;
        pending_actor_=a.actor;return true;}
    case K::EraseActor:
        if(!actor||a.actor==content_.scene().player_instance_index||!actor_bound_[a.actor]||actor_erased_[a.actor]||actor_binding_revisions_[a.actor]==std::numeric_limits<uint64_t>::max())return false;
        ++actor_binding_revisions_[a.actor];actor_erased_[a.actor]=1;actor_bound_[a.actor]=actor_visible_[a.actor]=actor_persistent_[a.actor]=actor_restore_[a.actor]=0;
        if(talker_==a.actor){talker_=kRoomNoActor;story_talker_={};}return true;
    case K::ActorPersistent:if(!actor)return false;actor_persistent_[a.actor]=1;return true;
    case K::StartWait:return true;
    case K::AwaitChoices:if(choice_group_!=kRoomNoIndex)return false;choice_group_=a.target_index;return true;
    case K::OpenSave:if(save_requested_||storage_requested_)return false;save_requested_=true;return true;
    case K::OpenStorage:if(save_requested_||storage_requested_)return false;storage_requested_=true;return true;
    case K::GrantKeyItem:{std::string why;return story_key_items_&&story_key_items_->validate_key_item(content_.string(a.target_index),uint32_t(a.value),why)&&story_key_items_->grant_key_item(content_.string(a.target_index),uint32_t(a.value),why);}
    case K::QueueFreeStoryObject:{std::string why;const auto path=content_.string(a.target_index);return story_objects_&&story_objects_->validate_story_object(content_.scene().stable_id,path,why)&&story_objects_->can_queue_free(path,why)&&story_objects_->queue_free(path,why);}
    case K::SetFlag:return write_story_flag(a.target_index,a.value!=0);
    case K::ShowDialogue:
        if(pending_dialogue_id_!=kRoomNoIndex||(!(a.flags&1)&&!actor))return false;
        pending_dialogue_id_=a.target_index;
        if(!(a.flags&1)){talker_=a.actor;story_talker_={DialogueTalkerKind::Actor,a.actor};}
        return true;
    case K::ReturnCamera:return persistent_player_.state_->camera_.return_offset({},a.duration);
    case K::MusicFadeOut:audio_.push_back({AudioRequestKind::FadeMusic,kRoomNoIndex,a.duration,a.phrase});return true;
    case K::SetTalker:talker_=a.actor;story_talker_=actor?DialogueTalker{DialogueTalkerKind::Actor,a.actor}:DialogueTalker{};return true;
    case K::StopInteraction:return (a.flags&1)?(story_talker_.kind==DialogueTalkerKind::OriginalNpc||story_talker_.kind==DialogueTalkerKind::None):(actor&&talker_==a.actor);
    case K::CallObjectDeferred:if(deferred_.size()>=64)return false;deferred_.push_back(a);return true;
    case K::OverworldBattleMusic:overworld_music_=a.value!=0;return true;
    case K::PlayMusicImmediate:audio_.push_back({AudioRequestKind::PlayDialogueMusic,a.target_index,0,a.phrase});return true;
    case K::PlaySound:audio_.push_back({AudioRequestKind::PlayEffect,a.target_index,a.duration,a.phrase});return true;
    case K::MoveActor:return actor&&actor_move_position(*actor,a.vector,float(a.value));
    case K::SetActorDirection:return actor&&actor_turn(*actor,a.vector);
    case K::TeleportActor:return actor&&actor_teleport(*actor,a.vector);
    case K::MoveActorPath:return actor&&actor_move_path(*actor,a.target_index);
    case K::StopActorLoop:return actor&&actor_stop_loop(*actor);
    case K::TurnActor:{
        if(!actor)return false;Vec2 direction=a.vector;
        if(a.flags&2){
            if(a.target_index>=actors_.size()||!actor_bound_[a.target_index])return false;
            const Vec2 delta{actors_[a.target_index].position.x-actor->position.x,actors_[a.target_index].position.y-actor->position.y};
            const float length=std::sqrt(delta.x*delta.x+delta.y*delta.y);
            direction=length>0?Vec2{delta.x/length,delta.y/length}:Vec2{};
            if(a.flags&4)direction.x=a.vector.x;if(a.flags&8)direction.y=a.vector.y;
        }
        return actor_turn_to(*actor,direction,a.duration,(a.flags&1)!=0);}

    case K::ShakeActor:return actor&&actor_shake(*actor,a.vector,a.duration);
    case K::JumpActor:return actor&&actor_jump(*actor,float(a.value),a.duration,a.flags+1);
    case K::SetActorProfile:{
        if(!actor||a.target_index>=content_.actor_profile_count())return false;
        ActorActionState visual;if(!initialize_actor(visual,content_,a.target_index,actor->position,actor->direction))return false;
        actor->profile_index=visual.profile_index;actor->sprite_position=visual.sprite_position;actor->sprite_offset=visual.sprite_offset;
        actor->emote_position=visual.emote_position;actor->frame=visual.frame;actor->clip_index=visual.clip_index;
        actor->emote_clip_index=visual.emote_clip_index;actor->emote_frame=visual.emote_frame;actor->shadow_visible=visual.shadow_visible;
        actor->animation_elapsed=0;actor->directional_animation=visual.directional_animation;return true;}
    case K::LearnSkill:{std::string error;return story_skills_&&story_skills_->learn_skill(content_.string(a.target_index),error);}
    case K::ScreenOverlay:overlay_clip_=a.target_index;overlay_time_=0;overlay_.rgb=uint32_t(a.value);overlay_.alpha=float(content_.key(content_.clip(a.target_index).first_key).frame)/65534.f;return true;
    case K::AnimateActor:return actor&&(!(a.flags&1)||actor->profile_index==a.auxiliary_index)&&actor_play_clip(*actor,a.target_index);
    case K::EmoteActor:return actor&&actor_play_emote(*actor,a.target_index);
    case K::ShakeCamera:return persistent_player_.state_->camera_.shake(a.value,a.duration,a.vector);
    case K::ChangeCamera:if(a.flags&1){follow_actor_=kRoomNoIndex;return persistent_player_.state_->camera_.change_to_dialogue();}if(!actor)return false;follow_actor_=a.actor;return persistent_player_.state_->camera_.change_to_actor(a.actor,actor->position);
    case K::MoveCamera:{
        Vec2 target=a.vector;const auto previous=persistent_player_.state_->camera_.global_position();
        if(a.flags){if(!(a.flags&1))target.x=previous.x;if(!(a.flags&2))target.y=previous.y;}
        return a.target_index==1&&persistent_player_.state_->camera_.move_to(target,a.duration);}
    case K::HideDialogue:
        if(story_hides_.size()>=64)return false;
        story_hides_.push_back((a.flags&1)!=0);return true;
    case K::QueueBattle:{
        const auto record=content_.battle(a.target_index);
        battle_.record_index=a.target_index;battle_.actor_index=record.actor_instance_index;battle_.win_flag_index=record.win_flag_index;
        battle_.enemy=content_.string_data(record.enemy_string);battle_.win_flag=record.win_flag_index==kRoomNoIndex?"":content_.string_data(content_.flag(record.win_flag_index).name_string);
        battle_.battle_resource_index=record.battle_resource_index;battle_.win_cutscene_string=record.win_cutscene_string;battle_.keep_actor_after_battle=(record.flags&4)!=0;
        battle_.advantage=record.advantage;battle_.can_run=(record.flags&1)!=0;battle_.overworld_music=(record.flags&8)?overworld_music_:(record.flags&2)!=0;
        if(battle_.overworld_music!=overworld_music_)return false;
        battle_.queued=true;return true;}
    case K::RestoreActor:
        if(!actor)return false;
        // Original update_npcs rounds its shared direction before copying it
        // to either the player or an NPC after a fractional movement vector.
        actor->direction={std::round(actor->direction.x),std::round(actor->direction.y)};
        if(a.actor!=content_.scene().player_instance_index){actor_restore_[a.actor]=1;return true;}
        persistent_player_.state_->player_.position=actor->position;persistent_player_.state_->player_.direction=actor->direction;if(!player_action_direction())return false;restore_pending_=true;return persistent_player_.state_->camera_.restore_player(persistent_player_.state_->player_.position);
    case K::ReleaseBattleActor:if(!actor||!battle_.queued||a.actor!=battle_.actor_index)return false;actor_persistent_[a.actor]=0;return true;
    case K::CutsceneEnded:cutscene_done_=true;return true;
    case K::DialogueDone:
        if(!cutscene_done_)return false;
        if(!battle_.queued){
            if(a.duration<=0||!persistent_player_.state_->camera_.return_offset({},a.duration))return false;
            if(!player_action_resume())return false;
            stage_=OpeningStage::Walking;persistent_player_.state_->house_paused_=false;
            if(!player_action().ready()){persistent_player_.state_->player_.walking=persistent_player_.state_->player_.running=persistent_player_.state_->player_.crouch=false;persistent_player_.state_->player_.velocity={};}
            persistent_player_.state_->player_.animation=MotionAnimation::Idle;
            persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
            if(!content_.frame_clip(persistent_player_.state_->clip_,clip)||!begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_))return false;
        }
        return true;
    case K::RequestBattle:
        if(!battle_.queued||!cutscene_done_||a.target_index!=battle_.record_index)return false;
        battle_.requested=true;stage_=OpeningStage::BattleRequested;persistent_player_.state_->camera_.pause();error_="Battle requested; battle/audio backend pending";return true;
    default:return false;
    }
}
}
