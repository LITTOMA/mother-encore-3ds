#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
namespace encore::upstream {
namespace {
float sign(float v){return v<0?-1.f:v>0?1.f:0.f;}
bool ray_rect(Vec2 origin,Vec2 direction,float length,Vec2 center,Vec2 extents,float& distance){
 float low=0,high=length;
 for(unsigned axis=0;axis<2;++axis){const float o=axis?origin.y:origin.x,d=axis?direction.y:direction.x,c=axis?center.y:center.x,e=axis?extents.y:extents.x;
  if(d==0){if(o<c-e||o>c+e)return false;continue;}
  float a=(c-e-o)/d,b=(c+e-o)/d;if(a>b)std::swap(a,b);low=std::max(low,a);high=std::min(high,b);if(low>high)return false;
 }distance=low;return true;
}
}
bool HouseRuntime::fail(const char* text){error_=text;phase_=HousePhase::Error;return false;}
void HouseRuntime::event(HouseEventKind kind,uint32_t object){events_.push_back({kind,object,physics_tick_,idle_frame_,world_->player().position});}
bool HouseRuntime::initialize(HouseView content,OpeningWorld& world,HousePresentation& presentation){
 *this=HouseRuntime{};if(!content.valid()||!world.healthy())return fail("Invalid house runtime inputs");
 content_=content;world_=&world;presentation_=&presentation;
 for(uint32_t i=0;i<content.count(HouseSection::Npcs);++i){auto n=content.npc(i);if(n.program_index!=house_no_index&&n.program_index>=world.content().program_count())return fail("NPC program reference exceeds room pack");if(n.room_actor_index!=house_no_index&&n.room_actor_index>=world.actor_count())return fail("NPC actor reference exceeds room pack");}
 for(uint32_t i=0;i<content.count(HouseSection::StoryTriggers);++i){auto t=content.story_trigger(i);if(t.disposition==2&&t.program_index>=world.content().program_count())return fail("House program reference exceeds room pack");}
 for(uint32_t i=0;i<world.content().command_count();++i){auto c=world.content().command(i);if(c.opcode!=uint16_t(DialogueActionKind::ShowDialogue))continue;bool found=false;for(uint32_t j=0;j<content.count(HouseSection::Dialogues);++j)found|=content.dialogue(j).id==c.target_index;if(!found)return fail("Dialogue reference absent from house pack");}

 door_inside_.assign(content.count(HouseSection::Doors),0);boundary_inside_.assign(content.count(HouseSection::Boundaries),0);
 story_inside_.assign(content.count(HouseSection::StoryTriggers),0);story_process_.assign(story_inside_.size(),0);story_ready_idle_.assign(story_inside_.size(),0);observed_physics_position_=world.player().position;
 openables_.assign(content.count(HouseSection::OpenableDoors),HouseOpenableState{});npc_restore_.resize(content.count(HouseSection::Npcs));
 for(uint32_t i=0;i<openables_.size();++i){const auto data=content.openable_door(i);auto& state=openables_[i];
  state.blocked=(data.policy&uint32_t(HouseDoorPolicy::Blocked))!=0;state.locked=(data.policy&uint32_t(HouseDoorPolicy::Locked))!=0;state.one_way=(data.policy&uint32_t(HouseDoorPolicy::OneWay))!=0;
  const OpenableDoorFlagRule rule{content.string(data.key),content.string(data.flag),state.blocked,state.locked,state.one_way};const auto flags=world.door_flag_state(rule);
  state.unlocked=flags.unlocked;state.player_disabled=flags.deferred_player_shape_disabled;
  if(!world.set_body_enabled(data.player_body_id,!state.player_disabled))return fail("Openable player-body binding rejected");
  state.sprite_visible=(data.normal_values&uint32_t(HouseDoorStateField::SpriteVisible))!=0;state.nonplayer_disabled=(data.normal_values&uint32_t(HouseDoorStateField::NonPlayerDisabled))!=0;
 }
 last_safe_position_=world.player().position;last_safe_direction_=world.player().direction;
 for(uint32_t n=0;n<content.count(HouseSection::Npcs);++n){bool found=false;for(uint32_t b=0;b<world.content().body_rule_count();++b)if(world.content().body_rule(b).body_id==content.npc(n).body_id)found=true;if(!found)return fail("NPC collision binding is absent");}
 for(uint32_t i=0;i<content.count(HouseSection::Npcs);++i){const auto n=content.npc(i);const auto p=presentation.npc_pose(i).position;if(!world.set_body_offset(n.body_id,{p.x-n.position.x,p.y-n.position.y}))return fail("NPC initial collision offset rejected");}
 presentation.set_text_completion_callback([](void* state){return static_cast<HouseRuntime*>(state)->text_finished();},this);
 return sync_npc_visibility();
}
bool HouseRuntime::rebind_scene(OpeningWorld&world,HousePresentation&presentation){
 if(!content_.valid()||!world.healthy())return false;
 world_=&world;presentation_=&presentation;
 presentation.set_text_completion_callback([](void*state){return static_cast<HouseRuntime*>(state)->text_finished();},this);return true;
}
bool HouseRuntime::restore_seen_dialogue(const std::set<uint32_t>&keys){
 if(!content_.valid()||physics_tick_||idle_frame_||phase_!=HousePhase::Idle)return false;
 for(auto key:keys){bool known=false;for(uint32_t i=0;i<content_.count(HouseSection::Npcs);++i)known|=content_.npc(i).seen_key==key;for(uint32_t i=0;i<content_.count(HouseSection::Overrides);++i)known|=content_.override_dialogue(i).seen_key==key;if(!key||!known)return false;}
 seen_=keys;return true;
}
bool HouseRuntime::set_player_nickname(std::string_view name){
 if(!content_.valid()||name.empty()||name.size()>content_.interaction().max_player_name_length)return false;
 for(unsigned char ch:name)if(ch<32||ch>126)return false;nickname_=std::string(name);return true;
}
std::string_view HouseRuntime::player_nickname()const{
 if(!nickname_.empty())return nickname_;const auto room=world_->content();return room.string(room.actor_instance(room.scene().player_instance_index).display_name_string);
}
bool HouseRuntime::blocks_player()const{return story_pending()||phase_==HousePhase::DoorAwaitIdle||phase_==HousePhase::DoorFadeIn||phase_==HousePhase::WarpAwaitIdle||(phase_==HousePhase::DoorFadeOut&&!door_unpaused_)||phase_==HousePhase::Dialogue||phase_==HousePhase::InspectionProgram||phase_==HousePhase::Unsupported||phase_==HousePhase::Error||phase_==HousePhase::SceneDoorPending;}
bool HouseRuntime::entering_door()const{return phase_==HousePhase::DoorFadeIn||phase_==HousePhase::WarpAwaitIdle||(phase_==HousePhase::DoorFadeOut&&!door_unpaused_)||phase_==HousePhase::SceneDoorPending;}
bool HouseRuntime::overlaps(Vec2 center,Vec2 extents,Vec2 player)const{
 const auto&room=world_->content();const auto scene=room.scene();
 const Vec2 rect[4]={{center.x-extents.x,center.y-extents.y},{center.x+extents.x,center.y-extents.y},{center.x+extents.x,center.y+extents.y},{center.x-extents.x,center.y+extents.y}};
 auto separated=[&](Vec2 axis){float amin=INFINITY,amax=-INFINITY,bmin=INFINITY,bmax=-INFINITY;
  for(uint32_t i=0;i<scene.actor_hull_count;++i){auto p=room.vertex(scene.actor_hull_first+i);const float dot=(p.x+player.x)*axis.x+(p.y+player.y)*axis.y;amin=std::min(amin,dot);amax=std::max(amax,dot);}
  for(auto p:rect){const float dot=p.x*axis.x+p.y*axis.y;bmin=std::min(bmin,dot);bmax=std::max(bmax,dot);}return amax<bmin||bmax<amin;
 };
 if(separated({1,0})||separated({0,1}))return false;
 for(uint32_t i=0;i<scene.actor_hull_count;++i){auto a=room.vertex(scene.actor_hull_first+i),b=room.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);if(separated({a.y-b.y,b.x-a.x}))return false;}return true;
}
bool HouseRuntime::overlaps_circle(Vec2 center,float radius,Vec2 player)const{
 const auto& room=world_->content();const auto scene=room.scene();bool positive=false,negative=false;
 for(uint32_t i=0;i<scene.actor_hull_count;++i){auto a=room.vertex(scene.actor_hull_first+i),b=room.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);a.x+=player.x;a.y+=player.y;b.x+=player.x;b.y+=player.y;
  const Vec2 edge{b.x-a.x,b.y-a.y},to{center.x-a.x,center.y-a.y};const float cross=edge.x*to.y-edge.y*to.x;positive|=cross>0;negative|=cross<0;
  const float denom=edge.x*edge.x+edge.y*edge.y;const float t=std::clamp((to.x*edge.x+to.y*edge.y)/denom,0.f,1.f);const float x=to.x-edge.x*t,y=to.y-edge.y*t;if(x*x+y*y<=radius*radius)return true;
 }return !(positive&&negative);
}
Vec2 HouseRuntime::npc_point(uint32_t index,Vec2 original)const{
 const auto initial=content_.npc(index).position,position=presentation_->npc_pose(index).position;
 return {original.x+position.x-initial.x,original.y+position.y-initial.y};
}
bool HouseRuntime::sync_npc_visibility(){
 for(uint32_t i=0;i<npc_restore_.size();++i){const auto n=content_.npc(i);
  const bool replaced=n.room_actor_index!=house_no_index&&world_->actor_bound(n.room_actor_index),visible=world_->body_visible(n.body_id);
  // npc.visibility_changed disables the original body/interact and its
  // physics callback while an Actor proxy owns the visible representation.
  if(!presentation_->set_npc_replaced(i,replaced)||!presentation_->set_npc_visible(i,visible)||!world_->set_body_enabled(n.body_id,visible&&!replaced))return fail("NPC visibility/collision binding rejected");
 }return true;
}
bool HouseRuntime::process_npc_restores(){
 if(!sync_npc_visibility())return false;
 for(uint32_t i=0;i<npc_restore_.size();++i){const auto n=content_.npc(i);if(n.room_actor_index==house_no_index)continue;auto&w=npc_restore_[i];
  if(!w.phase&&world_->actor_restore_requested(n.room_actor_index)){const auto&a=world_->actor(n.room_actor_index);w.frame_revision=presentation_->npc_frame_revision(i);w.phase=1;
   if(!presentation_->restore_npc_pose(i,a.position,{std::round(a.direction.x),std::round(a.direction.y)})||!world_->set_body_offset(n.body_id,{a.position.x-n.position.x,a.position.y-n.position.y}))return fail("NPC restored pose/collision rejected");}
  else if(w.phase==1&&presentation_->npc_frame_revision(i)!=w.frame_revision){w.phase=2;w.ready_idle=idle_frame_+1;}
  else if(w.phase==2&&idle_frame_>=w.ready_idle){if(!world_->complete_actor_restore(n.room_actor_index))return fail("NPC deferred restoration rejected");w.phase=0;}
 }
 return sync_npc_visibility();
}
bool HouseRuntime::before_physics(WalkInput&input){
 if(!world_||phase_==HousePhase::Error)return false;
 ++physics_tick_;
 if(!sync_npc_visibility())return false;
 const auto position=observed_physics_position_;bool any_overlap=false;
 // Player.pause retains the CutsceneArea collision bit (4096). Door and
 // boundary callbacks are masked, but destination story areas still notify.
 for(uint32_t i=0;i<story_inside_.size();++i){const auto trigger=content_.story_trigger(i);const bool inside=overlaps(trigger.center,trigger.extents,position);if(inside!=bool(story_inside_[i]))pending_contacts_.push_back({0,i,inside});story_inside_[i]=inside;any_overlap|=inside;}
 if(!update_openable_contacts(position))return false;
 if(world_->stage()!=OpeningStage::Walking||world_->house_paused()){
  std::fill(door_inside_.begin(),door_inside_.end(),0);std::fill(boundary_inside_.begin(),boundary_inside_.end(),0);
  input.paused=input.paused||blocks_player();input.entering_door=input.entering_door||entering_door();return true;
 }
 for(uint32_t i=0;i<content_.count(HouseSection::Npcs);++i){const auto npc=content_.npc(i);if(!presentation_->set_npc_looking(i,overlaps_circle(npc_point(i,npc.view_center),npc.view_radius,position)))return fail("NPC view area rejected");}


 for(uint32_t i=0;i<door_inside_.size();++i){const auto door=content_.door(i);const bool inside=overlaps(door.center,door.extents,position);if(inside&&!door_inside_[i])pending_contacts_.push_back({1,i,true});door_inside_[i]=inside;any_overlap|=inside;}
 for(uint32_t i=0;i<boundary_inside_.size();++i){const auto boundary=content_.boundary(i);const bool inside=overlaps(boundary.center,boundary.extents,position);if(inside&&!boundary_inside_[i])pending_contacts_.push_back({2,i,true});boundary_inside_[i]=inside;any_overlap|=inside;}
 if(pending_contacts_.size()>4096)return fail("Too many deferred area contacts");
 for(const auto& door:openables_)any_overlap|=door.inside;
 if(!any_overlap&&!blocks_player()){last_safe_position_=position;last_safe_direction_=world_->player().direction;}
 input.paused=input.paused||blocks_player();input.entering_door=input.entering_door||entering_door();return true;
}
bool HouseRuntime::deliver_area_contacts(){
 auto pending=std::move(pending_contacts_);pending_contacts_.clear();
 // Contact notifications follow this frame's physics and idle signal/process
 // callbacks. A guard which paused the player in _process removes Door's mask
 // before the next query batch; already delivered simultaneous contacts retain
 // their source race rather than gaining an invented guard priority.
 for(const auto&c:pending){
  if(c.kind==0){if(c.entered||world_->stage()!=OpeningStage::BattleRequested){story_process_[c.index]=c.entered;story_ready_idle_[c.index]=idle_frame_;}}
  else if(world_->stage()!=OpeningStage::Walking||world_->house_paused())continue;
  else if(c.kind==1&&!blocks_player()&&!entering_door()){
   active_=c.index;phase_=HousePhase::DoorAwaitIdle;await_idle_=idle_frame_;door_unpaused_=false;
   if(!world_->pause_for_house())return fail("Door pause rejected");
   event(HouseEventKind::Paused,c.index);
  }else if(c.kind==2&&!blocks_player()&&!entering_door()){
   if(scene_door_data_&&c.index<scene_door_ids_.size()&&scene_door_ids_[c.index]){if(!request_scene_door(c.index))return false;continue;}
   active_=c.index;phase_=HousePhase::Unsupported;error_="Unported source route; B returns to the last safe point";
   if(!world_->pause_for_house())return fail("Boundary pause rejected");
  }
 }
 return true;
}
bool HouseRuntime::after_physics(){
 if(!world_||!world_->healthy())return false;
 observed_physics_position_=world_->player().position;
 // SceneTree's deferred property writes are visible before idle AnimationPlayer
 // writes. Preserve that phase separation instead of unlocking at the callback.
 for(uint32_t i=0;i<openables_.size();++i){auto& state=openables_[i];if(!state.collision_pending)continue;state.collision_pending=false;state.player_disabled=state.collision_value;if(!world_->set_body_enabled(content_.openable_door(i).player_body_id,!state.player_disabled))return fail("Deferred door collider rejected");}
 return true;
}
bool HouseRuntime::open_openable(uint32_t index){
 auto& state=openables_[index];const auto data=content_.openable_door(index);
 state.action=true;state.animation_time=0;state.animation_pending=true;state.pending_action=true;state.pending_normal=false;
 if(!entering_door()&&!content_.string(data.start_sound).empty())sounds_.push_back({state.blocked&&!state.unlocked?data.ram_sound:data.start_sound});
 event(HouseEventKind::OpenableOpened,index);return true;
}
bool HouseRuntime::normal_openable(uint32_t index){
 auto& state=openables_[index];state.action=false;state.animation_time=0;state.animation_pending=true;state.pending_normal=true;state.pending_action=false;
 event(HouseEventKind::OpenableNormal,index);return true;
}
bool HouseRuntime::update_openable_contacts(Vec2 player){
 for(uint32_t i=0;i<openables_.size();++i){auto& state=openables_[i];const auto data=content_.openable_door(i);
  const bool inside=overlaps(data.trigger_center,data.trigger_extents,player),entered=inside&&!state.inside,exited=!inside&&state.inside;state.inside=inside;
  if(entered){
   if(state.locked&&!content_.string(data.flag).empty()&&world_->story_flag(content_.string(data.flag))){state.unlocked=true;state.locked=false;state.collision_pending=true;state.collision_value=true;}
   if(!state.timer_running&&state.unlocked&&!state.action&&!state.one_way)if(!open_openable(i))return false;
   // Source deliberately does not require a valid-body guard in this branch.
   // This slice supplies the original player contact; NPC motion is not added.
   if(state.blocked&&world_->player().running&&world_->player().direction.y==data.ram_required_y&&!state.unlocked){
    if(!world_->shake_house_camera(data.ram_strength,data.ram_duration,data.ram_direction)||!open_openable(i))return fail("Door ram rejected");
    state.unlocked=true;state.collision_pending=true;state.collision_value=true;event(HouseEventKind::OpenableUnlocked,i);state.blocked=false;
    if(!content_.string(data.flag).empty()){if(!world_->set_story_flag(content_.string(data.flag),true,false))return fail("Door ram flag rejected");event(HouseEventKind::OpenableFlagWritten,i);}
   }
  }
  if(exited){if(world_->house_paused()){if(!normal_openable(i))return false;}else if(state.unlocked){state.timer_running=true;state.timer_remaining=double(float(data.close_delay));}}
 }
 return true;
}
bool HouseRuntime::advance_openables(double delta){
 for(uint32_t i=0;i<openables_.size();++i){auto& state=openables_[i];const auto data=content_.openable_door(i);
  if(state.collision_pending){state.collision_pending=false;state.player_disabled=state.collision_value;if(!world_->set_body_enabled(data.player_body_id,!state.player_disabled))return fail("Deferred door collider rejected");}
  if(state.animation_pending){
   const uint32_t mask=state.pending_action?data.action_mask:data.normal_mask,values=state.pending_action?data.action_values:data.normal_values;
   if(mask&uint32_t(HouseDoorStateField::SpriteVisible))state.sprite_visible=(values&uint32_t(HouseDoorStateField::SpriteVisible))!=0;
   if(mask&uint32_t(HouseDoorStateField::PlayerDisabled)){state.player_disabled=(values&uint32_t(HouseDoorStateField::PlayerDisabled))!=0;if(!world_->set_body_enabled(data.player_body_id,!state.player_disabled))return fail("Door animation collider rejected");}
   if(mask&uint32_t(HouseDoorStateField::NonPlayerDisabled))state.nonplayer_disabled=(values&uint32_t(HouseDoorStateField::NonPlayerDisabled))!=0;
   state.animation_pending=state.pending_action=state.pending_normal=false;
  }
  state.animation_time=double(float(state.animation_time+delta));
  if(state.timer_running){state.timer_remaining-=delta;if(state.timer_remaining<0){state.timer_running=false;state.timer_remaining=0;
   if(state.inside)continue;
   if(!normal_openable(i))return false;
   if(!content_.string(data.end_sound).empty()&&!world_->house_paused())sounds_.push_back({data.end_sound});
   if(state.one_way){state.unlocked=false;state.collision_pending=true;state.collision_value=false;}
   // GDScript3 parses the trailing close() after the inline if as unconditional.
   // The native oracle confirms two calls when the preceding close also ran.
   sounds_.push_back({data.end_sound});
  }}
 }
 return true;
}
bool HouseRuntime::interact_openable(uint32_t index){
 auto& state=openables_[index];const auto data=content_.openable_door(index);if(state.unlocked)return true;
 if(basement_&&index==basement_door_){
  const auto&binding=basement_->door();const auto*key=basement_->key_item(binding.key_id);bool owned=false;
  if(!key||!basement_host_.key_owned(*key,owned,basement_error_))return fail(basement_error_.c_str());
  if(owned){
   // Source _use_key: flag write without flags_updated, Action, unlock,
   // optional inventory removal, then global.item and the feedback programme.
   if(!world_->set_story_flag(binding.flag,true,false))return fail("Basement door flag write rejected");
   event(HouseEventKind::OpenableFlagWritten,index);
   if(!open_openable(index))return false;
   state.unlocked=true;state.collision_pending=true;state.collision_value=true;
   event(HouseEventKind::OpenableUnlocked,index);
   if(binding.remove_key&&!basement_host_.remove_key_item(*key,basement_error_))return fail(basement_error_.c_str());
   if(!basement_host_.select_key_item(*key,basement_error_))return fail(basement_error_.c_str());
  }
  return begin_basement_program(owned?binding.opened:binding.locked,index);
 }
 if(!content_.string(data.activates_flag).empty()||!content_.string(data.deactivates_flag).empty())return fail("Door flag notification side effects are not implemented");
 if(!world_->pause_for_house())return fail("Door dialogue pause rejected");
 if(!state.blocked||data.blocked_dialogue==house_no_index){phase_=HousePhase::Unsupported;error_="Unported key/item interaction; B returns control";last_safe_position_=world_->player().position;last_safe_direction_=world_->player().direction;return true;}
 const auto dialogue=content_.dialogue(data.blocked_dialogue);const auto room=world_->content();const auto actor=room.actor_instance(room.scene().player_instance_index);
 if(!presentation_->begin_dialogue(dialogue.first_segment,dialogue.segment_count,room.string(actor.display_name_string)))return fail("Non-NPC dialogue rejected");
 active_=index;phase_=HousePhase::Dialogue;event(HouseEventKind::DoorDialogueOpened,index);return true;
}
bool HouseRuntime::story_conditions(uint32_t index)const{
 const auto trigger=content_.story_trigger(index);
 for(uint32_t i=0;i<trigger.condition_count;++i){const auto condition=content_.story_condition(trigger.first_condition+i);if(world_->story_flag(content_.string(condition.flag))!=(condition.value!=0))return false;}
 return true;
}
bool HouseRuntime::process_story_requests(){
 if(story_pending()||phase_==HousePhase::InspectionProgram||phase_==HousePhase::Unsupported||phase_==HousePhase::Error||phase_==HousePhase::SceneDoorPending)return true;
 for(uint32_t i=0;i<story_process_.size();++i)if(story_process_[i]&&idle_frame_>story_ready_idle_[i]){
  story_process_[i]=0;if(!story_conditions(i))continue;
  if(!world_->pause_for_house())return fail("Story trigger pause rejected");
  story_index_=i;event(HouseEventKind::StoryRequested,i);
  const auto trigger=content_.story_trigger(i);
  if(trigger.disposition==2){story_executing_=true;if(!world_->begin_house_program(trigger.program_index))return fail("Source house program start rejected");}

  error_="Original story requested; script/battle not implemented. B returns after fade";
  if(phase_!=HousePhase::DoorFadeIn&&phase_!=HousePhase::WarpAwaitIdle&&phase_!=HousePhase::DoorFadeOut)phase_=story_executing_?HousePhase::StoryRunning:HousePhase::StoryBoundary;
  break;
 }return true;
}
bool HouseRuntime::begin_door(uint32_t index){
 const auto door=content_.door(index);phase_=HousePhase::DoorFadeIn;fade_time_=0;
 if(!content_.string(door.start_sound).empty())sounds_.push_back({door.start_sound});
 event(HouseEventKind::DoorStarted,index);return true;
}
bool HouseRuntime::finish_door(){
 const auto door=content_.door(active_);if(!content_.string(door.end_sound).empty())sounds_.push_back({door.end_sound});
 if(!story_pending()&&(!world_->set_house_direction(door.direction)||!world_->unpause_from_house()))return fail("Door unpause rejected");
 door_unpaused_=true;event(HouseEventKind::DoorDone,active_);return true;
}
uint32_t HouseRuntime::program_for_path(std::string_view path)const{
 const auto room=world_->content();for(uint32_t i=0;i<room.program_count();++i)if(room.string(room.program(i).source_path_string)==path)return i;return house_no_index;
}
bool HouseRuntime::bind_basement(const BasementProgressionData&data,const BasementActorData&actors,HouseBasementHost host,std::string&e){
 auto reject=[&](const char*text){e=text;return false;};
 if(!world_||!content_.valid()||!data.valid()||!actors.valid()||phase_!=HousePhase::Idle||story_pending()||presentation_->dialogue_active())return reject("Basement binding requires idle initialized House");
 std::string pin;const char*hex="0123456789abcdef";for(unsigned i=32;i<52;++i){pin+=hex[content_.bytes()[i]>>4];pin+=hex[content_.bytes()[i]&15];}
 if(!data.bind_reviewed_commit(pin,e)||actors.reviewed_commit()!=pin)return reject("Basement source pin mismatch");
 if(!host.validate_key_item||!host.key_owned||!host.select_key_item||!host.remove_key_item||!host.grant_key_item||!host.validate_present_sound||!host.present_sound)return reject("Basement effect host incomplete");
 const auto&door=data.door();const auto&present=data.present();uint32_t target=house_no_index;
 for(uint32_t i=0;i<openables_.size();++i)if(content_.string(content_.openable_door(i).source_path)==door.node){if(target!=house_no_index)return reject("Ambiguous basement door binding");target=i;}
 if(target==house_no_index)return reject("Basement door node absent from House");
 const auto checked=content_.openable_door(target);const auto*key=data.key_item(door.key_id);const auto*item=data.key_item(present.key_id);
 if(!key||!item||!item->grant||content_.string(checked.key)!=key->source||content_.string(checked.flag)!=door.flag||bool(checked.policy&uint32_t(HouseDoorPolicy::RemoveKey))!=door.remove_key||!content_.string(checked.activates_flag).empty()||!content_.string(checked.deactivates_flag).empty()||(checked.policy&uint32_t(HouseDoorPolicy::Blocked)))return reject("Basement door source binding mismatch");
 for(const auto&path:{door.opened,door.locked,present.dialogue,present.empty})if(program_for_path(path)==house_no_index)return reject("Basement dialogue programme missing");
 auto programme_source_matches=[&](std::string_view identity,std::string_view source){
  const auto room=world_->content();const auto p=room.program(program_for_path(identity));
  for(uint32_t i=0;i<p.command_count;++i){const auto c=room.command(p.first_command+i);if(c.opcode!=uint16_t(DialogueActionKind::ShowDialogue))continue;
   for(uint32_t d=0;d<content_.count(HouseSection::Dialogues);++d){const auto text=content_.dialogue(d);if(text.id==c.target_index&&content_.string(text.source_path)==source)return true;}
  }return false;
 };
 if(!programme_source_matches(door.opened,content_.string(checked.opened_dialogue))||!programme_source_matches(door.locked,content_.string(checked.locked_dialogue)))return reject("Basement lock dialogue source binding mismatch");
 if(present.is_object_flag||present.reset_when_leaving_area||present.reset_when_leaving_region)return reject("Unsupported basement Present flag lifecycle");
 if(!validate_flag(door.flag,e)||!validate_flag(present.flag,e))return false;
 const auto*animation=actors.animation(actors.present_animation_id());const auto*resource=actors.resource(actors.present_resource_id());
 if(!animation||!resource||animation->resource_id!=resource->id||present.opened_frame>=resource->columns*resource->rows||present.closed_frame>=resource->columns*resource->rows||actors.present_sound_stop()>animation->length)return reject("Basement Present actor binding mismatch");
 if(!host.validate_key_item(*key,e)||!host.validate_key_item(*item,e)||!host.validate_present_sound(actors,e))return false;
 basement_=&data;basement_actors_=&actors;basement_host_=std::move(host);basement_door_=target;basement_present_={};basement_sound_playing_=basement_sound_pending_=false;e.clear();return true;
}
bool HouseRuntime::begin_basement_program(std::string_view path,uint32_t object){
 const auto program=program_for_path(path);if(program==house_no_index)return fail("Basement source programme absent");
 if(!world_->pause_for_house())return fail("Basement programme pause rejected");
 story_original_npc_=house_no_index;story_executing_=true;active_=object;phase_=HousePhase::StoryRunning;
 if(!world_->begin_house_program(program))return fail("Basement source programme rejected");
 event(HouseEventKind::DialogueOpened,object);return true;
}
bool HouseRuntime::basement_present_opened()const{return basement_&&world_&&world_->story_flag(basement_->present().flag);}
uint32_t HouseRuntime::basement_present_frame()const{
 if(!basement_||!basement_actors_)return 0;
 if(basement_present_.visible)return basement_actors_->frame(basement_present_.animation_id,basement_present_.elapsed);
 return basement_present_opened()?basement_->present().opened_frame:basement_->present().closed_frame;
}
const BasementActorResource*HouseRuntime::basement_present_resource()const{return basement_actors_?basement_actors_->resource(basement_actors_->present_resource_id()):nullptr;}
bool HouseRuntime::interact_basement_present(){
 if(!basement_)return fail("Basement Present is not admitted");
 if(!basement_->present().can_pickup)return true;
 const auto&binding=basement_->present();const auto player=world_->player();Vec2 facing{binding.geometry.x-player.position.x,binding.geometry.y-player.position.y};
 if(((std::abs(facing.x)>std::abs(facing.y))||!(binding.player_turn&2))&&facing.x!=0&&(binding.player_turn&1))facing={sign(facing.x),0};else if((binding.player_turn&2)&&facing.y!=0)facing={0,sign(facing.y)};else facing=player.direction;
 if(!world_->set_house_direction(facing))return fail("Basement Present turn rejected");
 // Present._warn_empty opens only the source empty programme. Reinspection
 // neither restarts the unwrapping track nor constructs another item UID.
 if(basement_present_opened())return begin_basement_program(binding.empty,binding.stable_id);
 if(!begin_basement_actor(*basement_actors_,basement_actors_->present_animation_id(),basement_present_,basement_error_))return fail(basement_error_.c_str());
 basement_sound_pending_=true;
 const auto*item=basement_->key_item(binding.key_id);
 if(!item||!basement_host_.grant_key_item(*item,basement_error_))return fail(basement_error_.c_str());
 if(!world_->set_story_flag(binding.flag,true,binding.emit_flag_updated_signal))return fail("Basement Present flag rejected");
 // The renderer observes opened=true immediately (Sparkles stop/hide), while
 // the independent animation retains its source keys and audio stop clock.
 return begin_basement_program(binding.dialogue,binding.stable_id);
}
bool HouseRuntime::advance_basement_present(double delta){
 if(!basement_||!basement_present_.visible)return true;
 const auto old=basement_present_.elapsed;
 // AnimationPlayer.play changes the clip immediately; its time-zero audio
 // track is applied on the next idle animation update, without advance(0).
 if(basement_sound_pending_){if(!basement_host_.present_sound(*basement_actors_,true,basement_error_))return fail(basement_error_.c_str());basement_sound_pending_=false;basement_sound_playing_=true;}
 if(!advance_basement_actor(*basement_actors_,delta,basement_present_,basement_error_))return fail(basement_error_.c_str());
 if(basement_sound_playing_&&old<basement_actors_->present_sound_stop()&&basement_present_.elapsed>=basement_actors_->present_sound_stop()){
  if(!basement_host_.present_sound(*basement_actors_,false,basement_error_))return fail(basement_error_.c_str());basement_sound_playing_=false;
 }return true;
}
bool HouseRuntime::bind_phone(PhoneRuntime&phone){
 if(!world_||!phone.valid())return fail("Phone model is not initialized");
 const auto view=phone.view();const auto room=world_->content();
 for(uint32_t i=0;i<view.count(PhoneSection::FlagRefs);++i){bool found=false;const auto name=view.string(view.flag_ref(i).identity);for(uint32_t f=0;f<room.flag_count();++f)found|=room.string(room.flag(f).name_string)==name;if(!found)return fail("Phone flag binding is absent");}
 for(uint32_t i=0;i<room.binding_count();++i){const auto b=room.binding(i);if(b.kind==uint16_t(RoomBindingKind::StartPhoneRing)&&b.target_index>=view.count(PhoneSection::Objects))return fail("Phone scene binding is absent");}
 phone_=&phone;return true;
}
bool HouseRuntime::bind_drawer(DrawerProgramView candidate,DrawerHost&effects){
 auto reject=[&](const char*text){error_=text;return false;};
 if(!world_||!content_.valid()||!candidate.valid()||phase_!=HousePhase::Idle||story_pending()||presentation_->dialogue_active())return reject("Inspection programme requires an idle scene");
 if(std::memcmp(candidate.reviewed_commit(),content_.bytes()+32,20))return reject("Inspection programme source pin mismatch");
 const auto binding=candidate.binding();const auto path=candidate.string(binding.source_path);
 std::string error;
 for(uint32_t i=0;i<candidate.count(DrawerSection::Commands);++i){const auto c=candidate.command(i);
  if(c.opcode==uint32_t(DrawerOpcode::ShowText)){bool found=false;for(uint32_t d=0;d<content_.count(HouseSection::Dialogues);++d){const auto text=content_.dialogue(d);found|=text.id==c.a&&content_.string(text.source_path)==path;}if(!found)return reject("Inspection programme text/provenance mismatch");}
  else if(c.opcode==uint32_t(DrawerOpcode::BranchFlag)||c.opcode==uint32_t(DrawerOpcode::SetFlag)){if(!validate_flag(candidate.string(c.a),error))return reject("Inspection programme flag is unregistered");}
  else if(c.opcode==uint32_t(DrawerOpcode::PlaySound)){if(!effects.validate_sound(candidate.string(c.a),error))return reject("Inspection programme audio is unbound");}
 }
 for(uint32_t i=0;i<candidate.count(DrawerSection::Templates);++i){const auto t=candidate.item_template(i);if(!effects.validate_item(t,candidate.string(t.source),error))return reject("Inspection programme grant is unbound");}
 drawer_=candidate;drawer_effects_=&effects;drawer_runtime_.reset();error_="";return true;
}
bool HouseRuntime::validate_text(uint32_t id,std::string&e){
 for(uint32_t d=0;d<content_.count(HouseSection::Dialogues);++d){const auto text=content_.dialogue(d);if(text.id==id&&drawer_&&content_.string(text.source_path)==drawer_.string(drawer_.binding().source_path)){e.clear();return true;}}
 e="Inspection programme text absent";return false;
}
bool HouseRuntime::validate_flag(std::string_view name,std::string&e){
 const auto room=world_->content();for(uint32_t f=0;f<room.flag_count();++f)if(room.string(room.flag(f).name_string)==name){e.clear();return true;}e="Inspection programme flag absent";return false;
}
bool HouseRuntime::flag(std::string_view name,bool&value,std::string&e){if(!validate_flag(name,e))return false;value=world_->story_flag(name);return true;}
bool HouseRuntime::set_flag(std::string_view name,bool value,std::string&e){if(!validate_flag(name,e))return false;if(!world_->set_story_flag(name,value,true)){e="Inspection programme flag write failed";return false;}return true;}
bool HouseRuntime::show_text(uint32_t id,std::string&e){
 if(!validate_text(id,e))return false;
 for(uint32_t d=0;d<content_.count(HouseSection::Dialogues);++d){const auto text=content_.dialogue(d);if(text.id!=id)continue;
  if(!presentation_->present_story_dialogue(text.first_segment,text.segment_count,player_nickname())){e=presentation_->error();return false;}return true;}
 e="Inspection programme text absent";return false;
}
bool HouseRuntime::bind_inspections(HouseInspectionView candidate){
 auto reject=[&](const char*text){error_=text;return false;};
 if(!world_||!content_.valid()||!candidate.valid()||phase_!=HousePhase::Idle||story_pending()||presentation_->dialogue_active())return reject("House inspection binding requires an idle initialized scene");
 if(std::memcmp(candidate.reviewed_commit(),content_.bytes()+32,20))return reject("House inspection provenance does not match house pack");
 const auto room=world_->content();
 auto flag_known=[&](std::string_view name){for(uint32_t f=0;f<room.flag_count();++f)if(room.string(room.flag(f).name_string)==name)return true;return false;};
 auto text_valid=[&](uint32_t index,std::string_view path){
  if(index!=house_no_index)return index<content_.count(HouseSection::Dialogues)&&content_.string(content_.dialogue(index).source_path)==path;
  if(drawer_&&path==drawer_.string(drawer_.binding().source_path))return true;
  // no_index is an explicit unsupported programme boundary, never a fallback
  // for a mismatched index to text already admitted by the selected House.
  for(uint32_t d=0;d<content_.count(HouseSection::Dialogues);++d)if(content_.string(content_.dialogue(d).source_path)==path)return false;
  return true;
 };
 for(uint32_t i=0;i<candidate.count(HouseInspectionSection::Objects);++i){const auto object=candidate.object(i);
  // InteractDialog queues itself free at Ready/flags_updated when hidden.
  // Nonempty lifecycle conditions need event/deferred-deletion ownership;
  // this reviewed slice admits only source objects without those conditions.
  if(!candidate.string(object.appear_flag).empty()||!candidate.string(object.disappear_flag).empty())return reject("House inspection appear/disappear lifecycle is not supported");
  if(!text_valid(object.default_dialogue_index,candidate.string(object.default_dialogue)))return reject("House inspection default text binding rejected");
  for(uint32_t j=0;j<object.override_count;++j){const auto rule=candidate.override_dialogue(object.first_override+j);
   if(!flag_known(candidate.string(rule.flag))||!text_valid(rule.dialogue_index,candidate.string(rule.dialogue)))return reject("House inspection flag/text override binding rejected");
  }
 }
 if(drawer_){bool found=false;for(uint32_t i=0;i<candidate.count(HouseInspectionSection::Objects);++i){const auto o=candidate.object(i);if(candidate.string(o.source_path)!=drawer_.string(drawer_.binding().inspection_source))continue;found=candidate.string(o.default_dialogue)==drawer_.string(drawer_.binding().source_path);for(uint32_t j=0;j<o.override_count;++j)found|=candidate.string(candidate.override_dialogue(o.first_override+j).dialogue)==drawer_.string(drawer_.binding().source_path);}if(!found)return reject("Inspection programme target absent");}
 inspections_=candidate;error_="";return true;
}
bool HouseRuntime::get_phone_flag(uint32_t index,bool&value)const{
 if(!phone_||index>=phone_->view().count(PhoneSection::FlagRefs))return false;
 value=world_->story_flag(phone_->view().string(phone_->view().flag_ref(index).identity));return true;
}
bool HouseRuntime::play_phone_sound(const PhoneSoundRequest&request){
 if(!phone_||request.object>=phone_->view().count(PhoneSection::Objects))return false;
 const auto requested=phone_->view().string(request.resource);const auto room=world_->content();
 uint32_t resource=house_no_index;
 for(uint32_t i=0;i<room.resource_count();++i){const auto r=room.resource(i);auto path=room.string(r.path_string);if(path.substr(0,6)=="res://")path.remove_prefix(6);if(r.kind==2&&path==requested){resource=i;break;}}
 if(resource==house_no_index||!world_->request_sound(resource))return false;
 phone_sounds_.push_back(request);return true;
}
bool HouseRuntime::interact_phone(uint32_t index){
 if(!phone_||index>=phone_->view().count(PhoneSection::Objects))return fail("Unknown phone interaction");
 const auto object=phone_->view().object(index);const auto p=world_->player();Vec2 facing{object.position.x-p.position.x,object.position.y-p.position.y};
 if(((std::abs(facing.x)>std::abs(facing.y))||!(object.policy&uint32_t(PhonePolicy::PlayerTurnY)))&&facing.x!=0&&(object.policy&uint32_t(PhonePolicy::PlayerTurnX)))facing={sign(facing.x),0};else if((object.policy&uint32_t(PhonePolicy::PlayerTurnY))&&facing.y!=0)facing={0,sign(facing.y)};else facing=p.direction;
 if(!world_->set_house_direction(facing)||!world_->pause_for_house())return fail("Phone player turn/pause rejected");
 PhoneInteraction interaction;if(!phone_->interact(index,*this,interaction))return fail(phone_->error());
 const uint32_t program=program_for_path(phone_->view().string(interaction.program));
 if(program==house_no_index){phase_=HousePhase::Unsupported;error_="Unported source phone dialogue; B returns control";last_safe_position_=p.position;last_safe_direction_=p.direction;return true;}
 // Phone is an InteractDialog, so no NPC talker/seen key or Actor replacement.
 story_original_npc_=house_no_index;story_executing_=true;phase_=HousePhase::StoryRunning;
 if(!world_->begin_house_program(program)||!play_phone_sound(interaction.sound))return fail("Phone program/sound rejected");
 return true;
}
bool HouseRuntime::resolve_npc_dialogue(uint32_t selected,uint32_t&first,uint32_t&count,uint32_t&program,uint32_t&selected_seen)const{
 if(!world_||!content_.valid()||selected>=content_.count(HouseSection::Npcs))return false;
 const auto npc=content_.npc(selected);first=npc.first_segment;count=npc.segment_count;program=npc.program_index;selected_seen=npc.seen_key;bool unsupported=false;
 for(uint32_t i=0;i<content_.count(HouseSection::Overrides);++i){const auto rule=content_.override_dialogue(i);if(rule.npc==selected&&world_->story_flag(content_.string(rule.flag))){selected_seen=rule.seen_key;program=program_for_path(content_.string(rule.dialogue));unsupported=rule.dialogue_index==house_no_index&&program==house_no_index;if(rule.dialogue_index!=house_no_index){const auto d=content_.dialogue(rule.dialogue_index);first=d.first_segment;count=d.segment_count;}}}
 return !unsupported&&(count||program!=house_no_index);
}
bool HouseRuntime::npc_interaction_supported(uint32_t i)const{uint32_t first=0,count=0,program=0,seen=0;return resolve_npc_dialogue(i,first,count,program,seen);}
bool HouseRuntime::door_interaction_supported(uint32_t i)const{if(basement_&&i==basement_door_)return true;
 if(!content_.valid()||i>=openables_.size())return false;const auto&state=openables_[i];const auto d=content_.openable_door(i);
 return !state.unlocked&&state.blocked&&d.blocked_dialogue!=house_no_index&&content_.string(d.activates_flag).empty()&&content_.string(d.deactivates_flag).empty();
}
bool HouseRuntime::phone_interaction_supported(uint32_t i)const{uint32_t program=0;return phone_&&phone_->interaction_program(i,*this,program)&&program_for_path(phone_->view().string(program))!=house_no_index;}
bool HouseRuntime::inspection_visible(uint32_t i)const{return world_&&inspections_.valid()&&i<inspections_.count(HouseInspectionSection::Objects);}
Vec2 HouseRuntime::inspection_position(uint32_t i)const{return inspection_visible(i)?inspections_.object(i).position:Vec2{};}
bool HouseRuntime::resolve_inspection_dialogue(uint32_t index,uint32_t&dialogue)const{
 if(!inspection_visible(index))return false;
 const auto object=inspections_.object(index);uint32_t selected=object.default_dialogue_index;
 // InteractDialog._get_right_dialog: all entries execute in source order,
 // including later matches replacing earlier matches, not first-match wins.
 for(uint32_t j=0;j<object.override_count;++j){const auto rule=inspections_.override_dialogue(object.first_override+j);if(world_->story_flag(inspections_.string(rule.flag)))selected=rule.dialogue_index;}
 if(selected==house_no_index||selected>=content_.count(HouseSection::Dialogues))return false;
 dialogue=selected;return true;
}
std::string_view HouseRuntime::inspection_path(uint32_t i)const{
 if(!inspection_visible(i))return {};const auto o=inspections_.object(i);auto path=inspections_.string(o.default_dialogue);
 for(uint32_t j=0;j<o.override_count;++j){const auto r=inspections_.override_dialogue(o.first_override+j);if(world_->story_flag(inspections_.string(r.flag)))path=inspections_.string(r.dialogue);}return path;
}
bool HouseRuntime::drawer_selected(uint32_t i)const{return drawer_&&inspection_visible(i)&&inspections_.string(inspections_.object(i).source_path)==drawer_.string(drawer_.binding().inspection_source)&&inspection_path(i)==drawer_.string(drawer_.binding().source_path);}
bool HouseRuntime::inspection_interaction_supported(uint32_t i)const{uint32_t dialogue=house_no_index;return resolve_inspection_dialogue(i,dialogue)||drawer_selected(i);}
bool HouseRuntime::interact_inspection(uint32_t index){
 if(!inspection_visible(index))return fail("Unbound house inspection interaction");
 const auto object=inspections_.object(index);const auto p=world_->player();uint32_t dialogue=house_no_index;
 Vec2 facing{object.position.x-p.position.x,object.position.y-p.position.y};
 // Actual Player._turn_to overrides party_object's unconstrained base method.
 if(((std::abs(facing.x)>std::abs(facing.y))||!(object.player_turn&2))&&facing.x!=0&&(object.player_turn&1))facing={sign(facing.x),0};else if((object.player_turn&2)&&facing.y!=0)facing={0,sign(facing.y)};else facing=p.direction;
 if(!world_->set_house_direction(facing)||!world_->pause_for_house())return fail("House inspection player turn/pause rejected");
 active_=object.id;
 if(drawer_selected(index)){
  phase_=HousePhase::InspectionProgram;
  if(!drawer_runtime_.start(drawer_,*this,drawer_error_))return fail(drawer_error_.c_str());
  event(HouseEventKind::DialogueOpened,object.id);return true;
 }
 if(!resolve_inspection_dialogue(index,dialogue)){phase_=HousePhase::Unsupported;error_="Unported source inspection programme; B returns control";last_safe_position_=p.position;last_safe_direction_=p.direction;return true;}
 const auto text=content_.dialogue(dialogue);
 if(!presentation_->begin_dialogue(text.first_segment,text.segment_count,player_nickname()))return fail("House inspection dialogue rejected");
 // InteractDialog opens a box with no NPC talker and never writes source
 // seen_dialogue_flags. Repeated inspection must not alter saved NPC history.
 phase_=HousePhase::Dialogue;event(HouseEventKind::DialogueOpened,object.id);return true;
}
bool HouseRuntime::interact(){
 if(world_->player().crouch)return true; // Telepathy is a separate unported action.
 const auto rules=content_.interaction();const auto p=world_->player();const Vec2 origin{p.position.x+rules.ray_origin.x,p.position.y+rules.ray_origin.y};
 const float length=std::sqrt(p.direction.x*p.direction.x+p.direction.y*p.direction.y);if(length<=0)return fail("Invalid interaction direction");
 const Vec2 direction{p.direction.x/length,p.direction.y/length};uint32_t selected=house_no_index;float nearest=std::numeric_limits<float>::infinity();
 for(uint32_t i=0;i<content_.count(HouseSection::Npcs);++i){if(!presentation_->npc_pose(i).visible)continue;const auto npc=content_.npc(i);float distance=0;if(ray_rect(origin,direction,rules.ray_length,npc_point(i,npc.interact_center),npc.interact_extents,distance)&&distance<nearest){selected=i;nearest=distance;}}
 uint32_t selected_door=house_no_index;
 for(uint32_t i=0;i<openables_.size();++i){const auto door=content_.openable_door(i);float distance=0;if(ray_rect(origin,direction,rules.ray_length,door.interact_center,door.interact_extents,distance)&&distance<nearest){selected_door=i;selected=house_no_index;nearest=distance;}}
 uint32_t selected_phone=house_no_index;
 if(phone_)for(uint32_t i=0;i<phone_->view().count(PhoneSection::Objects);++i){const auto object=phone_->view().object(i);float distance=0;if(ray_rect(origin,direction,rules.ray_length,object.interact_center,object.interact_extents,distance)&&distance<nearest){selected_phone=i;nearest=distance;selected=selected_door=house_no_index;}}
 uint32_t selected_inspection=house_no_index;
 if(inspections_)for(uint32_t i=0;i<inspections_.count(HouseInspectionSection::Objects);++i){if(!inspection_visible(i))continue;const auto object=inspections_.object(i);float distance=0;if(ray_rect(origin,direction,rules.ray_length,object.interact_center,object.interact_extents,distance)&&distance<nearest){selected_inspection=i;nearest=distance;selected=selected_door=selected_phone=house_no_index;}}

 bool selected_present=false;
 if(basement_){const auto&p=basement_->present();float distance=0;
  if(ray_rect(origin,direction,rules.ray_length,Vec2{p.interaction.x,p.interaction.y},Vec2{p.interaction.z,p.interaction.w},distance)&&distance<nearest){selected_present=true;nearest=distance;selected=selected_door=selected_phone=selected_inspection=house_no_index;}
 }
 if(selected_present)return interact_basement_present();
 // Unsupported inspection programmes still own the nearest hit. Never ray
 // through one to an NPC/door/phone or to another object behind it.
 if(selected_inspection!=house_no_index)return interact_inspection(selected_inspection);
 if(selected_phone!=house_no_index)return interact_phone(selected_phone);
 if(selected_door!=house_no_index)return interact_openable(selected_door);
 if(selected==house_no_index)return true;
 const auto npc=content_.npc(selected);uint32_t first=0,count=0,program=house_no_index,selected_seen=house_no_index;
 if(!resolve_npc_dialogue(selected,first,count,program,selected_seen)){if(!world_->pause_for_house())return false;phase_=HousePhase::Unsupported;error_="Unported source dialogue; B returns control";last_safe_position_=p.position;last_safe_direction_=p.direction;return true;}

 const auto position=presentation_->npc_pose(selected).position;Vec2 facing{position.x-p.position.x,position.y-p.position.y};
 if(((std::abs(facing.x)>std::abs(facing.y))||!(npc.flags&8))&&facing.x!=0&&(npc.flags&4))facing={sign(facing.x),0};else if((npc.flags&8)&&facing.y!=0)facing={0,sign(facing.y)};else facing=p.direction;
 if(!world_->set_house_direction(facing)||!world_->pause_for_house())return fail("NPC interaction pause rejected");
 const auto room=world_->content();const auto player=room.actor_instance(room.scene().player_instance_index);
 if(!presentation_->begin_npc_interaction(selected,p.position))return fail("Original NPC interaction rejected");
 if(program!=house_no_index){
  story_original_npc_=selected;story_executing_=true;phase_=HousePhase::StoryRunning;
  if(!world_->begin_house_program(program,selected))return fail("NPC source program rejected");
  active_=selected;event(HouseEventKind::DialogueOpened,selected);seen_.insert(selected_seen);return true;
 }
 if(!presentation_->begin_npc_dialogue(selected,first,count,player_nickname(),p.position))return fail("NPC dialogue rejected");
 active_=selected;phase_=HousePhase::Dialogue;event(HouseEventKind::DialogueOpened,selected);seen_.insert(selected_seen);event(HouseEventKind::DialogueSeen,selected);return true;
}
bool HouseRuntime::sync_story_dialogue(){
 for(bool sound:world_->take_story_hides())presentation_->hide_story_dialogue(sound);
 const auto requested=world_->pending_dialogue_id();
 if(world_->story_choices_waiting()&&choices_&&choices_->phase()==DialogueChoicesPhase::Closed){
  const auto room=world_->content();const auto program=room.program(world_->story_program_index());std::string error;
  if(!choices_data_||!choices_->prepare(*choices_data_,world_->pending_choice_group(),room.string(program.source_path_string),program.command_count,error))return fail("Story choice data binding rejected");
  const auto*group=choices_->group();
  auto target_valid=[&](uint32_t pc){return pc>=world_->story_next_command_index()&&pc<program.command_count&&(pc==0||room.command(program.first_command+pc-1).phrase!=room.command(program.first_command+pc).phrase);};
  if(!group||!target_valid(group->cancel_target_pc))return fail("Choice cancel target is not a forward phrase entry");
  for(const auto&option:group->options)if(!target_valid(option.target_pc))return fail("Choice target is not a forward phrase entry");
  choices_generation_=world_->story_generation();
 }
 if(requested==house_no_index||active_story_dialogue_!=house_no_index)return true;
 uint32_t selected=house_no_index;for(uint32_t i=0;i<content_.count(HouseSection::Dialogues);++i)if(content_.dialogue(i).id==requested){selected=i;break;}
 if(selected==house_no_index)return fail("Story dialogue ID absent");
 const auto text=content_.dialogue(selected);const auto room=world_->content();const auto player=room.actor_instance(room.scene().player_instance_index);
 if(!presentation_->present_story_dialogue(text.first_segment,text.segment_count,player_nickname()))return fail("Story text rejected");
 active_story_dialogue_=requested;return true;
}
bool HouseRuntime::advance_story_dialogue(bool automatic){
 active_story_dialogue_=house_no_index;
 if(!world_->finish_story_dialogue(automatic))return fail("Story dialogue completion rejected");
 if(!process_npc_restores()||!sync_story_dialogue())return false;
 if(world_->pending_dialogue_id()==house_no_index)presentation_->close_story_dialogue();
 return true;
}
bool HouseRuntime::select_story_option(uint32_t pc,uint32_t generation){
 if(!choices_||generation!=choices_generation_)return fail("Stale story option");
 presentation_->clear_story_text();active_story_dialogue_=house_no_index;choices_->close();
 if(!world_->choose_story_option(pc,generation))return fail("World choice rejected");
 return process_npc_restores()&&sync_story_dialogue();
}
bool HouseRuntime::close_story_submenu(uint32_t generation){
 active_story_dialogue_=house_no_index;
 if(!world_->close_story_submenu(generation))return fail("World submenu rejected");
 return process_npc_restores()&&sync_story_dialogue();
}
bool HouseRuntime::text_finished(){
 // This is the original _finish_phrase callback during printing, not a
 // synthetic Accept event on the following idle frame. Its source gating
 // includes caninput and the active minimum timer; automatic goto is silent.
 if(!world_||world_->stage()!=OpeningStage::ScriptRunning)return true;
 if(world_->story_choices_waiting()){
  std::string error;if(!choices_||!choices_->text_completed(error))return fail("Story choices completion rejected");
  presentation_->set_choice_rows(choices_data_->trailing_blank_lines());return true;
 }
 if(!world_->story_auto_advance_ready())return true;
 if(active_story_dialogue_==house_no_index)return fail("Automatic text lacks active phrase");
 return advance_story_dialogue(true);
}
bool HouseRuntime::idle_frame(double delta,bool accept,bool cancel){
 if(!content_.valid()||!std::isfinite(delta)||delta<0||delta>double(.1f))return fail("Invalid house idle delta");
 ++idle_frame_;delta=double(float(delta));
 if(phase_==HousePhase::Error)return false;
 // The source Phone AnimationPlayer processes before deferred scene methods.
 if(phone_&&!phone_->advance(delta,*this))return fail(phone_->error());
 for(const auto call:world_->take_scene_calls())if(!phone_||!phone_->ring(call.object))return fail("Unbound source phone scene call");
 // CutsceneArea.gd connects battle_to_ov to _stop_process on every area.
 // Contacts armed before battle must not fire after the win script changes
 // their conditions. This is distinct from a new post-return body entry.
 if(world_->stage()==OpeningStage::BattleRequested)observed_battle_=true;
 else if(observed_battle_){
  observed_battle_=false;
  std::fill(story_process_.begin(),story_process_.end(),0);
  pending_contacts_.clear();
 }
 if(!process_npc_restores()||!advance_basement_present(delta))return false;
 if(world_->stage()==OpeningStage::BattleRequested)return true;
 bool story_dialogue_input=false;
 if(world_->stage()==OpeningStage::ScriptRunning){
  if(!sync_story_dialogue())return false;
  if(active_story_dialogue_!=house_no_index){
   story_dialogue_input=true;
   const bool text_input=world_->story_input_allowed()||(world_->story_choices_waiting()&&choices_&&choices_->phase()==DialogueChoicesPhase::WaitingText);
   presentation_->input(accept&&text_input,cancel&&text_input,true,world_->story_has_next_phrase());
   if(!world_->set_story_talking(presentation_->talking()))return fail("Story talking state rejected");
   const auto talker=world_->story_talker();
   if(talker.kind==DialogueTalkerKind::OriginalNpc&&!presentation_->set_npc_talking(talker.index,presentation_->talking()))return fail("Original NPC talking state rejected");
   if(presentation_->take_dialogue_advance()){
    if(!advance_story_dialogue(false))return false;
   }
  }
 }

 if(story_executing_&&world_->stage()==OpeningStage::Walking&&world_->story_completed()){
  // Actor restoration can move Player during this idle callback, after the
  // last physics observation. The next contact query must use the restored
  // body, not its old location inside the triggering area.
  observed_physics_position_=world_->player().position;
  pending_contacts_.clear();
  if(story_original_npc_!=house_no_index){if(!presentation_->stop_npc_interaction(story_original_npc_))return fail("NPC interaction completion rejected");story_original_npc_=house_no_index;}
  story_executing_=false;story_index_=house_no_index;if(phase_==HousePhase::StoryRunning)phase_=HousePhase::Idle;error_="";
 }
 if(!advance_openables(delta))return false;
 if(phase_==HousePhase::StoryBoundary){if(cancel){story_index_=house_no_index;std::fill(story_process_.begin(),story_process_.end(),0);if(!world_->warp_same_scene(last_safe_position_,last_safe_direction_)||!world_->unpause_from_house())return fail("Story boundary recovery rejected");phase_=HousePhase::Idle;error_="";}return true;}
 if(phase_==HousePhase::Unsupported){if(cancel){if(!world_->warp_same_scene(last_safe_position_,last_safe_direction_)||!world_->unpause_from_house())return fail("Boundary recovery rejected");phase_=HousePhase::Idle;error_="";}return true;}
 if(phase_==HousePhase::DoorAwaitIdle){if(idle_frame_>await_idle_){if(!begin_door(active_))return false;}else return true;}
 if(phase_==HousePhase::WarpAwaitIdle){if(idle_frame_>await_idle_){phase_=HousePhase::DoorFadeOut;fade_time_=0;event(HouseEventKind::FadeOutStarted,active_);}else return true;}
 if(!process_story_requests()||!deliver_area_contacts())return false;
 if(phase_==HousePhase::DoorFadeIn||phase_==HousePhase::DoorFadeOut){
  const auto door=content_.door(active_);const bool in=phase_==HousePhase::DoorFadeIn;const double old=fade_time_;
  fade_time_=double(float(fade_time_+double(float(delta*(in?door.fade_in_speed:door.fade_out_speed)))));
  if(in&&fade_time_>=door.fade_in_length){fade_time_=door.fade_in_length;event(HouseEventKind::DoorEntered,active_);if(!world_->warp_same_scene(door.destination,door.direction))return fail("Same-scene warp rejected");event(HouseEventKind::PlayerMoved,active_);phase_=HousePhase::WarpAwaitIdle;await_idle_=idle_frame_;}
  else if(!in){if(!door_unpaused_&&old<=door.fade_out_mostly&&fade_time_>door.fade_out_mostly)if(!finish_door())return false;if(fade_time_>=door.fade_out_length){phase_=story_pending()?(story_executing_?HousePhase::StoryRunning:HousePhase::StoryBoundary):HousePhase::Idle;fade_time_=0;}}
 }
 if(phase_==HousePhase::InspectionProgram){
  if(drawer_runtime_.state()==DrawerState::WaitingText){
   // The source appends subsequent phrases to the existing box. Defer close
   // until End rather than closing/reopening between the branch and receipt.
   const bool continues=drawer_.command(drawer_runtime_.pc()).opcode!=uint32_t(DrawerOpcode::End);
   presentation_->input(accept,cancel,true,continues);
   if(presentation_->take_dialogue_advance()){
    if(!drawer_runtime_.advance_text(drawer_error_))return fail(drawer_error_.c_str());
    if(drawer_runtime_.state()==DrawerState::Complete)presentation_->close_story_dialogue();
   }
  }
  if(drawer_runtime_.state()==DrawerState::Complete&&presentation_->dialogue_done()){if(!world_->unpause_from_house())return fail("Inspection programme unpause rejected");event(HouseEventKind::DialogueClosed,active_);phase_=HousePhase::Idle;}
 }
 else if(phase_==HousePhase::Dialogue){presentation_->input(accept,cancel);if(presentation_->dialogue_done()){if(!world_->unpause_from_house())return fail("Dialogue unpause rejected");event(HouseEventKind::DialogueClosed,active_);phase_=HousePhase::Idle;}}
 else if((phase_==HousePhase::Idle||(phase_==HousePhase::DoorFadeOut&&door_unpaused_))&&accept&&!story_dialogue_input)return interact();
 return true;
}
float HouseRuntime::fade_alpha()const{
 if(phase_==HousePhase::WarpAwaitIdle)return 1;
 if(phase_!=HousePhase::DoorFadeIn&&phase_!=HousePhase::DoorFadeOut)return 0;
 const auto door=content_.door(active_);const auto cuts=content_.parameter(HouseParameter::FadeCuts),shader=content_.parameter(HouseParameter::FadeShader);
 const bool in=phase_==HousePhase::DoorFadeIn;const double duration=in?door.fade_in_opaque:door.fade_out_mostly;
 const float t=float(std::clamp(fade_time_/duration,0.0,1.0));const float cut=in?cuts.x+(cuts.y-cuts.x)*t:cuts.z+(cuts.w-cuts.z)*t;
 const float alpha=std::clamp((shader.x-cut)/shader.y,0.f,1.f);return alpha*alpha*(3-2*alpha);
}
BattleValue HouseRuntime::fade_color()const{auto c=active_<content_.count(HouseSection::Doors)?content_.door(active_).color:BattleValue{};c.w*=fade_alpha();return c;}
}
