#include "encore/battle_entry.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
float unit(float n){return std::clamp(n,0.0f,1.0f);}
float mix(float a,float b,float t){return a+(b-a)*t;}
// Native Godot Tween easing mechanisms, independent of game tuning.
float quadratic_out(float n){n=unit(n);return 1-(1-n)*(1-n);}
float quadratic_in(float n){n=unit(n);return n*n;}
float quartic_out(float n){n=unit(n);const float v=1-n;return 1-v*v*v*v;}
float quartic_in(float n){n=unit(n);return n*n*n*n;}
float back_out(float n){n=unit(n)-1;constexpr float overshoot=1.70158f;return n*n*((overshoot+1)*n+overshoot)+1;}
}
float battle_ease(float x,float c){x=unit(x);if(c>0)return c<1?1-std::pow(1-x,1/c):std::pow(x,c);if(c<0)return x<0.5f?std::pow(x*2,-c)*0.5f:(1-std::pow(1-(x-0.5f)*2,-c))*0.5f+0.5f;return 0;}
bool BattleEntry::fail(const char*s){phase_=BattleEntryPhase::Error;error_=s;return false;}
bool BattleEntry::begin(const BattleView&content,const RoomView&room,const OpeningBattleRequest&request,BattleEntrySnapshot snapshot){
 if(phase_!=BattleEntryPhase::Idle||!content.valid()||!room.valid()||!request.requested||request.record_index>=room.battle_count())return fail("Invalid battle entry request");
 const auto meta=content.metadata();const auto record=room.battle(request.record_index);
 if(meta.room_battle_id!=record.stable_id||content.string(meta.enemy_name)!=request.enemy||meta.player_instance!=room.scene().player_instance_index||meta.enemy_instance!=request.actor_index||bool(meta.flags&1)!=request.can_run||request.advantage!=0)return fail("Battle content does not bind this encounter");
 if(!std::isfinite(snapshot.player_screen.x)||!std::isfinite(snapshot.player_screen.y)||!std::isfinite(snapshot.enemy_screen.x)||!std::isfinite(snapshot.enemy_screen.y)||!std::isfinite(snapshot.enemy_offset.x)||!std::isfinite(snapshot.enemy_offset.y)||snapshot.nudge_sign<-1||snapshot.nudge_sign>1||snapshot.party_hp< -1||snapshot.party_hp>999||snapshot.party_pp< -1||snapshot.party_pp>999)return fail("Invalid battle entry snapshot");
 const auto nudge=content.parameter(BattleParameter::PartyNudge);const bool needs_nudge=std::abs(snapshot.player_screen.x-nudge.x)<nudge.y;
 if(needs_nudge&&snapshot.nudge_sign==0)return fail("Source center nudge requires an injected cosmetic random sign");
 const auto wait=content.parameter(BattleParameter::PartyJumpStart),scale=content.parameter(BattleParameter::PartyJumpScale),show=content.parameter(BattleParameter::PartyShow);
 if(wait.x<=0||wait.y<=0||wait.z<=0||wait.w<=0||scale.x<=0||scale.y<=0||scale.z<0||scale.w<=0||show.x<=0||show.y<0||nudge.y<0||nudge.z<0||nudge.w<=0)return fail("Invalid battle motion parameters");
 for(auto p:{BattleParameter::PlayerJumpDuration,BattleParameter::EnemyMoveDuration,BattleParameter::CursorDuration})if(content.parameter(p).x<=0)return fail("Invalid battle tween duration");
 for(auto p:{BattleParameter::CursorScale2,BattleParameter::CursorScale3,BattleParameter::PartyQuake1,BattleParameter::PartyQuake2,BattleParameter::PartyQuake3,BattleParameter::PartyQuake4,BattleParameter::PartyQuake5})if(content.parameter(p).z<=0)return fail("Invalid battle tween segment");
 if(content.parameter(BattleParameter::CursorRepeatDelay).x<0)return fail("Invalid battle cursor repeat");
 for(uint32_t i=0;i<content.count(BattleSection::Layouts);++i){auto l=content.layout(i);if(l.role==uint32_t(BattleRole::EnemyTransition)){auto resource=content.resource(l.resource);if(snapshot.enemy_frame>=resource.columns*resource.rows)return fail("Invalid world enemy frame");}}
 content_=content;snapshot_=snapshot;party_nudge_=needs_nudge?nudge.z*float(snapshot.nudge_sign):0;
 if(snapshot.camera_shaking)phase_=BattleEntryPhase::WaitingCamera;else start_scene();
 return true;
}
void BattleEntry::start_scene(){phase_=BattleEntryPhase::Entering;scene_time_=mask_time_=0;idle_count_=next_event_=pending_begin_=pending_end_=selection_=0;menu_time_=enemy_time_=party_time_=enemy_appear_time_=party_show_time_=landing_time_=cursor_time_=-1;mask_started_=false;enemy_visible_=false;enemy_transition_=true;encounter_audio_pending_=true;party_landed_=false;landing_exact_time_=-1;jump_wait_stage_=0;cursor_repeat_remaining_=0;cursor_from_=cursor_to_={};}
void BattleEntry::event(BattleEventKind kind){switch(kind){case BattleEventKind::StartEnemyMove:enemy_time_=0;break;case BattleEventKind::StartPartyJump:jump_wait_stage_=1;jump_wait_remaining_=content_.parameter(BattleParameter::PartyJumpStart).x;break;case BattleEventKind::ShowMenu:menu_time_=0;break;case BattleEventKind::ShowEnemy:enemy_visible_=true;enemy_appear_time_=0;break;case BattleEventKind::RemoveEnemyTransition:enemy_transition_=false;break;}}
bool BattleEntry::idle_frame(double delta,bool shaking){
 if(phase_==BattleEntryPhase::Error)return false;
 if(phase_==BattleEntryPhase::Idle)return true;
 if(!std::isfinite(delta)||delta<0||delta>double(.1f))return fail("Unsupported battle idle delta");
 if(delta==0)return true;
 if(phase_==BattleEntryPhase::WaitingCamera){if(shaking)return true;start_scene();return true;}
 const float dt=float(delta);
 // Animation method tracks use the native deferred-call phase. A key becomes
 // eligible on the half-open interval [old,new), then runs next idle pass.
 for(uint32_t i=pending_begin_;i<pending_end_;++i)event(BattleEventKind(content_.event(i).kind));
 pending_begin_=pending_end_;
 const float duration=content_.parameter(BattleParameter::SceneDuration).x;
 scene_time_=std::min(duration,scene_time_+dt);
 while(next_event_<content_.count(BattleSection::Events)&&content_.event(next_event_).time<scene_time_)++next_event_;
 pending_end_=next_event_;
 if(mask_started_)mask_time_=std::min(content_.parameter(BattleParameter::MaskDuration).x,mask_time_+dt);
 if(menu_time_>=0)menu_time_+=dt;
 if(enemy_time_>=0)enemy_time_+=dt;
 if(enemy_appear_time_>=0)enemy_appear_time_+=dt;
 if(landing_time_>=0){landing_time_+=dt;landing_exact_time_+=delta;}
 if(cursor_time_>=0)cursor_time_+=dt;
 cursor_repeat_remaining_=std::max(0.0f,cursor_repeat_remaining_-dt);
 // SceneTreeTimer completion is strict negative. A newly created follow-on
 // timer does not consume the current timer pass; its new tween does.
 if(jump_wait_stage_){jump_wait_remaining_-=delta;if(jump_wait_remaining_<0){if(jump_wait_stage_==1){jump_wait_stage_=2;jump_wait_remaining_=content_.parameter(BattleParameter::PartyJumpStart).y;}else{jump_wait_stage_=0;party_time_=0;}}}
 if(party_time_>=0&&!party_landed_){party_time_+=dt;const float end=content_.parameter(BattleParameter::PlayerJumpDuration).x;if(party_time_>=end){party_time_=end;party_landed_=true;landing_time_=0;landing_exact_time_=0;}}
 if(phase_==BattleEntryPhase::Entering&&scene_time_>=duration){phase_=BattleEntryPhase::Commands;party_show_time_=0;}
 if(party_show_time_>=0)party_show_time_+=dt;
 // uiManager yields one idle frame and then defers adding the mask.
 if(++idle_count_==1)mask_started_=true;
 return true;
}
Vec2 BattleEntry::cursor_offset()const{if(cursor_time_<0)return cursor_to_;const float duration=content_.parameter(BattleParameter::CursorDuration).x;const float t=quartic_out(cursor_time_/duration);return {mix(cursor_from_.x,cursor_to_.x,t),mix(cursor_from_.y,cursor_to_.y,t)};}
Vec2 BattleEntry::cursor_scale()const{
 if(cursor_time_<=0)return cursor_initial_scale_;
 const auto a=content_.parameter(BattleParameter::CursorScale1),b=content_.parameter(BattleParameter::CursorScale2),c=content_.parameter(BattleParameter::CursorScale3);
 float t=cursor_time_;const float d=content_.parameter(BattleParameter::CursorDuration).x;
 if(t<=d){const float w=quartic_out(t/d);return {mix(a.z,a.x,w),mix(a.w,a.y,w)};}t-=d;
 if(t<=b.z){const float w=quartic_out(t/b.z);return {mix(a.x,b.x,w),mix(a.y,b.y,w)};}t-=b.z;
 if(t<=c.z){const float w=quartic_out(t/c.z);return {mix(b.x,c.x,w),mix(c.w,c.y,w)};}
 return {c.x,c.y};
}
bool BattleEntry::input(int direction,bool confirm,bool cancel,bool navigation_pulse){
 if(direction<-1||direction>1)return fail("Invalid battle menu direction");
 if(phase_!=BattleEntryPhase::Commands)return phase_!=BattleEntryPhase::Error;
 const int n=int(content_.count(BattleSection::Menus));
 if(cancel){selection_=0;cursor_time_=-1;cursor_from_=cursor_to_={};cursor_initial_scale_={1,1};return true;}
 if(direction&&(navigation_pulse||cursor_repeat_remaining_<=0)){int next=int(selection_)+direction;if(content_.metadata().flags&2)next=(next+n)%n;else next=std::clamp(next,0,n-1);if(next!=int(selection_)){cursor_from_=cursor_offset();cursor_initial_scale_=cursor_scale();selection_=uint32_t(next);const auto first=content_.layout(content_.menu(0).layout),selected=content_.layout(content_.menu(selection_).layout);cursor_to_={selected.rect.x-first.rect.x,selected.rect.y-first.rect.y};cursor_time_=0;cursor_repeat_remaining_=content_.parameter(BattleParameter::CursorRepeatDelay).x;}}
 if(confirm){requested_action_=content_.menu(selection_).id;phase_=BattleEntryPhase::ActionRequested;error_="Action requested";}
 return true;
}
bool BattleEntry::resume_commands(bool reset_selection){
 if(phase_!=BattleEntryPhase::ActionRequested&&phase_!=BattleEntryPhase::Commands)return false;
 phase_=BattleEntryPhase::Commands;requested_action_=0;error_="";
 return input(0,false,reset_selection);
}
float BattleEntry::clock(BattleClock which)const{switch(which){case BattleClock::Scene:return scene_time_;case BattleClock::Mask:return mask_started_?mask_time_:-1;case BattleClock::Menu:return menu_time_;case BattleClock::EnemyMove:return enemy_time_;case BattleClock::PartyJump:return party_time_;case BattleClock::EnemyAppear:return enemy_appear_time_;case BattleClock::PartyShow:return party_show_time_;case BattleClock::PartyLanding:return landing_time_;case BattleClock::Cursor:return cursor_time_;}return -1;}
float BattleEntry::quake_offset()const{
 if(landing_time_<0)return 0;
 double t=landing_exact_time_;
 for(auto p:{BattleParameter::PartyQuake1,BattleParameter::PartyQuake2,BattleParameter::PartyQuake3,BattleParameter::PartyQuake4,BattleParameter::PartyQuake5}){const auto step=content_.parameter(p);if(t<=step.z)return mix(step.x,step.y,battle_ease(float(t/step.z),step.w));t-=step.z;}
 return content_.parameter(BattleParameter::PartyQuake5).y;
}
BattlePose BattleEntry::pose(uint32_t index)const{
 if(!content_.valid()||index>=content_.count(BattleSection::Layouts))return {};
 const auto layout=content_.layout(index);BattlePose result{layout.rect,layout.color,layout.frame,(layout.flags&1)!=0};
 const auto role=BattleRole(layout.role);const auto offset=content_.parameter(BattleParameter::PartyScreenOffset);
 if(role==BattleRole::EnemyTransition){result.rect.x=snapshot_.enemy_screen.x;result.rect.y=snapshot_.enemy_screen.y;result.frame=snapshot_.enemy_frame;result.visible=enemy_transition_;}
 if(role==BattleRole::PartyTransition){result.rect.x=snapshot_.player_screen.x+offset.x;result.rect.y=snapshot_.player_screen.y+offset.y;result.visible=!party_landed_;}
 if(role==BattleRole::EnemySprite)result.visible=enemy_visible_;
 if(role==BattleRole::MenuCursor||role==BattleRole::MenuIcon)result.visible=menu_time_>=0;
 for(uint32_t i=0;i<content_.count(BattleSection::Tracks);++i){
  const auto track=content_.track(i);if(track.target!=index)continue;const float time=clock(BattleClock(track.clock));if(time<0)continue;
  uint32_t k=0;while(k+1<track.count&&content_.key(track.first+k+1).time<=time)++k;
  auto first=content_.key(track.first+k);auto sample=first.value;
  if(k==0&&track.property==uint32_t(BattleProperty::Position)&&(role==BattleRole::EnemyTransition||role==BattleRole::PartyTransition)){sample.x=role==BattleRole::EnemyTransition?snapshot_.enemy_screen.x:snapshot_.player_screen.x+offset.x;sample.y=role==BattleRole::EnemyTransition?snapshot_.enemy_screen.y:snapshot_.player_screen.y+offset.y;}
  if(track.update==0&&k+1<track.count&&time>=first.time){const auto end=content_.key(track.first+k+1);const float weight=battle_ease((time-first.time)/(end.time-first.time),first.ease);sample={mix(sample.x,end.value.x,weight),mix(sample.y,end.value.y,weight),mix(sample.z,end.value.z,weight),mix(sample.w,end.value.w,weight)};}
  switch(BattleProperty(track.property)){case BattleProperty::Position:result.rect.x=sample.x;result.rect.y=sample.y;break;case BattleProperty::PositionX:result.rect.x=sample.x;break;case BattleProperty::PositionY:result.rect.y=sample.x;break;case BattleProperty::Scale:result.rect.z=layout.rect.z*sample.x;result.rect.w=layout.rect.w*sample.y;break;case BattleProperty::Alpha:result.color.w=sample.x;break;case BattleProperty::Frame:result.frame=uint32_t(sample.x);break;case BattleProperty::Visible:result.visible=sample.x!=0;break;case BattleProperty::Color:result.color=sample;break;case BattleProperty::FlashColor:result.flash_color=sample;break;case BattleProperty::FlashModifier:result.flash_modifier=sample.x;break;}
 }
 if((role==BattleRole::PartyHP||role==BattleRole::PartyPP)&&layout.binding<3){
  const auto amount=role==BattleRole::PartyHP?snapshot_.party_hp:snapshot_.party_pp;
  if(amount>=0){const uint32_t divisor=layout.binding==0?100:layout.binding==1?10:1;result.frame=(uint32_t(amount)/divisor%10)*content_.resource(layout.resource).columns;result.visible=layout.binding!=0||uint32_t(amount)>=divisor;}
 }
 if(role==BattleRole::EnemyTransition){result.rect.x+=snapshot_.enemy_offset.x;result.rect.y+=snapshot_.enemy_offset.y;}
 if(role==BattleRole::PartyTransition){
  const auto nudge=content_.parameter(BattleParameter::PartyNudge);result.rect.x+=party_nudge_*back_out(scene_time_/nudge.w);
  if(party_time_>=0){const auto start=content_.parameter(BattleParameter::PartyJumpStart),target=content_.parameter(BattleParameter::PartyJumpTarget),scale=content_.parameter(BattleParameter::PartyJumpScale);const float sx=snapshot_.player_screen.x+offset.x+party_nudge_,sy=snapshot_.player_screen.y+offset.y;const float apex=sy-(target.z+std::max(0.0f,sy-target.w));
   result.rect.x=mix(sx,target.x,unit(party_time_/content_.parameter(BattleParameter::PlayerJumpDuration).x));
   result.rect.y=party_time_<=start.z?mix(sy,apex,quadratic_out(party_time_/start.z)):mix(apex,target.y,quadratic_in((party_time_-start.z)/start.w));
   const float weight=quartic_in((party_time_-scale.z)/scale.w);result.rect.z=layout.rect.z*mix(1,scale.x,weight);result.rect.w=layout.rect.w*mix(1,scale.y,weight);
  }
 }
 if(role==BattleRole::PartyPlate||role==BattleRole::PartyName||role==BattleRole::PartyHP||role==BattleRole::PartyPP||role==BattleRole::PartySprite)result.rect.y+=quake_offset();
 if(role==BattleRole::MenuCursor&&content_.count(BattleSection::Menus)){const auto off=cursor_offset(),scale=cursor_scale();result.rect.x+=off.x;result.rect.y+=off.y;result.rect.x+=(result.rect.z-result.rect.z*scale.x)/2;result.rect.y+=(result.rect.w-result.rect.w*scale.y)/2;result.rect.z*=scale.x;result.rect.w*=scale.y;}
 return result;
}
}
