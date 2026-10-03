#include "encore/battle_outcome.hpp"
#ifdef ENCORE_HOUSE_ROUTE_TESTS
#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/battle_entry.hpp"
#endif
#include "room_fixture.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <filesystem>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<why<<'\n';std::exit(1);}}}
int main(int argc,char**argv){
#ifdef ENCORE_HOUSE_ROUTE_TESTS
 check(argc==4,"round, battle, and house paths required");
#else
 check(argc==3,"round and battle paths required");
#endif
 std::string error;BattleRoundData data;BattleData entry_data;
 check(data.load_file(argv[1],error),error.c_str());check(entry_data.load_file(argv[2],error),error.c_str());
 const auto r=data.view();const auto entry=entry_data.view();const auto binding=r.binding();const auto room=encore_test::room();
 OpeningWorld world;check(world.initialize(room),world.error());const double dt=double(float(1.0/60));
 for(unsigned n=0;n<1200&&world.stage()!=OpeningStage::BattleRequested;++n){check(world.advance({-1,0}),world.error());check(world.idle_frame(dt),world.error());}
 check(world.accept_battle_entry(),"accept real source battle request");check(world.idle_frame(dt),"flush original actor restoration");
 const auto area_music=world.area_music_resource();check(area_music!=kRoomNoIndex,"Lamp source acquires area music ownership");
 const auto original=world.player().position;const auto direction=world.player().direction;
 SourceRandom random(59);BattleActionPresentation p;check(p.begin(r,entry,random),p.error());
 for(uint32_t i=0;i<entry.count(BattleSection::Layouts);++i){const auto l=entry.layout(i);BattlePose pose{l.rect,l.color,l.frame,true};if(l.flags&2){pose.rect.x-=pose.rect.z/2;pose.rect.y-=pose.rect.w/2;}
  if(l.role==uint32_t(BattleRole::PartySprite)){pose.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(binding.player_participant,pose),"party pose");}
  if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(binding.enemy_participant,pose),"enemy pose");
  if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(pose.rect),"plate pose");
 }
 WorldBattleHost host;host.bind(p,world,r);BattleRound round;BattleOutcome outcome;
 check(outcome.initialize(r,room),outcome.error());check(round.begin(r,entry,random,host),round.error());
 check(round.request_menu(binding.basic_menu)&&round.target_input(0,true),round.error());
 for(unsigned n=0;n<2000&&round.phase()==BattleRoundPhase::Running;++n){const double delta=double(float(dt*p.advance_real_time(dt)));check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());}
 check(round.phase()==BattleRoundPhase::VictoryPending,"real seeded attack reaches win boundary");
 check(!world.instance_visible(world.battle_request().actor_index),"enemy erased at defeat before rewards");
 check(!world.story_flag(world.battle_request().win_flag),"no premature story flag");
 const auto draws=random.raw_draw_count();check(outcome.begin(round,p,world),outcome.error());
 auto frame=[&]{check(world.advance({1,0,true})&&world.idle_frame(dt),world.error());check(p.physics_frame(dt)&&p.idle_frame(dt),p.error());
  for(auto e:p.take_return_events()){
   if(e==RoundEventKind::HideBattleBackground)check(world.resume_battle_camera(),"resume camera");
   if(e==RoundEventKind::JumpPartyToWorld){auto c=world.cutscene_camera().center();check(p.begin_party_return({original.x-c.x+200,original.y-c.y+120},{80,60}),"jump back to existing world position");}
   if(e==RoundEventKind::PartyReturnLanded){auto turn=r.parameter(RoundParameter::ReturnPartyTurn);check(world.land_battle_player({turn.x,turn.y},uint16_t(r.parameter(RoundParameter::ReturnPartyFrames).y)),"world landing");}
   if(e==RoundEventKind::RotatePartyOriginal)check(world.rotate_battle_player(r.parameter(RoundParameter::ReturnPartyTurn).z),"restore original direction");
  }
  check(outcome.idle_frame(),outcome.error());
 };
 for(unsigned n=0;n<600&&outcome.phase()!=BattleOutcomePhase::ExperienceDialogue;++n)frame();
 check(outcome.phase()==BattleOutcomePhase::ExperienceDialogue,"native banner leads to XP text");
 for(unsigned n=0;n<180;++n)frame();
 check(p.dialogue().finished()&&!p.dialogue().done(),"XP text requires manual acknowledgment");
 check(outcome.state().experience==0&&outcome.state().bank==0&&!world.story_flag(world.battle_request().win_flag),"waiting leaves all rewards untouched");
 check(world.player().position.x==original.x&&world.player().position.y==original.y,"input blocked throughout victory wait");
 p.input(false,true);check(outcome.idle_frame(),outcome.error());
 check(outcome.phase()==BattleOutcomePhase::Returning,"actual cancel input acknowledges finished source text");
 check(world.area_music_resource()==area_music,"Ordinary Lamp return preserves owned area music");
 for(const auto&request:world.audio_requests())check(request.kind!=AudioRequestKind::StopMusicResource,"Lamp return does not issue boss-only area cleanup");
 check(outcome.state().experience==3&&outcome.state().level==1&&outcome.state().bank==5&&outcome.state().earned_cash==5&&outcome.state().cash==0,"source rewards values");
 check(world.story_flag(world.battle_request().win_flag)&&world.story_flag(r.string(r.victory().earned_cash_flag)),"source reward flags committed");
 const auto& events=outcome.events();check(events.size()==7&&events[3]==BattleRewardEvent::ExperienceCommitted&&events[4]==BattleRewardEvent::StoryFlag&&events[5]==BattleRewardEvent::CurrencyCommitted,"XP then story flag then currency ordering");
 for(unsigned n=0;n<300&&outcome.phase()!=BattleOutcomePhase::Complete;++n)frame();
 check(outcome.phase()==BattleOutcomePhase::Complete&&world.stage()==OpeningStage::Walking,"transition completion unpauses world");
 check(world.player().position.x==original.x&&world.player().position.y==original.y,"return preserves original world coordinates");
 check(world.player().direction.x==direction.x&&world.player().direction.y==direction.y,"return restores original facing");
 check(random.raw_draw_count()==draws,"no reward or return random draws");
 check(world.advance({})&&world.idle_frame(dt),world.error());
 check(!world.player().tap_run&&!world.player().crouch&&world.player().position.x==original.x&&world.player().position.y==original.y,"held battle button does not create a fresh toggle after unpause");
 for(unsigned n=0;n<100;++n){check(world.advance({1,0})&&world.idle_frame(dt),world.error());}
 check(world.player().position.x>510&&world.stage()==OpeningStage::Walking&&!world.battle_request().requested,"player moves after battle without trigger restart");
 const auto audio_before=world.audio_request_count();const auto random_before=random.raw_draw_count();
 for(unsigned n=0;n<660;++n){check(world.advance({})&&world.idle_frame(dt),world.error());}
 check(world.healthy()&&world.stage()==OpeningStage::Walking,"post-win world remains active beyond delayed room timer");
 check(world.audio_request_count()>audio_before&&random.raw_draw_count()>random_before,"source room shaker resumes after battle using shared RNG and typed sound");
#ifdef ENCORE_HOUSE_ROUTE_TESTS
 HouseData house_data;check(house_data.load_file(argv[3],error),error.c_str());const auto house=house_data.view();
 HousePresentation hp;check(hp.begin(house,entry,random),hp.error());HouseRuntime hr;check(hr.initialize(house,world,hp),hr.error());
 auto step=[&](WalkInput input,bool accept=false,bool cancel=false){
  check(hr.before_physics(input),hr.error());check(hp.physics_frame(dt,world.player().position),hp.error());check(world.advance(input),world.error());check(hr.after_physics(),hr.error());check(world.idle_frame(dt),world.error());
  check(hp.idle_frame(dt),hp.error());check(hr.idle_frame(dt,accept,cancel),hr.error());
 };
 auto move=[&](Vec2 target){unsigned n=0;while(n++<600){const auto p=world.player().position;const Vec2 diff{target.x-p.x,target.y-p.y};if(std::abs(diff.x)<1.2f&&std::abs(diff.y)<1.2f)break;
   WalkInput input;if(std::abs(diff.x)>=1.2f)input.x=diff.x>0?1:-1;else input.y=diff.y>0?1:-1;step(input);
  }if(n>=600){std::cerr<<"route blocked at "<<world.player().position.x<<","<<world.player().position.y<<" target "<<target.x<<","<<target.y<<" phase "<<int(hr.phase())<<'\n';check(false,"source route movement");}
 };
 auto door=[&](WalkInput drive,uint32_t expected){const auto before=hr.events().size();bool finished=false;
  for(unsigned n=0;n<500&&!finished;++n){step(hr.blocks_player()?WalkInput{}:drive);for(size_t k=before;k<hr.events().size();++k)if(hr.events()[k].kind==HouseEventKind::DoorDone){check(hr.events()[k].object==expected,"expected source door");finished=true;}}
  check(finished,"source door finishes");const auto destination=house.door(expected).destination;check(world.player().position.x==destination.x&&world.player().position.y==destination.y,"exact scaled same-scene destination");
  while(hr.phase()!=HousePhase::Idle)step({});
 };
 door({-1,0},0);check(world.player().position.x==220&&world.player().position.y==385,"native bedroom exit destination");
 move({220,410});door({-1,0},2);check(world.player().position.x==32&&world.player().position.y==681,"native stairs destination");
 move({32,752});move({192,752});move({192,732});
 step({},true);check(hr.phase()==HousePhase::Dialogue&&world.house_paused(),"source directional ray opens Carol dialogue");
 check(hr.seen_dialogue(house.npc(0).seen_key),"selected source dialogue marked seen");
 const auto paused=world.player().position;unsigned acknowledgments=0;
 for(unsigned n=0;n<1600&&hr.phase()==HousePhase::Dialogue;++n){const bool press=hp.dialogue_stopped()||hp.dialogue_finished();if(press)++acknowledgments;step({1,0},press);check(world.player().position.x==paused.x&&world.player().position.y==paused.y,"dialogue blocks player movement");}
 check(hr.phase()==HousePhase::Idle&&!world.house_paused()&&acknowledgments>=4,"four source sections close and restore controls");
 move({192,752});move({32,752});door({0,-1},3);check(world.player().position.x==155&&world.player().position.y==401,"native upstairs return");
 move({168,401});move({168,385});door({1,0},1);check(world.player().position.x==430&&world.player().position.y==387,"native bedroom return");
 check(outcome.state().experience==3&&outcome.state().bank==5&&world.story_flag(world.battle_request().win_flag),"round trip preserves earned state");
 check(!world.instance_visible(world.battle_request().actor_index)&&world.stage()==OpeningStage::Walking,"defeated actor stays erased after doors/dialogue");
 for(unsigned n=0;n<180;++n)step({});check(!world.battle_request().requested,"completed lamp area does not retrigger");
 const HouseEvent* paused_event=nullptr;const HouseEvent* started_event=nullptr;const HouseEvent* entered_event=nullptr;const HouseEvent* out_event=nullptr;const HouseEvent* done_event=nullptr;
 for(const auto&e:hr.events()){if(e.object!=0)continue;if(e.kind==HouseEventKind::Paused&&!paused_event)paused_event=&e;if(e.kind==HouseEventKind::DoorStarted&&!started_event)started_event=&e;if(e.kind==HouseEventKind::DoorEntered&&!entered_event)entered_event=&e;if(e.kind==HouseEventKind::FadeOutStarted&&!out_event)out_event=&e;if(e.kind==HouseEventKind::DoorDone&&!done_event)done_event=&e;}
 check(paused_event&&started_event&&entered_event&&out_event&&done_event,"source door event trace exists");
 check(started_event->idle_frame-paused_event->idle_frame==1&&entered_event->idle_frame-started_event->idle_frame==40&&out_event->idle_frame-entered_event->idle_frame==1&&done_event->idle_frame-out_event->idle_frame==15,"native F2/3/43/44/59 door event differences");
 struct Edge {Vec2 p;uint32_t door;bool touches;};const Edge edges[]={{{427,397},0,true},{{428,397},0,false},{{224,390},1,true},{{223,390},1,false},{{151,409},2,true},{{152,409},2,false},{{32,677},3,true},{{32,678},3,false}};
 for(const auto&edge:edges){
  OpeningWorld probe;check(probe.initialize(room)&&probe.warp_same_scene(edge.p,{1,0}),"native edge fixture");
  HousePresentation visual;check(visual.begin(house,entry,random),visual.error());HouseRuntime runtime;
  check(runtime.initialize(house,probe,visual),runtime.error());WalkInput input;
  check(runtime.before_physics(input),runtime.error());
  check(runtime.phase()==HousePhase::Idle&&!probe.house_paused(),"Area observation queues contact without an early Door pause");
  // This static contact fixture holds the supplied body position fixed; full
  // route movement and exact pause/start/warp intervals are checked above.
  check(runtime.after_physics()&&runtime.idle_frame(dt,false,false),runtime.error());
  check((runtime.phase()==HousePhase::DoorAwaitIdle)==edge.touches,"native touch-inclusive area edge delivers its pause in idle");
  if(edge.touches)check(runtime.active_object()==edge.door&&probe.house_paused(),"native boundary door identity and delivered pause");
 }
 // Source sibling-door oracle: reports/sister-door-reference/contract.json.
 // These checks continue the earned Lamp state through actual hall movement.
 uint32_t sister_openable=house_no_index;
 for(uint32_t i=0;i<house.count(HouseSection::OpenableDoors);++i)
  if(house.string(house.openable_door(i).source_path)=="Below/Openable Door2")sister_openable=i;
 check(sister_openable!=house_no_index,"source Mimmie openable binding exists");
 const auto sister=house.openable_door(sister_openable);
 auto doll_untouched=[&]{return !world.story_flag("doll_attack")&&!world.story_flag("doll_defeated")&&!world.story_flag("doll_melody")&&!world.battle_request().requested;};
 const auto defeated_lamp=world.battle_request().actor_index;
 auto earnings_preserved=[&]{return outcome.state().experience==3&&outcome.state().bank==5&&outcome.state().earned_cash==5&&outcome.state().cash==0&&world.story_flag("poltergeist")&&!world.instance_visible(defeated_lamp);};
 check(doll_untouched()&&earnings_preserved(),"sister route starts with earned Lamp state and untouched Doll flags");
 move({430,397});door({-1,0},0);move({220,376});move({64,376});move({64,382});step({0,-1});
 check(hr.phase()==HousePhase::Idle&&hr.openable_state(sister_openable).blocked&&!hr.openable_state(sister_openable).unlocked,"Mimmie door initially blocked");
 check(world.body_enabled(sister.player_body_id)&&hr.openable_state(sister_openable).sprite_visible,"source blocked collider and closed sprite");
 const auto door_dialogue_begin=hr.events().size();step({},true);
 check(hr.phase()==HousePhase::Dialogue&&world.house_paused(),"A ray opens source blocked-door dialogue");
 bool blocked_dialogue_event=false;for(size_t i=door_dialogue_begin;i<hr.events().size();++i)blocked_dialogue_event|=hr.events()[i].kind==HouseEventKind::DoorDialogueOpened&&hr.events()[i].object==sister_openable;
 check(blocked_dialogue_event&&house.string(house.dialogue(sister.blocked_dialogue).source_path)=="Data/Dialogue/Reusable/doorblocked.yaml","blocked A requests original doorblocked content");
 for(unsigned n=0;n<1200&&hr.phase()==HousePhase::Dialogue;++n)step({},hp.dialogue_stopped()||hp.dialogue_finished());
 check(hr.phase()==HousePhase::Idle&&!world.house_paused()&&!world.story_flag("mimmie_door_opened"),"blocked dialogue closes without unlocking");
 for(unsigned n=0;n<40;++n)step({0,-1});
 check(hr.openable_state(sister_openable).inside&&hr.openable_state(sister_openable).blocked&&world.player().position.y>364&&world.player().position.y<366,"walking reaches source blocker and cannot enter warp");
 for(unsigned n=0;n<18;++n)step({0,-1,true});
 check(hr.openable_state(sister_openable).blocked&&!world.story_flag("mimmie_door_opened")&&world.body_enabled(sister.player_body_id),"starting run while still overlapping cannot synthesize body-enter");
 step({});move({64,386});step({});
 check(!hr.openable_state(sister_openable).inside,"retreat exits openable before fresh running approach");
 const auto sister_trace_begin=hr.events().size();bool rammed=false;
 for(unsigned n=0;n<100&&!rammed;++n){
  WalkInput input{0,-1,true};check(hr.before_physics(input),hr.error());
  rammed=world.story_flag("mimmie_door_opened");
  if(rammed){const auto&state=hr.openable_state(sister_openable);
   check(state.unlocked&&!state.blocked&&state.collision_pending&&!state.player_disabled&&state.sprite_visible,"source bash writes flag before deferred collider and AnimationPlayer property changes");
   check(world.body_enabled(sister.player_body_id),"source body stays enabled inside bash callback");
  }
  check(hp.physics_frame(dt,world.player().position),hp.error());check(world.advance(input),world.error());check(hr.after_physics(),hr.error());
  if(rammed)check(!world.body_enabled(sister.player_body_id)&&hr.openable_state(sister_openable).player_disabled&&hr.openable_state(sister_openable).sprite_visible,"deferred collider disable precedes idle Action visibility");
  check(world.idle_frame(dt),world.error());check(hp.idle_frame(dt),hp.error());check(hr.idle_frame(dt,false,false),hr.error());
 }
 check(rammed,"fresh upward running body-enter unlocks Mimmie door");
 check(!hr.openable_state(sister_openable).sprite_visible&&hr.openable_state(sister_openable).nonplayer_disabled,"native Action hides door sprite and disables nonplayer collider");
 check(!hp.openable_door_pose(sister_openable,hr.openable_state(sister_openable).sprite_visible).visible,"render pose consumes source Action visibility");
 bool saw_story_during_fade=false,saw_done_paused=false;
 for(unsigned n=0;n<240&&hr.phase()!=HousePhase::StoryRunning;++n){
  const auto before=hr.events().size();step(hr.blocks_player()?WalkInput{}:WalkInput{0,-1,true});
  for(size_t i=before;i<hr.events().size();++i){const auto&e=hr.events()[i];
   if(e.kind==HouseEventKind::StoryRequested){saw_story_during_fade=true;check(hr.phase()==HousePhase::DoorFadeOut&&hr.entering_door()&&world.cutscene_active(),"Doll request starts source script before door fade is done");}
   if(e.kind==HouseEventKind::DoorDone&&e.object==4){saw_done_paused=true;check(hr.story_pending()&&world.cutscene_active()&&!hr.entering_door(),"mostly-done clears entering without unpausing story owner");}
  }
  check(doll_untouched()&&earnings_preserved(),"door fade does not prematurely set Doll battle flags or change earned state");
 }
 check(saw_story_during_fade&&saw_done_paused&&hr.phase()==HousePhase::StoryRunning,"source trigger executes after warp while fade completes");
 check(world.cutscene_active()&&world.actor_bound(room.scene().player_instance_index),"source actor owns player during Doll script");
 check(hr.story_path()=="Data/Dialogue/Podunk/cutscenes/doll_attack.yaml","boundary retains exact Doll source request");
 HouseEvent sister_paused{},sister_moved{},sister_normal{},sister_requested{},sister_done{};
 bool has_paused=false,has_moved=false,has_normal=false,has_requested=false,has_done=false;
 for(size_t i=sister_trace_begin;i<hr.events().size();++i){const auto&e=hr.events()[i];
  if(e.kind==HouseEventKind::Paused&&e.object==4&&!has_paused){sister_paused=e;has_paused=true;}
  if(e.kind==HouseEventKind::PlayerMoved&&e.object==4){sister_moved=e;has_moved=true;}
  if(e.kind==HouseEventKind::OpenableNormal&&e.object==sister_openable){sister_normal=e;has_normal=true;}
  if(e.kind==HouseEventKind::StoryRequested){sister_requested=e;has_requested=true;}
  if(e.kind==HouseEventKind::DoorDone&&e.object==4){sister_done=e;has_done=true;}
 }
 check(has_paused&&has_moved&&has_normal&&has_requested&&has_done,"native sister event sequence exists");
 std::cout<<"Sister relative frames: warp="<<sister_moved.idle_frame-sister_paused.idle_frame<<" openable_exit="<<sister_normal.idle_frame-sister_paused.idle_frame<<" request="<<sister_requested.idle_frame-sister_paused.idle_frame<<" done="<<sister_done.idle_frame-sister_paused.idle_frame<<'\n';

 check(hr.openable_state(sister_openable).sprite_visible&&hr.openable_state(sister_openable).player_disabled&&!hr.openable_state(sister_openable).nonplayer_disabled&&!hr.openable_state(sister_openable).timer_running,"paused warp exit Normal restores sprite/nonplayer only without timer");
 unsigned text_acknowledgments=0;bool phrase2=false,phrase4=false;
 for(unsigned n=0;n<1800&&world.stage()!=OpeningStage::BattleRequested;++n){
  const bool accept=hp.dialogue_active()&&hp.dialogue_finished()&&!hp.dialogue_closing();
  const auto old_text=world.pending_dialogue_id(),old_phrase=world.phrase();
  step({},accept);
  if(accept&&old_text!=kRoomNoIndex&&world.pending_dialogue_id()!=old_text){++text_acknowledgments;phrase2|=old_phrase==2;phrase4|=old_phrase==4;}
  check(earnings_preserved(),"Doll sequence preserves Lamp rewards and deletion");
  check(!world.story_flag("doll_defeated")&&!world.story_flag("doll_melody"),"Doll sequence cannot grant future victory or melody");
  if(world.phrase()<6)check(!world.story_flag("doll_attack"),"Doll battle flag only belongs to final phrase");
 }
 std::cerr<<"Doll integration stage="<<int(world.stage())<<" phrase="<<world.phrase()<<" acks="<<text_acknowledgments<<" p2="<<phrase2<<" p4="<<phrase4<<" text="<<hp.dialogue_active()<<" finished="<<hp.dialogue_finished()<<" request="<<world.pending_dialogue_id()<<"\n";
 check(world.stage()==OpeningStage::BattleRequested&&text_acknowledgments==2&&phrase2&&phrase4,"actual two source text confirmations reach Doll battle request");
 check(world.story_flag("doll_attack")&&world.battle_request().keep_actor_after_battle,"source final flag and keep-actor metadata");
 check(std::string(world.battle_request().enemy)=="doll"&&world.battle_request().win_flag_index==kRoomNoIndex,"Doll does not borrow Lamp win flag");
 check(room.string(world.battle_request().win_cutscene_string)=="Podunk/cutscenes/doll_defeated","source post-win script retained");
 check(world.story_flag("mimmie_door_opened")&&!world.body_enabled(sister.player_body_id),"story keeps source door unlock");
 std::cout<<"Sister relative physics ticks: warp="<<sister_moved.physics_tick-sister_paused.physics_tick<<" openable_exit="<<sister_normal.physics_tick-sister_paused.physics_tick<<" request="<<sister_requested.physics_tick-sister_paused.physics_tick<<" done="<<sister_done.physics_tick-sister_paused.physics_tick<<'\n';
 // Idle-event stamps count the frame being processed. before_physics events
 // retain the last completed idle stamp, so comparing their raw idle values
 // shifts an otherwise identical native physics callback by one frame. Anchor
 // the exit to the observed warp and compare the two physics-domain intervals.
 const auto native_exit_frame=(sister_moved.idle_frame-sister_paused.idle_frame)+(sister_normal.physics_tick-sister_moved.physics_tick);
 check(sister_normal.physics_tick-sister_moved.physics_tick==2&&sister_requested.physics_tick-sister_normal.physics_tick==1,"native warp-to-openable-exit two physics callbacks and next-idle story request");
 check(sister_moved.idle_frame-sister_paused.idle_frame==41&&native_exit_frame==43&&sister_requested.idle_frame-sister_paused.idle_frame==44&&sister_done.idle_frame-sister_paused.idle_frame==57,"official Godot sister warp/body-exit/request/done timings +41/+43/+44/+57");
 std::cout<<"Source-aligned sister frames: warp=41 openable_exit="<<native_exit_frame<<" request=44 done=57\n";
 BattleData doll_entry;const auto doll_path=std::filesystem::path(argv[2]).parent_path()/"doll-entry.encbattle";
 check(doll_entry.load_file(doll_path.c_str(),error),error.c_str());const auto dv=doll_entry.view();BattleEntry entry_runtime;
 const auto native=dv.parameter(BattleParameter::CanvasSize);const auto center=world.cutscene_camera().center();const auto enemy=world.actor(world.battle_request().actor_index);
 BattleEntrySnapshot snap{{world.player().position.x-center.x+native.x/2,world.player().position.y-center.y+native.y/2},{enemy.position.x-center.x+native.x/2,enemy.position.y-center.y+native.y/2},world.cutscene_camera().is_shaking(),enemy.frame,0,enemy.sprite_offset};
 const auto nudge=dv.parameter(BattleParameter::PartyNudge);if(std::abs(snap.player_screen.x-nudge.x)<nudge.y)snap.nudge_sign=(random.randi()%2)==1?-1:1;
 snap.party_hp=outcome.state().hp;snap.party_pp=outcome.state().pp;
 check(entry_runtime.begin(dv,room,world.battle_request(),snap),entry_runtime.error());check(world.accept_battle_entry(),"Doll real entry accepts retained actor");
 check(entry_runtime.idle_frame(double(.1f),world.cutscene_camera().is_shaking()),"nativefloat100ms catchup remains admitted");
 for(unsigned n=0;n<1000&&entry_runtime.phase()!=BattleEntryPhase::Commands;++n){check(world.advance({})&&world.idle_frame(dt),world.error());check(entry_runtime.idle_frame(dt,world.cutscene_camera().is_shaking()),entry_runtime.error());}
 check(entry_runtime.phase()==BattleEntryPhase::Commands,"Doll source entry reaches original command menu");
 for(uint32_t i=0;i<dv.count(BattleSection::Layouts);++i){auto l=dv.layout(i);if(l.role!=uint32_t(BattleRole::PartyHP)&&l.role!=uint32_t(BattleRole::PartyPP))continue;auto pose=entry_runtime.pose(i);const auto value=l.role==uint32_t(BattleRole::PartyHP)?snap.party_hp:snap.party_pp;const uint32_t divisor=l.binding==0?100:l.binding==1?10:1;check(pose.frame==(uint32_t(value)/divisor%10)*dv.resource(l.resource).columns,"retained HP/PP uses source digit row and transition-column stride");check(pose.visible==(l.binding!=0||uint32_t(value)>=100),"source plate only suppresses leading hundreds");}
 check(dv.participant(1).maxhp==38&&dv.participant(1).offense==4&&dv.participant(1).defense==9,"source Doll entry stats independent from Lamp");
 check(!world.story_flag("doll_defeated")&&!world.story_flag("doll_melody"),"entry alone does not grant victory");
 std::cout<<"Source house round trip, Carol dialogue, Mimmie door and Doll seven-phrase scene passed; ";
#endif
 std::cout<<checks<<" integrated victory checks: source rewards, delayed acknowledgment, world return, no RNG, no retrigger\n";
}
