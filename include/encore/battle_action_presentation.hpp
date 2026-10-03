#pragma once
#include "encore/battle_round.hpp"
#include "encore/battle_entry.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>
namespace encore::upstream {
struct BattleActionPose {
 uint32_t media=round_no_index,resource=round_no_index,role=0,frame=0;
 BattleValue rect{},color{1,1,1,1},modulate{1,1,1,1},flash_color{1,1,1,1},glow_color{1,1,1,1};
 Vec2 scale{1,1},offset{},anchor{};float rotation=0,flash_modifier=0,glow_modifier=0;
 bool visible=true,centered=false;float radius=0;std::string text;
};
// These clocks mirror source subsystems. Their completion is never implicitly
// promoted to action completion: only the scheduler's named gates may do that.
class BattleHpRoll {
public:
 bool begin(RoundView,int32_t hp,int32_t maximum);
 bool set_target(int32_t);bool set_instant(int32_t);int32_t stop_scrolling();bool idle_frame(double delta,bool defending=false,bool fast=false);
 int32_t current_hp()const{return current_+std::min<int32_t>(frame_,1);}
 int32_t display_hp()const{return current_;}int32_t target_hp()const{return target_;}
 uint32_t digit_frame(uint32_t place)const{return place<digits_.size()?digits_[place]:0;}
 bool digit_visible(uint32_t place)const;bool scrolling()const{return scrolling_;}
 bool take_done(){const bool d=done_;done_=false;return d;}
private:
 void instant(int32_t);RoundView content_;int32_t current_=0,target_=0,maximum_=0,frame_=0,radix_=0,frames_=0;
 double timer_=0;std::array<uint32_t,3>digits_{};bool scrolling_=false,increasing_=false,done_=false;
};
class BattleTextPacer {
public:
 bool begin(RoundView,std::string_view,bool auto_advance=true);void set_auto_advance(bool value){auto_advance_=value;}bool physics_frame(double);bool idle_frame(double);void input(bool accept,bool cancel);
 // A session override survives phrase begin(); no override uses checked pack
 // timing. The setter leaves the current clock and input multiplier untouched.
 bool set_text_speed(double seconds);double text_speed()const;
 bool done()const{return done_;}bool finished()const{return finished_;}uint32_t visible_characters()const{return visible_;}
 const std::string& text()const{return text_;}
 std::string visible_text()const;
private:RoundView content_;std::string text_;double elapsed_=0,remaining_=0,multiplier_=1,text_seconds_=0;uint32_t visible_=0,characters_=0;bool finished_=false,done_=true,timer_=false,empty_deferred_=false,auto_advance_=true;
};
using BattleTextResolver = bool (*)(void*,RoundView,uint32_t,std::string&);
class BattleActionPresentation final:public BattleRoundHost {
public:
 bool begin(RoundView,BattleView,SourceRandom&,const BattleSessionStats* session=nullptr);
 // Call after each encounter begin(), which restores its checked pack default.
 bool set_text_speed(double seconds);double text_speed()const{return text_.text_speed();}
 void set_text_resolver(BattleTextResolver resolver,void*context){text_resolver_=resolver;text_context_=context;}
 bool set_actor_base(uint32_t,const BattlePose&);bool set_plate_base(BattleValue);
 bool emit(const BattleRoundCue&,SourceRandom&)override;
 bool ready(BattleRoundGate,uint32_t)const override;int32_t current_hp(uint32_t)const override;
 bool physics_frame(double);bool idle_frame(double);void input(bool accept,bool cancel){text_.input(accept,cancel);}
 // Call once per actual frame with unscaled real delta. Source Slowmo uses real
 // ticks while animations, timers and HP use the resulting scaled game delta.
 double advance_real_time(double delta);double time_scale()const{return time_scale_;}
 BattleActionPose actor_pose(uint32_t)const;float plate_offset()const;
 std::vector<BattleActionPose> overlays()const;
 BattleActionPose sample(uint32_t media,double time,Vec2 origin={},Vec2 input={1,1})const;
 int32_t current_pp()const{return player_pp_;}
 const BattleHpRoll& hp()const{return hp_;}const BattleTextPacer& dialogue()const{return text_;}
 // Outcome presentation is still driven by the caller's source scheduler.
 // Return events expose world/camera callbacks; this class never mutates them.
 void set_auto_advance(bool value){text_.set_auto_advance(value);}
 bool refresh_session(const BattleSessionStats&);
 bool begin_victory();bool begin_outcome_text(std::string_view);bool begin_outcome_text(uint32_t text_index);
 bool begin_return();bool begin_party_return(Vec2 world_screen_position,Vec2 display_expansion={});
 bool victory_done()const{return victory_started_&&victory_remaining_<0;}
 bool return_done()const;
 double return_time()const{return return_timeline_.time;}
 bool battle_background_visible()const{return battle_background_visible_;}
 bool enemies_visible()const{return enemies_visible_;}
 std::vector<RoundEventKind> take_return_events();
 std::vector<BattleActionPose> return_overlays()const;
 BattleActionPose return_party_pose()const;
 float return_plate_offset()const;
 int32_t stop_hp_scrolling(){return hp_.stop_scrolling();}
 bool set_hp_instant(int32_t value){return hp_.set_instant(value);}
 bool target_open()const{return target_open_;}bool commands_visible()const{return commands_;}
 const char*error()const{return error_;}
 const RoundView& content()const{return content_;}
private:
 struct Clip {uint32_t media=round_no_index,target=round_no_index;double time=0;Vec2 origin{},input{1,1};bool active=false;std::string text;};
 struct Actor {BattlePose base{};Clip animation{},motion{};bool apply_damage=false,hide_pending=false,hidden=false,defending=false,dead=false;int32_t hp=0;uint32_t restore_media=round_no_index;};
 BattleTextResolver text_resolver_=nullptr;void*text_context_=nullptr;
 bool resolve_text(uint32_t,std::string&)const;
 bool fail(const char*);Clip clip(uint32_t,Vec2={},Vec2={1,1},std::string={})const;
 Clip slot(RoundPresentationSlot,Vec2={},Vec2={1,1},std::string={})const;
 void advance(Clip&,double,Actor* =nullptr);void start_hide(Actor&);void start_show(Actor&);Vec2 effect_position(uint32_t,bool top)const;
 RoundView content_;BattleView entry_;SourceRandom*random_=nullptr;std::vector<Actor>actors_;std::vector<Clip>effects_;
 Clip pointer_{},background_{},quake_{},dialogue_cursor_{};
 struct BossShake {Vec2 direction{},offset{};double timer=0;int32_t left=0;uint32_t source=round_no_index;};
 BossShake boss_shake_;Clip boss_flash_{};bool boss_defeat_=false;double boss_time_=0;uint32_t next_boss_shake_=0;int32_t player_maxhp_=0,player_pp_=0;
 Clip victory_banner_{},return_timeline_{},return_party_{};std::vector<RoundEventKind>return_events_;
 double victory_remaining_=0,return_frame_delta_=0;bool victory_started_=false,return_started_=false,return_jump_requested_=false,battle_background_visible_=true,enemies_visible_=true;BattleValue plate_{};BattleHpRoll hp_;BattleTextPacer text_;
 uint32_t player_=round_no_index,enemy_=round_no_index;bool target_open_=false,commands_=true,slowmo_=false;
 double slowmo_elapsed_=0,time_scale_=1;const char*error_="";
};
}
