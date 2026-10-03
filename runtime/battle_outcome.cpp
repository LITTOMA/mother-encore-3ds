#include "encore/battle_outcome.hpp"
#include <limits>
#include <algorithm>
#include <utility>
namespace encore::upstream {
bool BattleOutcome::fail(const char* e){error_=e;phase_=BattleOutcomePhase::Error;return false;}
bool BattleOutcome::initialize(RoundView content,RoomView room,const BattleSessionStats*session){
 *this=BattleOutcome{};
 if(!content.valid()||!room.valid())return fail("Invalid reward content");
 content_=content;const auto v=content.victory();
 if(v.initial_level==0||v.initial_level>=v.level_cap||v.level_cap!=room.experience_count()||v.next_level_exp!=room.experience(v.initial_level)||v.max_exp!=room.experience(v.level_cap-1))return fail("Reward progression contract mismatch");
 const auto encounter=content.encounter();
 if(v.reward_item_count||(!encounter.promoted_level&&uint64_t(v.initial_exp)+v.reward_exp>=v.next_level_exp))return fail("Unreviewed reward progression");
 if(encounter.promoted_level&&encounter.following_level_exp!=room.experience(encounter.promoted_level))return fail("Promoted level progression mismatch");
 if(uint64_t(v.initial_bank)+v.reward_cash>std::numeric_limits<uint32_t>::max()||uint64_t(v.initial_earned_cash)+v.reward_cash>std::numeric_limits<uint32_t>::max())return fail("Reward currency overflow");
 state_.experience=v.initial_exp;state_.level=v.initial_level;state_.bank=v.initial_bank;state_.cash=v.initial_cash;state_.earned_cash=v.initial_earned_cash;
 if(session)state_=*session;
 if(content.version()==5){
  // v5 carries the live session through either side of one reviewed promotion.
  // Threshold values remain external and must match the checked room table.
  if(state_.level!=v.initial_level&&state_.level!=encounter.promoted_level)return fail("Session level outside reviewed progression");
  if(state_.experience<room.experience(state_.level-1)||state_.experience>=room.experience(state_.level)||uint64_t(state_.experience)+v.reward_exp>=encounter.following_level_exp||uint64_t(state_.experience)+v.reward_exp>v.max_exp)return fail("Session experience outside reviewed progression");
  if(state_.cash>INT32_MAX||uint64_t(state_.bank)+v.reward_cash>INT32_MAX||uint64_t(state_.earned_cash)+v.reward_cash>INT32_MAX)return fail("Session reward currency overflow");
  BattleParticipant checked;
  if(session&&!apply_session_stats(checked,state_))return fail("Invalid carried reward session");
 }else if(session&&(state_.level!=v.initial_level||state_.experience!=v.initial_exp||uint64_t(state_.bank)+v.reward_cash>INT32_MAX||uint64_t(state_.earned_cash)+v.reward_cash>INT32_MAX))return fail("Session reward state outside reviewed progression");
 return true;
}
bool BattleOutcome::begin(const BattleRound& round,BattleActionPresentation& p,OpeningWorld& w){
 if(!content_.valid()||phase_!=BattleOutcomePhase::Idle||round.phase()!=BattleRoundPhase::VictoryPending)return fail("Invalid victory boundary");
 round_=&round;presentation_=&p;world_=&w;p.set_auto_advance(false);phase_=BattleOutcomePhase::WaitDialogue;
 // _win waits only while did_finish is false, not while an already-finished
 // dialogue is waiting for another input event.
 return p.dialogue().finished()?start_victory():true;
}
bool BattleOutcome::start_victory(){
 const auto actor=content_.binding().player_participant;const auto& b=round_->battler(actor);const auto& hp=presentation_->hp();
 state_.hp=b.target_hp<=hp.current_hp()?presentation_->stop_hp_scrolling():b.target_hp;
 if(state_.hp==0){state_.hp=1;if(!presentation_->set_hp_instant(state_.hp))return fail(presentation_->error());}
 state_.pp=b.target_pp;state_.maxhp=b.source.maxhp;state_.maxpp=b.source.maxpp;state_.offense=b.source.offense;state_.defense=b.source.defense;state_.speed=b.source.speed;state_.iq=b.source.iq;state_.guts=b.source.guts;events_.push_back(BattleRewardEvent::HpCommitted);
 if(!presentation_->begin_victory())return fail(presentation_->error());
 events_.push_back(BattleRewardEvent::VictoryStarted);phase_=BattleOutcomePhase::VictoryBanner;return true;
}
bool BattleOutcome::commit_experience(){
 const auto v=content_.victory();const auto e=content_.encounter();
 const bool conditional=content_.version()==5;
 const bool promote=e.promoted_level&&(!conditional||(state_.level==v.initial_level&&state_.experience<v.next_level_exp&&uint64_t(state_.experience)+v.reward_exp>=v.next_level_exp));
 BattleRewardState grown=state_;
 if(promote){
  grown.level=e.promoted_level;
  for(uint32_t i=0;i<content_.count(RoundSection::Growth);++i){const auto g=content_.growth(i);int32_t*current=nullptr;
   switch(RoundStat(g.stat)){case RoundStat::MaxHp:current=&grown.maxhp;break;case RoundStat::MaxPp:current=&grown.maxpp;break;case RoundStat::Offense:current=&grown.offense;break;case RoundStat::Defense:current=&grown.defense;break;case RoundStat::Speed:current=&grown.speed;break;case RoundStat::Iq:current=&grown.iq;break;case RoundStat::Guts:current=&grown.guts;break;}
   if(!current)return fail("Unknown growth stat");
   if(!conditional&&*current!=int32_t(g.before))return fail("Session growth baseline differs from source equipment/stats");
   // Source growth changes base stats, leaving the live equipment offset in
   // effective stats intact. The checked before/after pair supplies the delta.
   const int64_t gain=int64_t(g.after)-g.before;
   const int64_t next=conditional?int64_t(*current)+gain:int64_t(g.after);
   if(next<0||next>INT32_MAX)return fail("Effective growth stat overflow");
   *current=int32_t(next);
   if(g.stat==uint32_t(RoundStat::MaxHp)){
    const int64_t hp=int64_t(grown.hp)+gain;if(hp<0||hp>INT32_MAX)return fail("Growth HP overflow");grown.hp=int32_t(hp);
   }
   if(g.stat==uint32_t(RoundStat::MaxPp)){
    const int64_t pp=int64_t(grown.pp)+gain;if(pp<0||pp>INT32_MAX)return fail("Growth PP overflow");grown.pp=int32_t(pp);
   }
  }
  BattleParticipant checked;
  if(conditional&&!apply_session_stats(checked,grown))return fail("Invalid promoted reward session");
 }
 state_.experience+=v.reward_exp;events_.push_back(BattleRewardEvent::ExperienceCommitted);
 if(!promote)return commit_rewards();
 grown.experience=state_.experience;state_=std::move(grown);growth_texts_.push_back(e.level_text);
 for(uint32_t i=0;i<content_.count(RoundSection::Growth);++i){const auto g=content_.growth(i);if(g.text)growth_texts_.push_back(g.text);}
 events_.push_back(BattleRewardEvent::LevelCommitted);
 const std::string skill(content_.string(e.learned_skill));if(std::find(state_.learned_skills.begin(),state_.learned_skills.end(),skill)==state_.learned_skills.end()){state_.learned_skills.push_back(skill);growth_texts_.push_back(e.learned_text);events_.push_back(BattleRewardEvent::SkillLearned);}
 if(!presentation_->refresh_session(state_))return fail(presentation_->error());
 return next_growth_text();
}
bool BattleOutcome::next_growth_text(){
 if(growth_text_index_>=growth_texts_.size())return commit_rewards();
 const auto text=growth_texts_[growth_text_index_++];
 if(!presentation_->begin_outcome_text(text))return fail(presentation_->error());
 events_.push_back(BattleRewardEvent::LevelPrompt);phase_=BattleOutcomePhase::LevelDialogue;return true;
}
bool BattleOutcome::commit_rewards(){
 const auto v=content_.victory();
 if(!content_.string(content_.binding().win_flag).empty()){
  if(!world_->set_battle_story_flag())return fail("Battle story flag rejected");
  events_.push_back(BattleRewardEvent::StoryFlag);
 }
 state_.earned_cash+=v.reward_cash;state_.bank+=v.reward_cash;
 if(!world_->set_story_flag(content_.string(v.earned_cash_flag),true,false))return fail("Earned currency flag rejected");
 events_.push_back(BattleRewardEvent::CurrencyCommitted);
 // _end_battle_to_overworld removes boss area music before transitionOut
 // and battle_to_ov. The reviewed encounter data selects this source branch.
 if(content_.encounter().stop_area_music_if_overworld&&world_->battle_request().overworld_music){
  if(!world_->stop_area_music())return fail("Battle return area music cleanup rejected");
  events_.push_back(BattleRewardEvent::AreaMusicStopped);
 }
 if(!presentation_->begin_return())return fail(presentation_->error());
 // battle_to_ov invokes the player's camera _scoping_stop synchronously.
 const auto camera=content_.parameter(RoundParameter::ReturnCamera);
 if(!world_->start_battle_return_camera({camera.y,camera.z},camera.x))return fail("Return camera rejected");
 events_.push_back(BattleRewardEvent::ReturnStarted);phase_=BattleOutcomePhase::Returning;return true;
}
bool BattleOutcome::idle_frame(){
 switch(phase_){
 case BattleOutcomePhase::WaitDialogue:if(presentation_->dialogue().done())return start_victory();break;
 case BattleOutcomePhase::VictoryBanner:if(presentation_->victory_done()){
   const auto text=content_.victory().exp_text;
   if(!presentation_->begin_outcome_text(text))return fail(presentation_->error());
   events_.push_back(BattleRewardEvent::ExperiencePrompt);phase_=BattleOutcomePhase::ExperienceDialogue;
  }break;
 case BattleOutcomePhase::ExperienceDialogue:if(presentation_->dialogue().done())return commit_experience();break;
 case BattleOutcomePhase::LevelDialogue:if(presentation_->dialogue().done())return next_growth_text();break;
 case BattleOutcomePhase::Returning:if(presentation_->return_done()){
   if(!content_.string(content_.encounter().post_win_script).empty()){events_.push_back(BattleRewardEvent::BattleEnded);events_.push_back(BattleRewardEvent::PostWinRequested);phase_=BattleOutcomePhase::PostWinRequested;return true;}
   if(!world_->finish_battle_return())return fail("World return rejected");
   events_.push_back(BattleRewardEvent::BattleEnded);phase_=BattleOutcomePhase::Complete;
  }break;
 case BattleOutcomePhase::Error:return false;
 default:break;
 }return true;
}
bool BattleOutcome::advance_post_win(){
 if(phase_!=BattleOutcomePhase::PostWinRequested||!world_)return false;
 if(!post_win_started_){if(!world_->begin_battle_continuation(post_win_script()))return fail("Post-win source program rejected");post_win_started_=true;}
 else if(world_->story_completed())phase_=BattleOutcomePhase::Complete;
 return true;
}
bool WorldBattleHost::emit(const BattleRoundCue& cue,SourceRandom& random){
 world_->attach_random(random);
 if(!presentation_->emit(cue,random))return false;
 return cue.kind!=BattleRoundCueKind::Defeat||cue.target!=content_.binding().enemy_participant||content_.encounter().keep_actor||world_->erase_battle_actor(content_.victory().enemy_body_id);
}
}
