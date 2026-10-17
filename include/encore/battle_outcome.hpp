#pragma once
#include "encore/battle_action_presentation.hpp"
#include "encore/world.hpp"
#include "encore/battle_loot.hpp"
namespace encore::upstream {
enum class BattleOutcomePhase:uint8_t {Idle,WaitDialogue,VictoryBanner,ExperienceDialogue,LevelDialogue,Returning,PostWinRequested,Complete,Error,LootDialogue};
using BattleRewardState=BattleSessionStats;
enum class BattleRewardEvent:uint8_t {HpCommitted,VictoryStarted,ExperiencePrompt,ExperienceCommitted,StoryFlag,CurrencyCommitted,ReturnStarted,BattleEnded,LevelCommitted,LevelPrompt,SkillLearned,PostWinRequested,AreaMusicStopped,ItemGranted,LootPrompt};
class BattleOutcome {
public:
 bool initialize(RoundView,RoomView,const BattleSessionStats* session=nullptr,BattleLoot* loot=nullptr);
 static bool preflight(RoundView,RoomView,const BattleSessionStats*,std::string& error);
 bool begin(const BattleRound&,BattleActionPresentation&,OpeningWorld&);
 bool idle_frame();
 bool advance_post_win();
 BattleOutcomePhase phase()const{return phase_;}
 const BattleRewardState& state()const{return state_;}
 // Publish the completed reward transaction for ordinary and post-win returns.
 // The caller copies it before a later encounter can replace this owner.
 const BattleRewardState* committed_state()const{return phase_==BattleOutcomePhase::Complete||phase_==BattleOutcomePhase::PostWinRequested?&state_:nullptr;}
 const std::vector<BattleRewardEvent>& events()const{return events_;}
 std::string_view post_win_script()const{return content_.string(content_.encounter().post_win_script);}
 const char* error()const{return error_;}
private:
 bool prepare(RoundView,RoomView,const BattleSessionStats*,BattleLoot*,bool require_loot);
 bool fail(const char*);bool start_victory();bool commit_experience();bool next_growth_text();bool commit_rewards();bool begin_loot();bool next_loot_text();
 RoundView content_;BattleRewardState state_{};const BattleRound* round_=nullptr;BattleActionPresentation* presentation_=nullptr;OpeningWorld* world_=nullptr;
 std::vector<uint32_t>growth_texts_;size_t growth_text_index_=0;
 BattleLoot* loot_=nullptr;uint32_t loot_text_index_=0;
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
