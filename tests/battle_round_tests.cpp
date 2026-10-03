#include "encore/battle_round.hpp"
#ifdef ENCORE_REAL_PRESENTATION
#include "encore/battle_action_presentation.hpp"
#endif
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using namespace encore::upstream;
namespace {
void check(bool value,const char*why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
// Same explicitly limited adapters as the native mechanics oracle: dialogue
// and attack signals are ready next idle; HP is instantaneous. Only values,
// action order, terminal boundary and RNG continuation are compared here.
#ifndef ENCORE_REAL_PRESENTATION
struct Host final:BattleRoundHost {
 RoundView content;int32_t maximum_hp=0;int32_t hp[2]{};std::vector<BattleRoundCue>events;
 bool emit(const BattleRoundCue&cue,SourceRandom&random)override{
  events.push_back(cue);
  if(cue.kind==BattleRoundCueKind::Hit){
   // Extracted FlyingNumber.run consumes these two global calls. HitEffect's
   // unused script is not bound in Battle.tscn and consumes no random draws.
   (void)random.rand_range(32,64);(void)random.randi();hp[cue.target]=cue.hp_after;
   if(cue.target==content.binding().player_participant&&cue.amount>maximum_hp/content.parameter(RoundParameter::PartyHit).x)(void)random.rand_range(1,content.parameter(RoundParameter::PartyHit).w);
  }
  return true;
 }
 bool ready(BattleRoundGate,uint32_t)const override{return true;}
 int32_t current_hp(uint32_t index)const override{return hp[index];}
};
#else
using Host=BattleActionPresentation;
#endif
}
int main(int argc,char**argv){
 check(argc==4,"round pack, battle pack and seed required");
 std::string error;BattleRoundData round_data;BattleData battle_data;
 check(round_data.load_file(argv[1],error),error.c_str());check(battle_data.load_file(argv[2],error),error.c_str());
 const auto content=round_data.view();const auto entry=battle_data.view();const auto binding=content.binding();
 SourceRandom random(std::strtoull(argv[3],nullptr,10));Host host;
#ifndef ENCORE_REAL_PRESENTATION
 host.content=content;host.maximum_hp=entry.participant(binding.player_participant).maxhp;
 for(unsigned i=0;i<2;++i)host.hp[i]=entry.participant(i).hp;
#else
 check(host.begin(content,entry,random),host.error());
 for(uint32_t i=0;i<entry.count(BattleSection::Layouts);++i){const auto layout=entry.layout(i);BattlePose p{layout.rect,layout.color,layout.frame,true};
  if(layout.flags&2){p.rect.x-=p.rect.z/2;p.rect.y-=p.rect.w/2;}
  if(layout.role==uint32_t(BattleRole::PartySprite)){p.rect.y-=content.parameter(RoundParameter::PartyShown).x;check(host.set_actor_base(binding.player_participant,p),"party base");}
  if(layout.role==uint32_t(BattleRole::EnemySprite))check(host.set_actor_base(binding.enemy_participant,p),"enemy base");
  if(layout.role==uint32_t(BattleRole::PartyPlate)&&layout.kind==uint32_t(BattleDrawKind::NinePatch))check(host.set_plate_base(p.rect),"plate base");
 }
#endif
 BattleRound round;check(round.begin(content,entry,random,host),round.error());
 check(round.request_menu(binding.basic_menu),round.error());check(round.phase()==BattleRoundPhase::Targeting,"basic opens actual target selection");
 check(round.target_input(0,false,true),round.error());check(round.phase()==BattleRoundPhase::Commands&&round.take_menu_return(),"target cancel restores commands without consuming RNG");
 check(random.raw_draw_count()==0,"menu navigation consumes no battle RNG");
 check(round.request_menu(binding.basic_menu)&&round.target_input(0,true),round.error());
 unsigned frames=0;while(round.phase()==BattleRoundPhase::Running&&frames++<2000){
  double dt=double(float(1.0/60));
#ifdef ENCORE_REAL_PRESENTATION
  dt*=host.advance_real_time(dt);check(host.physics_frame(dt)&&host.idle_frame(dt),host.error());
#endif
  check(round.idle_frame(dt),round.error());
 }
 check(frames<2000,"round reaches explicit boundary");
 const auto count=random.raw_draw_count();const auto next=random.randi();
 std::cout<<"{\"boundary\":\""<<(round.phase()==BattleRoundPhase::Commands?"next_menu":round.phase()==BattleRoundPhase::VictoryPending?"win":"unexpected")<<"\",\"player_hp\":"<<round.battler(binding.player_participant).target_hp<<",\"enemy_hp\":"<<round.battler(binding.enemy_participant).target_hp<<",\"turn\":"<<round.number()<<",\"raw_draw_count\":"<<count<<",\"next_randi\":\""<<next<<"\",\"decisions\":[";
 bool first=true;for(const auto&d:round.decisions()){if(!first)std::cout<<',';first=false;std::cout<<"{\"actor\":"<<d.actor<<",\"target\":"<<d.target<<",\"damage\":"<<d.damage<<",\"smash\":"<<(d.smash?"true":"false")<<'}';}std::cout<<"]}\n";
}
