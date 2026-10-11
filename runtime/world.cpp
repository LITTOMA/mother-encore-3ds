#include "encore/world.hpp"
#include "encore/opening_trigger.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
unsigned direction_index(Vec2 d){if(d.y>0)return d.x<0?4:(d.x>0?5:0);if(d.y<0)return d.x<0?6:(d.x>0?7:3);return d.x<0?1:2;}
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
    content_=content;choice_group_=kRoomNoIndex;save_requested_=storage_requested_=false;persistent_player_.state_->party_leader_.clear();story_talker_={};effects_.clear();story_hides_.clear();scene_calls_.clear();flagged_bodies_.clear();area_music_resource_=kRoomNoIndex;pending_dialogue_id_=kRoomNoIndex;viewport_=viewport;battle_accepted_=false;persistent_player_.state_->return_player_visible_=false;persistent_player_.state_->house_paused_=false;last_idle_delta_=0;
    const auto scene=content.scene();
    if(!flags_.initialize(content))return false;
    if(story_flags){for(uint32_t i=0;i<story_flags->size();++i)if(!flags_.set_story_flag(i,(*story_flags)[i],false).applied)return false;}
    else for(uint32_t i=0;i<scene.initial_flag_count;++i){const auto flag=content.initial_flag(scene.initial_flag_first+i);if(!flags_.set_story_flag(flag.flag_index,flag.value).applied)return false;}
    dialogue_=DialoguePlayer{};battle_=OpeningBattleRequest{};audio_.clear();trace_.clear();deferred_.clear();boundaries_.clear();room_shakers_.clear();random_=nullptr;
    actors_.assign(content.actor_instance_count(),ActorActionState{});
    actor_visible_.assign(actors_.size(),0);actor_restore_.assign(actors_.size(),0);
    actor_bound_.assign(actors_.size(),0);actor_persistent_.assign(actors_.size(),0);
    inside_trigger_.assign(content.trigger_count(),0);trigger_pending_.assign(content.trigger_count(),0);
    stage_=OpeningStage::Walking;cutscene_done_=restore_pending_=overworld_music_=false;
    pending_actor_=talker_=kRoomNoActor;program_index_=follow_actor_=kRoomNoIndex;physics_tick_=idle_frame_=0;
    if(++generation_==0)++generation_;
    std::vector<uint32_t> active;active.reserve(content.polygon_count());
    for(uint32_t i=0;i<content.polygon_count();++i){bool enabled=false;if(!body_active(content.polygon(i).body_id,flags_,enabled,reviewed_mutations))return fail("Unmapped body activation rule");if(enabled)active.push_back(i);}
    active_polygons_=active;polygon_offsets_.assign(content.polygon_count(),{});erased_bodies_.clear();
    if(!solver_.configure_room(content,active))return fail("Unsupported collision geometry");
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
bool OpeningWorld::advance(WalkInput input) {
    if(!healthy_)return false;
    ++physics_tick_;
    // Input edges expire even while story/battle owns the player. A button
    // held through unpause must not become a new movement toggle press.
    if(stage_!=OpeningStage::Walking)persistent_player_.state_->player_.previous_toggle=input.toggle;
    if(stage_==OpeningStage::BattleRequested){if(!battle_accepted_)return true;if(persistent_player_.state_->return_player_visible_&&!actor_physics_step(actors_[content_.scene().player_instance_index]))return fail("Return actor physics rejected");for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!persistent_player_.state_->camera_.update_actor_position(i,actors_[i].position))return fail("Actor camera position rejected");return persistent_player_.state_->camera_.physics_frame(1.0/60.0);}
    if(stage_==OpeningStage::ScriptRunning){
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
    auto player=persistent_player_.state_->player_;if(!advance_walk(player,input,solver_,content_))return fail("Movement rejected");
    const uint32_t selected=walk_clip(player.animation,player.direction);FrameClip clip;
    if(!content_.frame_clip(selected,clip))return fail("Missing motion animation binding");
    auto playback=persistent_player_.state_->playback_;
    if(selected!=persistent_player_.state_->clip_&&!begin_clip(clip,content_.clip(selected).frame_count,playback))return fail("Motion animation rejected");
    if(!input.paused&&!input.entering_door&&!advance_clip(clip,1.0f/60.0f,content_.clip(selected).frame_count,playback))return fail("Motion animation update rejected");
    persistent_player_.state_->player_=player;persistent_player_.state_->playback_=playback;persistent_player_.state_->clip_=selected;persistent_player_.state_->camera_.update_player_position(persistent_player_.state_->player_.position);return persistent_player_.state_->camera_.physics_frame(1.0/60.0);
}
bool OpeningWorld::begin_house_program(uint32_t program_index,uint32_t original_npc){
    if(!healthy_||stage_!=OpeningStage::Walking||program_index>=content_.program_count()||dialogue_.active())return false;
    dialogue_=DialoguePlayer{};battle_=OpeningBattleRequest{};battle_accepted_=false;persistent_player_.state_->return_player_visible_=false;
    cutscene_done_=restore_pending_=false;choice_group_=kRoomNoIndex;save_requested_=storage_requested_=false;story_hides_.clear();pending_dialogue_id_=kRoomNoIndex;pending_actor_=talker_=kRoomNoActor;
    if(++generation_==0)++generation_;
    story_talker_=original_npc==kRoomNoIndex?DialogueTalker{}:DialogueTalker{DialogueTalkerKind::OriginalNpc,original_npc};
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
    for(auto&bound:flagged_bodies_)if(bound.state.in_tree&&bound.state.queued_for_deletion){
        const auto body=content_.binding(bound.binding).target_index;
        if(!set_body_enabled(body,false))return fail("Deferred flag-body deletion rejected");
        if(std::find(erased_bodies_.begin(),erased_bodies_.end(),body)==erased_bodies_.end())erased_bodies_.push_back(body);
        flush_flag_landmark_deletion(bound.state);
    }return true;
}
bool OpeningWorld::write_story_flag(uint32_t index,bool value,bool emit){
    if(!healthy_)return false;const auto result=flags_.set_story_flag(index,value,emit);
    return result.applied&&(!result.flags_updated||refresh_scene_rules());
}
bool OpeningWorld::request_sound(uint32_t resource_index){
    if(!healthy_||resource_index>=content_.resource_count()||content_.resource(resource_index).kind!=2)return false;
    audio_.push_back({AudioRequestKind::PlayEffect,resource_index,0,phrase()});return true;
}
bool OpeningWorld::set_story_talking(bool talking){
    if(!healthy_)return false;
    return story_talker_.kind!=DialogueTalkerKind::Actor||(story_talker_.index<actors_.size()&&actor_set_talking(actors_[story_talker_.index],talking));
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
bool OpeningWorld::complete_actor_restore(uint32_t index){if(!healthy_||index>=actors_.size()||!actor_restore_[index])return false;actor_restore_[index]=0;actor_bound_[index]=0;actor_persistent_[index]=0;persistent_player_.state_->camera_.remove_actor(index);return true;}
bool OpeningWorld::accept_battle_entry(){if(!healthy_||!battle_.requested||battle_accepted_)return false;battle_accepted_=true;battle_original_direction_=persistent_player_.state_->player_.direction;return true;}
bool OpeningWorld::pause_for_house(){
    if(!healthy_||stage_!=OpeningStage::Walking)return false;
    persistent_player_.state_->house_paused_=true;persistent_player_.state_->player_.crouch=persistent_player_.state_->player_.walking=false;persistent_player_.state_->player_.velocity={};
    if(!persistent_player_.state_->player_.tap_run)persistent_player_.state_->player_.running=false;
    persistent_player_.state_->player_.animation=MotionAnimation::Idle;persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
    return content_.frame_clip(persistent_player_.state_->clip_,clip)&&begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_);
}
bool OpeningWorld::unpause_from_house(){if(!healthy_||stage_!=OpeningStage::Walking)return false;persistent_player_.state_->house_paused_=false;return true;}
bool OpeningWorld::set_house_direction(Vec2 direction){
    if(!healthy_||stage_!=OpeningStage::Walking)return false;
    if(direction.x==0&&direction.y==0)return true;
    const float x=std::abs(direction.x),y=std::abs(direction.y);const bool grid=(x==0||x==1)&&(y==0||y==1);const bool diagonal=x==y&&std::abs(x-std::sqrt(.5f))<1e-6f;
    if(!std::isfinite(direction.x)||!std::isfinite(direction.y)||(!grid&&!diagonal))return false;
    persistent_player_.state_->player_.direction=direction;persistent_player_.state_->player_.animation=MotionAnimation::Idle;persistent_player_.state_->clip_=walk_clip(persistent_player_.state_->player_.animation,persistent_player_.state_->player_.direction);FrameClip clip;
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
    if(changed){
        persistent_player_.state_->clip_=walk_clip(anim,direction);FrameClip clip;
        if(!content_.frame_clip(persistent_player_.state_->clip_,clip)||!begin_clip(clip,content_.clip(persistent_player_.state_->clip_).frame_count,persistent_player_.state_->playback_))return false;
    }
    if(walking){FrameClip clip;const auto selected=persistent_player_.state_->clip_;
        if(!content_.frame_clip(selected,clip)||!advance_clip(clip,1.f/60.f,content_.clip(selected).frame_count,persistent_player_.state_->playback_))return false;}
    return persistent_player_.state_->camera_.relocate_player(position);
}
bool OpeningWorld::set_initial_actor_pose(uint32_t index,Vec2 position,Vec2 direction){
    if(!healthy_||stage_!=OpeningStage::Walking||physics_tick_||idle_frame_||index>=actors_.size()||index==content_.scene().player_instance_index||actor_bound_[index]||!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(direction.x)||!std::isfinite(direction.y)||(!direction.x&&!direction.y))return false;
    auto&actor=actors_[index];actor.position=position;actor.direction=direction;return true;
}
bool OpeningWorld::body_enabled(uint32_t body_id)const{
    for(auto i:active_polygons_)if(content_.polygon(i).body_id==body_id)return true;
    return false;
}
bool OpeningWorld::set_body_enabled(uint32_t body_id,bool enabled){
    if(!healthy_)return false;
    bool known=false;for(uint32_t i=0;i<content_.polygon_count();++i)if(content_.polygon(i).body_id==body_id){known=true;break;}
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
bool OpeningWorld::set_story_flag(std::string_view name,bool value,bool emit){if(!healthy_)return false;const auto result=flags_.set_story_flag(name,value,emit);return result.applied&&(!result.flags_updated||refresh_scene_rules());}
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
            if(trigger_conditions(i)){program_index_=content_.trigger(i).program_index;if(!dialogue_.start(content_,program_index_,*this,generation_))return fail(dialogue_.error());break;}
        }
    }
    const uint32_t controlled=content_.scene().player_instance_index;
    for(uint32_t i=0;i<actors_.size();++i)if(i!=controlled||actor_bound_[i])if(!actor_idle_animations(actors_[i],delta))return fail("Actor animation rejected");
    if(was_active&&!dialogue_.idle_process(delta,*this))return fail(dialogue_.error());
    if(!flush_deferred())return false;
    for(uint32_t i=0;i<actors_.size();++i)if(actor_bound_[i]&&!actor_scene_timers(actors_[i],delta))return fail("Actor timer rejected");
    if(stage_==OpeningStage::ScriptRunning&&!persistent_player_.state_->camera_.idle_frame(delta))return fail("Camera idle rejected");
    for(auto& b:boundaries_){b.remaining-=delta;if(b.remaining<0&&stage_!=OpeningStage::BattleRequested)return fail("Deferred binding reached unsupported execution boundary");}
    if(stage_==OpeningStage::Walking&&!persistent_player_.state_->camera_.idle_frame(delta))return fail("Walking camera idle rejected");
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
        stage_=OpeningStage::ScriptRunning;persistent_player_.state_->player_.crouch=persistent_player_.state_->player_.walking=false;if(!persistent_player_.state_->player_.tap_run)persistent_player_.state_->player_.running=false;
        persistent_player_.state_->player_.animation=MotionAnimation::Idle;return true;
    case K::BindActor:{
        if(!actor||pending_actor_!=kRoomNoActor)return false;
        const auto instance=content_.actor_instance(a.actor);const bool controlled=instance.binding_kind==1;
        const auto position=controlled?persistent_player_.state_->player_.position:actor->position,direction=controlled?persistent_player_.state_->player_.direction:actor->direction;
        if(!initialize_actor(*actor,content_,instance.profile_index,position,direction))return false;
        pending_actor_=a.actor;return true;}
    case K::ActorPersistent:if(!actor)return false;actor_persistent_[a.actor]=1;return true;
    case K::StartWait:return true;
    case K::AwaitChoices:if(choice_group_!=kRoomNoIndex)return false;choice_group_=a.target_index;return true;
    case K::OpenSave:if(save_requested_||storage_requested_)return false;save_requested_=true;return true;
    case K::OpenStorage:if(save_requested_||storage_requested_)return false;storage_requested_=true;return true;
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
    case K::AnimateActor:return actor&&actor_play_clip(*actor,a.target_index);
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
        persistent_player_.state_->player_.position=actor->position;persistent_player_.state_->player_.direction=actor->direction;restore_pending_=true;return persistent_player_.state_->camera_.restore_player(persistent_player_.state_->player_.position);
    case K::ReleaseBattleActor:if(!actor||!battle_.queued||a.actor!=battle_.actor_index)return false;actor_persistent_[a.actor]=0;return true;
    case K::CutsceneEnded:cutscene_done_=true;return true;
    case K::DialogueDone:
        if(!cutscene_done_)return false;
        if(!battle_.queued){
            if(a.duration<=0||!persistent_player_.state_->camera_.return_offset({},a.duration))return false;
            stage_=OpeningStage::Walking;persistent_player_.state_->house_paused_=false;persistent_player_.state_->player_.walking=persistent_player_.state_->player_.running=persistent_player_.state_->player_.crouch=false;persistent_player_.state_->player_.velocity={};persistent_player_.state_->player_.animation=MotionAnimation::Idle;
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
