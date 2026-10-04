#include "encore/battle_action_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
void check(bool value,const char*message){if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
void start(BattleActionPresentation&p,RoundView round,BattleView entry,SourceRandom&random){
 check(p.begin(round,entry,random),p.error());check(p.set_plate_base({128,132,65,49}),"inject original plate base");
 check(!p.begin_return(),"return remains gated before victory");
 check(p.begin_victory(),p.error());check(p.idle_frame(round.rule(RoundRule::VictoryBannerSeconds)+.001),p.error());
 check(p.victory_done(),"source victory timer releases");
 check(p.begin_outcome_text(round.victory().exp_text),p.error());check(!p.begin_return(),"outcome acknowledgment gate remains active");
 p.input(true,false);check(p.physics_frame(10),p.error());check(p.dialogue().finished(),"original experience message printed");
 p.input(true,false);check(p.dialogue().done(),"original outcome acknowledged");
 check(p.idle_frame(1.0/60),p.error());check(p.begin_return(),p.error());
 check(!p.begin_party_return({220,130},{80,60}),"jump cannot precede source callback");
}
void append(std::vector<RoundEventKind>&target,BattleActionPresentation&p){const auto events=p.take_return_events();target.insert(target.end(),events.begin(),events.end());}
}
int main(int argc,char**argv){
 check(argc==4,"baseline return round, changed round and original entry supplied");
 BattleRoundData first,second;BattleData entry;std::string error;
 check(first.load_file(argv[1],error),error.c_str());check(second.load_file(argv[2],error),error.c_str());check(entry.load_file(argv[3],error),error.c_str());
 const auto a=first.view(),b=second.view();SourceRandom random_a(13),random_b(13);BattleActionPresentation original,changed;
 start(original,a,entry.view(),random_a);start(changed,b,entry.view(),random_b);
 std::vector<RoundEventKind>events_a,events_b;bool jumped=false;
 for(unsigned tick=0;tick<200&&!jumped;++tick){
  check(original.idle_frame(1.0/60)&&changed.idle_frame(1.0/60),"paired real return advances");
  const auto left=original.take_return_events(),right=changed.take_return_events();check(left==right,"external projection binding preserves source callback ordering");
  events_a.insert(events_a.end(),left.begin(),left.end());events_b.insert(events_b.end(),right.begin(),right.end());
  for(auto event:left)if(event==RoundEventKind::JumpPartyToWorld){
   check(original.begin_party_return({220,130},{80,60}),original.error());check(changed.begin_party_return({220,130},{80,60}),changed.error());jumped=true;
  }
 }
 check(jumped,"original transitionOut requests projected world jump");
 check(!original.battle_background_visible()&&!changed.battle_background_visible()&&!original.enemies_visible()&&!changed.enemies_visible(),"source background and enemy callbacks executed");
 check(original.return_overlays().size()==2&&changed.return_overlays().size()==2,"original split top/bottom lanes consumed");
 check(original.idle_frame(.55)&&changed.idle_frame(.55),"source world jump reaches x destination before landing");
 const auto pose_a=original.return_party_pose(),pose_b=changed.return_party_pose();
 check(pose_a.visible&&pose_b.visible,"world jump still displayed before source landing");
 check(std::abs((pose_b.rect.x-pose_a.rect.x)-1)<1e-5,"same runtime executable consumes changed external world destination binding");
 check(pose_a.rect.y==pose_b.rect.y&&pose_a.frame==pose_b.frame&&pose_a.scale.x==pose_b.scale.x&&pose_a.scale.y==pose_b.scale.y,"source jump y, sprite frame and scale preserved");
 append(events_a,original);append(events_b,changed);
 for(unsigned tick=0;tick<200&&!original.return_done();++tick){check(original.idle_frame(1.0/60)&&changed.idle_frame(1.0/60),"source return completes");append(events_a,original);append(events_b,changed);}
 check(original.return_done()&&changed.return_done(),"original full transition timeline completes");check(events_a==events_b,"all original return and landing events preserved");
 const std::vector<RoundEventKind>expected{RoundEventKind::TurnPartyToWorld,RoundEventKind::HideBattleBackground,RoundEventKind::HideEnemies,RoundEventKind::JumpPartyToWorld,RoundEventKind::PartyReturnLanded,RoundEventKind::RotatePartyOriginal};
 check(events_a==expected,"reviewed source callbacks and real landing event delivered exactly once");
 check(random_a.state()==random_b.state(),"projection change preserves gameplay RNG");
 std::cout<<"round return recipe actual victory, acknowledgment, callbacks and world projection differential passed\n";
}
