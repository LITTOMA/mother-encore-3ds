#pragma once
#include "encore/battle_data.hpp"
#include "encore/world.hpp"
namespace encore::upstream {
enum class BattleEntryPhase:uint8_t { Idle,WaitingCamera,Entering,Commands,ActionRequested,Error };
struct BattlePose {BattleValue rect{},color{};uint32_t frame=0;bool visible=false;BattleValue flash_color{};float flash_modifier=0;};
// Positions are original-viewport projections before the source party screen offset.
// The caller injects the cosmetic random sign only when the source center-nudge applies.
struct BattleEntrySnapshot {Vec2 player_screen{},enemy_screen{};bool camera_shaking=false;uint32_t enemy_frame=0;int8_t nudge_sign=0;Vec2 enemy_offset{};int32_t party_hp=-1,party_pp=-1;};
class BattleEntry {
public:
 bool begin(const BattleView&,const RoomView&,const OpeningBattleRequest&,BattleEntrySnapshot);
 bool idle_frame(double delta,bool camera_shaking=false);
 bool input(int direction,bool confirm,bool cancel=false,bool navigation_pulse=false);
 bool resume_commands(bool reset_selection=true);
 BattlePose pose(uint32_t layout_index)const;
 BattleEntryPhase phase()const{return phase_;}
 bool mask_active()const{return mask_started_&&mask_time_<content_.parameter(BattleParameter::MaskDuration).x;}
 float scene_time()const{return scene_time_;}float mask_time()const{return mask_time_;}
 float party_jump_time()const{return party_time_;}
 bool party_landed()const{return party_landed_;}
 uint32_t selection()const{return selection_;}
 uint32_t requested_action()const{return requested_action_;}
 bool encounter_audio_pending()const{return encounter_audio_pending_;}
 uint32_t take_encounter_audio(){encounter_audio_pending_=false;return content_.metadata().encounter_audio;}
 const BattleView&content()const{return content_;}
 BattleParticipant participant(uint32_t i)const{return content_.participant(i);}
 const char*error()const{return error_;}
private:
 bool fail(const char*);void start_scene();void event(BattleEventKind);float clock(BattleClock)const;
 float quake_offset()const;Vec2 cursor_offset()const;Vec2 cursor_scale()const;
 BattleView content_;BattleEntrySnapshot snapshot_{};
 BattleEntryPhase phase_=BattleEntryPhase::Idle;const char*error_="";
 float scene_time_=0,mask_time_=0,menu_time_=-1,enemy_time_=-1,party_time_=-1,enemy_appear_time_=-1,party_show_time_=-1,landing_time_=-1,cursor_time_=-1;
 uint32_t next_event_=0,pending_begin_=0,pending_end_=0,selection_=0,requested_action_=0,idle_count_=0;
 uint32_t jump_wait_stage_=0;double jump_wait_remaining_=0,landing_exact_time_=-1;
 Vec2 cursor_from_{},cursor_to_{},cursor_initial_scale_{1,1};float cursor_repeat_remaining_=0,party_nudge_=0;
 bool mask_started_=false,enemy_visible_=false,enemy_transition_=true,encounter_audio_pending_=false,party_landed_=false;
};
float battle_ease(float value,float curve);
}
