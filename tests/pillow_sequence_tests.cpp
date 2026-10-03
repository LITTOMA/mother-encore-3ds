#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/battle_outcome.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace encore::upstream;
namespace {
unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::cerr<<"Pillow sequence line "<<__LINE__<<": "<<#v<<"\n";std::exit(1);}}while(false)
constexpr double dt=double(float(1./60));
struct Fixture {
 RoomData room;HouseData house;BattleData font,entry;BattleRoundData round;DialogueChoicesData choice_data;
 SourceRandom random{34};OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;DialogueChoices choices;
 unsigned tick=0;
 explicit Fixture(const std::string&root){
  std::string error;
  CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));
  CHECK(font.load_file((root+"/opening.encbattle").c_str(),error));CHECK(entry.load_file((root+"/pillow-entry.encbattle").c_str(),error));
  CHECK(round.load_file((root+"/pillow-entry.encround").c_str(),error));CHECK(choice_data.load_file((root+"/opening.encchoices").c_str(),error));
  CHECK(world.initialize(room.view(),{400,240}));world.attach_random(random);
  // Explicit generated post-Lamp harness. It does not claim a full NewGame run.
  CHECK(world.set_story_flag("poltergeist",true,true));CHECK(world.warp_same_scene({176,394},{0,-1}));
  CHECK(presentation.begin(house.view(),font.view(),random));CHECK(runtime.initialize(house.view(),world,presentation));runtime.bind_choices(choice_data,choices);
 }
 void step(WalkInput input={},bool accept=false,bool cancel=false){
  ++tick;
  auto fail=[&](bool good,const char*error){if(!good)std::cerr<<"tick "<<tick<<" phrase "<<world.phrase()<<" world "<<unsigned(world.stage())<<" house "<<unsigned(runtime.phase())<<" pos "<<world.player().position.x<<","<<world.player().position.y<<" "<<error<<"\n";CHECK(good);};
  fail(runtime.before_physics(input),runtime.error());fail(presentation.physics_frame(dt,world.player().position),presentation.error());
  fail(world.advance(input),world.error());fail(runtime.after_physics(),runtime.error());fail(world.idle_frame(dt),world.error());
  fail(presentation.idle_frame(dt),presentation.error());fail(runtime.idle_frame(dt,accept,cancel),runtime.error());fail(world.end_scene_frame(),world.error());
 }
 void auto_step(WalkInput in={}){step(in,presentation.dialogue_active()&&tick%20==0);}
 void settle(){for(unsigned n=0;n<600&&(world.stage()!=OpeningStage::Walking||world.has_cutscene_actors()||presentation.dialogue_active());++n)auto_step();CHECK(world.stage()==OpeningStage::Walking);}
 void warp(Vec2 p,Vec2 d){CHECK(world.warp_same_scene(p,d));CHECK(runtime.after_physics());}
};
void bases(BattleActionPresentation&p,RoundView r,BattleView e){
 for(uint32_t i=0;i<e.count(BattleSection::Layouts);++i){auto l=e.layout(i);BattlePose q{l.rect,l.color,l.frame,true};if(l.flags&2){q.rect.x-=q.rect.z/2;q.rect.y-=q.rect.w/2;}
  if(l.role==uint32_t(BattleRole::PartySprite)){q.rect.y-=r.parameter(RoundParameter::PartyShown).x;CHECK(p.set_actor_base(0,q));}
  if(l.role==uint32_t(BattleRole::EnemySprite))CHECK(p.set_actor_base(1,q));
  if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))CHECK(p.set_plate_base(q.rect));
 }
}
void win_pillow(Fixture&f){
 const auto r=f.round.view();const auto e=f.entry.view();const auto source=e.participant(0);BattleSessionStats s;
 s.hp=source.hp;s.pp=source.pp;s.maxhp=source.maxhp;s.maxpp=source.maxpp;s.offense=source.offense;s.defense=source.defense;s.speed=source.speed;s.iq=source.iq;s.guts=source.guts;s.level=1;s.experience=3;s.bank=5;s.earned_cash=5;s.learned_skills={"strike","splitShot"};
 CHECK(f.world.accept_battle_entry());f.step();
 BattleActionPresentation p;BattleRound round;BattleOutcome outcome;WorldBattleHost host;
 CHECK(p.begin(r,e,f.random,&s));bases(p,r,e);host.bind(p,f.world,r);CHECK(outcome.initialize(r,f.room.view(),&s));CHECK(round.begin(r,e,f.random,host,&s));
 for(unsigned n=0;n<14000&&outcome.phase()!=BattleOutcomePhase::PostWinRequested;++n){
  if(round.phase()==BattleRoundPhase::Commands){CHECK(round.request_menu(r.binding().basic_menu));CHECK(round.target_input(0,true));}
  if(n%30==0)p.input(true,false);
  CHECK(p.physics_frame(dt));CHECK(p.idle_frame(dt));CHECK(round.idle_frame(dt));f.step();
  if(round.phase()==BattleRoundPhase::VictoryPending&&outcome.phase()==BattleOutcomePhase::Idle)CHECK(outcome.begin(round,p,f.world));
  for(auto event:p.take_return_events()){
   if(event==RoundEventKind::HideBattleBackground)CHECK(f.world.resume_battle_camera());
   else if(event==RoundEventKind::JumpPartyToWorld){const auto pos=f.world.player().position,cam=f.world.cutscene_camera().center();CHECK(p.begin_party_return({pos.x-cam.x+200,pos.y-cam.y+120},{80,60}));}
   else if(event==RoundEventKind::PartyReturnLanded){const auto turn=r.parameter(RoundParameter::ReturnPartyTurn),frames=r.parameter(RoundParameter::ReturnPartyFrames);CHECK(f.world.land_battle_player({turn.x,turn.y},uint16_t(frames.y)));}
   else if(event==RoundEventKind::RotatePartyOriginal)CHECK(f.world.rotate_battle_player(r.parameter(RoundParameter::ReturnPartyTurn).z));
  }
  if(!outcome.idle_frame())std::cerr<<outcome.error()<<'\n';CHECK(outcome.phase()!=BattleOutcomePhase::Error);
 }
 CHECK(outcome.phase()==BattleOutcomePhase::PostWinRequested);CHECK(outcome.state().experience==8&&outcome.state().level==1&&outcome.state().bank==10);
 CHECK(!f.world.story_flag("minnie_leave")&&!f.world.body_visible(12));
 CHECK(outcome.advance_post_win());
 for(unsigned n=0;n<2200&&!f.world.story_completed();++n)f.auto_step();
 CHECK(f.world.story_completed());CHECK(outcome.advance_post_win());CHECK(outcome.phase()==BattleOutcomePhase::Complete);f.settle();
 CHECK(f.world.story_flag("minnie_leave")&&!f.world.story_flag("minnie_door"));
 const auto minnie=f.presentation.npc_pose(3).position;CHECK(minnie.x==64&&minnie.y==370);
}
void tutorial(Fixture&f,bool yes,bool cancel){
 const auto pos=f.presentation.npc_pose(3).position;f.warp({pos.x,pos.y+28},{0,-1});f.step({},true);
 for(unsigned n=0;n<2000&&f.choices.phase()!=DialogueChoicesPhase::Active;++n)f.auto_step();
 CHECK(f.choices.phase()==DialogueChoicesPhase::Active);std::string error;
 CHECK(f.choices.step(dt,{yes?0:1,0,!cancel,cancel},error));DialogueChoicesEvent event;bool selected=false;
 while(f.choices.poll_event(event))if(event.kind==DialogueChoicesEventKind::Selected){CHECK(f.runtime.select_story_option(event.target_pc,f.world.story_generation()));selected=true;}
 CHECK(selected);
 for(unsigned n=0;n<2500&&(f.world.stage()!=OpeningStage::Walking||f.presentation.dialogue_active());++n)f.auto_step();
 CHECK(f.world.stage()==OpeningStage::Walking);CHECK(f.world.story_flag("minnie_door"));CHECK(!f.world.story_flag("mimmie_door_opened")&&!f.world.story_flag("doll_melody"));
}
}
int main(int argc,char**argv){CHECK(argc==2);Fixture f(argv[1]);
 CHECK(f.house.view().count(HouseSection::Doors)==8);
 for(unsigned n=0;n<1800&&f.world.stage()!=OpeningStage::BattleRequested;++n)f.auto_step({0,-1});
 CHECK(f.world.stage()==OpeningStage::BattleRequested);CHECK(f.world.battle_request().actor_index==6);CHECK(f.world.story_flag("pillow_attack"));CHECK(!f.world.story_flag("minnie_leave"));
 win_pillow(f);
 // Use actual return warp and hallway walking to arm the original Area7.
 for(unsigned n=0;n<500&&f.world.player().position.y<300;++n)f.auto_step({0,1});
 CHECK(f.world.player().position.y>300);
 for(unsigned n=0;n<400&&f.world.stage()!=OpeningStage::ScriptRunning;++n)f.auto_step({0,-1});
 for(unsigned n=0;n<1800&&(!f.world.story_flag("minnie_door")||f.world.stage()!=OpeningStage::Walking);++n)f.auto_step();
 CHECK(f.world.story_flag("minnie_door"));f.settle();CHECK(f.presentation.npc_pose(3).position.x==40);
 tutorial(f,true,false);tutorial(f,false,false);tutorial(f,true,true);
 // Walk back from Minnie, line up with the existing original sister blocker,
 // then use the tutorial's native toggle+up input; no flag is injected.
 for(unsigned n=0;n<80&&f.world.player().position.x<63;++n)f.step({1,0});
 for(unsigned n=0;n<100&&f.world.player().position.y>386;++n)f.step({0,-1});
 for(unsigned n=0;n<2400&&f.world.stage()!=OpeningStage::BattleRequested;++n)f.auto_step({0,-1,true});
 if(!f.world.story_flag("mimmie_door_opened"))std::cerr<<"ram ended position "<<f.world.player().position.x<<","<<f.world.player().position.y<<" house "<<unsigned(f.runtime.phase())<<" world "<<unsigned(f.world.stage())<<" flag "<<f.world.story_flag("mimmie_door_opened")<<" minnie "<<f.presentation.npc_pose(3).position.x<<","<<f.presentation.npc_pose(3).position.y<<"\n";
 CHECK(f.world.story_flag("mimmie_door_opened"));CHECK(f.world.stage()==OpeningStage::BattleRequested);
 CHECK(f.world.battle_request().actor_index==2&&f.world.story_flag("doll_attack"));
 CHECK(!f.world.story_flag("doll_defeated")&&!f.world.story_flag("doll_melody"));
 std::cout<<"Pillow sequence: "<<checks<<" checks; generated post-Lamp harness -> actual Mom warp/Pillow script/combat/reward -> Minnie leave/hallway trigger -> Yes/No/Cancel tutorial -> real ram/sister warp/Doll encounter\n";
}
