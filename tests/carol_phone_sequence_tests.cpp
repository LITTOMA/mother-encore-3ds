#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check_result(bool ok,const char*why,unsigned line){++checks;if(!ok){std::cerr<<"Carol/Phone check "<<checks<<" at line "<<line<<": "<<why<<'\n';std::exit(1);}}
#define check(ok,why) do{const bool passed=(ok);check_result(passed,(why),__LINE__);}while(false)
constexpr double dt=double(float(1.0/60));
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool ends_with(const std::string&s,const std::string&tail){return s.size()>=tail.size()&&s.compare(s.size()-tail.size(),tail.size(),tail)==0;}
bool close(float a,float b){return std::abs(a-b)<.0002f;}
uint32_t npc_index(HouseView h,std::string_view path){for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)if(h.string(h.npc(i).source_path)==path)return i;check(false,"Required original NPC absent");return house_no_index;}
uint32_t program_index(RoomView r,std::string_view path){for(uint32_t i=0;i<r.program_count();++i)if(r.string(r.program(i).source_path_string)==path)return i;check(false,"Required source program absent");return kRoomNoIndex;}
uint32_t trigger_index(HouseView h,std::string_view path){for(uint32_t i=0;i<h.count(HouseSection::StoryTriggers);++i)if(h.string(h.story_trigger(i).source_path)==path)return i;check(false,"Required source area absent");return house_no_index;}
uint32_t entrance_body(RoomView r){for(uint32_t i=0;i<r.body_rule_count();++i)if(r.string(r.body_rule(i).source_path_string)=="DoorBlock/Entrance")return r.body_rule(i).body_id;check(false,"Original entrance blocker absent");return kRoomNoIndex;}
uint32_t seen_key(HouseView h,uint32_t npc,std::string_view flag){for(uint32_t i=0;i<h.count(HouseSection::Overrides);++i){const auto o=h.override_dialogue(i);if(o.npc==npc&&h.string(o.flag)==flag)return o.seen_key;}check(false,"Carol override identity absent");return house_no_index;}
size_t action_index(const OpeningWorld&w,DialogueActionKind kind,uint32_t phrase,size_t first=0){const auto&t=w.action_trace();for(size_t i=first;i<t.size();++i)if(t[i].action.kind==kind&&t[i].action.phrase==phrase)return i;check(false,"Expected executed source action absent");return 0;}
std::string printed(const HousePresentation&p){std::string s;for(const auto&line:p.dialogue_pose().lines){if(!s.empty())s+=' ';s+=line.text;}return s;}
unsigned sound_count(const HouseRuntime&r,PhoneSoundKind kind){return unsigned(std::count_if(r.phone_sounds().begin(),r.phone_sounds().end(),[&](const PhoneSoundRequest&s){return s.kind==kind;}));}
std::vector<bool> seen_snapshot(HouseView h,const HouseRuntime&r){std::vector<bool>s;for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)s.push_back(r.seen_dialogue(h.npc(i).seen_key));for(uint32_t i=0;i<h.count(HouseSection::Overrides);++i)s.push_back(r.seen_dialogue(h.override_dialogue(i).seen_key));return s;}
void no_confirm(HousePresentation&p){for(const auto&e:p.take_audio_events())check(e.kind!=HouseAudioKind::Confirm,"Automatic/hidden progression emits no synthetic Confirm");}

struct Fixture {
 RoomView room;HouseView house;SourceRandom random{123};OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;PhoneRuntime phone;
 uint32_t carol,carol_actor,leader,blocker;uint64_t frames=0;uint32_t phrase_after_print=0;bool blocker_before_end_scene=false;
 Fixture(RoomView r,HouseView h,BattleView font,PhoneView p,Vec2 viewport,bool ring_flag=false):room(r),house(h),carol(npc_index(h,"Objects/npc")),carol_actor(h.npc(carol).room_actor_index),leader(r.scene().player_instance_index),blocker(entrance_body(r)){
  check(world.initialize(r,viewport),world.error());world.attach_random(random);
  // BOUNDED POST-MELODY FIXTURE, not a fresh whole-game playthrough: provide
  // only completed main-route flags and a safe living-room starting position.
  // Pillow/Minnie, cash, inventory and any save controller are deliberately absent.
  for(auto flag:{"doll_attack","doll_defeated","doll_melody","mimmie_door_opened"})check(world.set_story_flag(flag,true,true),"Post-melody fixture flag exists");
  check(world.set_story_flag("poltergeist",false,true),"Post-melody clears Poltergeist");
  check(world.set_story_flag("phone_ring",ring_flag,true),"Fixture phone flag exists");
  check(presentation.begin(h,font,random),presentation.error());
  check(world.warp_same_scene({148,720},{0,-1}),world.error());
  check(runtime.initialize(h,world,presentation),runtime.error());
  check(phone.initialize(p),phone.error());check(runtime.bind_phone(phone),runtime.error());
  check(!phone.ringing(0),"Initialization never invents Ring playback from flags");
 }
 void step(WalkInput input={},bool accept=false,bool cancel=false){
  ++frames;check(runtime.before_physics(input),runtime.error());
  check(presentation.physics_frame(dt,world.player().position),presentation.error());phrase_after_print=world.phrase();
  check(world.advance(input),world.error());check(runtime.after_physics(),runtime.error());
  check(world.idle_frame(dt),world.error());check(presentation.idle_frame(dt),presentation.error());
  const bool ok=runtime.idle_frame(dt,accept,cancel);
  if(!ok)std::cerr<<"frame="<<frames<<" stage="<<unsigned(world.stage())<<" phrase="<<world.phrase()<<" requested="<<world.pending_dialogue_id()<<" presentation="<<presentation.error()<<'\n';
  check(ok,runtime.error());
  blocker_before_end_scene=world.body_enabled(blocker);
  check(world.end_scene_frame(),world.error());
 }
 void warp(Vec2 p,Vec2 d){check(world.warp_same_scene(p,d),world.error());check(runtime.after_physics(),runtime.error());}
 void await_text(){for(unsigned i=0;i<16&&!presentation.dialogue_active();++i)step();if(!presentation.dialogue_active())std::cerr<<"await text: house="<<unsigned(runtime.phase())<<" world="<<unsigned(world.stage())<<" index="<<runtime.story_index()<<" pos="<<world.player().position.x<<","<<world.player().position.y<<" error="<<runtime.error()<<"\n";check(presentation.dialogue_active(),"Source interaction presents its checked text");}
 void interact_carol(){const auto p=presentation.npc_pose(carol).position;warp({p.x,p.y+27},{0,-1});step({},true);await_text();}
 void interact_phone(){warp({148,713},{0,-1});step({},true);await_text();}
 unsigned finish_text(uint32_t phrase){
  unsigned waits=0;
  for(unsigned i=0;i<3000&&!presentation.dialogue_finished();++i){
   check(world.phrase()==phrase,"Manual phrase cannot auto-advance");
   if(presentation.dialogue_stopped()){++waits;for(unsigned n=0;n<3;++n)step();check(presentation.dialogue_stopped(),"WAIT remains blocked without input");step({},true);}
   else step();
  }
  check(world.phrase()==phrase&&presentation.dialogue_finished(),"Source text reaches final manual confirmation");return waits;
 }
 void settle_open_animation(){for(double elapsed=0;elapsed<house.clip(house.clip_for(HouseClipRole::DialogueOpen)).duration;elapsed+=dt)step();}
 void confirm(uint32_t next){settle_open_animation();check(presentation.dialogue_finished(),"Confirm supplied only after source text completion");step({},true);check(world.phrase()==next,"Manual confirmation selects next original label");}
 void close_story(){
  settle_open_animation();check(presentation.dialogue_finished(),"Final source phrase finished before close");step({},true);
  for(unsigned i=0;i<300&&(world.stage()!=OpeningStage::Walking||world.has_cutscene_actors()||presentation.dialogue_active());++i)step();
  if(world.stage()!=OpeningStage::Walking||runtime.phase()!=HousePhase::Idle||runtime.story_executing()||runtime.blocks_player())std::cerr<<"close state: world="<<unsigned(world.stage())<<" house="<<unsigned(runtime.phase())<<" executing="<<runtime.story_executing()<<" paused="<<world.house_paused()<<" phrase="<<world.phrase()<<" text="<<printed(presentation)<<" frames="<<frames<<"\n";
  check(world.stage()==OpeningStage::Walking&&runtime.phase()==HousePhase::Idle&&!runtime.story_executing()&&!runtime.blocks_player(),"Final source confirmation restores movement ownership");
  check(!world.has_cutscene_actors()&&!presentation.dialogue_active(),"All source Actor restoration and dialogue close work complete");
 }
 void no_optional_prerequisites()const{
  for(auto f:{"pillow_attack","minnie_leave","minnie_door","carol_ask_key","mick_telepathy","got_diary","saved"})check(!world.story_flag(f),"Phone route neither requires nor invents optional progression");
 }
};

void check_source_geometry(Fixture&f){
 const auto call=f.house.story_trigger(trigger_index(f.house,"Cutscene Area4")),reminder=f.house.story_trigger(trigger_index(f.house,"Cutscene Area3"));
 check(same(call.center,{136,816})&&same(call.extents,{32,8}),"Area4 includes inherited (4,4) shape offset and (8,2) scale");
 check(same(reminder.center,{136,824})&&same(reminder.extents,{32,8}),"Area3 includes inherited shape offset and scale");
 check(call.program_index==program_index(f.room,"Podunk/cutscenes/carol_call")&&reminder.program_index==program_index(f.room,"Podunk/cutscenes/carol_phone"),"Both source areas bind their distinct programs");
 const auto p=f.phone.view().object(0);
 check(f.phone.view().string(p.source_path)=="Objects/Phone"&&same(p.position,{148,677})&&same(p.sprite_center,{148,680}),"Source Phone retains original object and sprite positions");
 check(same(p.interact_center,{148,692})&&same(p.collider_center,{149,685.5f}),"Phone interaction and collision stay separate from sprite position");
 check((p.policy&uint32_t(PhonePolicy::Free))!=0,"House phone uses the free source object policy");
 for(uint32_t i=0;i<f.house.count(HouseSection::Npcs);++i)check(f.house.string(f.house.npc(i).source_path)!="Objects/Phone","Phone is not fabricated as an NPC");
}
void check_phone_identity(Fixture&f,const std::vector<bool>&seen){
 check(!f.world.has_cutscene_actors(),"Phone dialogue creates no Actor proxies");
 check(f.world.story_talker().kind==DialogueTalkerKind::None,"Phone has no invented NPC or Actor talker");
 check(seen_snapshot(f.house,f.runtime)==seen,"Phone preserves every original NPC seen identity");
}
void check_no_answer(Fixture&f){
 const auto seen=seen_snapshot(f.house,f.runtime);const auto rings=sound_count(f.runtime,PhoneSoundKind::Ring),hangups=sound_count(f.runtime,PhoneSoundKind::Hangup);
 for(unsigned i=0;i<90;++i)f.step();
 check(!f.phone.ringing(0)&&f.runtime.phone_sounds().empty(),"Post-melody flags alone produce no ring or sound");
 f.interact_phone();check(f.runtime.phase()==HousePhase::StoryRunning,"Phone before phone_ring opens no-answer program");
 check_phone_identity(f,seen);check(!f.presentation.dialogue_pose().name_visible,"No-answer narration has no name tag");
 check(f.finish_text(0)==0&&printed(f.presentation)=="Beeep...","Original no-answer line prints without a source WAIT");
 check(sound_count(f.runtime,PhoneSoundKind::Hangup)==hangups+1&&sound_count(f.runtime,PhoneSoundKind::Ring)==rings,"No-answer interaction plays exactly one Hangup request");
 check(!f.world.story_flag("phone_ring")&&!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"No-answer leaves Dad and entrance gates unchanged");
 f.close_story();check_phone_identity(f,seen);f.no_optional_prerequisites();
}

void check_carol_call(Fixture&f,bool area){
 const uint32_t automatic=area?2:1,hidden=automatic+1,ring=automatic+2,answer=automatic+3,final=automatic+4;
 const auto original=f.presentation.npc_pose(f.carol).position;const auto seen_before=seen_snapshot(f.house,f.runtime);const auto trace_start=f.world.action_trace().size();
 Vec2 entered{};
 if(area){
  f.warp({136,795},{0,1});for(unsigned i=0;i<4;++i)f.step();
  check(!f.runtime.story_executing(),"Flags and nearby position outside Area4 do not start Carol");
  entered={136,806};f.warp(entered,{0,1});
  for(unsigned i=0;i<16&&!f.runtime.story_executing();++i)f.step();
  f.await_text();check(f.runtime.story_index()==trigger_index(f.house,"Cutscene Area4"),"Actual Area4 overlap starts area Carol program");
 }else f.interact_carol();
 check(f.runtime.phase()==HousePhase::StoryRunning,"Carol dispatch selects original source program");
 for(unsigned i=0;i<16&&!f.world.story_input_allowed();++i)f.step();
 check(f.world.actor_bound(f.carol_actor)&&f.world.actor_bound(f.leader)==area,"Direct Carol binds only Carol; area version also binds leader");
 check(!f.presentation.npc_pose(f.carol).visible&&!f.world.body_enabled(f.house.npc(f.carol).body_id),"Actor replacement hides original Carol and disables original collider");
 check(f.world.story_talker().kind==DialogueTalkerKind::Actor&&f.world.story_talker().index==f.carol_actor,"Carol source actor owns speech");
 check(f.presentation.dialogue_pose().speaker=="Carol"&&f.presentation.dialogue_pose().name_visible,"Original Carol voice/name presentation retained");
 check(f.finish_text(0)==(area?0:2),"Direct and area initial phrases retain their different source WAIT counts");
 if(area){
  check(printed(f.presentation)=="Are you okay, Ninten?","Area greeting is its own original phrase");f.confirm(1);
  check(f.world.actor(f.leader).moving&&f.world.actor(f.leader).movement.speed==64,"Area phrase1 begins original speed64 movement before text completes");
  bool queued=false;for(const auto&t:f.world.actor(f.leader).turns)queued|=t.waiting;check(queued,"Source leader turn is queued behind movement action");
  check(f.finish_text(1)==1,"Area second phrase has one original WAIT");
  check(close(f.world.actor(f.leader).position.y,entered.y-32)&&close(f.world.actor(f.leader).position.x,entered.x),"Area leader follows original up32 path at speed64");
  check(f.world.actor(f.leader).direction.x>0&&f.world.actor(f.leader).direction.y<0,"Queued leader turn faces Carol after source movement/wait");
  f.confirm(automatic);
 }else{
  check(printed(f.presentation)=="Are you okay, Ninten? What on earth is happening to our home? I'm so scared...","Direct initial Carol phrase preserves all three source lines");
  f.confirm(automatic);
 }
 check(!f.phone.ringing(0)&&!f.world.story_flag("phone_ring"),"Phone remains idle until original ring command");
 f.presentation.take_audio_events();
 unsigned auto_ticks=0;
 while(f.world.phrase()==automatic&&auto_ticks++<1000){f.step();no_confirm(f.presentation);}
 check(f.world.phrase()==hidden&&f.phrase_after_print==hidden,"Natural text completion advances in physics callback without a fake Accept");
 check(!f.presentation.dialogue_pose().text_visible&&!f.world.story_input_allowed(),"Source label4 hides text and disables input");
 const auto hide_action=action_index(f.world,DialogueActionKind::HideDialogue,hidden,trace_start),wait_action=action_index(f.world,DialogueActionKind::StartWait,hidden,trace_start);
 check(hide_action<wait_action,"Source hidden phrase hides before starting its wait");
 check(f.world.action_trace()[wait_action].action.duration==.3,"Original hidden wait remains 0.3 seconds");
 // This timer is born in the physics text callback, before the same frame's
 // idle Timer processing, so the first dt has already elapsed.
 double remaining=.3-dt;unsigned hidden_ticks=1;
 while(remaining>=0){check(f.world.phrase()==hidden&&!f.phone.ringing(0),"Ring cannot start before hidden wait expires");f.step({},true,true);remaining-=dt;++hidden_ticks;no_confirm(f.presentation);if(remaining>=0)check(f.world.phrase()==hidden,"Hidden wait uses source strict-negative expiry");}
 check(f.world.phrase()==ring&&f.phone.ringing(0)&&!f.world.story_flag("phone_ring"),"Ring begins after wait but before phone_ring flag phase");
 check(!f.presentation.dialogue_pose().text_visible&&!f.world.story_input_allowed(),"Original ring pause keeps dialogue hidden and blocks input");
 check(f.phone.clip_time(0)==0&&f.phone.pose(0).frame==0,"Deferred _ring starts after this frame's Phone animation phase");
 check(sound_count(f.runtime,PhoneSoundKind::Ring)==0,"Deferred ring command itself does not synthesize a sound pulse");
 const auto ring_wait=action_index(f.world,DialogueActionKind::StartWait,ring,trace_start),deferred=action_index(f.world,DialogueActionKind::CallObjectDeferred,ring,trace_start),turn=action_index(f.world,DialogueActionKind::TurnActor,ring,trace_start);
 check(ring_wait<deferred&&deferred<turn,"Ring source scheduling preserves wait, deferred object call, Carol turn order");
 check(f.world.action_trace()[ring_wait].action.duration==1&&f.world.action_trace()[turn].action.duration==.05&&same(f.world.action_trace()[turn].action.vector,{-1,0}),"Original one-second autowait and 0.05-second Carol left turn preserved");
 remaining=1;unsigned ring_ticks=0;
 while(remaining>=0){f.step({},true,true);remaining-=dt;++ring_ticks;no_confirm(f.presentation);check(!f.world.story_flag("phone_ring"),"Ringing animation does not write story flag early");if(remaining>=0)check(f.world.phrase()==ring,"A/B cannot skip one-second ring timer");if(ring_ticks==1)check(f.phone.pose(0).frame==1&&f.phone.clip_time(0)==dt,"Phone animation advances once per shared idle, including cutscene");}
 check(f.world.phrase()==answer&&sound_count(f.runtime,PhoneSoundKind::Ring)==2,"One source ring cycle emits exactly its 0.083/0.332 sound keys");
 const auto phone_object=f.phone.view().object(0);for(const auto&s:f.runtime.phone_sounds())if(s.kind==PhoneSoundKind::Ring)check(s.object==0&&s.resource==phone_object.ring_sound&&s.bus==phone_object.audio_bus&&s.positional&&same(s.position,phone_object.audio_center),"Every ring pulse retains original resource, player identity, bus and position");
 check(close(f.world.actor(f.carol_actor).direction.x,-1)&&close(f.world.actor(f.carol_actor).direction.y,0),"Carol completes source left turn during hidden ring pause");
 check(f.finish_text(answer)==0&&printed(f.presentation)=="Oh! That must be him.","Source answer recognition line is manual");
 check(!f.world.story_flag("phone_ring"),"Flag remains false until label7 starts");const auto printed_before_final=f.presentation.visible_characters();f.confirm(final);
 check(f.world.story_flag("phone_ring")&&f.presentation.visible_characters()==printed_before_final,"phone_ring changes synchronously with final line, before printing");
 const auto show=action_index(f.world,DialogueActionKind::ShowDialogue,final,trace_start),flag=action_index(f.world,DialogueActionKind::SetFlag,final,trace_start);
 check(show<flag&&f.world.action_trace()[show].idle_frame==f.world.action_trace()[flag].idle_frame,"Final text precedes flag in the same source command phase");
 check(f.finish_text(final)==0&&ends_with(printed(f.presentation),"Could you get it, Ninten?"),"Final Carol line preserves source name substitution");
 f.close_story();
 check(f.presentation.npc_pose(f.carol).visible&&same(f.presentation.npc_pose(f.carol).position,original)&&f.world.body_enabled(f.house.npc(f.carol).body_id),"Original Carol regains source pose, visibility and collision");
 check(!f.world.actor_bound(f.carol_actor)&&!f.world.actor_restore_requested(f.carol_actor)&&f.world.story_talker().kind==DialogueTalkerKind::None,"Carol proxy and talker fully release through original-NPC restoration");
 if(area){check(seen_snapshot(f.house,f.runtime)==seen_before,"Area cutscene creates no NPC-interaction seen flag");check(close(f.world.player().position.y,entered.y-32),"Area restores leader at actual moved position");}
 else check(f.runtime.seen_dialogue(seen_key(f.house,f.carol,"doll_melody")),"Direct source override marks its own original seen identity");
 check(f.phone.ringing(0)&&f.world.story_flag("phone_ring")&&!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"Carol completion keeps ring active and Dad-gated entrance blocked");
 f.no_optional_prerequisites();std::cout<<(area?"Area":"Direct")<<" Carol: "<<auto_ticks<<" natural text ticks, "<<hidden_ticks<<" hidden timer ticks, "<<ring_ticks<<" ring timer ticks\n";
}

void check_direct_reminder(Fixture&f){
 f.interact_carol();check(!f.world.has_cutscene_actors(),"Direct phone reminder does not invent Carol Actor binding");
 check(f.world.story_talker().kind==DialogueTalkerKind::OriginalNpc&&f.world.story_talker().index==f.carol,"Direct reminder retains original Carol as talker");
 check(f.finish_text(0)==0&&printed(f.presentation)=="Telephone! Ninten, would you please get it?","Last matching phone_ring override selects source reminder");
 check(f.runtime.seen_dialogue(seen_key(f.house,f.carol,"phone_ring")),"Direct phone reminder stores its distinct seen key");f.close_story();
}
void check_area_reminder(Fixture&f){
 const auto seen=seen_snapshot(f.house,f.runtime);f.warp({136,800},{0,1});for(unsigned i=0;i<5;++i)f.step();
 check(!f.runtime.story_executing(),"Ringing flags alone do not replay Area4");const Vec2 entered{136,808};f.warp(entered,{0,1});
 for(unsigned i=0;i<16&&!f.runtime.story_executing();++i){f.step();}f.await_text();
 check(f.runtime.story_index()==trigger_index(f.house,"Cutscene Area3"),"Entrance overlap selects Area3 reminder, not first Dad call");
 for(unsigned i=0;i<16&&!f.world.story_input_allowed();++i)f.step();
 check(f.world.actor_bound(f.leader)&&f.world.actor_bound(f.carol_actor),"Area3 binds original leader and Carol");
 check(f.finish_text(0)==0&&printed(f.presentation)=="Telephone! Ninten, would you please get it?","Area reminder retains original text");
 check(close(f.world.actor(f.leader).position.y,entered.y-16),"Area3 executes wait0.1 then up16 at speed64");
 f.close_story();check(close(f.world.player().position.y,entered.y-16)&&seen_snapshot(f.house,f.runtime)==seen,"Area3 restores moved leader without NPC seen mutation");
 check(!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"Area reminder cannot grant Dad progression");
}
void check_hint_colors(const HousePresentation&p,const std::vector<std::string>&hints){
 const auto pose=p.dialogue_pose();std::string text;std::vector<uint32_t>colors;
 for(const auto&line:pose.lines){check(line.text.size()==line.colors.size(),"Every rendered byte retains its source color");if(!text.empty()){text+=' ';colors.push_back(0xffffffffu);}text+=line.text;colors.insert(colors.end(),line.colors.begin(),line.colors.end());}
 // Original translated [color] runs use the audited dialogue hint ea8b2c,
 // packed by the shared renderer as ABGR, followed by an explicit white reset.
 for(const auto&hint:hints){const auto at=text.find(hint);check(at!=std::string::npos,"Original highlighted hint text is present");for(size_t i=at;i<at+hint.size();++i)if(text[i]!=' ')check(colors[i]==0xff2c8beau,"Hint letters retain original ea8b2c color");if(at)check(colors[at-1]==0xffffffffu,"Text before hint stays white");if(at+hint.size()<colors.size())check(colors[at+hint.size()]==0xffffffffu,"Color reset returns following text to white");}
}
void check_first_dad(Fixture&f){
 const auto seen=seen_snapshot(f.house,f.runtime);const auto trace_start=f.world.action_trace().size();const auto ring_requests=sound_count(f.runtime,PhoneSoundKind::Ring),hangups=sound_count(f.runtime,PhoneSoundKind::Hangup);
 f.interact_phone();const auto ring_at_pickup=sound_count(f.runtime,PhoneSoundKind::Ring);
 check(ring_at_pickup>=ring_requests&&!f.phone.ringing(0),"Phone pickup selects Idle immediately after any due source pulse");
 check(sound_count(f.runtime,PhoneSoundKind::Hangup)==hangups+1&&f.runtime.phone_sounds().back().kind==PhoneSoundKind::Hangup,"Pickup replaces the source audio player with Hangup");
 const auto sound=f.runtime.phone_sounds().back();const auto object=f.phone.view().object(0);
 check(sound.object==0&&sound.positional&&same(sound.position,object.audio_center)&&sound.bus==object.audio_bus&&sound.resource==object.hangup_sound,"Hangup retains same source object, bus and positional binding as Ring");
 check_phone_identity(f,seen);check(f.world.body_enabled(f.blocker)&&!f.world.story_flag("talked_to_dad"),"Picking up does not set talked_to_dad early");
 check(f.finish_text(0)==0&&printed(f.presentation)=="Ninten picked up the phone."&&!f.presentation.dialogue_pose().name_visible,"Original pickup narration stays unnamed");
 f.confirm(1);check(f.finish_text(1)==5,"First Dad explanation retains five original WAIT markers");check_phone_identity(f,seen);
 check(f.presentation.dialogue_pose().speaker=="Dad"&&f.presentation.dialogue_pose().name_visible,"Adult Dad dialogue uses original name tag");check_hint_colors(f.presentation,{"PSI","diary"});
 check(!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"Long first Dad explanation does not unlock entrance");
 const auto camera_before=f.world.cutscene_camera().global_position();f.confirm(2);
 const auto move=action_index(f.world,DialogueActionKind::MoveCamera,2,trace_start);check(f.world.action_trace()[move].action.flags==1&&f.world.action_trace()[move].action.duration==1,"Source x-only camera command retains one-second tween");
 for(unsigned i=0;i<61;++i){f.step();check(close(f.world.cutscene_camera().global_position().y,camera_before.y),"Absolute camera X movement preserves current global Y");}
 if(!close(f.world.cutscene_camera().global_position().x,96))std::cerr<<"Dad camera global="<<f.world.cutscene_camera().global_position().x<<","<<f.world.cutscene_camera().global_position().y<<" center="<<f.world.cutscene_camera().center().x<<","<<f.world.cutscene_camera().center().y<<" before="<<camera_before.x<<","<<camera_before.y<<"\n";
 check(close(f.world.cutscene_camera().global_position().x,96),"Dad camera reaches source absolute world X96");
 check(f.finish_text(2)==1,"Basement hint retains one original WAIT");check_phone_identity(f,seen);check_hint_colors(f.presentation,{"basement","key"});
 check(!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"Basement/key hints do not grant progress or unlock entrance");
 f.confirm(3);for(unsigned i=0;i<61;++i)f.step();
 check(same(f.world.cutscene_camera().global_position(),f.world.player().position),"Source returncam restores player camera over one second");
 check(f.finish_text(3)==3,"Dad adventure/save explanation retains three original WAIT markers");check_phone_identity(f,seen);check_hint_colors(f.presentation,{"SAVE your progress","call your dear old dad"});
 check(!f.world.story_flag("talked_to_dad")&&f.world.body_enabled(f.blocker),"Save advice is still before the final progression command");
 const auto printed_before_final=f.presentation.visible_characters();f.confirm(4);check(f.world.story_flag("talked_to_dad")&&f.presentation.visible_characters()==printed_before_final,"Final click phase sets talked_to_dad before its text prints");
 // FlagLandmark queues deletion in the flag signal and SceneTree flushes it
 // after this frame's callbacks. The fixture observes both sides of that flush.
 check(f.blocker_before_end_scene&&!f.world.body_enabled(f.blocker),"DoorBlock remains until the final flag frame's deferred deletion flush");
 f.step();check(!f.world.body_enabled(f.blocker)&&!f.presentation.dialogue_finished(),"DoorBlock stays removed while final click text is printing");
 const auto show=action_index(f.world,DialogueActionKind::ShowDialogue,4,trace_start),flag=action_index(f.world,DialogueActionKind::SetFlag,4,trace_start);
 check(show<flag&&f.world.action_trace()[show].idle_frame==f.world.action_trace()[flag].idle_frame,"Dad final source text and flag execute in original same-phase order");
 check(f.finish_text(4)==0&&ends_with(printed(f.presentation),"(Click! Beep-beep-beep...)")&&!f.presentation.dialogue_pose().name_visible,"Final phone narration preserves exact original text and no name tag");
 check_phone_identity(f,seen);f.close_story();for(unsigned i=0;i<180;++i)f.step();
 check_phone_identity(f,seen);check(sound_count(f.runtime,PhoneSoundKind::Ring)==ring_at_pickup&&!f.phone.ringing(0)&&f.phone.pose(0).frame==0,"No ringing pulses resume during or after first Dad call");
 check(f.world.story_flag("phone_ring")&&f.world.story_flag("talked_to_dad")&&!f.world.body_enabled(f.blocker),"First Dad retains ring history and only unlocks its intended entrance gate");
 for(size_t i=trace_start;i<f.world.action_trace().size();++i)check(f.world.action_trace()[i].action.kind!=DialogueActionKind::BindActor,"First Dad never binds any Actor");
 f.no_optional_prerequisites();
 // Probe the actual reminder area away from the separate unported exterior.
 f.warp({136,808},{0,1});for(unsigned i=0;i<8;++i)f.step();check(!f.runtime.story_pending()&&!f.runtime.story_executing(),"talked_to_dad disables the source entrance reminder");
}
}
int main(int argc,char**argv){
 check(argc==5,"Room, House, Battle-font and Phone packs required");std::string error;RoomData room;HouseData house;BattleData font;PhoneData phone;
 check(room.load_file(argv[1],error),error.c_str());check(house.load_file(argv[2],error),error.c_str());check(font.load_file(argv[3],error),error.c_str());check(phone.load_file(argv[4],error),error.c_str());
 for(const Vec2 viewport:{Vec2{400,240},Vec2{320,180}}){
  Fixture direct(room.view(),house.view(),font.view(),phone.view(),viewport);check_source_geometry(direct);check_no_answer(direct);check_carol_call(direct,false);check_direct_reminder(direct);check_first_dad(direct);
  Fixture area(room.view(),house.view(),font.view(),phone.view(),viewport);check_carol_call(area,true);check_area_reminder(area);check_first_dad(area);
  Fixture loaded_flag(room.view(),house.view(),font.view(),phone.view(),viewport,true);for(unsigned i=0;i<180;++i)loaded_flag.step();check(!loaded_flag.phone.ringing(0)&&loaded_flag.runtime.phone_sounds().empty(),"Existing phone_ring flag does not replay dormant PhoneRing animation on initialization");
  std::cout<<"Carol/Phone shared pipeline passed at "<<viewport.x<<'x'<<viewport.y<<"\n";
 }
 std::cout<<"Carol/Phone integration: "<<checks<<" checks; bounded post-melody fixtures, direct/area Carol, original-NPC restoration, no-answer, first Dad, camera/color/flag timing and Phone audio ownership. No fresh-game/save-UI/hardware claim.\n";
}
