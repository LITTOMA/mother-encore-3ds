#pragma once
#include "encore/battle_action_presentation.hpp"
#include "encore/world.hpp"
namespace encore::upstream {
enum class BattleOutcomePhase:uint8_t {Idle,WaitDialogue,VictoryBanner,ExperienceDialogue,LevelDialogue,Returning,PostWinRequested,Complete,Error};
using BattleRewardState=BattleSessionStats;
enum class BattleRewardEvent:uint8_t {HpCommitted,VictoryStarted,ExperiencePrompt,ExperienceCommitted,StoryFlag,CurrencyCommitted,ReturnStarted,BattleEnded,LevelCommitted,LevelPrompt,SkillLearned,PostWinRequested,AreaMusicStopped};
class BattleOutcome {
public:
 bool initialize(RoundView,RoomView,const BattleSessionStats* session=nullptr);
 bool begin(const BattleRound&,BattleActionPresentation&,OpeningWorld&);
 bool idle_frame();
 bool advance_post_win();
 BattleOutcomePhase phase()const{return phase_;}
 const BattleRewardState& state()const{return state_;}
 const std::vector<BattleRewardEvent>& events()const{return events_;}
 std::string_view post_win_script()const{return content_.string(content_.encounter().post_win_script);}
 const char* error()const{return error_;}
private:
 bool fail(const char*);bool start_victory();bool commit_experience();bool next_growth_text();bool commit_rewards();
 RoundView content_;BattleRewardState state_{};const BattleRound* round_=nullptr;BattleActionPresentation* presentation_=nullptr;OpeningWorld* world_=nullptr;
 std::vector<uint32_t>growth_texts_;size_t growth_text_index_=0;
 bool post_win_started_=false;
 BattleOutcomePhase phase_=BattleOutcomePhase::Idle;std::vector<BattleRewardEvent>events_;const char*error_="";
};
// BattleParticipant.defeat deletes its associated overworld actor synchronously.
class WorldBattleHost final:public BattleRoundHost {
public:
 void bind(BattleActionPresentation& p,OpeningWorld& w,RoundView r){presentation_=&p;world_=&w;content_=r;}
 bool emit(const BattleRoundCue& cue,SourceRandom& random)override;
 bool ready(BattleRoundGate gate,uint32_t actor)const override{return presentation_->ready(gate,actor);}
 int32_t current_hp(uint32_t actor)const override{return presentation_->current_hp(actor);}
private:BattleActionPresentation*presentation_=nullptr;OpeningWorld*world_=nullptr;RoundView content_;
};
}
