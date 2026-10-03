#include "encore/battle_round.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
bool BattleRound::fail(const char*message){phase_=BattleRoundPhase::Error;error_=message;return false;}
bool BattleRound::cue(BattleRoundCueKind kind,uint32_t actor,uint32_t target,uint32_t skill,uint32_t text,int32_t amount,bool smash,bool adrenaline){
 BattleRoundCue event{kind,actor,target,skill,text,amount,0,smash,adrenaline};
 if(target<battlers_.size())event.hp_after=battlers_[target].target_hp;
 if(!host_->emit(event,*random_))return fail("Battle presentation rejected source action");
 return true;
}
bool apply_session_stats(BattleParticipant&p,const BattleSessionStats&s){
 if(!s.level||s.level>INT32_MAX||s.experience>INT32_MAX||s.maxhp<=0||s.maxpp<0||s.hp<=0||s.hp>s.maxhp||s.pp<0||s.pp>s.maxpp||s.offense<0||s.defense<0||s.speed<0||s.iq<0||s.guts<0)return false;
 p.level=int32_t(s.level);p.xp=int32_t(s.experience);p.hp=s.hp;p.pp=s.pp;p.maxhp=s.maxhp;p.maxpp=s.maxpp;p.offense=s.offense;p.defense=s.defense;p.speed=s.speed;p.iq=s.iq;p.guts=s.guts;return true;
}
bool BattleRound::begin(RoundView content,BattleView entry,SourceRandom&random,BattleRoundHost&host,const BattleSessionStats*session){
 *this=BattleRound{};content_=content;entry_=entry;random_=&random;host_=&host;
 if(!content.valid()||!entry.valid())return fail("Missing checked round/battle data");
 const auto binding=content.binding();
 if(binding.battle_id!=entry.metadata().room_battle_id||binding.player_participant>=entry.count(BattleSection::Participants)||binding.enemy_participant>=entry.count(BattleSection::Participants)||binding.player_participant==binding.enemy_participant)return fail("Round participant bindings differ from entry");
 for(uint32_t i=0;i<entry.count(BattleSection::Participants);++i){const auto p=entry.participant(i);battlers_.push_back({p,p.hp,p.pp,false});}
 if(battlers_.size()!=2)return fail("Round execution currently requires the reviewed two-participant encounter");
 if(session){auto&p=battlers_[binding.player_participant];if(!apply_session_stats(p.source,*session))return fail("Invalid carried battle session");p.target_hp=p.source.hp;p.target_pp=p.source.pp;}
 queue_.reserve(battlers_.size());selected_target_=binding.enemy_participant;phase_=BattleRoundPhase::Commands;
 return cue(BattleRoundCueKind::ShowMenu,binding.player_participant);
}
bool BattleRound::alive(uint32_t index)const{
 if(index>=battlers_.size())return false;
 return index==content_.binding().player_participant?host_->current_hp(index)>0:battlers_[index].target_hp>0;
}
void BattleRound::delay(Step step,double seconds){step_=step;remaining_=double(float(seconds));}
bool BattleRound::tick_delay(double delta){remaining_=double(float(remaining_)-float(delta));return remaining_<0;}
bool BattleRound::request_menu(uint32_t menu){
 if(phase_!=BattleRoundPhase::Commands)return phase_!=BattleRoundPhase::Error;
 const auto b=content_.binding();
 if(menu==b.basic_menu){phase_=BattleRoundPhase::Targeting;selected_target_=b.enemy_participant;return cue(BattleRoundCueKind::TargetOpen,b.player_participant,selected_target_,b.basic_skill);}
 if(menu==b.guard_menu){
  if(!queue_actions(b.guard_skill))return false;
  battlers_[b.player_participant].defending=true;
  return cue(BattleRoundCueKind::Guard,b.player_participant,b.player_participant,b.guard_skill);
 }
 if(menu==b.items_menu){phase_=BattleRoundPhase::Items;return true;}
 return fail("Unknown external battle menu binding");
}
bool BattleRound::return_from_items(){
 if(phase_!=BattleRoundPhase::Items)return fail("Items return outside item menu");
 phase_=BattleRoundPhase::Commands;menu_return_=true;return cue(BattleRoundCueKind::ShowMenu,content_.binding().player_participant);
}
bool BattleRound::target_input(int direction,bool confirm,bool cancel){
 if(direction<-1||direction>1)return fail("Invalid target direction");
 if(phase_==BattleRoundPhase::Unsupported&&cancel){phase_=BattleRoundPhase::Commands;error_="";menu_return_=true;return cue(BattleRoundCueKind::ShowMenu,content_.binding().player_participant);}
 if(phase_!=BattleRoundPhase::Targeting)return phase_!=BattleRoundPhase::Error;
 const auto b=content_.binding();
 if(cancel){phase_=BattleRoundPhase::Commands;menu_return_=true;if(!cue(BattleRoundCueKind::TargetClose,b.player_participant,selected_target_))return false;return cue(BattleRoundCueKind::ShowMenu,b.player_participant);}
 if(!confirm)return true; // Exactly one reviewed enemy; directional input keeps it selected.
 if(!alive(selected_target_))return fail("Selected battle target is not conscious");
 if(!cue(BattleRoundCueKind::TargetClose,b.player_participant,selected_target_))return false;
 return queue_actions(b.basic_skill);
}
bool BattleRound::queue_actions(uint32_t skill){
 const auto b=content_.binding();if(skill>=content_.count(RoundSection::Skills))return fail("Missing selected battle skill");
 queue_.clear();action_index_=0;
 const auto selected=content_.skill(skill);
 queue_.push_back({b.player_participant,selected.target_type==uint32_t(RoundTargetType::Self)?b.player_participant:selected_target_,skill});
 if(!cue(BattleRoundCueKind::PrepareAction,b.player_participant,queue_.front().target,skill))return false;
 double weight=0;for(uint32_t i=0;i<content_.count(RoundSection::EnemyChoices);++i)weight+=content_.enemy_choice(i).weight;
 if(!(weight>0)||!std::isfinite(weight))return fail("Empty weighted enemy skill pool");
 const double pick=random_->rand_range(0,weight);double current=0;uint32_t chosen=round_no_index;
 for(uint32_t i=0;i<content_.count(RoundSection::EnemyChoices);++i){const auto choice=content_.enemy_choice(i);current+=choice.weight;if(pick<=current){chosen=choice.skill;break;}}
 if(chosen==round_no_index)return fail("Source weighted skill choice fell outside pool");
 queue_.push_back({b.enemy_participant,round_no_index,chosen});
 std::stable_sort(queue_.begin(),queue_.end(),[this](const auto&a,const auto&b){const auto pa=content_.skill(a.skill).priority,pb=content_.skill(b.skill).priority;return pa!=pb?pa>pb:battlers_[a.actor].source.speed>battlers_[b.actor].source.speed;});
 phase_=BattleRoundPhase::Running;delay(Step::EnemyDelay,content_.rule(RoundRule::EnemyChoiceDelay));return true;
}
bool BattleRound::chance(double percentage){
 if(percentage<=0)return false;
 const double scale=content_.rule(RoundRule::PercentScale);
 const double effective=(1.0-std::pow(1.0-percentage/scale,1.0))*scale;
 const auto roll=random_->randi()%uint32_t(scale)+1;
 return double(roll)<=effective;
}
int32_t BattleRound::calculate_damage(const RoundActionState&action,bool adrenaline,bool smash){
 const auto skill=content_.skill(action.skill);const auto&user=battlers_[action.actor].source;const auto&target=battlers_[action.target];
 // Reviewed normal, non-PSI, no-status/no-affinity branch. Compiler rejects
 // unsupported modifiers rather than treating them as identity operations.
 double value=double(skill.power)+user.offense;
 value-=double(target.source.defense)/content_.rule(RoundRule::DefenseDivisor);
 if(adrenaline)value*=content_.rule(RoundRule::AdrenalineMultiplier);
 if(smash)value*=content_.rule(RoundRule::SmashMultiplier);
 value=std::max(value,double(content_.rule(RoundRule::MinimumDamage)));
 if(target.defending)value/=content_.rule(RoundRule::GuardDivisor);
 value=std::floor(value+random_->randf()*skill.variance-double(skill.variance)/2.0);
 if(value<=0)return 0;
 value=std::max(double(content_.rule(RoundRule::MinimumDamage)),std::round(value));
 if(value>std::numeric_limits<int32_t>::max()){fail("Battle damage exceeds supported integer range");return 0;}
 return int32_t(value);
}
bool BattleRound::start_action(){
 const auto b=content_.binding();
 // Source checks victory BEFORE incapacitated queued actor and emits no done
 // for that action. Preserve a real result boundary without changing world flags.
 if(!alive(b.enemy_participant)){phase_=BattleRoundPhase::VictoryPending;return true;}
 if(!alive(b.player_participant)){phase_=BattleRoundPhase::DefeatPending;return true;}
 if(action_index_>=queue_.size())return finish_round();
 if(!alive(queue_[action_index_].actor))return finish_action();
 step_=Step::CheckAbleness;return true;
}
bool BattleRound::execute_skill(){
 const auto&a=queue_[action_index_];const auto skill=content_.skill(a.skill);
 if(!alive(a.actor))return finish_action();
 if(chance(skill.fail_chance))return fail("Unreviewed nonzero skill failure path");
 miss_=chance(skill.miss_chance);
 if(skill.traits&uint32_t(RoundTrait::Guard)){
  if(content_.string(content_.text(skill.dialog).text).empty())return finish_action();
  delay(Step::ActionEndDelay,content_.rule(RoundRule::ActionEndDelay));return true;
 }
 if(skill.action_type==uint32_t(RoundActionType::Damage)){
  if(!cue(BattleRoundCueKind::Attack,a.actor,a.target,a.skill))return false;
  step_=Step::WaitImpact;return true;
 }
 // The reviewed float action is Other: no attack or damage, but source still
 // yields pre-hit completion and the per-target delay.
 step_=Step::PreHitIdle;return true;
}
bool BattleRound::apply_impact(){
 const auto&a=queue_[action_index_];const auto skill=content_.skill(a.skill);const auto b=content_.binding();
 RoundDecision decision{number_,a.actor,a.target,a.skill,0,miss_,false,0,0};
 if(miss_){if(!cue(BattleRoundCueKind::Miss,a.actor,a.target,a.skill))return false;}
 else{
  const bool adrenaline=a.actor==b.player_participant&&battlers_[a.actor].target_hp<=0;
  const int64_t guts=std::max<int64_t>(0,int64_t(skill.crit_chance)+battlers_[a.actor].source.guts);
  const double critical=std::max(guts>0?double(content_.rule(RoundRule::MinimumCritPercent)):0.0,double(guts)/content_.rule(RoundRule::GutsDivisor)*content_.rule(RoundRule::PercentScale));
  const bool smash=chance(critical);const int32_t damage=calculate_damage(a,adrenaline,smash);if(phase_==BattleRoundPhase::Error)return false;
  auto&target=battlers_[a.target];target.target_hp=int32_t(std::clamp<int64_t>(int64_t(target.target_hp)-damage,0,target.source.maxhp));
  decision.damage=damage;decision.smash=smash;
  if(!cue(BattleRoundCueKind::Hit,a.actor,a.target,a.skill,round_no_index,damage,smash,adrenaline))return false;
  if(a.target==b.enemy_participant&&target.target_hp==0){
   if(!cue(BattleRoundCueKind::Defeat,a.actor,a.target,a.skill))return false;
   if(content_.encounter().boss){step_=Step::BossDefeat;}
   else if(b.show_intro_outro){delay(Step::DefeatDialogueDelay,content_.rule(RoundRule::DefeatDialogDelay));}
   else step_=Step::DamageIdle;
  }else if(a.target==b.player_participant&&target.target_hp==0){
   if(!cue(BattleRoundCueKind::Dialogue,a.actor,a.target,a.skill,b.mortal_damage))return false;
   step_=Step::DefeatDialogue;
  }else step_=Step::DamageIdle;
 }
 if(miss_)delay(Step::TargetDelay,content_.rule(RoundRule::TargetEndDelay));
 decision.random_state=random_->state();decision.raw_draw_count=random_->raw_draw_count();decisions_.push_back(decision);return true;
}
bool BattleRound::finish_action(){
 const auto&a=queue_[action_index_];const auto b=content_.binding();
 if(a.actor==b.player_participant&&!battlers_[a.actor].defending)if(!cue(BattleRoundCueKind::HideParty,a.actor))return false;
 ++action_index_;
 if(action_index_>=queue_.size())return finish_round();
 return start_action();
}
bool BattleRound::finish_round(){
 const auto b=content_.binding();
 if(!cue(BattleRoundCueKind::HideParty,b.player_participant))return false;
 if(!alive(b.enemy_participant)){phase_=BattleRoundPhase::VictoryPending;return true;}
 if(!alive(b.player_participant)){phase_=BattleRoundPhase::DefeatPending;return true;}
 ++number_;
 for(uint32_t i=0;i<battlers_.size();++i)if(battlers_[i].defending){battlers_[i].defending=false;if(i==b.player_participant)if(!cue(BattleRoundCueKind::HideParty,i))return false;}
 delay(Step::RoundEndDelay,content_.rule(RoundRule::RoundEndDelay));return true;
}
bool BattleRound::idle_frame(double delta){
 if(!std::isfinite(delta)||delta<0||delta>1)return fail("Invalid battle round idle delta");
 if(phase_!=BattleRoundPhase::Running)return phase_!=BattleRoundPhase::Error;
 // Engine idle/signal continuations run before SceneTreeTimer processing.
 // A timer started here receives this frame's delta. A timer started from a
 // timeout below is beyond the timer-list sentinel and begins next frame.
 switch(step_){
 case Step::DeferredAction:if(!start_action())return false;break;
 case Step::CheckAbleness:{const auto&a=queue_[action_index_];if(!cue(BattleRoundCueKind::TurnStart,a.actor,a.target,a.skill))return false;delay(Step::StartDelay,content_.rule(RoundRule::ActionStartDelay));break;}
 case Step::EmptyDialogue:{
  auto&a=queue_[action_index_];const auto skill=content_.skill(a.skill);const auto b=content_.binding();
  if(skill.target_type==uint32_t(RoundTargetType::Self))a.target=a.actor;
  else if(a.target==round_no_index||!alive(a.target)){const uint32_t target=a.actor==b.player_participant?b.enemy_participant:b.player_participant;(void)random_->randi();a.target=target;}
  if(!alive(a.target))return finish_action();
  if(!content_.string(content_.text(skill.dialog).text).empty()){
   if(!cue(BattleRoundCueKind::Dialogue,a.actor,a.target,a.skill,skill.dialog))return false;
   step_=Step::Dialogue;
  }else if(!execute_skill())return false;
  break;
 }
 case Step::Dialogue:if(host_->ready(BattleRoundGate::DialogueDone,queue_[action_index_].actor))if(!execute_skill())return false;break;
 case Step::StartSkill:if(!execute_skill())return false;break;
 case Step::WaitImpact:if(host_->ready(BattleRoundGate::ApplyDamage,queue_[action_index_].actor))step_=Step::PreHitIdle;break;
 case Step::PreHitIdle:
  if(content_.skill(queue_[action_index_].skill).action_type==uint32_t(RoundActionType::Damage)){if(!apply_impact())return false;}
  else delay(Step::TargetDelay,content_.rule(RoundRule::TargetEndDelay));
  break;
 case Step::DamageIdle:delay(Step::TargetDelay,content_.rule(RoundRule::TargetEndDelay));break;
 case Step::BossDefeat:if(host_->ready(BattleRoundGate::BossDefeatDone,content_.binding().enemy_participant))phase_=BattleRoundPhase::VictoryPending;break;
 case Step::DefeatDialogue:if(host_->ready(BattleRoundGate::DialogueDone,queue_[action_index_].actor))delay(Step::TargetDelay,content_.rule(RoundRule::TargetEndDelay));break;
 default:break;
 }
 if(phase_!=BattleRoundPhase::Running)return phase_!=BattleRoundPhase::Error;
 switch(step_){
 case Step::EnemyDelay:if(tick_delay(delta))return start_action();break;
 case Step::StartDelay:if(tick_delay(delta))step_=Step::EmptyDialogue;break;
 case Step::TargetDelay:if(tick_delay(delta)){const auto skill=content_.skill(queue_[action_index_].skill);if(content_.string(content_.text(skill.dialog).text).empty())return finish_action();delay(Step::ActionEndDelay,content_.rule(RoundRule::ActionEndDelay));}break;
 case Step::ActionEndDelay:if(tick_delay(delta))return finish_action();break;
 case Step::RoundEndDelay:if(tick_delay(delta)){phase_=BattleRoundPhase::Commands;queue_.clear();menu_return_=true;return cue(BattleRoundCueKind::ShowMenu,content_.binding().player_participant);}break;
 case Step::DefeatDialogueDelay:if(tick_delay(delta)){const auto&a=queue_[action_index_];if(!cue(BattleRoundCueKind::Dialogue,a.actor,a.target,a.skill,content_.binding().enemy_outro))return false;step_=Step::DefeatDialogue;}break;
 default:break;
 }
 return true;
}
}
