#include "encore/house_presentation.hpp"
#include "encore/battle_action_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
std::string pose(const HousePresentation&p){std::ostringstream out;const auto d=p.dialogue_pose();out<<p.visible_characters()<<':'<<p.dialogue_finished()<<':'<<p.dialogue_stopped()<<':'<<d.cursor_visible;for(const auto&l:d.lines){out<<':'<<l.text<<':'<<l.y<<':'<<l.bullet;for(auto color:l.colors)out<<':'<<color;}return out.str();}
void same_audio(HousePresentation&a,HousePresentation&b){auto x=a.take_audio_events(),y=b.take_audio_events();check(x.size()==y.size(),"matching source voice count");for(size_t i=0;i<x.size();++i)check(x[i].kind==y[i].kind&&x[i].voice==y[i].voice&&x[i].pitch==y[i].pitch,"matching source voice/RNG pitch bytes");}
struct Reference {double elapsed=0,multiplier=1,seconds=0,remaining=0;uint32_t visible=0;bool finished=false,done=false;void frame(double dt,size_t count,double auto_delay){if(finished||done)return;elapsed+=dt;const auto period=seconds/multiplier;while(elapsed>period&&!finished){++visible;if(visible>count){finished=true;remaining=auto_delay;elapsed=0;}elapsed-=period;}}};
}
int main(int argc,char**argv){
 check(argc==5,"fixture directory, round, font and default source oracle supplied");std::string error;BattleRoundData round;BattleData font;HouseData base;
 if(!round.load_file(argv[2],error))check(false,error.c_str());if(!font.load_file(argv[3],error))check(false,error.c_str());if(!base.load_file((std::string(argv[1])+"/default.enchouse").c_str(),error))check(false,error.c_str());const auto v=round.view();const auto h=base.view();
 const double default_speed=v.rule(RoundRule::TextSecondsPerChar);std::ifstream cases(std::string(argv[1])+"/cases.tsv");check(bool(cases),"source-derived speed cases");double speed;unsigned first,cells,index=0;
 for(double invalid:{0.,-1.,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){BattleTextPacer p;check(!p.set_text_speed(invalid),"pacer rejects invalid choice");}
 HousePresentation unopened;BattleActionPresentation no_battle;check(!unopened.set_text_speed(default_speed)&&!no_battle.set_text_speed(default_speed),"presentation setter needs checked content");
 while(cases>>first>>speed>>cells){
  HouseData reference_data;if(!reference_data.load_file((std::string(argv[1])+"/speed-"+std::to_string(index)+".enchouse").c_str(),error))check(false,error.c_str());SourceRandom ra(123),rb(123);HousePresentation actual,reference;
  check(actual.begin(h,font.view(),ra)&&reference.begin(reference_data.view(),font.view(),rb),"house presenters begin");check(actual.text_speed()==h.interaction().text_seconds,"house uses pack default");check(actual.set_text_speed(speed)&&actual.text_speed()==speed,"selected house timing applied");
  for(double invalid:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})check(!actual.set_text_speed(invalid)&&actual.text_speed()==speed,"house invalid choice does not alter timing");
  check(actual.begin_dialogue(first,2,"Ninten")&&reference.begin_dialogue(first,2,"Ninten"),"source delay/WAIT phrase begins");check(actual.physics_frame(speed,{})&&reference.physics_frame(speed,{})&&actual.visible_characters()==0,"selected house strict greater boundary");
  const auto open=h.clip(h.clip_for(HouseClipRole::DialogueOpen)).duration;check(actual.idle_frame(open)&&reference.idle_frame(open),"open source animation gate");same_audio(actual,reference);
  bool resumed=false;for(unsigned tick=0;tick<1000&&!actual.dialogue_finished();++tick){
   if(actual.dialogue_stopped()){check(actual.visible_characters()==cells+3,"delay count uses current speed before WAIT");actual.input(true,false);reference.input(true,false);resumed=true;}
   if(tick==2){actual.input(true,false);reference.input(true,false);}if(tick==5){actual.input(false,true);reference.input(false,true);}
   check(actual.physics_frame(1./60,{})&&reference.physics_frame(1./60,{}),"source world timing frame");check(pose(actual)==pose(reference),"override frame/visible bytes equal checked speed pack");same_audio(actual,reference);check(ra.state()==rb.state()&&ra.raw_draw_count()==rb.raw_draw_count(),"override keeps source RNG stream");
  }
  check(resumed&&actual.dialogue_finished()&&actual.visible_characters()==cells+5,"source delay count and final overrun");
  actual.clear_story_text();check(actual.present_story_dialogue(first,2,"Ninten")&&actual.physics_frame(speed,{})&&actual.visible_characters()==0,"new world phrase resets elapsed/acceleration and retains selected speed");
  check(actual.physics_frame(speed/2,{}),"world accumulates selected period");const auto before=actual.visible_characters();check(actual.set_text_speed(speed/4)&&actual.physics_frame(0,{})&&actual.visible_characters()>before,"world setter preserves pending clock");
  check(actual.begin(h,font.view(),ra)&&actual.text_speed()==h.interaction().text_seconds,"new house lifetime restores checked pack default");
  check(actual.set_text_speed(std::numeric_limits<double>::denorm_min())&&!actual.begin_dialogue(first,2,"Ninten"),"huge dynamic delay expansion fails closed");

  BattleTextPacer pacer;check(pacer.set_text_speed(speed)&&pacer.begin(v,"ABCDE"),"battle speed can precede first phrase");Reference model;model.seconds=speed;
  for(unsigned tick=0;tick<100&&!pacer.finished();++tick){if(tick==2){pacer.input(true,false);model.multiplier=v.rule(RoundRule::TextAcceptMultiplier);}if(tick==4){pacer.input(false,true);model.multiplier=v.rule(RoundRule::TextCancelMultiplier);}model.frame(1./60,pacer.text().size(),v.rule(RoundRule::TextAutoAdvanceSeconds));check(pacer.physics_frame(1./60),"battle selected source timing frame");check(pacer.visible_characters()==model.visible&&pacer.finished()==model.finished,"battle source model timing/acceleration unchanged");}
  check(pacer.finished()&&!pacer.done(),"battle finishes without acknowledging on speedup");check(pacer.idle_frame(v.rule(RoundRule::TextAutoAdvanceSeconds))&&!pacer.done(),"auto timer strict zero");check(pacer.idle_frame(speed)&&pacer.done(),"auto timer negative done");
  check(pacer.begin(v,"AB",false)&&pacer.text_speed()==speed&&pacer.physics_frame(speed)&&pacer.visible_characters()==0,"battle phrase reset retains speed and resets elapsed/input");check(pacer.physics_frame(speed/2)&&pacer.visible_characters()==1,"battle selected one character");check(pacer.set_text_speed(speed/2)&&pacer.physics_frame(speed/4)&&pacer.visible_characters()==2,"battle setter preserves pending elapsed");pacer.input(false,true);check(pacer.set_text_speed(speed)&&pacer.physics_frame(speed)&&pacer.finished()&&!pacer.done(),"battle setter preserves B acceleration/manual gate");pacer.idle_frame(100);check(!pacer.done(),"manual outcome has no auto timer");pacer.input(true,false);check(pacer.done(),"manual outcome later acknowledgment");
  check(pacer.begin(v,"")&&pacer.idle_frame(0)&&pacer.done()&&pacer.text_speed()==speed,"empty phrase deferred and speed retained");
  SourceRandom random(456);BattleActionPresentation action;check(action.begin(v,font.view(),random)&&action.text_speed()==default_speed,"action checked default");check(action.set_text_speed(speed),"action session setter");BattleRoundCue cue;cue.kind=BattleRoundCueKind::Dialogue;cue.text=v.skill(v.binding().basic_skill).dialog;check(action.emit(cue,random)&&action.physics_frame(speed)&&action.dialogue().visible_characters()==0,"actual battle cue uses selected speed");check(action.physics_frame(speed)&&action.dialogue().visible_characters()==1,"actual battle cue next boundary");action.input(false,true);check(action.physics_frame(speed)&&action.dialogue().finished(),"actual battle cue accelerates");
  check(action.begin_victory()&&action.idle_frame(v.rule(RoundRule::VictoryBannerSeconds)+speed)&&action.victory_done(),"victory source gate");check(action.begin_outcome_text(v.victory().exp_text)&&action.physics_frame(speed)&&action.dialogue().visible_characters()==0,"actual outcome text retains selected speed and resets clock");
  check(action.begin(v,font.view(),random)&&action.text_speed()==default_speed,"new encounter lifetime restores checked default");++index;
 }
 check(index>1,"all source choices exercised");
 // Replay existing official Godot baseline, without re-running a broad suite.
 BattleTextPacer baseline;check(baseline.begin(v,"Ninten attacks!")&&baseline.text_speed()==default_speed,"plain pacer checked default");std::ifstream oracle(argv[4]);check(bool(oracle),"official default oracle exists");int frame,cur,sub,current,ones,tens,hundreds,scroll,visible,finished;unsigned rows=0;while(oracle>>frame>>cur>>sub>>current>>ones>>tens>>hundreds>>scroll>>visible>>finished){check(baseline.physics_frame(1./60)&&baseline.visible_characters()==unsigned(visible)&&baseline.finished()==bool(finished),"default timing unchanged against official Godot trace");++rows;}check(rows==100,"all recorded default frames replayed");
 std::cout<<"settings-text-speed: "<<checks<<" checks passed; "<<index<<" source choices, "<<rows<<" recorded Godot baseline frames\n";
}
