#include "encore/fresh_house.hpp"
#include "encore/save_menu.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>

using namespace encore::upstream;
namespace {
unsigned checks=0;
std::string error;
void checked(bool ok,const char*expression,unsigned line){++checks;if(!ok){std::fprintf(stderr,"FreshHouseRestore line %u: %s; %s\n",line,expression,error.c_str());std::exit(1);}}
#define CHECK(x) checked(bool(x),#x,__LINE__)
constexpr double dt=double(float(1.0/60));
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
void flag(SessionSnapshot&s,std::string_view id,bool value){for(auto&f:s.flags)if(f.id==id){f.value=value;return;}CHECK(false);}
bool flag(const SessionSnapshot&s,std::string_view id){for(const auto&f:s.flags)if(f.id==id)return f.value;CHECK(false);return false;}
uint32_t npc(HouseView h,std::string_view path){for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)if(h.string(h.npc(i).source_path)==path)return i;CHECK(false);return house_no_index;}
uint32_t body(RoomView r,std::string_view path){for(uint32_t i=0;i<r.body_rule_count();++i)if(r.string(r.body_rule(i).source_path_string)==path)return r.body_rule(i).body_id;CHECK(false);return kRoomNoIndex;}
uint32_t program(RoomView r,std::string_view path){for(uint32_t i=0;i<r.program_count();++i)if(r.string(r.program(i).source_path_string)==path)return i;CHECK(false);return kRoomNoIndex;}
std::string printed(const HousePresentation&p){std::string text;for(const auto&line:p.dialogue_pose().lines){if(line.text.empty())continue;if(!text.empty())text+=' ';text+=line.text;}return text;}
struct Resources {
 NativeSessionData session;RoomData room;HouseData house;BattleRoundData round;ItemData items;BattleData font;PhoneData phone;RestoreData restore;DialogueChoicesData choices;SaveMenuData save;
 explicit Resources(const std::string&root){
  CHECK(session.load_file((root+"/opening.encsession").c_str(),error));CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));CHECK(round.load_file((root+"/doll-entry.encround").c_str(),error));CHECK(items.load_file((root+"/opening.encitems").c_str(),error));CHECK(font.load_file((root+"/opening.encbattle").c_str(),error));CHECK(phone.load_file((root+"/opening.encphone").c_str(),error));CHECK(restore.load_file((root+"/opening.encrestore").c_str(),room.view(),house.view(),error));CHECK(choices.load_file((root+"/opening.encchoices").c_str(),error));CHECK(save.load_file((root+"/opening.encsavemenu").c_str(),error));
 }
 bool prepare(const SessionSnapshot&s,PreparedSessionRestore&p)const{return prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),font.view(),s,p,error);}
 bool fresh(const PreparedSessionRestore&p,SourceRandom&r,Vec2 viewport,std::unique_ptr<FreshHouseState>&out)const{return prepare_fresh_house(p,restore,room.view(),house.view(),font.view(),phone.view(),r,viewport,out,error);}
 std::vector<uint8_t>encode(const SessionSnapshot&s)const{std::vector<uint8_t>bytes;CHECK(encode_session_save(s,session.compatibility(),bytes,error));return bytes;}
};
struct Live {
 const Resources&r;PreparedSessionRestore saved;SourceRandom random{98234};std::unique_ptr<FreshHouseState>scene;DialogueChoices choices;SaveMenu menu;unsigned frames=0,earned_reads=0,bank_reads=0;
 explicit Live(const Resources&resources):r(resources){}
 bool values(HouseTokenKind kind,std::string&out){
  if(kind==HouseTokenKind::EarnedCash){++earned_reads;out=std::to_string(saved.stats.earned_cash);saved.stats.earned_cash=0;return scene->world.set_story_flag(r.session.earned_cash_flag_id(),false,false);}
  if(kind==HouseTokenKind::BankCash){++bank_reads;out=std::to_string(saved.stats.bank);return true;}
  if(kind==HouseTokenKind::CurrentCash){out=std::to_string(saved.stats.cash);return true;}return false;
 }
 void bind(){scene->house.bind_choices(r.choices,choices);scene->presentation.set_text_value_callback([](void*data,HouseTokenKind kind,std::string&out){return static_cast<Live*>(data)->values(kind,out);},this);}
 void step(WalkInput input={},bool accept=false,bool cancel=false){
  ++frames;auto&w=scene->world;auto&h=scene->house;auto&p=scene->presentation;
  CHECK(h.before_physics(input));CHECK(p.physics_frame(dt,w.player().position));{const bool okay=w.advance(input);if(!okay)error=std::string(w.error())+" frame="+std::to_string(frames);CHECK(okay);}CHECK(h.after_physics());CHECK(w.idle_frame(dt));CHECK(p.idle_frame(dt));CHECK(h.idle_frame(dt,accept,cancel));CHECK(w.end_scene_frame());
 }
 void text(uint32_t phrase){
  auto&w=scene->world;auto&p=scene->presentation;
  for(unsigned n=0;n<4000&&!p.dialogue_finished();++n){CHECK(w.phrase()==phrase);step({},p.dialogue_stopped(),false);}
  CHECK(w.phrase()==phrase&&p.dialogue_finished());
 }
 void next(uint32_t phrase){for(unsigned n=0;n<30;++n)step();step({},true);CHECK(scene->world.phrase()==phrase);}
 SessionSnapshot snapshot()const{
  auto state=saved.state;const auto&w=scene->world;const auto&h=scene->house;const auto p=w.player();state.position_x=p.position.x;state.position_y=p.position.y;state.direction_x=p.direction.x;state.direction_y=p.direction.y;
  state.playtime_seconds=saved.state.playtime_seconds+frames*dt;state.saved_at="2026-10-02T09:00:00Z";
  for(auto&f:state.flags)f.value=w.story_flag(f.id);
  // Preserve explicit false entries and add newly-seen keys, as the platform
  // must do when starting with the complete restored snapshot.
  for(auto&f:state.seen_dialogue_flags){for(uint32_t key:h.seen_dialogue_keys())if(r.house.view().string(key)==f.id)f.value=true;}
  for(uint32_t key:h.seen_dialogue_keys()){const std::string id(r.house.view().string(key));if(std::none_of(state.seen_dialogue_flags.begin(),state.seen_dialogue_flags.end(),[&](const SessionFlag&f){return f.id==id;}))state.seen_dialogue_flags.push_back({id,true});}
  SessionSnapshot result;CHECK(build_native_session_snapshot(r.session,r.room.view(),r.house.view(),r.round.view(),r.items.view(),{state,&saved.stats,&saved.inventory},result,error));return result;
 }
};
SessionSnapshot post_dad(const Resources&r){
 auto s=r.session.defaults();auto&c=s.characters.front();const auto&row=r.session.levels().back();c.level=row.level;c.experience=row.minimum_exp+1;c.hp=19;c.pp=3;c.learned_skills=row.skills;c.nickname="Ana";c.inventory.front().uid=0;s.key_items.front().uid=UINT32_MAX;
 s.position_x=148;s.position_y=724;s.direction_x=-std::sqrt(.5);s.direction_y=std::sqrt(.5);s.bank=55;s.cash=7;s.earned_cash=15;s.player_name="Player";s.favorite_food="Bread";s.playtime_seconds=1234.75;s.saved_at="2026-10-02T08:00:00Z";
 for(auto name:{"doll_attack","doll_defeated","doll_melody","mimmie_door_opened","phone_ring","talked_to_dad","earned_cash","saved"}){flag(s,name,true);}
 flag(s,"poltergeist",false);
 std::set<std::string>seen;const auto h=r.house.view();for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i){const std::string id(h.string(h.npc(i).seen_key));if(!id.empty()&&seen.insert(id).second)s.seen_dialogue_flags.push_back({id,s.seen_dialogue_flags.size()%2==0});}
 for(uint32_t i=0;i<h.count(HouseSection::Overrides);++i){const std::string id(h.string(h.override_dialogue(i).seen_key));if(!id.empty()&&seen.insert(id).second)s.seen_dialogue_flags.push_back({id,s.seen_dialogue_flags.size()%2==0});}
 for(uint32_t i=0;i<r.room.view().battle_count();++i){s.encountered.push_back({std::string(r.room.view().string(r.room.view().battle(i).enemy_string)),true});}
 return s;
}
void collision_and_directions(const Resources&r,Vec2 viewport){
 SourceRandom random(8923);PreparedSessionRestore initial,restored;CHECK(r.prepare(r.session.defaults(),initial));CHECK(r.prepare(post_dad(r),restored));std::unique_ptr<FreshHouseState>authored,event;CHECK(r.fresh(initial,random,viewport,authored));CHECK(r.fresh(restored,random,viewport,event));CHECK(authored->finish_scene_ready()&&event->finish_scene_ready());
 CHECK(authored->world.warp_same_scene({504,88},{-1,0}));CHECK(event->world.warp_same_scene({496,80},{-1,0}));
 for(unsigned n=0;n<60;++n){CHECK(authored->world.advance({-1,0}));CHECK(event->world.advance({-1,0}));}
 const auto a=authored->world.player().position,b=event->world.player().position;CHECK(std::abs(a.x-b.x-8)<.001f&&std::abs(a.y-b.y-8)<.001f);CHECK(a.x>472&&b.x>464&&a.x<504&&b.x<496);
 const float d=std::sqrt(.5f);for(Vec2 direction:{Vec2{0,1},Vec2{1,0},Vec2{0,-1},Vec2{-1,0},Vec2{1,1},Vec2{-1,1},Vec2{1,-1},Vec2{-1,-1},Vec2{d,d},Vec2{-d,d},Vec2{d,-d},Vec2{-d,-d}}){auto s=post_dad(r);s.direction_x=direction.x;s.direction_y=direction.y;PreparedSessionRestore prepared;CHECK(r.prepare(s,prepared));std::unique_ptr<FreshHouseState>candidate;CHECK(r.fresh(prepared,random,viewport,candidate));CHECK(same(candidate->world.player().direction,direction));CHECK(candidate->finish_scene_ready());CHECK(candidate->world.advance({}));CHECK(same(candidate->world.player().direction,direction));}
}
void run(const Resources&r,Vec2 viewport,const std::filesystem::path&directory){
 const auto original=post_dad(r);const auto path=(directory/("slot-"+std::to_string(int(viewport.x))+".encsave")).string();
 CHECK(write_session_save(path.c_str(),original,r.session.compatibility(),error));SessionSnapshot disk;CHECK(read_session_save(path.c_str(),r.session.compatibility(),disk,error));CHECK(r.encode(disk)==r.encode(original));
 Live live(r);CHECK(r.prepare(r.session.defaults(),live.saved));CHECK(r.fresh(live.saved,live.random,viewport,live.scene));CHECK(live.scene->finish_scene_ready());
 // Deliberately retain a ringing phone, camera shake and running source script
 // in the former owner. A candidate failure must not mutate any of them.
 CHECK(live.scene->world.begin_house_program(program(r.room.view(),"Podunk/cutscenes/lamp_attack")));
 for(unsigned n=0;n<2400&&live.scene->world.stage()!=OpeningStage::BattleRequested;++n){CHECK(live.scene->world.advance({}));CHECK(live.scene->world.idle_frame(dt));}
 CHECK(live.scene->world.stage()==OpeningStage::BattleRequested&&live.scene->world.accept_battle_entry());CHECK(live.scene->world.has_cutscene_actors());CHECK(live.scene->world.erase_battle_actor(body(r.room.view(),"Objects/lamp")));CHECK(!live.scene->world.body_visible(body(r.room.view(),"Objects/lamp")));CHECK(live.scene->phone.ring(0));CHECK(live.scene->world.shake_house_camera(2,4,{1,0}));
 auto*old=live.scene.get();const auto old_stage=old->world.stage();const auto old_random=live.random.state();const auto old_draws=live.random.raw_draw_count();const auto old_trace=old->world.action_trace().size();
 PreparedSessionRestore rejected;auto invalid=disk;invalid.characters.front().nickname="\xc3\xa9";CHECK(!r.prepare(invalid,rejected));CHECK(!r.fresh(rejected,live.random,viewport,live.scene));
 CHECK(live.scene.get()==old&&old->phone.ringing(0)&&old->world.stage()==old_stage&&old->world.action_trace().size()==old_trace&&old->world.cutscene_camera().is_shaking());CHECK(live.random.state()==old_random&&live.random.raw_draw_count()==old_draws);
 CHECK(r.prepare(disk,live.saved));CHECK(!prepare_fresh_house(live.saved,RestoreData{},r.room.view(),r.house.view(),r.font.view(),r.phone.view(),live.random,viewport,live.scene,error));CHECK(live.scene.get()==old&&old->phone.ringing(0));
 const auto before=live.random.state(),draws=live.random.raw_draw_count();CHECK(r.fresh(live.saved,live.random,viewport,live.scene));CHECK(live.scene.get()!=old); // The former owner is now destroyed.
 CHECK(live.random.state()==before&&live.random.raw_draw_count()==draws);live.bind();auto&w=live.scene->world;auto&h=live.scene->house;auto&p=live.scene->presentation;
 CHECK(live.scene->scene_ready_pending());CHECK(same(w.player().position,{148,724}));CHECK(same(w.player().direction,{float(original.direction_x),float(original.direction_y)}));CHECK(w.player().animation==MotionAnimation::Idle&&!w.player().running&&!w.player().crouch);
 CHECK(w.stage()==OpeningStage::Walking&&!w.house_paused()&&!w.has_cutscene_actors()&&!w.actor_cleanup_pending());CHECK(w.action_trace().empty()&&!w.battle_request().requested&&!w.battle_request().queued&&!w.take_save_request());CHECK(!w.cutscene_camera().is_shaking()&&!w.cutscene_camera().is_moving()&&!w.cutscene_camera().follows_actor());
 CHECK(h.phase()==HousePhase::Idle&&!h.story_pending()&&!h.story_executing()&&!h.blocks_player()&&!p.dialogue_active());CHECK(!live.scene->phone.ringing(0)&&live.scene->phone.clip_time(0)==0);CHECK(h.seen_dialogue_keys()==live.saved.seen_dialogue_keys);
 const auto hv=r.house.view();for(const auto&entry:std::vector<std::pair<std::string,Vec2>>{{"Objects/npc",{192,704}},{"Objects/npc2",{120,88}},{"Objects/npc3",{464,80}},{"Objects/npcdoll",{40,40}}}){const auto i=npc(hv,entry.first),actor=hv.npc(i).room_actor_index;CHECK(same(p.npc_pose(i).position,entry.second));CHECK(p.npc_pose(i).visible&&w.body_enabled(hv.npc(i).body_id));if(actor!=house_no_index){CHECK(same(w.actor(actor).position,entry.second));CHECK(!w.actor_bound(actor)&&!w.actor_restore_requested(actor));}}
 const auto lamp=body(r.room.view(),"Objects/lamp"),door=body(r.room.view(),"DoorBlock/Entrance");CHECK(w.body_visible(lamp)&&w.body_enabled(lamp));bool lamp_position=false;for(uint32_t i=0;i<w.actor_count();++i)if(same(r.room.view().actor_instance(i).position,{496,390})){lamp_position=true;CHECK(same(w.actor(i).position,{496,390})&&w.instance_visible(i)&&!w.actor_bound(i));}CHECK(lamp_position);
 CHECK(w.body_enabled(door)&&w.body_visible(door));CHECK(w.audio_request_count()==0&&w.area_music_resource()==kRoomNoIndex);
 CHECK(live.scene->finish_scene_ready());CHECK(!live.scene->scene_ready_pending()&&!w.body_enabled(door)&&!w.body_visible(door));CHECK(w.audio_request_count()==1&&w.audio_requests().front().kind==AudioRequestKind::FadeInMusic&&w.audio_requests().front().duration==1&&w.audio_requests().front().gain_db==0);CHECK(w.area_music_resource()==r.restore.music_areas().front().room_resource_index);
 CHECK(live.scene->finish_scene_ready()&&w.audio_request_count()==1);CHECK(live.random.state()==before&&live.random.raw_draw_count()==draws);
 for(unsigned n=0;n<300;++n){live.step();}
 CHECK(!w.cutscene_camera().is_shaking()&&!live.scene->phone.ringing(0)&&h.phone_sounds().empty()&&w.action_trace().empty());CHECK(live.random.state()==before&&live.random.raw_draw_count()==draws);CHECK(same(w.player().direction,{float(original.direction_x),float(original.direction_y)}));
 const auto standing=w.player().position;for(unsigned n=0;n<8;++n)live.step({0,-1});CHECK(w.player().position.y<standing.y&&w.player().position.x==standing.x);CHECK(same(w.player().direction,{0,-1}));
 live.step({},true);for(unsigned n=0;n<30&&!p.dialogue_active();++n)live.step();CHECK(p.dialogue_active()&&h.story_executing());CHECK(w.story_program_index()==program(r.room.view(),"Reusable/dad_normal"));
 live.text(0);CHECK(printed(p).find("Ana? It's your dad.")!=std::string::npos&&printed(p).find("Ninten?")==std::string::npos);live.next(10);CHECK(live.earned_reads==1&&live.saved.stats.earned_cash==0&&!w.story_flag("earned_cash"));live.text(10);CHECK(printed(p).find("$15")!=std::string::npos);live.next(11);live.text(11);CHECK(live.bank_reads==1&&live.choices.active()&&w.story_choices_waiting());
 CHECK(live.choices.step(0,{0,0,true,false},error));DialogueChoicesEvent choice;CHECK(live.choices.poll_event(choice)&&choice.kind==DialogueChoicesEventKind::Selected);const auto generation=w.story_generation();CHECK(h.select_story_option(choice.target_pc,generation));CHECK(w.phrase()==12&&w.story_submenu_waiting()&&!w.story_input_allowed()&&!w.story_flag("saved"));CHECK(w.take_save_request()&&!w.take_save_request());
 SaveSlotMetadata metadata;metadata.occupied=true;metadata.lead_name="Ana";metadata.scene_label=original.scene_label;metadata.menu_flavor=original.settings.menu_flavor;metadata.highest_level=2;metadata.playtime_seconds=original.playtime_seconds;metadata.party=original.party;std::vector<SaveSlotMetadata>slots(r.save.slot_count());slots.front()=metadata;CHECK(live.menu.open(r.save,slots,1,error));for(unsigned n=0;n<90;++n){CHECK(live.menu.step(dt,{},error));live.step();}CHECK(live.menu.phase()==SaveMenuPhase::Slots);CHECK(live.menu.step(0,{0,0,true,false},error));CHECK(live.menu.phase()==SaveMenuPhase::OverwriteYield);CHECK(live.menu.step(0,{},error)&&live.menu.phase()==SaveMenuPhase::Overwrite);CHECK(live.menu.step(0,{0,0,true,false},error));SaveMenuEvent event;unsigned writes=0;while(live.menu.poll_event(event))if(event.kind==SaveMenuEventKind::SaveRequested){++writes;CHECK(event.slot==1);}CHECK(writes==1&&live.menu.phase()==SaveMenuPhase::Writing);
 const auto recorded=live.snapshot();CHECK(recorded.characters.front().nickname=="Ana"&&recorded.characters.front().inventory.front().uid==0&&recorded.key_items.front().uid==UINT32_MAX);CHECK(recorded.characters.front().hp==19&&recorded.characters.front().pp==3&&recorded.characters.front().experience==original.characters.front().experience&&recorded.characters.front().learned_skills==original.characters.front().learned_skills);CHECK(recorded.bank==55&&recorded.cash==7&&recorded.earned_cash==0&&!flag(recorded,"earned_cash")&&!flag(recorded,"saved"));
 auto expected=original;expected.position_x=recorded.position_x;expected.position_y=recorded.position_y;expected.direction_x=recorded.direction_x;expected.direction_y=recorded.direction_y;expected.playtime_seconds=recorded.playtime_seconds;expected.saved_at=recorded.saved_at;expected.earned_cash=0;flag(expected,"earned_cash",false);flag(expected,"saved",false);CHECK(r.encode(recorded)==r.encode(expected));
 CHECK(write_session_save(path.c_str(),recorded,r.session.compatibility(),error));SessionSnapshot reread,backup;CHECK(read_session_save(path.c_str(),r.session.compatibility(),reread,error));CHECK(read_session_save((path+".bak").c_str(),r.session.compatibility(),backup,error));CHECK(r.encode(reread)==r.encode(recorded)&&r.encode(backup)==r.encode(original));CHECK(!w.story_flag("saved"));
 CHECK(w.set_story_flag("saved",true,false));metadata.playtime_seconds=recorded.playtime_seconds;CHECK(live.menu.acknowledge_save(true,&metadata,error));CHECK(w.story_submenu_waiting());CHECK(live.menu.step(0,{0,0,false,true},error));CHECK(live.menu.poll_event(event)&&event.kind==SaveMenuEventKind::Closed&&event.any_saved);CHECK(h.close_story_submenu(generation));CHECK(w.phrase()==13);live.text(13);CHECK(printed(p)=="All done.");live.next(14);live.text(14);live.next(15);live.text(15);for(unsigned n=0;n<30;++n)live.step();live.step({},true);for(unsigned n=0;n<300&&(w.stage()!=OpeningStage::Walking||p.dialogue_active()||h.story_executing());++n)live.step();CHECK(w.stage()==OpeningStage::Walking&&h.phase()==HousePhase::Idle&&!h.blocks_player()&&!p.dialogue_active());CHECK(h.seen_dialogue_keys()==live.saved.seen_dialogue_keys);const auto at=w.player().position;live.step({0,1});CHECK(w.player().position.y>at.y);
 // A second fresh owner can be prepared directly from the real re-save.
 PreparedSessionRestore again;CHECK(r.prepare(reread,again));std::unique_ptr<FreshHouseState>reloaded;CHECK(r.fresh(again,live.random,viewport,reloaded)&&reloaded->finish_scene_ready());CHECK(same(reloaded->world.player().position,{float(recorded.position_x),float(recorded.position_y)}));CHECK(again.inventory.instance(0).id==0&&again.stats.hp==19&&again.stats.pp==3&&again.state.characters.front().nickname=="Ana");
 CHECK(!std::filesystem::exists(path+".tmp")&&!std::filesystem::exists(path+".bak.tmp"));std::printf("FreshHouseRestore %dx%d: real file -> checked fresh owner -> deferred ready -> movement -> actual Dad text/Record -> real overwrite/backup -> checked fresh reload passed\n",int(viewport.x),int(viewport.y));
}
}
int main(int argc,char**argv){
 const Resources resources(argc>1?argv[1]:"romfs/data");std::filesystem::path directory;std::error_code ec;const char*tmp=std::getenv("TMPDIR");if(!tmp)tmp="/tmp";for(unsigned i=0;i<1000;++i){const auto candidate=std::filesystem::path(tmp)/("encore-fresh-house-restore-"+std::to_string(i));if(std::filesystem::create_directory(candidate,ec)){directory=candidate;break;}}CHECK(!directory.empty());
 for(Vec2 viewport:{Vec2{400,240},Vec2{320,180}}){run(resources,viewport,directory);collision_and_directions(resources,viewport);}
 std::filesystem::remove_all(directory,ec);CHECK(!ec);std::printf("FreshHouseRestore: %u checks; no GUI, renderer, original full-scene or hardware claim\n",checks);
}
