#pragma once
#include "encore/battle_round_data.hpp"
#include "encore/source_random.hpp"
#include <cstdint>
#include <vector>
namespace encore::upstream {
enum class BattleRoundPhase:uint8_t {Idle,Commands,Items,Targeting,Running,VictoryPending,DefeatPending,Unsupported,Error};
enum class BattleRoundCueKind:uint8_t {PrepareAction,TargetOpen,TargetClose,TurnStart,Dialogue,Attack,Hit,Miss,HideParty,Guard,Defeat,ShowMenu};
enum class BattleRoundGate:uint8_t {DialogueDone,ApplyDamage,BossDefeatDone};
struct BattleRoundCue {
 BattleRoundCueKind kind{};
 uint32_t actor=round_no_index,target=round_no_index,skill=round_no_index,text=round_no_index;
 int32_t amount=0,hp_after=0;
 bool smash=false,adrenaline=false;
};
// Synchronous presentation hooks preserve the original shared random stream.
// The host is shared C++ state: GPU IO and platform input remain outside it.
class BattleRoundHost {
public:
 virtual ~BattleRoundHost()=default;
 virtual bool emit(const BattleRoundCue&,SourceRandom&)=0;
 virtual bool ready(BattleRoundGate,uint32_t actor)const=0;
 virtual int32_t current_hp(uint32_t actor)const=0;
};
struct BattleSessionStats {
 uint32_t experience=0,level=0,bank=0,cash=0,earned_cash=0;
 int32_t hp=0,pp=0,maxhp=0,maxpp=0,offense=0,defense=0,speed=0,iq=0,guts=0;
 std::vector<std::string> learned_skills;
};
// Checked transient session values never modify the immutable entry resource.
bool apply_session_stats(BattleParticipant&,const BattleSessionStats&);
struct RoundBattlerState {BattleParticipant source{};int32_t target_hp=0,target_pp=0;bool defending=false;};
struct RoundActionState {uint32_t actor=0,target=round_no_index,skill=0;};
struct RoundDecision {
 uint32_t round=0,actor=0,target=0,skill=0;
 int32_t damage=0;bool miss=false,smash=false;
 uint64_t random_state=0,raw_draw_count=0;
};
class BattleRound {
public:
 bool begin(RoundView,BattleView,SourceRandom&,BattleRoundHost&,const BattleSessionStats* session=nullptr);
 bool request_menu(uint32_t menu_id);
 bool return_from_items();
 bool target_input(int direction,bool confirm,bool cancel=false);
 bool idle_frame(double delta);
 BattleRoundPhase phase()const{return phase_;}
 uint32_t number()const{return number_;}
 const RoundBattlerState& battler(uint32_t i)const{return battlers_[i];}
 const std::vector<RoundDecision>& decisions()const{return decisions_;}
 uint32_t selected_target()const{return selected_target_;}
 bool take_menu_return(){const bool result=menu_return_;menu_return_=false;return result;}
 const char*error()const{return error_;}
 const RoundView&content()const{return content_;}
private:
 enum class Step:uint8_t {EnemyDelay,DeferredAction,CheckAbleness,StartDelay,EmptyDialogue,Dialogue,StartSkill,WaitImpact,PreHitIdle,DamageIdle,TargetDelay,ActionEndDelay,RoundEndDelay,DefeatDialogueDelay,DefeatDialogue,BossDefeat};
 bool fail(const char*);bool cue(BattleRoundCueKind,uint32_t actor=round_no_index,uint32_t target=round_no_index,uint32_t skill=round_no_index,uint32_t text=round_no_index,int32_t amount=0,bool smash=false,bool adrenaline=false);
 bool queue_actions(uint32_t skill);bool start_action();bool execute_skill();bool apply_impact();bool finish_action();bool finish_round();
 bool chance(double percentage);int32_t calculate_damage(const RoundActionState&,bool adrenaline,bool smash);
 bool alive(uint32_t)const;void delay(Step,double);bool tick_delay(double);
 RoundView content_;BattleView entry_;SourceRandom*random_=nullptr;BattleRoundHost*host_=nullptr;
 std::vector<RoundBattlerState>battlers_;std::vector<RoundActionState>queue_;std::vector<RoundDecision>decisions_;
 BattleRoundPhase phase_=BattleRoundPhase::Idle;Step step_=Step::EnemyDelay;
 uint32_t number_=1,action_index_=0,selected_target_=0;double remaining_=0;
 bool menu_return_=false,miss_=false;const char*error_="";
};
}
