#include "encore/battle_outcome.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
#define check(value,message) do { ++checks; if(!(value)){std::cerr<<"check "<<checks<<" line "<<__LINE__<<": "<<(message)<<'\n';std::exit(1);} } while(false)
struct RecordingHost final:BattleRoundHost {
 WorldBattleHost host;
 unsigned frame=0,defeat=0;
 std::vector<std::array<int64_t,11>> cues;
 bool emit(const BattleRoundCue&cue,SourceRandom&random)override {
  cues.push_back({int64_t(frame),int64_t(cue.kind),cue.actor,cue.target,cue.skill,cue.text,cue.amount,cue.hp_after,cue.smash,cue.adrenaline,int64_t(random.raw_draw_count())});
  if(cue.kind==BattleRoundCueKind::Defeat)defeat=frame;
  return host.emit(cue,random);
 }
 bool ready(BattleRoundGate gate,uint32_t actor)const override{return host.ready(gate,actor);}
 int32_t current_hp(uint32_t actor)const override{return host.current_hp(actor);}
};
struct Trace {
 unsigned defeat=0,flash=0,hidden=0,killed=0,done=0,prompts=0;
 uint64_t random_state=0,draws=0;
 std::vector<std::array<int64_t,11>> cues;
 std::vector<RoundDecision> decisions;
 std::vector<std::array<double,23>> poses;
 std::vector<std::array<uint64_t,2>> random;
 BattleRewardState state;
 std::vector<BattleRewardEvent> rewards;
 Vec2 actor_anchor{},flash_anchor{};
};
void bases(BattleActionPresentation&p,RoundView r,BattleView e){
 for(uint32_t i=0;i<e.count(BattleSection::Layouts);++i){
  auto l=e.layout(i);BattlePose q{l.rect,l.color,l.frame,true};
  if(l.flags&2){q.rect.x-=q.rect.z/2;q.rect.y-=q.rect.w/2;}
  if(l.role==uint32_t(BattleRole::PartySprite)){q.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(r.binding().player_participant,q),p.error());}
  if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(r.binding().enemy_participant,q),p.error());
  if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(q.rect),p.error());
 }
}
std::array<double,23> pose_trace(const BattleActionPose&p){
 // Anchor is the deliberately changed input; compare the other sampled output.
 return {double(p.media),double(p.resource),double(p.role),double(p.frame),p.rect.x,p.rect.y,p.rect.z,p.rect.w,p.scale.x,p.scale.y,p.offset.x,p.offset.y,p.rotation,p.modulate.x,p.modulate.y,p.modulate.z,p.modulate.w,p.flash_modifier,p.glow_modifier,p.radius,double(p.visible),double(p.centered),p.color.w};
}
Trace run(const std::string&fixture,const char*entry_path,const char*room_path,const std::vector<std::string>&skills){
 BattleRoundData data;BattleData entry;RoomData room_data;std::string error;
 check(data.load_file(fixture.c_str(),error),error);
 check(entry.load_file(entry_path,error)&&room_data.load_file(room_path,error),error);
 auto r=data.view();auto e=entry.view();auto room=room_data.view();auto binding=r.binding();auto policy=r.encounter();auto victory=r.victory();
 check(policy.boss&&policy.keep_actor&&binding.battle_id==e.metadata().room_battle_id,"Checked real retained boss encounter binding");
 const auto player=e.participant(binding.player_participant);
 BattleSessionStats session;
 session.experience=victory.initial_exp;session.level=player.level;session.bank=victory.initial_bank;session.cash=victory.initial_cash;session.earned_cash=victory.initial_earned_cash;
 session.hp=player.hp;session.pp=player.pp;session.maxhp=player.maxhp;session.maxpp=player.maxpp;session.offense=player.offense;session.defense=player.defense;session.speed=player.speed;session.iq=player.iq;session.guts=player.guts;session.learned_skills=skills;
 SourceRandom random(0);BattleActionPresentation p;check(p.begin(r,e,random,&session),p.error());bases(p,r,e);
 OpeningWorld world;check(world.initialize(room),world.error());world.attach_random(random);
 const double dt=double(float(1./60));
 // Enter through the existing original Doll story program, including its
 // retained overworld actor and source cutscene request; no fabricated cue.
 check(world.warp_same_scene({64,128},{0,-1})&&world.begin_house_program(1),world.error());
 unsigned text_frames=0;
 for(unsigned n=0;n<3000&&world.stage()!=OpeningStage::BattleRequested;++n){
  if(world.pending_dialogue_id()!=kRoomNoIndex&&++text_frames>=180){check(world.finish_story_dialogue(),world.error());text_frames=0;}
  check(world.advance({})&&world.idle_frame(dt),world.error());
 }
 check(world.stage()==OpeningStage::BattleRequested&&world.battle_request().keep_actor_after_battle,"Original source cutscene requests the retained Doll");
 check(world.accept_battle_entry()&&world.idle_frame(dt),world.error());
 RecordingHost host;host.host.bind(p,world,r);BattleRound round;check(round.begin(r,e,random,host,&session),round.error());
 Trace result;
 auto tick=[&]{
  ++host.frame;const auto delta=double(float(dt*p.advance_real_time(dt)));
  check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());
  if(host.defeat){
   auto actor=p.actor_pose(binding.enemy_participant);
   if(actor.media==r.presentation(RoundPresentationSlot::EnemyDefeat)){
    check(actor.anchor.x==r.media(actor.media).anchor.x&&actor.anchor.y==r.media(actor.media).anchor.y,"Actual enemy defeat pose consumes checked anchor");result.actor_anchor=actor.anchor;
   }
   result.poses.push_back(pose_trace(actor));
   if(!actor.visible&&!result.hidden)result.hidden=host.frame;
   for(const auto&overlay:p.overlays())if(overlay.role==uint32_t(RoundMediaRole::BossFlash)){
    if(!result.flash)result.flash=host.frame;
    check(overlay.media==policy.boss_flash_media,"Real callback starts the bound flash media");
    check(overlay.anchor.x==r.media(overlay.media).anchor.x&&overlay.anchor.y==r.media(overlay.media).anchor.y,"Actual flash overlay consumes checked recipe anchor");
    result.flash_anchor=overlay.anchor;result.poses.push_back(pose_trace(overlay));
   }
   if(!p.enemies_visible()&&!result.killed)result.killed=host.frame;
   if(p.ready(BattleRoundGate::BossDefeatDone,binding.enemy_participant)&&!result.done)result.done=host.frame;
  }
  result.random.push_back({random.state(),random.raw_draw_count()});
 };
 // Guard and then real Bash actions exercise the actual command scheduler,
 // source weighted enemy AI and shared combat/presentation RNG.
 check(round.request_menu(binding.guard_menu),round.error());
 for(unsigned n=0;n<4000&&round.phase()==BattleRoundPhase::Running;++n)tick();
 check(round.phase()==BattleRoundPhase::Commands,"Original guard response returns commands");
 for(unsigned turn=0;turn<12&&round.phase()!=BattleRoundPhase::VictoryPending;++turn){
  check(round.phase()==BattleRoundPhase::Commands,"Original party remains conscious");
  check(round.request_menu(binding.basic_menu)&&round.target_input(0,true),round.error());
  for(unsigned n=0;n<4000&&round.phase()==BattleRoundPhase::Running;++n)tick();
 }
 result.defeat=host.defeat;
 check(round.phase()==BattleRoundPhase::VictoryPending&&result.defeat&&result.flash&&result.hidden&&result.killed&&result.done,"Actual combat triggers every boss callback through victory");
 check(result.defeat<result.flash&&result.flash<result.killed&&result.killed<result.done,"Source flash start, kill and done gates execute in order");
 check(world.instance_visible(world.battle_request().actor_index),"Original retained overworld actor survives defeat");
 result.cues=host.cues;result.decisions=round.decisions();
 check(std::any_of(result.cues.begin(),result.cues.end(),[](const auto&c){return c[1]==int64_t(BattleRoundCueKind::Defeat);}),"Defeat came from BattleRound, not a synthetic presentation cue");
 const auto before_rewards=random.raw_draw_count();const auto reward_state=random.state();
 BattleOutcome outcome;check(outcome.initialize(r,room,&session)&&outcome.begin(round,p,world),outcome.error());
 for(unsigned n=0;n<6000&&outcome.phase()!=BattleOutcomePhase::Returning;++n){
  check(p.physics_frame(dt)&&p.idle_frame(dt),p.error());
  check(world.advance({})&&world.idle_frame(dt),world.error());
  if((outcome.phase()==BattleOutcomePhase::ExperienceDialogue||outcome.phase()==BattleOutcomePhase::LevelDialogue)&&p.dialogue().finished()){
   ++result.prompts;p.input(true,false);
  }
  check(outcome.idle_frame(),outcome.error());
 }
 check(outcome.phase()==BattleOutcomePhase::Returning,"Source acknowledged progression reaches return");
 result.state=outcome.state();result.rewards=outcome.events();result.random_state=random.state();result.draws=random.raw_draw_count();
 check(result.draws==before_rewards&&result.random_state==reward_state,"Progression consumes no RNG");
 check(result.state.experience==victory.initial_exp+victory.reward_exp&&result.state.bank==victory.initial_bank+victory.reward_cash,"Original source EXP and currency applied");
 check(result.state.level==policy.promoted_level&&result.state.learned_skills.size()==skills.size()+1&&result.state.learned_skills.back()==r.string(policy.learned_skill),"Original promotion retains stable learned skill identity");
 return result;
}
bool same_state(const BattleSessionStats&a,const BattleSessionStats&b){
 return a.experience==b.experience&&a.level==b.level&&a.bank==b.bank&&a.cash==b.cash&&a.earned_cash==b.earned_cash&&a.hp==b.hp&&a.pp==b.pp&&a.maxhp==b.maxhp&&a.maxpp==b.maxpp&&a.offense==b.offense&&a.defense==b.defense&&a.speed==b.speed&&a.iq==b.iq&&a.guts==b.guts&&a.learned_skills==b.learned_skills;
}
}
int main(int argc,char**argv){
 check(argc==4,"Fixture directory, Doll entry and original room paths required");
 const std::string directory=argv[1];auto path=[&](const char*name){return directory+"/"+name;};
 BattleRoundData checked;std::string error;check(checked.load_file(path("baseline.encround").c_str(),error),error);
 const auto baseline_anchor=checked.view().media(checked.view().presentation(RoundPresentationSlot::EnemyDefeat)).anchor;
 for(const auto*name:{"bad-crc.encround","bad-anchor.encround","truncated.encround"}){
  check(!checked.load_file(path(name).c_str(),error),"Corrupt boss resource fails checked loading");
  auto kept=checked.view();auto anchor=kept.media(kept.presentation(RoundPresentationSlot::EnemyDefeat)).anchor;
  check(kept.valid()&&anchor.x==baseline_anchor.x&&anchor.y==baseline_anchor.y,"Rejected load preserves prior checked boss owner");
 }
 std::ifstream input(path("learned-skills.txt"));check(bool(input),"Source initial learned skills fixture readable");std::vector<std::string>skills;std::string skill;while(input>>skill)skills.push_back(skill);check(!skills.empty(),"Source initial skills present");
 const auto a=run(path("baseline.encround"),argv[2],argv[3],skills);
 const auto b=run(path("anchor.encround"),argv[2],argv[3],skills);
 check(a.actor_anchor.x!=b.actor_anchor.x&&b.actor_anchor.x==.25f&&b.actor_anchor.y==.5f,"Same executable observes recipe mutation in real defeat actor");
 check(a.flash_anchor.x!=b.flash_anchor.x&&b.flash_anchor.x==.125f&&b.flash_anchor.y==.25f,"Same executable observes recipe mutation in actual flash overlay");
 check(a.defeat==b.defeat&&a.flash==b.flash&&a.hidden==b.hidden&&a.killed==b.killed&&a.done==b.done,"Source boss callback frames unchanged");
 check(a.poses==b.poses,"All captured non-anchor boss poses remain unchanged");
 check(a.random==b.random&&a.random_state==b.random_state&&a.draws==b.draws,"Shared RNG state and raw draws unchanged at every battle frame");
 check(a.cues==b.cues,"Actual round source cues unchanged");
 check(a.decisions.size()==b.decisions.size()&&!a.decisions.empty(),"Real combat decision count unchanged");
 for(size_t i=0;i<a.decisions.size();++i){const auto&x=a.decisions[i];const auto&y=b.decisions[i];check(x.round==y.round&&x.actor==y.actor&&x.target==y.target&&x.skill==y.skill&&x.damage==y.damage&&x.miss==y.miss&&x.smash==y.smash&&x.random_state==y.random_state&&x.raw_draw_count==y.raw_draw_count,"Source combat decision and RNG receipt unchanged");}
 check(same_state(a.state,b.state)&&a.rewards==b.rewards&&a.prompts==b.prompts,"Acknowledged source progression, stable identity and reward events unchanged");
 std::cout<<checks<<" boss consumer checks: real Doll combat, checked mutated anchors, callback/RNG/progression invariance\n";
}
