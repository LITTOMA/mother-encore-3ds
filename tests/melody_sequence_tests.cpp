#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check_result(bool ok,const char* why,unsigned line){
 ++checks;if(!ok){std::cerr<<"Melody sequence check "<<checks<<" at line "<<line<<": "<<why<<'\n';std::exit(1);}
}
#define check(ok,why) do{const bool passed=(ok);check_result(passed,(why),__LINE__);}while(false)
constexpr double dt=double(float(1.0/60));
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
uint32_t npc_index(HouseView h,std::string_view path){
 for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)if(h.string(h.npc(i).source_path)==path)return i;
 check(false,"Required source NPC missing");return house_no_index;
}
uint32_t program_index(RoomView r,std::string_view path){
 for(uint32_t i=0;i<r.program_count();++i)if(r.string(r.program(i).source_path_string)==path)return i;
 check(false,"Required source program missing");return kRoomNoIndex;
}
uint32_t resource_index(RoomView r,std::string_view path){
 for(uint32_t i=0;i<r.resource_count();++i)if(r.string(r.resource(i).path_string)==path)return i;
 check(false,"Required source resource missing");return kRoomNoIndex;
}
HouseOverride override_for(HouseView h,uint32_t npc,std::string_view flag){
 for(uint32_t i=0;i<h.count(HouseSection::Overrides);++i){auto o=h.override_dialogue(i);if(o.npc==npc&&h.string(o.flag)==flag)return o;}
 check(false,"Required source override missing");return {};
}
std::string printed(const HousePresentation&p){
 std::string result;for(const auto&line:p.dialogue_pose().lines){if(!result.empty())result+=' ';result+=line.text;}return result;
}
size_t action_index(const OpeningWorld&w,DialogueActionKind kind,uint32_t phrase,size_t first=0){
 const auto&trace=w.action_trace();for(size_t i=first;i<trace.size();++i)if(trace[i].action.kind==kind&&trace[i].action.phrase==phrase)return i;
 check(false,"Expected executed source action missing");return 0;
}
double phrase_wait(RoomView room,uint32_t program,uint32_t phrase){
 const auto p=room.program(program);for(uint32_t i=0;i<p.command_count;++i){const auto c=room.command(p.first_command+i);if(c.opcode==uint16_t(DialogueActionKind::StartWait)&&c.phrase==phrase)return c.duration;}
 check(false,"Source phrase timer missing");return 0;
}
bool event_seen(const HouseRuntime&r,HouseEventKind kind){
 return std::any_of(r.events().begin(),r.events().end(),[&](const HouseEvent&e){return e.kind==kind;});
}
struct Fixture {
 RoomView room;HouseView house;SourceRandom random{123};OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;
 uint32_t mimmie,doll,minnie,leader,melody;uint64_t frames=0;
 Fixture(RoomView r,HouseView h,BattleView font,Vec2 viewport={400,240}):room(r),house(h),
  mimmie(npc_index(h,"Objects/npc2")),doll(npc_index(h,"Objects/npcdoll")),minnie(npc_index(h,"Objects/npc3")),
  leader(r.scene().player_instance_index),melody(program_index(r,"Podunk/dollmelody")){
  check(world.initialize(r,viewport),world.error());world.attach_random(random);
  // Bounded post-win setup only. house_npc_restore and doll_round own the
  // original attack, battle, rewards and eight-phrase arrival path.
  for(auto flag:{"doll_attack","doll_defeated","pillow_attack","mimmie_door_opened"})check(world.set_story_flag(flag,true,true),"Post-win setup flag exists");
  check(world.set_story_flag("poltergeist",false,true),"Post-win clears source Poltergeist flag");
  check(presentation.begin(h,font,random),presentation.error());
  check(presentation.restore_npc_pose(mimmie,{112,88},{0,1}),presentation.error());
  check(world.set_body_offset(h.npc(mimmie).body_id,{-8,0}),world.error());
  check(world.warp_same_scene({112,115},{0,-1}),world.error());
  check(runtime.initialize(h,world,presentation),runtime.error());
 }
 void step(WalkInput input={},bool accept=false,bool cancel=false){
  ++frames;
  // The application pipeline: area observations before player physics,
  // fixed printing/actor physics, then idle timers, animation and input.
  check(runtime.before_physics(input),runtime.error());
  check(presentation.physics_frame(dt,world.player().position),presentation.error());
  check(world.advance(input),world.error());check(runtime.after_physics(),runtime.error());
  check(world.idle_frame(dt),world.error());check(presentation.idle_frame(dt),presentation.error());
  const bool ok=runtime.idle_frame(dt,accept,cancel);
  if(!ok)std::cerr<<"frame="<<frames<<" stage="<<unsigned(world.stage())<<" phrase="<<world.phrase()<<" requested="<<world.pending_dialogue_id()<<" presentation="<<presentation.error()<<'\n';
  check(ok,runtime.error());
 }
 void warp(Vec2 position,Vec2 direction){check(world.warp_same_scene(position,direction),world.error());check(runtime.after_physics(),runtime.error());}
 void interact(uint32_t npc){
  const auto p=presentation.npc_pose(npc).position;warp({p.x,p.y+27},{0,-1});step({},true,false);
  for(unsigned i=0;i<8&&!presentation.dialogue_active();++i)step();
  check(presentation.dialogue_active(),"Original NPC interaction opens checked dialogue");
 }
 void finish_printing(){
  for(unsigned i=0;i<1000&&!presentation.dialogue_finished();++i){
   const bool advance=presentation.dialogue_stopped();step({},advance,false);
  }
  check(presentation.dialogue_finished(),"Source text reaches final manual confirmation");
 }
 void close_ordinary(){
  check(presentation.dialogue_finished(),"Ordinary dialogue has reached its end");step({},true,false);
  for(unsigned i=0;i<60&&presentation.dialogue_active();++i)step();
  check(runtime.phase()==HousePhase::Idle&&!runtime.blocks_player(),"Ordinary dialogue returns movement control");
 }
};

void check_mimmie(Fixture&f,bool after){
 const auto initial_draws=f.random.raw_draw_count();f.interact(f.mimmie);
 check(f.runtime.phase()==HousePhase::Dialogue&&f.runtime.active_object()==f.mimmie,"Mimmie uses ordinary original NPC dialogue");
 check(f.presentation.dialogue_pose().name_visible&&f.presentation.dialogue_pose().speaker=="Mimmie","Mimmie keeps source name tag");
 check(!f.world.has_cutscene_actors(),"Ordinary Mimmie dialogue creates no Actor proxies");
 unsigned waits=0;
 for(unsigned i=0;i<1000&&!f.presentation.dialogue_finished();++i){
  if(f.presentation.dialogue_stopped()){++waits;check(waits==1,"Mimmie source phrase has exactly one WAIT");
   check(printed(f.presentation)==(after?"It's dead? What a relief!":"Hurry up!"),"Mimmie pre-WAIT text matches source branch");
   for(unsigned n=0;n<5;++n){f.step();}check(f.presentation.dialogue_stopped(),"Mimmie WAIT needs input");f.step({},true,false);
  }else f.step();
 }
 check(waits==1&&f.presentation.dialogue_finished(),"Mimmie completes the one source WAIT and final text");
 check(printed(f.presentation).find(after?"I knew we shouldn't have taken that doll from the basement...":"I won't be able to sleep at night knowing it might still be alive...")!=std::string::npos,"Mimmie final source branch text is retained");
 check(f.random.raw_draw_count()>initial_draws,"Mimmie Kid voice consumes the shared source RNG");
 const auto key=after?override_for(f.house,f.mimmie,"doll_melody").seen_key:f.house.npc(f.mimmie).seen_key;
 check(f.runtime.seen_dialogue(key),"Selected Mimmie branch stores its own source seen key");f.close_ordinary();
}

void check_melody(Fixture&f){
 const auto doll_actor=f.house.npc(f.doll).room_actor_index;
 check(f.house.npc(f.doll).program_index==f.melody,"Original Doll's checked program resolves by source path");
 check(!f.world.story_flag("doll_melody"),"Melody flag is initially unset");
 const auto before_draws=f.random.raw_draw_count();const auto before_audio=f.world.audio_request_count();
 f.presentation.take_audio_events();f.interact(f.doll);
 check(f.runtime.phase()==HousePhase::StoryRunning&&f.runtime.story_executing(),"Original Doll interaction starts source story");
 check(f.world.story_talker().kind==DialogueTalkerKind::OriginalNpc&&f.world.story_talker().index==f.doll,"Silent narration inherits original Doll NPC talker");
 check(f.world.actor_bound(f.leader)&&!f.world.actor_bound(doll_actor),"Only leader is replaced by an Actor");
 check(f.presentation.npc_pose(f.doll).visible&&f.world.body_enabled(f.house.npc(f.doll).body_id),"Original Doll keeps visibility and collision");
 check(!f.presentation.dialogue_pose().name_visible,"Doll narration has no invented name tag");
 check(f.runtime.seen_dialogue(f.house.npc(f.doll).seen_key),"Initial Doll interaction records source seen key");
 const auto text_action=action_index(f.world,DialogueActionKind::ShowDialogue,0);
 const auto bind_action=action_index(f.world,DialogueActionKind::BindActor,0);
 check(text_action<bind_action,"Phrase 0 starts text before leader creation");
 unsigned waits=0;bool saw_talking=false;
 for(unsigned i=0;i<1000&&!f.presentation.dialogue_finished();++i){
  saw_talking|=f.presentation.talking();
  check(f.world.phrase()==0&&!f.world.story_flag("doll_melody"),"Initial text never starts the melody or writes its flag early");
  check(!f.world.actor_bound(doll_actor)&&f.presentation.npc_pose(f.doll).frame==0,"Silent Doll talker does not gain a proxy or nonexistent Talk animation");
  if(f.presentation.dialogue_stopped()){
   ++waits;check(waits<=2,"Melody phrase 0 has only its two source WAIT markers");
   check(!f.presentation.talking(),"Doll talking stops at source WAIT");
   for(unsigned n=0;n<5;++n){f.step();}check(f.presentation.dialogue_stopped(),"Melody WAIT remains until explicit input");
   f.step({},true,false);
  }else f.step();
 }
 check(waits==2&&saw_talking&&f.presentation.dialogue_finished(),"Silent Doll prints, pauses twice and reaches confirmation");
 const auto all=printed(f.presentation);
 check(all.find("There's an old music box hidden inside the doll.")!=std::string::npos&&all.find("Ninten opened it.")!=std::string::npos&&all.find("A broken melody began to play.")!=std::string::npos,"Original melody text and PartyLead substitution survive both WAITs");
 for(unsigned i=0;i<30;++i)f.step();
 check(f.world.phrase()==0&&f.world.audio_request_count()==before_audio,"Completed initial text waits for manual confirmation");
 const auto captured_center=f.world.cutscene_camera().center();f.step({},true,false);
 check(f.world.phrase()==1&&!f.world.story_input_allowed(),"Source phrase 3 starts its unskippable timer");
 check(f.world.effect_requests().size()==1,"Appear is delivered once on phrase transition");
 const auto appear=f.world.effect_requests().front();
 check(appear.appear&&appear.npc_index==f.doll&&appear.actor_mask==(uint64_t(1)<<f.leader),"Effect ownership is original Doll plus leader only");
 check(appear.resource_index==resource_index(f.room,"world-effect/melody.encfx"),"Appearance uses the checked external melody effect");
 check(same(appear.center,captured_center),"Effect captures the current world camera center");
 check(f.world.audio_request_count()==before_audio+1,"Unattached root MusicArea stop leaves new melody audio running");
 check(f.world.audio_requests().back().kind==AudioRequestKind::PlayDialogueMusic&&f.world.audio_requests().back().resource_index==resource_index(f.room,"res://Audio/Music/Melodies/melody1.mp3"),"Synchronous one-shot melody uses source audio resource");
 check(f.world.area_music_resource()==kRoomNoIndex,"House area does not own the one-shot melody");
 const auto wait_action=action_index(f.world,DialogueActionKind::StartWait,1);
 const auto deferred_stop=action_index(f.world,DialogueActionKind::CallObjectDeferred,1);
 const auto deferred_appear=action_index(f.world,DialogueActionKind::CallObjectDeferred,1,deferred_stop+1);
 const auto music_action=action_index(f.world,DialogueActionKind::PlayMusicImmediate,1);
 check(wait_action<deferred_stop&&deferred_stop<deferred_appear&&deferred_appear<music_action,"Source phase 3 schedules wait, targeted stop, appear, then synchronous melody");
 double remaining=phrase_wait(f.room,f.melody,1);check(remaining==4,"Checked source melody wait is four seconds");
 unsigned ticks=0;
 while(remaining>=0){
  check(f.world.phrase()==1&&!f.world.story_input_allowed(),"A/B cannot skip source four-second wait");
  check(f.world.story_talker().kind==DialogueTalkerKind::OriginalNpc&&f.world.story_talker().index==f.doll,"Original Doll remains the inherited talker during the no-text music phrase");
  check(!f.world.story_flag("doll_melody"),"Flag remains false throughout music wait");
  f.step({},true,true);remaining-=dt;++ticks;
  if(remaining>=0)check(f.world.phrase()==1,"Timer does not advance before strict-negative expiry");
 }
 check(f.world.phrase()==2&&f.world.story_flag("doll_melody"),"Timer expiry starts phrase 4 and synchronously sets melody flag");
 check(f.presentation.dialogue_active()&&!f.presentation.dialogue_closing()&&f.presentation.visible_characters()==0,"Remembered-tune text opens while minimum-input timer starts");
 check(!f.world.story_input_allowed(),"New 0.6-second guard is not consumed by expired timer's overshoot");
 check(f.world.effect_requests().size()==2&&!f.world.effect_requests().back().appear,"Deferred disappearance is requested with remembered text still active");
 check(f.world.effect_requests().back().actor_mask==appear.actor_mask&&f.world.effect_requests().back().npc_index==f.doll,"Disappear retains original NPC and leader ownership");
 check(f.world.effect_requests().back().resource_index==appear.resource_index,"Disappearance targets the same external melody effect");
 check(f.world.area_music_resource()==resource_index(f.room,"res://Audio/Music/House.mp3"),"Phrase 4 explicitly restores source House area music");
 const auto&audio=f.world.audio_requests();
 check(audio.size()==before_audio+3&&audio[before_audio+1].kind==AudioRequestKind::PlayEffect&&audio[before_audio+1].resource_index==resource_index(f.room,"res://Audio/Sound effects/M3/heal_se.wav")&&audio[before_audio+2].kind==AudioRequestKind::PlayMusic,"Synchronous heal SFX precedes deferred House playback");
 const auto show=action_index(f.world,DialogueActionKind::ShowDialogue,2),wait=action_index(f.world,DialogueActionKind::StartWait,2);
 const auto deferred_play=action_index(f.world,DialogueActionKind::CallObjectDeferred,2),deferred_hide=action_index(f.world,DialogueActionKind::CallObjectDeferred,2,deferred_play+1);
 const auto heal=action_index(f.world,DialogueActionKind::PlaySound,2),flag=action_index(f.world,DialogueActionKind::SetFlag,2);
 check(show<wait&&wait<deferred_play&&deferred_play<deferred_hide&&deferred_hide<heal&&heal<flag,"Phrase 4 preserves source text, wait, deferred calls, SFX and flag scheduling order");
 check(f.world.action_trace()[show].idle_frame==f.world.action_trace()[flag].idle_frame,"Text and flag begin in the same timer callback");
 remaining=phrase_wait(f.room,f.melody,2);check(remaining==.6,"Checked source minimum input delay is 0.6 seconds");
 unsigned guard_ticks=0,expected_characters=0;bool printed_while_guarded=false;double text_elapsed=0;
 const auto final_characters=std::string("Ninten remembered the tune.").size()+1;
 while(remaining>=0){
  check(!f.world.story_input_allowed(),"Remembered text input remains blocked before timeout");
  const auto before=f.presentation.visible_characters();const bool still_guarded=remaining-dt>=0;
  f.step({},still_guarded,still_guarded);remaining-=dt;++guard_ticks;
  text_elapsed+=dt;while(text_elapsed>f.house.interaction().text_seconds&&expected_characters<final_characters){++expected_characters;text_elapsed-=f.house.interaction().text_seconds;}
  check(f.presentation.visible_characters()==expected_characters,"Blocked A/B preserves source natural printing rate");
  printed_while_guarded|=f.presentation.visible_characters()>before;
  check(f.world.phrase()==2&&!f.presentation.dialogue_closing(),"Blocked A/B cannot close remembered text");
  if(remaining>=0)check(!f.world.story_input_allowed(),"Minimum delay also uses strict-negative expiry");
 }
 check(printed_while_guarded&&f.world.story_input_allowed(),"Text printing proceeds concurrently with minimum input delay");
 f.finish_printing();check(printed(f.presentation)=="Ninten remembered the tune.","Source final text resolves PartyLead");
 for(unsigned i=0;i<60;++i)f.step();
 check(f.world.stage()==OpeningStage::ScriptRunning&&f.presentation.dialogue_finished()&&!f.presentation.dialogue_closing(),"Remembered text never auto-closes after its wait or completion");
 check(f.random.raw_draw_count()==before_draws,"Silent Doll sequence consumes no voice RNG");
 for(const auto&e:f.presentation.take_audio_events())check(e.kind!=HouseAudioKind::VoiceStart,"Silent melody never emits voice playback");
 f.step({},true,false);
 for(const auto&e:f.presentation.take_audio_events())check(e.kind!=HouseAudioKind::Confirm,"Final no-goto melody confirmation closes without an invented InputSound");
 check(f.world.stage()==OpeningStage::Walking&&f.world.story_completed()&&f.runtime.phase()==HousePhase::Idle&&!f.runtime.story_executing(),"Final manual confirmation restores world and House ownership");
 check(f.world.story_talker().kind==DialogueTalkerKind::None,"Source completion clears inherited original NPC talker");
 check(!f.world.actor_bound(doll_actor)&&!f.world.actor_restore_requested(doll_actor),"Original Doll never enters Actor restore lifecycle");
 check(f.world.story_flag("doll_defeated")&&f.world.story_flag("pillow_attack")&&!f.world.story_flag("poltergeist")&&!f.world.story_flag("minnie_leave")&&!f.world.story_flag("minnie_door")&&!f.world.story_flag("phone_ring")&&!f.world.story_flag("talked_to_dad"),"Melody preserves prior flags without inventing later progression");
 bool found_exit_blocker=false;
 for(uint32_t i=0;i<f.room.body_rule_count();++i){const auto body=f.room.body_rule(i);if(f.room.string(body.source_path_string)=="DoorBlock/Entrance"){found_exit_blocker=true;check(f.world.body_enabled(body.body_id),"Melody does not remove the separate Dad-gated outdoor collider");}}
 check(found_exit_blocker,"Source outdoor blocker remains externally defined");
 check(same(f.presentation.npc_pose(f.minnie).position,{472,88}),"Melody does not replay initialization-only Minnie event positions");
 for(unsigned i=0;i<60&&f.presentation.dialogue_active();++i)f.step();
 check(!f.world.actor_bound(f.leader)&&f.presentation.npc_pose(f.doll).visible&&f.world.body_enabled(f.house.npc(f.doll).body_id),"Leader restores while original Doll remains visible and collidable");
 const auto position=f.world.player().position;f.step({1,0});check(f.world.player().position.x>position.x,"Movement input works after final confirmation");
 std::cout<<"Melody timer observations: "<<ticks<<" four-second ticks, "<<guard_ticks<<" minimum-input ticks at float 1/60\n";
}

void check_repeat_doll(Fixture&f){
 const auto audio=f.world.audio_request_count(),effects=f.world.effect_requests().size(),trace=f.world.action_trace().size(),draws=f.random.raw_draw_count();
 f.interact(f.doll);check(f.runtime.phase()==HousePhase::Dialogue&&!f.runtime.story_executing(),"Repeat Doll uses original NPC override instead of replaying program");
 f.finish_printing();check(printed(f.presentation)=="It's not moving anymore.","Repeat Doll source text retained");
 check(!f.presentation.dialogue_pose().name_visible&&!f.presentation.dialogue_stopped(),"Repeat Doll remains unnamed, silent and has no WAIT");
 check(f.world.audio_request_count()==audio&&f.world.effect_requests().size()==effects&&f.world.action_trace().size()==trace,"Repeat interaction emits no melody, effect or story commands");
 check(f.random.raw_draw_count()==draws,"Silent repeat dialogue consumes no RNG");
 check(f.runtime.seen_dialogue(override_for(f.house,f.doll,"doll_melody").seen_key),"Repeat Doll stores its distinct source seen key");f.close_ordinary();
}

void check_exit_after_melody(Fixture&f){
 uint32_t door=house_no_index;for(uint32_t i=0;i<f.house.count(HouseSection::Doors);++i)if(f.house.string(f.house.door(i).source_path)=="Doors/Sister_Upstair")door=i;
 check(door!=house_no_index,"Existing sister-room exit remains in checked geometry");
 f.warp({128,160},{0,1});
 for(unsigned i=0;i<240&&!event_seen(f.runtime,HouseEventKind::DoorStarted);++i)f.step({0,1});
 check(event_seen(f.runtime,HouseEventKind::DoorStarted)&&!f.runtime.story_pending(),"Learned melody disables Mimmie exit guard and permits the existing door");
 for(unsigned i=0;i<240&&(f.runtime.phase()!=HousePhase::Idle||!event_seen(f.runtime,HouseEventKind::DoorDone));++i)f.step();
 check(event_seen(f.runtime,HouseEventKind::DoorDone)&&f.runtime.phase()==HousePhase::Idle,"Existing same-scene door finishes its full fade/warp lifecycle");
 check(same(f.world.player().position,f.house.door(door).destination)&&same(f.world.player().direction,f.house.door(door).direction),"Sister exit uses existing source destination and direction");
}

void check_exit_guard(RoomView room,HouseView house,BattleView font,bool run,float start_y=160){
 Fixture f(room,house,font);f.warp({128,start_y},{0,1});
 for(unsigned i=0;i<240&&f.world.stage()==OpeningStage::Walking;++i)f.step({0,1,run});
 check(f.world.stage()==OpeningStage::ScriptRunning&&f.runtime.story_path()=="Data/Dialogue/Podunk/cutscenes/mimmie_ignore.yaml","Source Mimmie guard intercepts exit before melody");
 if(event_seen(f.runtime,HouseEventKind::DoorStarted)){
  std::cerr<<"Guard overlap case: run="<<run<<" start_y="<<start_y<<" frames="<<f.frames<<" player_y="<<f.world.player().position.y<<'\n';
  for(const auto&e:f.runtime.events())std::cerr<<"event="<<unsigned(e.kind)<<" physics="<<e.physics_tick<<" idle="<<e.idle_frame<<" y="<<e.position.y<<'\n';
 }
 check(!event_seen(f.runtime,HouseEventKind::DoorStarted),"Mimmie guard starts before overlapping sister-room door");
 const auto trigger_position=f.world.player().position;
 for(unsigned i=0;i<8&&!f.presentation.dialogue_active();++i)f.step();
 check(f.world.actor_bound(f.leader)&&f.world.actor_bound(f.house.npc(f.mimmie).room_actor_index),"Exit guard replaces leader and Mimmie only");
 check(f.world.story_talker().kind==DialogueTalkerKind::Actor&&f.world.story_talker().index==f.house.npc(f.mimmie).room_actor_index,"Exit guard selects Mimmie Actor talker");
 const auto guard_program=program_index(room,"Podunk/cutscenes/mimmie_ignore");
 check(house.story_trigger(f.runtime.story_index()).program_index==guard_program,"Guard resolves source program through checked trigger");
 for(unsigned i=0;i<8&&!f.world.story_input_allowed();++i)f.step();
 const auto camera=action_index(f.world,DialogueActionKind::ChangeCamera,0);
 check((f.world.action_trace()[camera].action.flags&1)!=0&&f.world.action_trace()[camera].action.actor==kRoomNoActor,"Guard uses DialogueBox-owned camera rather than an invented actor target");
 f.finish_printing();check(printed(f.presentation)=="Hey, where are you going?!","Original guard question prints before its movement phrase");
 f.presentation.take_audio_events();f.step({},true,false);
 check(f.presentation.dialogue_active()&&!f.presentation.dialogue_closing(),"Contiguous guard phrase preserves its open dialogue box");
 check(printed(f.presentation).find("Hey, where are you going?!")==0,"Guard continuation retains the previous question");
 f.step();
 unsigned confirmations=0;for(const auto&e:f.presentation.take_audio_events()){
  check(e.kind!=HouseAudioKind::MenuClose&&e.kind!=HouseAudioKind::MenuOpen,"Contiguous source guard phrase does not close or reopen its box");
  confirmations+=e.kind==HouseAudioKind::Confirm;
 }
 check(confirmations==1,"Source goto emits one explicit confirmation sound");
 for(unsigned i=0;i<120&&(!f.presentation.dialogue_active()||f.presentation.dialogue_closing());++i)f.step();
 f.finish_printing();check(printed(f.presentation)=="Hey, where are you going?! Please make sure it's dead...","Guard appends its source response after manual confirmation");
 for(unsigned i=0;i<60;++i)f.step();
 const auto leader=f.world.actor(f.leader);
 check(std::abs(leader.position.y-(trigger_position.y-12))<.001f&&leader.position.x==trigger_position.x,"Second guard phrase moves leader twelve source units back into room");
 check(f.world.stage()==OpeningStage::ScriptRunning&&!f.world.story_flag("doll_melody"),"Guard remains manual and never awards melody flag");
 f.presentation.take_audio_events();f.step({},true,false);
 for(const auto&e:f.presentation.take_audio_events())check(e.kind!=HouseAudioKind::Confirm,"Final no-goto guard confirmation closes without an invented InputSound");
 check(f.world.stage()==OpeningStage::Walking&&f.runtime.phase()==HousePhase::Idle&&!f.runtime.story_pending(),"Guard completion restores original world control");
 check(same(f.world.player().direction,{0,-1}),"Completed source guard movement restores the leader facing up");
 check(!event_seen(f.runtime,HouseEventKind::DoorStarted)&&!event_seen(f.runtime,HouseEventKind::PlayerMoved),"Guard completion never traverses the exit door");
 for(unsigned i=0;i<120&&(f.presentation.dialogue_active()||f.world.has_cutscene_actors());++i)f.step();
 if(f.world.has_cutscene_actors()){std::cerr<<"Guard restore case run="<<run<<" start_y="<<start_y<<" stage="<<unsigned(f.world.stage())<<" phase="<<unsigned(f.runtime.phase())<<" player="<<f.world.player().position.x<<","<<f.world.player().position.y<<"\n";for(uint32_t i=0;i<f.world.actor_count();++i)std::cerr<<"actor="<<i<<" bound="<<f.world.actor_bound(i)<<" restore="<<f.world.actor_restore_requested(i)<<"\n";}
 check(!f.world.has_cutscene_actors(),"Guard actor restoration completes through original signal waits");
}

void check_simultaneous_overlap(RoomView room,HouseView house,BattleView font){
 Fixture f(room,house,font);f.warp({128,175.1f},{0,1});
 // The official-engine probe intentionally starts inside both Areas. Source
 // Door.enter resumes first, then guard starts in the same idle cycle. This
 // only verifies initiation: the probe does not establish later fade/warp
 // behavior while both independent source coroutines remain active.
 for(unsigned i=0;i<8&&!(f.runtime.story_executing()&&event_seen(f.runtime,HouseEventKind::DoorStarted));++i)f.step();
 check(event_seen(f.runtime,HouseEventKind::DoorStarted)&&f.runtime.story_executing(),"Artificial simultaneous overlap preserves source Door/guard initiation race");
 size_t door=f.runtime.events().size(),guard=door;
 for(size_t i=0;i<f.runtime.events().size();++i){const auto kind=f.runtime.events()[i].kind;if(kind==HouseEventKind::DoorStarted)door=i;if(kind==HouseEventKind::StoryRequested)guard=i;}
 check(door<guard,"Source simultaneous overlap has Door initiation before guard, not blanket guard priority");
 check(!event_seen(f.runtime,HouseEventKind::PlayerMoved),"Race observation stops at initiation without claiming unverified later warp");
}
}

int main(int argc,char**argv){
 check(argc==4,"Room, house and battle-font packs required");std::string error;RoomData room;HouseData house;BattleData font;
 check(room.load_file(argv[1],error),error.c_str());check(house.load_file(argv[2],error),error.c_str());check(font.load_file(argv[3],error),error.c_str());
 Fixture full(room.view(),house.view(),font.view());check_mimmie(full,false);check_melody(full);check_repeat_doll(full);check_mimmie(full,true);check_exit_after_melody(full);
 Fixture reference(room.view(),house.view(),font.view(),{320,180});check_melody(reference);
 check_exit_guard(room.view(),house.view(),font.view(),true);check_exit_guard(room.view(),house.view(),font.view(),false);
 // The original-engine contact probe also starts at y174, inside the guard
 // but just short of the door's y175 player-hull contact. This is separate
 // from its artificial simultaneous y175.1 overlap race, where source Door
 // initiation wins and a blanket guard-priority expectation would be wrong.
 check_exit_guard(room.view(),house.view(),font.view(),false,174);check_exit_guard(room.view(),house.view(),font.view(),true,174);
 check_simultaneous_overlap(room.view(),house.view(),font.view());
 std::cout<<"Melody integration: "<<checks<<" checks; original NPC interactions, source timers, silent RNG, effects/audio ownership, repeat and existing exit\n";
}
