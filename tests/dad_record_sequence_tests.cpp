#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/save_menu.hpp"
#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <fstream>
#include <iterator>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check_result(bool ok,const char* why,unsigned line){++checks;if(!ok){std::cerr<<"Dad/Record check "<<checks<<" at line "<<line<<": "<<why<<'\n';std::exit(1);}}
#define check(ok,why) do{const bool passed=(ok);check_result(passed,(why),__LINE__);}while(false)
constexpr double dt=double(float(1.0/60));
// Source label positions in Reusable/dad_normal, not shipping content bindings.
constexpr uint32_t greeting=0,earned=10,question=11,record=12,saved_reply=13,farewell=14,hangup=15;
std::string printed(const HousePresentation&p){std::string s;for(const auto&line:p.dialogue_pose().lines){if(!line.text.empty()){if(!s.empty())s+=' ';s+=line.text;}}return s;}
bool ends_with(const std::string&s,const std::string&end){return s.size()>=end.size()&&s.compare(s.size()-end.size(),end.size(),end)==0;}
uint32_t program_index(RoomView r,std::string_view path){for(uint32_t i=0;i<r.program_count();++i)if(r.string(r.program(i).source_path_string)==path)return i;check(false,"Reviewed source program exists");return kRoomNoIndex;}
unsigned action_count(const OpeningWorld&w,DialogueActionKind kind){return unsigned(std::count_if(w.action_trace().begin(),w.action_trace().end(),[&](const OpeningTraceEvent&e){return e.action.kind==kind;}));}
std::vector<bool> seen_snapshot(HouseView h,const HouseRuntime&r){std::vector<bool>result;for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)result.push_back(r.seen_dialogue(h.npc(i).seen_key));for(uint32_t i=0;i<h.count(HouseSection::Overrides);++i)result.push_back(r.seen_dialogue(h.override_dialogue(i).seen_key));return result;}
struct Fixture {
 HouseView house;SourceRandom random{123};OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;PhoneRuntime phone;DialogueChoices choices;SaveMenu menu;
 const DialogueChoicesData&choice_data;const SaveMenuData&menu_data;std::vector<bool>seen;
 int64_t cash=7,bank=55,earned_cash=15;unsigned earned_reads=0,bank_reads=0,current_reads=0,frames=0,selection_sounds=0,save_requests=0;bool active_after_physics=false;uint32_t saved_generation=0;
 Fixture(RoomView room,HouseView h,BattleView font,PhoneView p,const DialogueChoicesData&c,const SaveMenuData&m,Vec2 viewport,bool earned_flag,bool previous_saved=false,bool first_dad_complete=true,std::string_view leader="ninten"):
  house(h),choice_data(c),menu_data(m){
  check(world.initialize(room,viewport),world.error());world.attach_random(random);world.set_party_leader(leader);
  // Bounded post-melody fixture. The first-call case below earns talked_to_dad
  // through the actual phone script; other cases start immediately after it.
  for(auto flag:{"doll_attack","doll_defeated","doll_melody","mimmie_door_opened","phone_ring"})check(world.set_story_flag(flag,true,true),"Reviewed post-melody flag exists");
  check(world.set_story_flag("poltergeist",false,true),"Poltergeist source flag exists");
  check(world.set_story_flag("talked_to_dad",first_dad_complete,true),"First-Dad flag exists");
  check(world.set_story_flag("earned_cash",earned_flag,false),"Earned flag exists");check(world.set_story_flag("saved",previous_saved,false),"Saved flag exists");
  check(presentation.begin(h,font,random),presentation.error());
  presentation.set_text_value_callback([](void*state,HouseTokenKind kind,std::string&out){return static_cast<Fixture*>(state)->text_value(kind,out);},this);
  check(world.warp_same_scene({148,713},{0,-1}),world.error());check(runtime.initialize(h,world,presentation),runtime.error());
  check(phone.initialize(p),phone.error());check(runtime.bind_phone(phone),runtime.error());runtime.bind_choices(c,choices);seen=seen_snapshot(h,runtime);
 }
 bool text_value(HouseTokenKind kind,std::string&out){
  if(kind==HouseTokenKind::EarnedCash){++earned_reads;check(world.phrase()==earned,"EarnedCash capture occurs at source phrase entry");check(earned_reads==1,"One EarnedCash replacement per entered deposit phrase");out=std::to_string(earned_cash);earned_cash=0;return world.set_story_flag("earned_cash",false,false);}
  if(kind==HouseTokenKind::BankCash){++bank_reads;out=std::to_string(bank);return true;}
  if(kind==HouseTokenKind::CurrentCash){++current_reads;out=std::to_string(cash);return true;}
  return false;
 }
 void step(bool accept=false,bool cancel=false,WalkInput input={}){
  ++frames;check(runtime.before_physics(input),runtime.error());check(presentation.physics_frame(dt,world.player().position),presentation.error());active_after_physics=choices.active();
  check(world.advance(input),world.error());check(runtime.after_physics(),runtime.error());check(world.idle_frame(dt),world.error());check(presentation.idle_frame(dt),presentation.error());
  const bool okay=runtime.idle_frame(dt,accept,cancel);if(!okay)std::cerr<<"frame="<<frames<<" phrase="<<world.phrase()<<" world="<<unsigned(world.stage())<<" choice="<<unsigned(choices.phase())<<" presentation="<<presentation.error()<<'\n';check(okay,runtime.error());check(world.end_scene_frame(),world.error());
 }
 void interact_phone(){
  check(!menu.is_open()&&choices.phase()==DialogueChoicesPhase::Closed,"A new phone interaction starts outside both menus");
  check(world.warp_same_scene({148,713},{0,-1}),world.error());check(runtime.after_physics(),runtime.error());step(true);
  for(unsigned i=0;i<16&&!presentation.dialogue_active();++i)step();
  check(presentation.dialogue_active()&&runtime.story_executing(),"Actual source phone interaction starts dialogue");
  check(!world.has_cutscene_actors()&&world.story_talker().kind==DialogueTalkerKind::None,"Phone does not manufacture an Actor or NPC talker");
 }
 void settle_open(){for(double t=0;t<house.clip(house.clip_for(HouseClipRole::DialogueOpen)).duration;t+=dt)step();}
 unsigned finish_text(uint32_t phase){
  unsigned waits=0;presentation.take_audio_events();
  for(unsigned i=0;i<3000&&!presentation.dialogue_finished();++i){
   check(world.phrase()==phase,"Source phrase waits for its real manual input");
   if(presentation.dialogue_stopped()){
    ++waits;for(unsigned n=0;n<3;++n)step();check(presentation.dialogue_stopped(),"Source WAIT remains blocked without input");
    step(true);check(!presentation.dialogue_stopped(),"Real Accept releases a source WAIT, including before choices");
    unsigned confirms=0;for(const auto&e:presentation.take_audio_events())confirms+=e.kind==HouseAudioKind::Confirm;check(confirms==1,"One actual WAIT Accept emits one source Confirm");
   }else {step();for(const auto&e:presentation.take_audio_events())check(e.kind!=HouseAudioKind::Confirm,"Natural printing/completion emits no synthetic Confirm");}
  }
  check(presentation.dialogue_finished()&&world.phrase()==phase,"Natural text printer reaches source phrase completion");return waits;
 }
 void confirm(uint32_t next){settle_open();check(presentation.dialogue_finished(),"Only finished source text receives manual confirmation");step(true);check(world.phrase()==next,"Original next label selected");}
 void prepare_options(bool has_earned,uint32_t leader_phrase=0){
  interact_phone();check(world.story_program_index()==program_index(world.content(),"Reusable/dad_normal"),"Phone override resolves actual Dad-normal program");
  check(finish_text(greeting)==0&&ends_with(printed(presentation),"Ninten? It's your dad."),"Original Dad greeting prints");
  const uint32_t after_leader=has_earned?earned:question;
  confirm(leader_phrase?leader_phrase:after_leader);
  if(leader_phrase){check(finish_text(leader_phrase)>=1,"Alternate leader source greeting retains its WAIT");confirm(after_leader);}
  if(has_earned){
   check(earned_reads==1&&earned_cash==0&&!world.story_flag("earned_cash"),"Deposit interpolation captures then clears amount and flag before printing");
   check(bank==55&&cash==7,"Deposit report does not award the same money twice");
   check(finish_text(earned)==0&&ends_with(printed(presentation),"I've deposited $15 into your bank account."),"Original deposit amount is retained after capture resets live amount");confirm(question);
  }else check(earned_reads==0&&earned_cash==15,"False earned flag skips interpolation and leaves stored amount untouched");
  check(world.story_choices_waiting()&&choices.phase()==DialogueChoicesPhase::WaitingText&&!choices.pose().visible,"Choices are prepared but hidden before question text completion");
  std::string error;check(choices.step(dt,{1,0,true,true},error),error.c_str());DialogueChoicesEvent e;check(!choices.poll_event(e),"Early option input cannot select while source text prints");
  check(bank_reads==1&&bank==55&&cash==7,"Question reads bank once and mutates no currency");
  // Finish with actual printer frames and one source WAIT input. Never call
  // DialogueChoices::text_completed or synthesize a final text callback.
  check(finish_text(question)==1,"Question retains exactly one source WAIT");
  check(choices.active()&&active_after_physics,"Choices open synchronously in the actual physics text-finished callback");
  check(choices.pose().selected==0&&choice_data.groups()[0].options.size()==2,"Record is default; cancel has no visible option");
  check(!presentation.dialogue_pose().cursor_visible,"Completed choice text suppresses ordinary continue cursor");
  check(ends_with(printed(presentation),"Anyways, what do you need from me?"),"Question text remains until result dispatch");
  presentation.take_audio_events();
  for(unsigned i=0;i<8;++i){step(true,true);check(world.phrase()==question&&choices.active(),"Ordinary dialogue Accept/Cancel cannot bypass active choices");for(const auto&a:presentation.take_audio_events())check(a.kind!=HouseAudioKind::Confirm,"Choice wait emits no synthetic Confirm");}
 }
 void select(bool nothing,bool cancel){
  std::string error;DialogueChoicesEvent event;
  if(nothing){check(choices.step(0,{1,0,false,false},error),error.c_str());check(choices.poll_event(event)&&event.kind==DialogueChoicesEventKind::SoundRequested&&event.sound==DialogueChoiceSound::Move,"Source navigation emits one move sound");check(!choices.poll_event(event),"Navigation has no selection side effect");}
  const uint32_t generation=world.story_generation();const auto expected_pc=cancel?choice_data.groups()[0].cancel_target_pc:choice_data.groups()[0].options[nothing?1:0].target_pc;
  check(choices.step(0,{0,0,!cancel,cancel},error),error.c_str());check(choices.poll_event(event)&&event.kind==DialogueChoicesEventKind::Selected,"Source choice emits a result");
  check(event.target_pc==expected_pc&&event.cancelled==cancel&&event.clear_dialogue&&event.sound_after_target,"Selection carries source target and clear-before-target, sound-after-target contract");check(!choices.poll_event(event),"Confirm sound is not emitted early as a separate event");
  check(runtime.select_story_option(expected_pc,generation),runtime.error());
  check(choices.phase()==DialogueChoicesPhase::Closed&&printed(presentation).empty()&&presentation.visible_characters()==0,"Result clears old text and options before new phrase printing");
  check(world.phrase()==((nothing||cancel)?farewell:record),"Target phrase has executed before caller plays selection sound");
  // The event consumer, as on CTR, plays this one InputSound only after the
  // HouseRuntime callback has cleared old text and handled the selected label.
  ++selection_sounds;saved_generation=generation;
 }
 void open_menu(){
  check(world.phrase()==record&&world.story_submenu_waiting()&&!world.story_input_allowed(),"Hidden Record suspends with dialogue input disabled");
  check(!world.story_flag("saved"),"Record clears a previous saved flag before requesting SaveSelect");
  check(!presentation.dialogue_pose().text_visible&&world.pending_dialogue_id()==house_no_index,"Hidden Record hides old dialogue and clears its pending text identity");
  check(world.take_save_request()&&!world.take_save_request(),"One OpenSave command becomes one consumable request");
  std::string error;check(menu.open(menu_data,std::vector<SaveSlotMetadata>(menu_data.slot_count()),1,error),error.c_str());
  const auto position=world.player().position;
  for(unsigned i=0;i<90;++i){check(menu.step(dt,{},error),error.c_str());step(true,true,{1,0});check(world.player().position.x==position.x&&world.player().position.y==position.y,"SaveSelect retains movement ownership");check(world.phrase()==record&&world.story_submenu_waiting()&&!world.take_save_request(),"Hidden empty text, elapsed time and normal input cannot resume or duplicate save");}
  check(menu.phase()==SaveMenuPhase::Slots,"Real SaveSelect activation delay expires");
 }
 SaveSlotMetadata metadata()const{SaveSlotMetadata m;m.occupied=true;m.lead_name="Ninten";m.scene_label="Ninten's House";m.menu_flavor="Plain";m.highest_level=2;m.playtime_seconds=123.5;m.party={"ninten"};return m;}
 void request_write(bool overwrite=false){
  std::string error;check(menu.step(0,{0,0,true,false},error),error.c_str());SaveMenuEvent e;
  if(overwrite){check(menu.phase()==SaveMenuPhase::OverwriteYield,"Occupied source card yields before overwrite prompt");while(menu.poll_event(e))check(e.kind!=SaveMenuEventKind::SaveRequested,"Occupied card cannot write before overwrite confirmation");check(menu.step(0,{},error)&&menu.phase()==SaveMenuPhase::Overwrite,error.c_str());check(menu.step(0,{0,0,true,false},error),error.c_str());}
  bool requested=false;while(menu.poll_event(e)){if(e.kind==SaveMenuEventKind::SaveRequested){check(!requested&&e.slot==1,"One confirmed operation targets selected source slot");requested=true;++save_requests;}}
  check(requested&&menu.phase()==SaveMenuPhase::Writing,"Real SaveSelect emits a write request and waits for acknowledgement");
  check(world.phrase()==record&&world.story_submenu_waiting(),"Write request does not resume Dad early");
 }
 void acknowledge(bool success){
  std::string error;auto slot=metadata();slot.playtime_seconds+=save_requests;
  // Disk/codec success is supplied by the separately tested session bridge.
  // This suite exercises the live UI callback contract and result branch.
  if(success)check(world.set_story_flag("saved",true,false),"Successful write callback sets the source saved flag");
  check(menu.acknowledge_save(success,success?&slot:nullptr,error),error.c_str());
  check(menu.is_open()&&menu.phase()==SaveMenuPhase::Slots&&world.story_submenu_waiting(),"Acknowledged write keeps SaveSelect open and Dad suspended");
  for(unsigned i=0;i<12;++i){step(true,true);check(world.phrase()==record,"Success or failure cannot implicitly close submenu");}
 }
 void close_menu(bool successful){
  std::string error;check(menu.step(0,{0,0,false,true},error),error.c_str());SaveMenuEvent event;check(menu.poll_event(event)&&event.kind==SaveMenuEventKind::Closed&&event.any_saved==successful,"Source Cancel returns whether any write succeeded");check(!menu.poll_event(event)&&!menu.is_open(),"Submenu emits exactly one close callback");
  check(runtime.close_story_submenu(saved_generation),runtime.error());check(world.phrase()==(successful?saved_reply:farewell),"Only actual submenu close dispatches source saved/unsaved branch");
  check(!world.story_submenu_waiting()&&!world.take_save_request()&&world.pending_dialogue_id()!=house_no_index,"Close installs new source text without suspended or repeated save request");
  if(successful){check(finish_text(saved_reply)==0&&printed(presentation)=="All done.","Successful save alone prints original acknowledgment");confirm(farewell);}
 }
 void finish_call(){
  check(world.phrase()==farewell,"Call reaches original farewell");check(finish_text(farewell)==1,"Farewell retains its original WAIT");
  check(printed(presentation).find("Anyways, what do you need from me?")==std::string::npos,"Previous option text never leaks into selected branch");confirm(hangup);
  check(finish_text(hangup)==0&&ends_with(printed(presentation),"(Click! Beep-beep-beep...)"),"Original final hangup phrase prints");settle_open();step(true);
  for(unsigned i=0;i<300&&(world.stage()!=OpeningStage::Walking||presentation.dialogue_active()||runtime.story_executing());++i)step();
  check(world.stage()==OpeningStage::Walking&&runtime.phase()==HousePhase::Idle&&!runtime.story_executing()&&!runtime.blocks_player(),"Final source confirmation returns ownership to world movement");
  check(!world.has_cutscene_actors()&&!presentation.dialogue_active()&&seen_snapshot(house,runtime)==seen,"Dad preserves original NPC identities and tears down its dialogue");
  check(!phone.ringing(0)&&choices.phase()==DialogueChoicesPhase::Closed&&!menu.is_open(),"Phone and both menus are inactive after return");
  check(world.story_flag("talked_to_dad")&&world.story_flag("doll_melody"),"Record preserves source story progression");
  const auto position=world.player().position;step(false,false,{0,1});check(world.player().position.y>position.y,"Real movement input works after final dialogue teardown");
  for(auto flag:{"pillow_attack","minnie_leave","minnie_door","carol_ask_key","mick_telepathy","got_diary"})check(!world.story_flag(flag),"Dad does not invent optional prerequisites");
 }
};
void nothing_or_cancel(Fixture&f,bool cancel,bool earned_flag){f.prepare_options(earned_flag);f.select(!cancel,cancel);check(!f.world.take_save_request()&&f.world.story_flag("saved"),"Nothing and hidden Cancel preserve prior saved state and never open SaveSelect");f.finish_call();check(action_count(f.world,DialogueActionKind::OpenSave)==0&&f.selection_sounds==1,"Non-Record result has no save command and exactly one selection sound");}
void record_cancel(Fixture&f,bool fail_write){f.prepare_options(false);f.select(false,false);f.open_menu();if(fail_write){f.request_write();f.acknowledge(false);check(!f.menu.any_saved()&&!f.world.story_flag("saved")&&!f.menu.slots()[0].occupied,"Failed write leaves result and card unsaved");}f.close_menu(false);f.finish_call();check(!f.world.story_flag("saved")&&action_count(f.world,DialogueActionKind::OpenSave)==1,"Unsuccessful Record restores world without claiming saved");}
void record_success(Fixture&f){
 f.prepare_options(true);f.select(false,false);f.open_menu();f.request_write();f.acknowledge(false);f.request_write();f.acknowledge(true);
 check(f.menu.any_saved()&&f.world.story_flag("saved")&&f.menu.slots()[0].occupied,"Retry success publishes result and refreshed card");
 f.request_write(true);f.acknowledge(true);const auto playtime=f.menu.slots()[0].playtime_seconds;f.request_write(true);f.acknowledge(false);
 check(f.menu.any_saved()&&f.world.story_flag("saved")&&f.menu.slots()[0].playtime_seconds==playtime,"Later failed overwrite preserves earlier successful result and metadata");
 f.close_menu(true);f.finish_call();check(f.earned_reads==1&&f.bank_reads==1&&f.current_reads==0&&f.bank==55&&f.cash==7&&f.earned_cash==0,"Whole Record interaction preserves single source currency replacement effects");
 check(action_count(f.world,DialogueActionKind::OpenSave)==1&&f.save_requests==4&&f.selection_sounds==1,"Repeated file writes belong to one source submenu invocation");
}
bool saved_flag(const SessionSnapshot&s,const std::string&id){for(const auto&f:s.flags)if(f.id==id)return f.value;check(false,"Complete saved flag registry contains identity");return false;}
void disk_record(Fixture&f,const NativeSessionData&data,RoundView round,ItemView items){
 f.world.set_party_leader(data.leader_id());f.prepare_options(true);f.select(false,false);f.open_menu();f.request_write();
 InventoryState inventory;check(inventory.initialize(items),"Real session inventory initializes from checked pack");
 const auto&row=data.levels().back();BattleSessionStats live;live.level=row.level;live.experience=round.victory().initial_exp+round.victory().reward_exp;
 live.maxhp=row.stats[0];live.maxpp=row.stats[1];live.offense=row.stats[2];live.defense=row.stats[3];live.speed=row.stats[4];live.iq=row.stats[5];live.guts=row.stats[6];live.hp=row.stats[0]-10;live.pp=row.stats[1];
 live.cash=uint32_t(f.cash);live.bank=uint32_t(f.bank);live.earned_cash=uint32_t(f.earned_cash);live.learned_skills={std::string(round.string(round.encounter().learned_skill))};
 // This is a bounded post-Doll live stats fixture. Position/flags/seen are
 // collected from this test's actual running world at the Record save request.
 NativeSnapshotInput input;input.stats=&live;input.inventory=&inventory;
 auto collect=[&](SessionSnapshot&out){
  input.state=data.defaults();const auto player=f.world.player();input.state.position_x=player.position.x;input.state.position_y=player.position.y;input.state.direction_x=player.direction.x;input.state.direction_y=player.direction.y;
  input.state.playtime_seconds=f.frames*dt;input.state.saved_at="2026-10-02T08:20:00Z";input.state.flags.clear();
  const auto room=f.world.content();for(uint32_t i=0;i<room.flag_count();++i){const std::string id(room.string(room.flag(i).name_string));input.state.flags.push_back({id,f.world.story_flag(id)});}
  input.state.seen_dialogue_flags.clear();for(auto key:f.runtime.seen_dialogue_keys())input.state.seen_dialogue_flags.push_back({std::string(f.house.string(key)),true});
  std::string error;check(build_native_session_snapshot(data,room,f.house,round,items,input,out,error),error.c_str());
 };
 std::filesystem::path directory;std::error_code filesystem_error;
 const char*temporary_root=std::getenv("TMPDIR");if(!temporary_root)temporary_root="/tmp";
 for(unsigned i=0;i<1000;++i){auto candidate=std::filesystem::path(temporary_root)/("encore-dad-record-sequence-"+std::to_string(i));if(std::filesystem::create_directory(candidate,filesystem_error)){directory=candidate;break;}}
 check(!directory.empty(),"Create an exclusive integration-only save directory");const auto path=(directory/"slot1.encsave").string();
 SessionSnapshot before,first,second,backup;std::string error;collect(before);
 check(!saved_flag(before,data.saved_flag_id())&&!f.world.story_flag(data.saved_flag_id()),"First real snapshot captures saved=false before successful write");
 check(before.position_x==f.world.player().position.x&&before.position_y==f.world.player().position.y&&before.direction_y==f.world.player().direction.y,"Snapshot contains actual phone world position and facing");
 check(before.seen_dialogue_flags.size()==f.runtime.seen_dialogue_keys().size(),"Snapshot contains the actual runtime seen registry");
 check(before.characters[0].level==row.level&&before.characters[0].experience==live.experience&&before.characters[0].hp==live.hp&&before.characters[0].pp==live.pp,"Source-derived post-Doll stats enter actual Record snapshot");
 check(before.characters[0].inventory.size()==inventory.size()&&before.characters[0].inventory[0].uid==inventory.instance(0).id,"Snapshot uses stable live inventory identities");
 check(before.bank==55&&before.cash==7&&before.earned_cash==0&&!saved_flag(before,data.earned_cash_flag_id()),"Disk candidate sees already-consumed EarnedCash replacement");
 check(write_session_save(path.c_str(),before,data.compatibility(),error),error.c_str());
 check(read_session_save(path.c_str(),data.compatibility(),first,error),error.c_str());check(validate_native_session_snapshot(data,f.world.content(),f.house,round,items,first,error),error.c_str());
 check(!saved_flag(first,data.saved_flag_id())&&!f.world.story_flag(data.saved_flag_id()),"Real committed file and live world remain false before success callback");
 f.acknowledge(true);check(f.world.story_flag(data.saved_flag_id())&&f.world.story_submenu_waiting(),"Only verified commit acknowledgment sets live saved while keeping Dad suspended");
 f.request_write(true);collect(before);check(saved_flag(before,data.saved_flag_id()),"Repeated write captures already-successful live saved=true");
 check(write_session_save(path.c_str(),before,data.compatibility(),error),error.c_str());check(read_session_save(path.c_str(),data.compatibility(),second,error),error.c_str());check(read_session_save((path+".bak").c_str(),data.compatibility(),backup,error),error.c_str());
 check(validate_native_session_snapshot(data,f.world.content(),f.house,round,items,second,error),error.c_str());check(validate_native_session_snapshot(data,f.world.content(),f.house,round,items,backup,error),error.c_str());
 check(saved_flag(second,data.saved_flag_id())&&!saved_flag(backup,data.saved_flag_id())&&backup.playtime_seconds==first.playtime_seconds,"Real overwrite retains previous complete unsaved-flag snapshot as backup");
 f.acknowledge(true);f.close_menu(true);f.finish_call();
 check(!std::filesystem::exists(path+".tmp")&&!std::filesystem::exists(path+".bak.tmp"),"Successful real writes leave no staging files");
 std::filesystem::remove_all(directory,filesystem_error);check(!filesystem_error,"Remove only exclusive integration save artifacts");
 std::cout<<"Dad/Record real-file path passed: actual world snapshot, checked level2/inventory, write/read validation, saved timing and overwrite backup\n";
}

void first_to_normal(Fixture&f){
 f.interact_phone();check(f.world.story_program_index()==program_index(f.world.content(),"Podunk/dad_poltergeist"),"First actual Dad call still selects source introduction");
 for(uint32_t phrase=0;phrase<5;++phrase){f.finish_text(phrase);if(phrase<4)f.confirm(phrase+1);}
 check(f.world.story_flag("talked_to_dad"),"First call earns normal-call override at original final phrase");f.settle_open();f.step(true);
 for(unsigned i=0;i<300&&(f.world.stage()!=OpeningStage::Walking||f.presentation.dialogue_active()||f.runtime.story_executing());++i)f.step();
 check(f.world.stage()==OpeningStage::Walking&&!f.runtime.story_executing(),"First call ends before repeat phone interaction");
 f.prepare_options(false);f.select(true,false);f.finish_call();
}
}
int main(int argc,char**argv){
 check(argc==7,"Room, House, Battle-font, Phone, Choices and SaveMenu packs required");std::string error;RoomData room;HouseData house;BattleData font;PhoneData phone;DialogueChoicesData choices;SaveMenuData menu;
 check(room.load_file(argv[1],error),error.c_str());check(house.load_file(argv[2],error),error.c_str());check(font.load_file(argv[3],error),error.c_str());check(phone.load_file(argv[4],error),error.c_str());check(choices.load_file(argv[5],error),error.c_str());check(menu.load_file(argv[6],error),error.c_str());
 for(const Vec2 viewport:{Vec2{400,240},Vec2{320,180}}){
  Fixture nothing(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,false,true);nothing_or_cancel(nothing,false,false);
  Fixture cancel(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,true,true);nothing_or_cancel(cancel,true,true);
  Fixture empty(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,false,true);record_cancel(empty,false);
  Fixture failed(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,false,true);record_cancel(failed,true);
  Fixture success(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,true,true);record_success(success);
  Fixture first(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,false,false,false);first_to_normal(first);
  for(const auto&leader:std::vector<std::pair<std::string,uint32_t>>{{"lloyd",5},{"ana",6},{"teddy",7},{"pippi",8}}){Fixture alternate(room.view(),house.view(),font.view(),phone.view(),choices,menu,viewport,false,false,true,leader.first);alternate.prepare_options(false,leader.second);alternate.select(false,true);alternate.finish_call();}
  std::cout<<"Dad/Record shared pipeline passed at "<<viewport.x<<'x'<<viewport.y<<'\n';
 }
 // A checked choice pack may be structurally sound yet bind an option to an
 // interior command or an already executed phrase. Reject that cross-pack join.
 std::ifstream choice_file(argv[5],std::ios::binary);std::vector<uint8_t>original((std::istreambuf_iterator<char>(choice_file)),{});
 const std::string record_text="Record";const auto found=std::search(original.begin(),original.end(),record_text.begin(),record_text.end());check(found!=original.end(),"Record display label exists in fixture");
 const auto target_offset=size_t(found-original.begin())+record_text.size();
 for(uint32_t target:{0u,32u}){
  auto corrupt=original;auto put=[&](size_t off,uint32_t value){for(unsigned j=0;j<4;++j)corrupt[off+j]=uint8_t(value>>(8*j));};put(target_offset,target);put(16,encore::crc32(corrupt.data()+24,corrupt.size()-24));
  DialogueChoicesData bad;check(bad.load(corrupt.data(),corrupt.size(),error),"Standalone choices remain structurally valid");
  Fixture invalid(room.view(),house.view(),font.view(),phone.view(),bad,menu,{400,240},false,false);invalid.interact_phone();invalid.finish_text(greeting);invalid.settle_open();check(invalid.world.finish_story_dialogue(),invalid.world.error());
  check(!invalid.runtime.idle_frame(dt,false,false),"Cross-pack choice target must be a forward phrase entry");
 }
 const auto pack_root=std::filesystem::path(argv[1]).parent_path();NativeSessionData session;BattleRoundData round;ItemData items;
 check(session.load_file((pack_root/"opening.encsession").string().c_str(),error),error.c_str());check(round.load_file((pack_root/"doll-entry.encround").string().c_str(),error),error.c_str());check(items.load_file((pack_root/"opening.encitems").string().c_str(),error),error.c_str());
 Fixture disk(room.view(),house.view(),font.view(),phone.view(),choices,menu,{400,240},true,true);disk_record(disk,session,round.view(),items.view());
 std::cout<<"Dad/Record integration: "<<checks<<" checks; actual phone dispatch, all leader branches, currency interpolation, real text-completion choices, Nothing/hidden Cancel, hidden Record, actual SaveMenu activation/write acknowledgments/retries/close, and movement return. Fault cases use fixture acknowledgments; one Record route commits and validates real native files and backup. No renderer, fresh-game or hardware claim.\n";
}
