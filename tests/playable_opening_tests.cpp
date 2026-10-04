// Shared-core acceptance test; actual supported original packs and consumers.
// No renderer, emulator, hardware, or independently playable desktop product.
// Runs original Introduction consumers before the checked FreshHouse boundary.
// Postwin NPC/phone positioning uses explicit public warp shortcuts; the House
// route to Doll uses actual movement, door callbacks, dialogue and source script.
#include "encore/battle_outcome.hpp"
#include "encore/battle_entry.hpp"
#include "encore/continue_menu.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/fresh_house.hpp"
#include "encore/introduction.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <chrono>
using namespace encore::upstream;
namespace {unsigned checks=0;void check_result(bool ok,const char* why,unsigned line){++checks;if(!ok){std::cerr<<"Opening acceptance line "<<line<<": "<<why<<'\n';std::exit(1);}}}
#define check(ok,why) do {const bool passed=(ok);check_result(passed,(why),__LINE__);}while(false)
void lamp_to_doll_entry(FreshHouseState&fresh,HouseView house,SourceRandom&random,BattleSessionStats&live,char**argv){
 std::string error;BattleRoundData data;BattleData entry_data;
 check(data.load_file(argv[1],error),error.c_str());check(entry_data.load_file(argv[2],error),error.c_str());
 const auto r=data.view();const auto entry=entry_data.view();const auto binding=r.binding();const auto room=fresh.world.content();
 auto&world=fresh.world;const double dt=double(float(1.0/60));
 for(unsigned n=0;n<1200&&world.stage()!=OpeningStage::BattleRequested;++n){check(world.advance({-1,0}),world.error());check(world.idle_frame(dt),world.error());}
 check(world.stage()==OpeningStage::BattleRequested,"Actual Lamp script requests encounter");
 {
 BattleEntry lamp_entry;const auto canvas=entry.parameter(BattleParameter::CanvasSize);
 const auto camera=world.cutscene_camera().center();const auto enemy=world.actor(world.battle_request().actor_index);
 BattleEntrySnapshot entry_snapshot{{world.player().position.x-camera.x+canvas.x/2,world.player().position.y-camera.y+canvas.y/2},{enemy.position.x-camera.x+canvas.x/2,enemy.position.y-camera.y+canvas.y/2},world.cutscene_camera().is_shaking(),enemy.frame,0,enemy.sprite_offset};
 const auto nudge=entry.parameter(BattleParameter::PartyNudge);if(std::abs(entry_snapshot.player_screen.x-nudge.x)<nudge.y)entry_snapshot.nudge_sign=(random.randi()%2)==1?-1:1;
 entry_snapshot.party_hp=live.hp;entry_snapshot.party_pp=live.pp;
 check(lamp_entry.begin(entry,room,world.battle_request(),entry_snapshot),lamp_entry.error());
 check(world.accept_battle_entry(),"accept real source battle request");check(world.idle_frame(dt),"flush original actor restoration");
 for(unsigned n=0;n<1000&&lamp_entry.phase()!=BattleEntryPhase::Commands;++n){check(world.advance({})&&world.idle_frame(dt),world.error());check(lamp_entry.idle_frame(dt,world.cutscene_camera().is_shaking()),lamp_entry.error());}
 check(lamp_entry.phase()==BattleEntryPhase::Commands,"Actual Lamp entry reaches command ownership");
 }
 const auto area_music=world.area_music_resource();check(area_music!=kRoomNoIndex,"Lamp source acquires area music ownership");
 const auto original=world.player().position;const auto direction=world.player().direction;
 BattleActionPresentation p;check(p.begin(r,entry,random,&live),p.error());
 for(uint32_t i=0;i<entry.count(BattleSection::Layouts);++i){const auto l=entry.layout(i);BattlePose pose{l.rect,l.color,l.frame,true};if(l.flags&2){pose.rect.x-=pose.rect.z/2;pose.rect.y-=pose.rect.w/2;}
  if(l.role==uint32_t(BattleRole::PartySprite)){pose.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(binding.player_participant,pose),"party pose");}
  if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(binding.enemy_participant,pose),"enemy pose");
  if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(pose.rect),"plate pose");
 }
 WorldBattleHost host;host.bind(p,world,r);BattleRound round;BattleOutcome outcome;
 check(outcome.initialize(r,room,&live),outcome.error());check(round.begin(r,entry,random,host,&live),round.error());
 unsigned lamp_turns=0;
 while(round.phase()!=BattleRoundPhase::VictoryPending&&lamp_turns++<16){
  check(round.phase()==BattleRoundPhase::Commands,"Actual conscious party resumes Lamp commands");
  check(round.request_menu(binding.basic_menu)&&round.target_input(0,true),round.error());
  for(unsigned n=0;n<4000&&round.phase()==BattleRoundPhase::Running;++n){const double delta=double(float(dt*p.advance_real_time(dt)));check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());}
 }
 check(round.phase()==BattleRoundPhase::VictoryPending,"Actual player commands reach Lamp win boundary");
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

 auto&hp=fresh.presentation;auto&hr=fresh.house;
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
  for(unsigned n=0;n<600&&hr.phase()!=HousePhase::Idle;++n)step({});
  check(hr.phase()==HousePhase::Idle,"Source door releases House ownership within budget");
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
 check(doll_entry.load_file(doll_path.string().c_str(),error),error.c_str());const auto dv=doll_entry.view();BattleEntry entry_runtime;
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
 live=outcome.state();
 std::cout<<"Actual Lamp victory and House route to Doll entry passed\n";

}

namespace {
constexpr double tick=double(float(1.0/60));
void bases(BattleActionPresentation&p,RoundView r,BattleView e){for(uint32_t i=0;i<e.count(BattleSection::Layouts);++i){auto l=e.layout(i);BattlePose q{l.rect,l.color,l.frame,true};if(l.flags&2){q.rect.x-=q.rect.z/2;q.rect.y-=q.rect.w/2;}if(l.role==uint32_t(BattleRole::PartySprite)){q.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(r.binding().player_participant,q),p.error());}if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(r.binding().enemy_participant,q),p.error());if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(q.rect),p.error());}}
struct HouseDriver {
 FreshHouseState&scene;PreparedSessionRestore&saved;const NativeSessionData&session;DialogueChoices choices;unsigned frames=0,earned_reads=0;
 HouseDriver(FreshHouseState&sc,PreparedSessionRestore&st,const NativeSessionData&se,const DialogueChoicesData&data):scene(sc),saved(st),session(se){scene.house.bind_choices(data,choices);scene.presentation.set_text_value_callback([](void*d,HouseTokenKind kind,std::string&out){return static_cast<HouseDriver*>(d)->value(kind,out);},this);}
 bool value(HouseTokenKind kind,std::string&out){if(kind==HouseTokenKind::EarnedCash){++earned_reads;out=std::to_string(saved.stats.earned_cash);saved.stats.earned_cash=0;return scene.world.set_story_flag(session.earned_cash_flag_id(),false,false);}if(kind==HouseTokenKind::BankCash){out=std::to_string(saved.stats.bank);return true;}if(kind==HouseTokenKind::CurrentCash){out=std::to_string(saved.stats.cash);return true;}return false;}
 void step(WalkInput input={},bool accept=false){++frames;auto&w=scene.world;auto&h=scene.house;auto&p=scene.presentation;check(h.before_physics(input),h.error());check(p.physics_frame(tick,w.player().position),p.error());check(w.advance(input),w.error());check(h.after_physics(),h.error());check(w.idle_frame(tick),w.error());check(p.idle_frame(tick),p.error());check(h.idle_frame(tick,accept,false),h.error());check(w.end_scene_frame(),w.error());}
 bool idle()const{return scene.world.stage()==OpeningStage::Walking&&scene.house.phase()==HousePhase::Idle&&!scene.house.story_executing()&&!scene.presentation.dialogue_active();}
 void auto_step(){auto&p=scene.presentation;step({},p.dialogue_active()&&!p.dialogue_closing()&&(p.dialogue_stopped()||p.dialogue_finished()));}
 void finish(){for(unsigned n=0;n<12000&&!idle();++n)auto_step();if(!idle())std::cerr<<"House stalled stage="<<int(scene.world.stage())<<" phrase="<<scene.world.phrase()<<" house="<<int(scene.house.phase())<<"\n";check(idle(),"Actual dialogue/program returns to walking");}
 void interact_npc(HouseView house,std::string_view path){uint32_t index=house_no_index;for(uint32_t i=0;i<house.count(HouseSection::Npcs);++i)if(house.string(house.npc(i).source_path)==path)index=i;check(index!=house_no_index,"Source NPC exists");const auto point=scene.presentation.npc_pose(index).position;check(scene.world.warp_same_scene({point.x,point.y+27},{0,-1}),scene.world.error());check(scene.house.after_physics(),scene.house.error());step({},true);for(unsigned n=0;n<16&&!scene.presentation.dialogue_active();++n)step();check(scene.presentation.dialogue_active(),"Real NPC interaction opens dialogue");}
 void interact_phone(PhoneView phone){const auto point=phone.object(0).interact_center;check(scene.world.warp_same_scene({point.x,point.y+21},{0,-1}),scene.world.error());check(scene.house.after_physics(),scene.house.error());step({},true);for(unsigned n=0;n<16&&!scene.presentation.dialogue_active();++n)step();check(scene.presentation.dialogue_active()&&scene.house.story_executing(),"Actual phone dispatch opens source program");}
};
void complete_chain(const std::string&root,const NativeSessionData&session,const RestoreData&restore,const RoomData&room,const HouseData&house,const BattleData&font,const PhoneData&phone,const BattleRoundData&rounddata,const ItemData&items,const ContinueMenuData&title,const SaveMenuData&save,PreparedSessionRestore&saved,SourceRandom&random,std::vector<uint32_t>&ledger,const LoadRngClockProvider&clock,std::unique_ptr<FreshHouseState>&fresh,std::string&error){
 auto&world=fresh->world;auto*original_owner=fresh.get();const auto r=rounddata.view();BattleData entry;check(entry.load_file((root+"/doll-entry.encbattle").c_str(),error),error.c_str());const auto e=entry.view();
 DialogueChoicesData choice_data;check(choice_data.load_file((root+"/opening.encchoices").c_str(),error),error.c_str());HouseDriver drive(*fresh,saved,session,choice_data);
 BattleActionPresentation p;check(p.begin(r,e,random,&saved.stats),p.error());bases(p,r,e);WorldBattleHost host;host.bind(p,world,r);BattleRound round;check(round.begin(r,e,random,host,&saved.stats),round.error());
 unsigned turns=0;while(round.phase()!=BattleRoundPhase::VictoryPending&&turns++<16){check(round.phase()==BattleRoundPhase::Commands,"Actual conscious party retains command ownership");check(round.request_menu(r.binding().basic_menu)&&round.target_input(0,true),round.error());for(unsigned n=0;n<5000&&round.phase()==BattleRoundPhase::Running;++n){const auto delta=double(float(tick*p.advance_real_time(tick)));check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());}}
 check(round.phase()==BattleRoundPhase::VictoryPending,"Actual Doll round wins through actions and boss callback");check(!world.story_flag("doll_defeated"),"Victory cannot bypass source postwin");
 BattleOutcome outcome;check(outcome.initialize(r,room.view(),&saved.stats),outcome.error());check(outcome.begin(round,p,world),outcome.error());
 auto frame=[&]{check(world.advance({})&&world.idle_frame(tick),world.error());check(p.physics_frame(tick)&&p.idle_frame(tick),p.error());for(auto kind:p.take_return_events()){if(kind==RoundEventKind::HideBattleBackground)check(world.resume_battle_camera(),world.error());if(kind==RoundEventKind::JumpPartyToWorld){auto c=world.cutscene_camera().center();check(p.begin_party_return({world.player().position.x-c.x+200,world.player().position.y-c.y+120},{80,60}),p.error());}if(kind==RoundEventKind::PartyReturnLanded){auto turn=r.parameter(RoundParameter::ReturnPartyTurn);check(world.land_battle_player({turn.x,turn.y},uint16_t(r.parameter(RoundParameter::ReturnPartyFrames).y)),world.error());}if(kind==RoundEventKind::RotatePartyOriginal)check(world.rotate_battle_player(r.parameter(RoundParameter::ReturnPartyTurn).z),world.error());}if((outcome.phase()==BattleOutcomePhase::ExperienceDialogue||outcome.phase()==BattleOutcomePhase::LevelDialogue)&&p.dialogue().finished()&&!p.dialogue().done())p.input(true,false);check(outcome.idle_frame(),outcome.error());};
 for(unsigned n=0;n<16000&&outcome.phase()!=BattleOutcomePhase::PostWinRequested;++n)frame();check(outcome.phase()==BattleOutcomePhase::PostWinRequested,"Acknowledged actual rewards return through world transition");saved.stats=outcome.state();check(saved.stats.experience==11&&saved.stats.bank==15&&saved.stats.level==2,"Two original victory rewards retained");check(outcome.advance_post_win(),outcome.error());
 for(unsigned n=0;n<12000&&outcome.phase()==BattleOutcomePhase::PostWinRequested;++n){drive.auto_step();check(outcome.advance_post_win(),outcome.error());}check(outcome.phase()==BattleOutcomePhase::Complete,"Actual eight-phrase Doll postwin completes");drive.finish();check(world.story_flag("doll_defeated")&&world.story_flag("pillow_attack")&&!world.story_flag("doll_melody"),"Source postwin earns only its own flags");
 std::cout<<"Actual Doll action/victory/growth/postwin passed\n";
 // Explicit test positioning shortcuts call the shared world warp API; they
 // do not manufacture story success, edit flags, or load postwin snapshots.
 drive.interact_npc(house.view(),"Objects/npcdoll");drive.finish();check(world.story_flag("doll_melody"),"Actual Doll melody interaction earns its own flag");
 drive.interact_npc(house.view(),"Objects/npc");drive.finish();check(world.story_flag("phone_ring")&&fresh->phone.ringing(0),"Actual Carol call requests ringing phone");
 drive.interact_phone(phone.view());drive.finish();check(world.story_flag("talked_to_dad"),"Actual first Dad call earns repeat-call dispatch");
 drive.interact_phone(phone.view());for(unsigned n=0;n<12000&&!drive.choices.active();++n)drive.auto_step();check(drive.choices.active()&&world.story_choices_waiting(),"Dad question reaches real text-completion choices");check(drive.earned_reads==1&&saved.stats.earned_cash==0&&saved.stats.bank==15,"Actual EarnedCash text consumes report once without awarding twice");
 check(drive.choices.step(0,{0,0,true,false},error),error.c_str());DialogueChoicesEvent chosen;check(drive.choices.poll_event(chosen)&&chosen.kind==DialogueChoicesEventKind::Selected,"Actual Record option emits one selection");const auto generation=world.story_generation();check(fresh->house.select_story_option(chosen.target_pc,generation),fresh->house.error());check(world.take_save_request()&&!world.take_save_request()&&world.story_submenu_waiting(),"Record source emits exactly one suspended SaveSelect request");
 SaveMenu menu;std::vector<SaveSlotMetadata>slots(save.slot_count());check(menu.open(save,slots,1,error),error.c_str());for(unsigned n=0;n<90;++n){check(menu.step(tick,{},error),error.c_str());drive.step();}check(menu.phase()==SaveMenuPhase::Slots,"Actual SaveSelect activation completes");check(menu.step(0,{0,0,true,false},error),error.c_str());SaveMenuEvent event;unsigned writes=0;uint32_t written_slot=0;while(menu.poll_event(event))if(event.kind==SaveMenuEventKind::SaveRequested){++writes;written_slot=event.slot;}check(writes==1&&written_slot==1&&menu.phase()==SaveMenuPhase::Writing,"Actual menu requests selected slot write");
 auto state=saved.state;const auto player=world.player();state.position_x=player.position.x;state.position_y=player.position.y;state.direction_x=player.direction.x;state.direction_y=player.direction.y;state.playtime_seconds+=drive.frames*tick;state.saved_at="2026-10-04T03:30:00Z";for(auto&flag:state.flags)flag.value=world.story_flag(flag.id);for(auto key:fresh->house.seen_dialogue_keys()){const std::string id(house.view().string(key));auto found=std::find_if(state.seen_dialogue_flags.begin(),state.seen_dialogue_flags.end(),[&](const SessionFlag&v){return v.id==id;});if(found==state.seen_dialogue_flags.end())state.seen_dialogue_flags.push_back({id,true});else found->value=true;}for(const auto&id:{"lamp","doll"}){auto found=std::find_if(state.encountered.begin(),state.encountered.end(),[&](const SessionFlag&v){return v.id==id;});if(found==state.encountered.end())state.encountered.push_back({id,true});else found->value=true;}
 SessionSnapshot snapshot;check(build_native_session_snapshot(session,room.view(),house.view(),r,items.view(),{state,&saved.stats,&saved.inventory},snapshot,error),error.c_str());const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();const auto directory=std::filesystem::temp_directory_path()/("encore-opening-acceptance-"+std::to_string(stamp));check(std::filesystem::create_directory(directory),"Exclusive real-file test directory");const auto path=(directory/("slot"+std::to_string(written_slot)+".encsave")).string();
 check(write_session_save(path.c_str(),snapshot,session.compatibility(),error),error.c_str());SessionSnapshot disk;check(read_session_save(path.c_str(),session.compatibility(),disk,error),error.c_str());check(validate_native_session_snapshot(session,room.view(),house.view(),r,items.view(),disk,error),error.c_str());check(!world.story_flag(session.saved_flag_id()),"File commit precedes live saved callback");check(world.set_story_flag(session.saved_flag_id(),true,false),"Actual successful file callback marks saved");
 SaveSlotMetadata metadata;metadata.occupied=true;metadata.lead_name=disk.characters.front().nickname;metadata.scene_label=disk.scene_label;metadata.menu_flavor=disk.settings.menu_flavor;metadata.highest_level=saved.stats.level;metadata.playtime_seconds=disk.playtime_seconds;metadata.party=disk.party;check(menu.acknowledge_save(true,&metadata,error),error.c_str());check(menu.step(0,{0,0,false,true},error),error.c_str());check(menu.poll_event(event)&&event.kind==SaveMenuEventKind::Closed&&event.any_saved,"Actual SaveSelect close returns successful write result");check(fresh->house.close_story_submenu(generation),fresh->house.error());drive.finish();check(fresh.get()==original_owner,"Original fresh owner survived all actual opening/Record stages");
 std::cout<<"Dad Record real-file commit/acknowledgment passed\n";
 slots[written_slot-1]=metadata;ContinueMenu continuation;check(continuation.open(title,save,slots,written_slot,error),error.c_str());ContinueEvent request;while(continuation.poll_event(request)){}check(continuation.step(0,{0,0,true,false},error),error.c_str());for(const auto delta:{.54,0.,.5,.2})check(continuation.step(delta,{},error),error.c_str());check(continuation.phase()==ContinuePhase::Slots,"Actual Continue LOAD activation reaches slots");while(continuation.poll_event(request)){}check(continuation.step(0,{0,0,true,false},error),error.c_str());check(continuation.phase()==ContinuePhase::Actions,"Occupied slot opens Play action");check(continuation.step(0,{0,0,true,false},error),error.c_str());check(continuation.step(1,{},error),error.c_str());unsigned loads=0;uint32_t loaded_slot=0;while(continuation.poll_event(request))if(request.kind==ContinueEventKind::LoadRequested){++loads;loaded_slot=request.slot;}check(loads==1&&loaded_slot==written_slot&&continuation.phase()==ContinuePhase::LoadPending,"Actual Continue emits same slot LOAD after fade");
 SessionSnapshot loaded;check(read_session_save(path.c_str(),session.compatibility(),loaded,error),error.c_str());PreparedSessionRestore restored;check(prepare_session_restore(session,room.view(),house.view(),r,items.view(),font.view(),loaded,restored,error),error.c_str());std::unique_ptr<FreshHouseState>candidate;check(prepare_fresh_house(restored,restore,room.view(),house.view(),font.view(),phone.view(),random,{400,240},candidate,error),error.c_str());check(candidate&&candidate.get()!=original_owner&&fresh.get()==original_owner,"Checked candidate preparation preserves former owner until application");
 std::vector<LoadInventoryAllocation>allocations;
 for(const auto&row:restore.inventory_load_order()){size_t count=0;if(row.rebuilds_inventory){if(row.kind==RestoreInventoryKind::KeyItems)count=loaded.key_items.size();else if(row.kind==RestoreInventoryKind::Storage)count=loaded.storage.size();else{auto found=std::find_if(loaded.characters.begin(),loaded.characters.end(),[&](const SessionCharacter&character){return character.character_id==row.character_id;});count=found==loaded.characters.end()?row.projected_items.size():found->inventory.size();}}allocations.push_back({row.order_id,uint32_t(count)});}
 auto next_random=random;auto next_ledger=ledger;check(apply_load_uid_allocations(next_random,next_ledger,allocations,clock,error),error.c_str());check(next_ledger.size()>ledger.size(),"Actual source LOAD rebuild consumes UID allocation callbacks");
 check(candidate->presentation.set_text_speed(restored.state.settings.text_speed),candidate->presentation.error());
 check(candidate->finish_scene_ready()&&candidate->world.pause_for_house(),"Actual loaded ready boundary pauses candidate before frontend reveal");
 random=next_random;ledger=std::move(next_ledger);fresh.swap(candidate);
 check(continuation.acknowledge_load(true,error),error.c_str());check(fresh->world.house_paused(),"Restored world stays paused at initial black frontend boundary");
 check(continuation.step(0,{},error),error.c_str());check(fresh->world.unpause_from_house(),"Actual WorldReveal idle boundary releases restored player");
 for(const auto delta:{.4,.4})check(continuation.step(delta,{},error),error.c_str());check(!continuation.is_open()&&fresh->world.stage()==OpeningStage::Walking,"Actual reveal releases frontend to restored world");check(fresh->world.story_flag("doll_defeated")&&fresh->world.story_flag("doll_melody")&&fresh->world.story_flag("talked_to_dad"),"Real file restores actual earned progression");check(restored.stats.experience==saved.stats.experience&&restored.stats.bank==saved.stats.bank&&restored.stats.hp==saved.stats.hp&&restored.stats.pp==saved.stats.pp,"Actual restored combat/session state matches Record");check(restored.inventory.instance(0).id==saved.inventory.instance(0).id&&restored.state.characters.front().nickname==saved.state.characters.front().nickname,"Restored native naming and inventory stable identity");const auto at=fresh->world.player().position;check(at.x==float(snapshot.position_x)&&at.y==float(snapshot.position_y),"Fresh owner restores same saved phone position");check(fresh->world.advance({0,1})&&fresh->world.idle_frame(tick),fresh->world.error());check(fresh->world.player().position.y>at.y,"Restored world accepts actual movement input");check(std::filesystem::weakly_canonical(directory).parent_path()==std::filesystem::weakly_canonical(std::filesystem::temp_directory_path())&&directory.filename().string().find("encore-opening-acceptance-")==0,"Verify exclusive cleanup target stays within temporary root");std::error_code ec;std::filesystem::remove_all(directory,ec);check(!ec,"Remove only exclusive acceptance test save artifacts");
}
}

int main(int argc,char**argv){
 std::cerr<<"Opening shared-core acceptance begins\n";
 check(argc==2||argc==3,"RomFS data root and optional explicitly prepared Naming pack required");const std::string root=argv[1];std::string error;
 ContinueMenuData title;SaveMenuData save;NewGameSetupData naming;StartupSettingsData settings;
 RoomData room;HouseData house;PhoneData phone;BattleData font;BattleRoundData round;ItemData items;NativeSessionData session;RestoreData restore;
 check(title.load_file((root+"/opening.enccontinue").c_str(),error),error.c_str());check(save.load_file((root+"/opening.encsavemenu").c_str(),error),error.c_str());
 std::vector<SaveSlotMetadata>slots(save.slot_count());ContinueMenu front;check(front.open(title,save,slots,0,error),error.c_str());ContinueEvent event;while(front.poll_event(event)){}
 check(front.step(0,{0,0,true,false},error),error.c_str());bool requested=false;while(front.poll_event(event))if(event.kind==ContinueEventKind::BoundaryRequested&&event.boundary==ContinueBoundary::NewGame)requested=true;
 check(requested,"Actual title dispatches NewGame boundary");front.close();
 const std::string naming_path=argc==3?argv[2]:root+"/opening.encnewgame";
 check(naming.load_file(naming_path.c_str(),error),error.c_str());check(settings.load_file((root+"/opening.encsettings").c_str(),error),error.c_str());
 check(room.load_file((root+"/opening.encroom").c_str(),error),error.c_str());check(house.load_file((root+"/opening.enchouse").c_str(),error),error.c_str());check(phone.load_file((root+"/opening.encphone").c_str(),error),error.c_str());check(font.load_file((root+"/opening.encbattle").c_str(),error),error.c_str());check(round.load_file((root+"/doll-entry.encround").c_str(),error),error.c_str());check(items.load_file((root+"/opening.encitems").c_str(),error),error.c_str());check(session.load_file((root+"/opening.encsession").c_str(),error),error.c_str());check(restore.load_file((root+"/opening.encrestore").c_str(),room.view(),house.view(),error),error.c_str());
 SourceRandom random(59);std::vector<uint32_t>ledger;SessionSnapshot startup;uint64_t calls=0;
 LoadRngClockProvider clock=[&](LoadRngClockSample&s,std::string&){s={1700000000,++calls};return true;};check(stage_new_game_startup(session,restore,random,ledger,clock,startup,error),error.c_str());
 NewGameSetup setup;check(setup.open(naming,settings,error),error.c_str());
 for(unsigned field=0;field<6;++field){check(setup.field_index()==field,"Source six-field order");check(setup.step(.1,{0,0,false,false,false,true},error),error.c_str());check(setup.step(.1,{0,0,true},error),error.c_str());check(!setup.name().empty(),"Actual default button selects source name");check(setup.step(.1,{0,0,false,false,false,false,true},error),error.c_str());}
 check(setup.phase()==NamingPhase::Settings,"Food completion opens actual settings");for(unsigned i=0;i<3;++i)check(setup.step(.1,{0,1},error),error.c_str());check(setup.step(.1,{0,0,true},error),error.c_str());check(setup.phase()==NamingPhase::Confirmation,"Settings opens confirmation");check(setup.step(.1,{0,0,true},error),error.c_str());check(setup.phase()==NamingPhase::Accepted,"Actual confirmation accepts startup");
 check(setup.apply(startup,session,error),error.c_str());
 IntroductionData intro_data;Introduction intro;check(intro_data.load_file((root+"/opening.encintro").c_str(),error),error.c_str());
 const auto& destination=intro_data.house_destination();check(destination.scene==startup.scene_id&&destination.x==startup.position_x&&destination.y==startup.position_y&&destination.dx==startup.direction_x&&destination.dy==startup.direction_y&&destination.set_respawn&&destination.unpause,"Original Introduction hands off to supported House destination");
 check(intro.begin(intro_data,random,"en",400,240,error),error.c_str());const auto before_intro=random.raw_draw_count();
 // Exercise the original ui_select path in each scene; the separate Intro
 // acceptance test also runs the full natural historical/landscape timelines.
 unsigned skipped=0;for(unsigned frame=0;frame<1800&&!intro.house_ready();++frame){const bool skip=intro.phase()==IntroPhase::Playing;if(skip)++skipped;check(intro.step(double(float(1.0/60)),false,skip,error),error.c_str());intro.take_audio();}
 check(intro.house_ready()&&skipped==2&&intro.playtime_started(),"Both original scene skip callbacks reach House DoorOut");
 check(random.raw_draw_count()==before_intro,"Skip before caption reveal preserves shared random stream");
 PreparedSessionRestore prepared;check(prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),font.view(),startup,prepared,error),error.c_str());
 std::unique_ptr<FreshHouseState>fresh;check(prepare_fresh_house(prepared,restore,room.view(),house.view(),font.view(),phone.view(),random,{400,240},fresh,error),error.c_str());check(fresh->finish_scene_ready(),"FreshHouse scene ready");auto*owner=fresh.get();
 for(unsigned frame=0;frame<300&&!intro.complete();++frame){check(intro.step(double(float(1.0/60)),false,false,error),error.c_str());intro.take_audio();}
 check(intro.complete()&&intro.house_unpaused(),"Original final DoorOut unpauses the prepared House");intro.close();
 std::string paths[]={root+"/opening.encround",root+"/opening.encbattle",root+"/opening.enchouse"};char*route[]={argv[0],paths[0].data(),paths[1].data(),paths[2].data()};lamp_to_doll_entry(*fresh,house.view(),random,prepared.stats,route);check(fresh.get()==owner,"Same fresh world owner throughout title/naming-to-Doll entry path");
 complete_chain(root,session,restore,room,house,font,phone,round,items,title,save,prepared,random,ledger,clock,fresh,error);
 std::cout<<"Opening acceptance: "<<checks<<" checks; title NewGame -> six fields/settings -> original Introduction/Mt. Itoi -> FreshHouse -> original Lamp/Doll victories/postwin -> Melody/Carol/first Dad -> Dad Record real file -> Continue LOAD -> checked fresh restoration. 400x240; no GPU/emulator/hardware claim\n";
}
