#include "encore/house_presents.hpp"
#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/native_session.hpp"
#include "encore/session_restore.hpp"
#include "encore/field_equipment_menu.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
using namespace encore::upstream;
static unsigned checks=0;static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Basement failure line %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
struct Effects final:PresentHost,PresentEffects,StorySkillHost {
 const NativeSessionData&session;InventoryState inventory;std::vector<SessionItem>keys;
 std::map<std::string,bool>flags;std::vector<uint32_t>texts;std::vector<std::string>sounds;
 std::vector<std::string>skills;bool space=true,reject=false,reject_binding=false;unsigned previews=0,learns=0;uint32_t uid=0xabcd;
 explicit Effects(const NativeSessionData&s,ItemView items):session(s){CHECK(inventory.initialize(items));for(const auto&f:s.defaults().flags)flags[f.id]=f.value;skills=s.defaults().characters[0].learned_skills;}
 bool validate_text(uint32_t,std::string_view,std::string&)override{return true;}
 bool validate_flag(std::string_view name,std::string&)override{return flags.count(std::string(name));}
 bool validate_item(PresentTemplate t,std::string_view name,std::string&e)override{
  if(reject_binding){e="Fixture item binding rejected";return false;}
  if(t.key_item){for(const auto&k:session.key_acquisitions())if(k.item_id==name&&k.doses==t.doses)return true;}
  else for(const auto&a:session.acquisitions())if(a.item_id==name&&a.doses==t.doses)return true;
  return false;
 }
 bool validate_sound(std::string_view name,std::string&)override{return name.substr(0,6)=="Audio/";}
 bool validate_story(std::string_view path,std::string&)override{return path=="Data/Dialogue/Podunk/cutscenes/ggf_diary.yaml";}
 bool show_text(uint32_t id,std::string&)override{texts.push_back(id);return true;}
 bool flag(std::string_view name,bool&value,std::string&)override{value=flags[std::string(name)];return true;}
 bool set_flag(std::string_view name,bool value,std::string&)override{flags[std::string(name)]=value;return true;}
 bool inventory_space()const override{return space;}
 bool preview_item(PresentTemplate,std::string_view,std::string&)override{++previews;return true;}
 bool start_story(std::string_view,std::string&)override{return true;}
 bool grant_item(PresentTemplate t,std::string_view name,std::string&e)override{
  if(reject){e="Fixture rejected grant";return false;}
  if(t.key_item){keys.push_back({std::string(name),false,t.doses,uid++});return true;}
  const auto data=inventory.content();for(uint32_t i=0;i<data.count(ItemSection::Definitions);++i)if(data.string(data.definition(i).source)==name)return inventory.append(i,t.doses,uid++,e);
  return false;
 }
 bool play_sound(std::string_view name,std::string&)override{sounds.emplace_back(name);return true;}
 bool validate_skill(std::string_view id,std::string&)override{for(const auto&p:session.skill_acquisitions())if(p.skill_id==id)return true;return false;}
 bool learn_skill(std::string_view id,std::string&e)override{++learns;return native_session_learn_skill(session,id,skills,e);}
};
static uint32_t object(PresentView view,std::string_view item){for(uint32_t i=0;i<view.count(PresentSection::Objects);++i)if(view.string(view.object(i).item)==item)return i;return kPresentNone;}
static uint32_t u32(const std::vector<uint8_t>&v,size_t p){return uint32_t(v[p])|uint32_t(v[p+1])<<8|uint32_t(v[p+2])<<16|uint32_t(v[p+3])<<24;}
static void put(std::vector<uint8_t>&v,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)v[p+i]=uint8_t(n>>(i*8));}
int main(int argc,char**argv){
 const std::string base=argc>1?argv[1]:"romfs";PresentData data;HouseData house;RoomData room;ItemData items;NativeSessionData session;BattleData font;BattleRoundData round;FieldEquipmentData menu_data;
 CHECK(data.load_file((base+"/world/house/opening.encpresent").c_str(),error));CHECK(house.load_file((base+"/world/house/opening.enchouse").c_str(),error));
 CHECK(room.load_file((base+"/world/house/opening.encroom").c_str(),error));CHECK(items.load_file((base+"/ui/opening.encitems").c_str(),error));
 CHECK(session.load_file((base+"/session/opening.encsession").c_str(),error));CHECK(font.load_file((base+"/battle/lamp/opening.encbattle").c_str(),error));
 CHECK(round.load_file((base+"/battle/lamp/opening.encround").c_str(),error));CHECK(menu_data.load_file((base+"/ui/opening.encfield").c_str(),error));
 auto view=data.view();auto bat=object(view,"BatPlastic"),diary=object(view,"GGFDiary");CHECK(bat!=kPresentNone&&diary!=kPresentNone);
 Effects effects(session,items.view());PresentRuntime present;SourceRandom random(47);CHECK(present.initialize(view,effects,error));CHECK(present.scene_ready(random,error));
 CHECK(present.start(bat,error));CHECK(effects.texts.back()==77&&effects.inventory.size()==2);const auto received=effects.inventory.instance(1);CHECK(received.equipped==0&&received.doses==1);
 CHECK(present.advance_text(error));CHECK(effects.texts.back()==78&&effects.sounds.size()==1);CHECK(present.advance_text(error));CHECK(effects.texts.back()==79);
 CHECK(present.advance_text(error));CHECK(present.state()==PresentProgramState::Complete);CHECK(present.advance(.5,error));CHECK(present.pose(bat).frame==4&&!present.pose(bat).sparkles);
 CHECK(present.start(bat,error));CHECK(effects.texts.back()==72&&effects.inventory.size()==2);CHECK(present.advance_text(error));
 auto saved_flags=present.object_flags();CHECK(saved_flags.size()==1&&saved_flags.front().value);
 Effects full(session,items.view());full.space=false;PresentRuntime full_present;CHECK(full_present.initialize(view,full,error));CHECK(full_present.scene_ready(random,error));
 CHECK(full_present.start(bat,error));CHECK(full.previews==1&&full.inventory.size()==1&&full_present.object_flags().empty());CHECK(full.texts.back()==80);
 CHECK(full_present.advance(view.clip(0).length,error));CHECK(full_present.pose(bat).frame==4&&full_present.pose(bat).sparkles);auto full_audio=full_present.take_audio();CHECK(!full_audio.empty()&&full_audio.front().kind==PresentAudioKind::Play);CHECK(full_present.advance(view.clip(1).length,error));CHECK(full_present.pose(bat).frame==0);CHECK(full_present.advance_text(error));
 full.space=true;CHECK(full_present.start(bat,error));CHECK(full.inventory.size()==2&&full.previews==1);CHECK(full_present.object_flags().size()==1);
 Effects refused(session,items.view());refused.reject=true;PresentRuntime rejection;CHECK(rejection.initialize(view,refused,error));CHECK(rejection.scene_ready(random,error));CHECK(!rejection.start(bat,error));CHECK(rejection.object_flags().empty()&&refused.inventory.size()==1&&refused.texts.empty());
 // Real House interaction and the shared room scheduler, using ordinary input.
 Effects story(session,items.view());story.uid=0xbcde;OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;PresentRuntime boxes;SourceRandom story_random(47);
 CHECK(world.initialize(room.view()));world.attach_random(story_random);CHECK(world.bind_story_skills(story));CHECK(world.set_party_leader(session.leader_id()));
 for(const auto*flag:{"doll_attack","doll_defeated","doll_melody","mimmie_door_opened"})CHECK(world.set_story_flag(flag,true,true));CHECK(world.set_story_flag("poltergeist",false,true));
 uint32_t house_music=kRoomNoIndex;for(uint32_t i=0;i<room.view().resource_count();++i)if(room.view().string(room.view().resource(i).path_string)=="res://Audio/Music/House.mp3")house_music=i;
 CHECK(house_music!=kRoomNoIndex&&world.start_area_music(house_music,0,0));const auto audio_before=world.audio_requests().size();
 CHECK(world.warp_same_scene({128,1040},{0,-1}));CHECK(presentation.begin(house.view(),font.view(),story_random));CHECK(runtime.initialize(house.view(),world,presentation));
 CHECK(runtime.set_player_nickname(session.defaults().characters[0].nickname));
 story.reject_binding=true;CHECK(!runtime.bind_presents(boxes,view,story));CHECK(std::string(runtime.error())=="House present binding rejected: Fixture item binding rejected");story.reject_binding=false;
 CHECK(runtime.bind_presents(boxes,view,story));CHECK(boxes.scene_ready(story_random,error));
 const double dt=double(1.f/60);unsigned frames=0;std::set<uint32_t>phrases;float max_alpha=0;bool special=false;std::set<uint32_t>sleep_frames,wake_frames;
 auto tick=[&](bool accept,bool cancel){WalkInput input{};CHECK(runtime.before_physics(input));CHECK(presentation.physics_frame(dt,world.player().position));CHECK(world.advance(input));CHECK(runtime.after_physics());CHECK(world.idle_frame(dt));CHECK(presentation.idle_frame(dt));CHECK(runtime.idle_frame(dt,accept,cancel));CHECK(world.end_scene_frame());++frames;};
 tick(false,false);tick(true,false);CHECK(runtime.phase()==HousePhase::StoryRunning&&world.story_flag("got_diary"));CHECK(story.keys.size()==1&&story.keys[0].item_id=="GGFDiary");
 while(runtime.phase()!=HousePhase::Idle&&frames<3600){phrases.insert(world.phrase());max_alpha=std::max(max_alpha,world.screen_overlay().alpha);special|=world.actor(room.view().scene().player_instance_index).profile_index!=room.view().actor_instance(room.view().scene().player_instance_index).profile_index;if(world.actor(0).profile_index!=0){if(world.phrase()==3)sleep_frames.insert(world.actor(0).frame);if(world.phrase()==7)wake_frames.insert(world.actor(0).frame);}tick(frames%2&&presentation.dialogue_finished(),!presentation.dialogue_finished());}
 CHECK(frames<3600&&frames*dt>=16.6);CHECK(world.stage()==OpeningStage::Walking&&!world.house_paused()&&story.learns==1&&special&&max_alpha>.15f);
 CHECK(world.player().position.x==128&&world.player().position.y==1040&&world.player().direction.y==1);CHECK(boxes.pose(diary).frame==4&&!boxes.pose(diary).sparkles);
 for(uint32_t phrase=0;phrase<=10;++phrase)CHECK(phrases.count(phrase));CHECK(std::find(story.skills.begin(),story.skills.end(),"lifeUpA")!=story.skills.end());
 unsigned stops=0,plays=0,learned=0;for(size_t i=audio_before;i<world.audio_requests().size();++i){const auto&a=world.audio_requests()[i];stops+=a.kind==AudioRequestKind::StopMusicResource;plays+=a.kind==AudioRequestKind::PlayMusic;if(a.kind==AudioRequestKind::PlayEffect)learned+=room.view().string(room.view().resource(a.resource_index).path_string)=="res://Audio/Sound effects/M3/Learned PSI.wav";}
 CHECK(stops==1&&plays==1&&learned==1);CHECK(sleep_frames.count(0)&&sleep_frames.count(34)&&wake_frames.count(35)&&wake_frames.count(52));CHECK(world.effect_requests().size()==2&&world.effect_requests()[0].appear&&!world.effect_requests()[1].appear);
 tick(false,false);CHECK(world.set_house_direction({0,-1}));tick(false,false);tick(true,false);CHECK(runtime.phase()==HousePhase::PresentProgram&&story.keys.size()==1&&story.learns==1);
 for(unsigned i=0;i<600&&runtime.phase()!=HousePhase::Idle;++i)tick(i%2&&presentation.dialogue_finished(),!presentation.dialogue_finished());CHECK(runtime.phase()==HousePhase::Idle);
 // Diary/key identity, bat equip/stat change and object flag survive save/LOAD.
 auto state=session.defaults();state.position_x=128;state.position_y=1040;state.direction_y=1;state.object_flags=saved_flags;state.key_items.insert(state.key_items.end(),story.keys.begin(),story.keys.end());
 for(auto&f:state.flags)if(f.id=="got_diary")f.value=true;state.characters[0].learned_skills=story.skills;
 const auto bat_definition=items.view().definition(received.definition);CHECK(items.view().string(bat_definition.source)=="BatPlastic"&&bat_definition.equipment_slot==0);
 state.characters[0].inventory.push_back({"BatPlastic",true,received.doses,received.id});CHECK(validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),state,error));
 std::array<int32_t,7>stats{};CHECK(native_session_derived_stats(session,items.view(),state.characters[0],stats,error));CHECK(stats[2]==session.levels()[0].stats[2]+3);
 std::vector<uint8_t>encoded;CHECK(encode_session_save(state,session.compatibility(),encoded,error));SessionSnapshot decoded;CHECK(decode_session_save(encoded.data(),encoded.size(),session.compatibility(),decoded,error));
 PreparedSessionRestore restored;CHECK(prepare_session_restore(session,room.view(),house.view(),round.view(),items.view(),font.view(),decoded,restored,error));CHECK(restored.inventory.instance(1).id==received.id&&restored.inventory.instance(1).equipped==1&&restored.state.key_items.back().uid==story.keys[0].uid);
 auto invalid=state;invalid.characters[0].learned_skills.pop_back();CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),invalid,error));invalid=state;invalid.object_flags.clear();CHECK(!validate_native_session_snapshot(session,room.view(),house.view(),round.view(),items.view(),invalid,error));
 // Learned healing is selectable; target cancellation does not invoke effects.
 FieldEquipmentSnapshot current;current.owner=session.leader_id();current.nickname=state.characters[0].nickname;current.level=1;current.inventory=restored.inventory;current.stats=stats;current.pp=10;current.learned_skills=story.skills;
 unsigned uses=0;FieldEquipmentHost host;host.read=[&](auto&out,std::string&){out=current;return true;};host.preview=[](auto,auto,auto,auto&,std::string&){return false;};host.commit=[](auto,auto,auto,std::string&){return false;};host.use_skill=[&](uint32_t index,std::string&message,std::string&){CHECK(menu_data.view().psi_skill(index).id=="lifeUpA");++uses;current.pp-=5;message="fixture";return true;};
 FieldEquipmentMenu menu;CHECK(menu.initialize(menu_data.view(),host));CHECK(menu.open());CHECK(menu.idle_frame(1));CHECK(menu.input(1,0,true,false));CHECK(menu.idle_frame(1));CHECK(menu.psi_skill_count()==1&&menu.psi_selected_skill().id=="lifeUpA");
 CHECK(menu.input(0,0,true,false));CHECK(menu.phase()==FieldEquipmentPhase::PsiTarget);CHECK(menu.input(0,0,false,true));CHECK(uses==0&&current.pp==10);CHECK(menu.input(0,0,true,false));CHECK(menu.input(0,0,true,false));CHECK(menu.phase()==FieldEquipmentPhase::PsiResult&&uses==1&&current.pp==5);CHECK(menu.input(0,0,true,false));CHECK(menu.phase()==FieldEquipmentPhase::Psi);
 FieldPsiSkill healing;for(uint32_t i=0;i<menu_data.view().psi_skill_count();++i)if(menu_data.view().psi_skill(i).action==2)healing=menu_data.view().psi_skill(i);CHECK(healing.id=="lifeUpA");
 SourceRandom heal_random(771),expected_random(771);FieldHealingResult result;const auto roll=int32_t(std::floor(65+15/5+expected_random.randf()*5-2.5));CHECK(field_healing(healing,1,200,10,15,heal_random,result,error));CHECK(result.rolled==roll&&result.hp==1+roll&&result.pp==5&&!result.full&&heal_random.randi()==expected_random.randi());
 SourceRandom unchanged(771),control(771);CHECK(!field_healing(healing,1,200,4,15,unchanged,result,error));CHECK(unchanged.randi()==control.randi());CHECK(field_healing(healing,200,200,10,15,heal_random,result,error));CHECK(result.hp==200&&result.pp==5&&result.full);
 // Every new opcode/version/operand path rejects malformed data transactionally.
 std::vector<uint8_t>raw;CHECK(encore::read_file((base+"/world/house/opening.encpresent").c_str(),raw,65536,error));const auto command_base=u32(raw,64+(uint32_t(PresentSection::Commands)-1)*16+4);
 auto bad=[&](size_t offset,uint32_t value){auto bytes=raw;put(bytes,offset,value);put(bytes,16,encore::crc32(bytes.data()+32,bytes.size()-32));CHECK(!data.load(bytes.data(),bytes.size(),error));CHECK(data.view().count(PresentSection::Objects)==4);};
 bad(20,4);bad(20,2);for(uint32_t i=0;i<view.count(PresentSection::Commands);++i){auto c=view.command(i);const auto at=command_base+20*i;if(c.opcode==uint32_t(PresentOpcode::BranchSpace)){bad(at+4,UINT32_MAX);bad(at+8,1);}if(c.opcode==uint32_t(PresentOpcode::PreviewItem)||c.opcode==uint32_t(PresentOpcode::QueueClip))bad(at+4,UINT32_MAX);if(c.opcode==uint32_t(PresentOpcode::StartStory)){bad(at+4,0);bad(at+8,1);}}
 std::vector<uint8_t>room_bytes;CHECK(encore::read_file((base+"/world/house/opening.encroom").c_str(),room_bytes,kMaxRoomBytes,error));
 const auto room_commands=u32(room_bytes,128+(uint32_t(RoomSection::Command)-1)*24+4);
 auto bad_room=[&](size_t at,uint32_t value){auto b=room_bytes;put(b,at,value);put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));CHECK(!room.load(b.data(),b.size(),error));};
 bad_room(36,10);bad_room(36,8);
 for(uint32_t i=0;i<room.view().command_count();++i){const auto c=room.view().command(i);const auto at=room_commands+i*48;
  if(c.opcode==uint32_t(DialogueActionKind::SetActorProfile)){bad_room(at+8,UINT32_MAX);bad_room(at+12,1);}
  if(c.opcode==uint32_t(DialogueActionKind::LearnSkill)){bad_room(at+8,UINT32_MAX);bad_room(at+40,0);}
  if(c.opcode==uint32_t(DialogueActionKind::ScreenOverlay)){bad_room(at+8,0);bad_room(at+28,0xbff00000);}
  if(c.opcode==uint32_t(DialogueActionKind::AnimateActor)&&c.flags==1){bad_room(at+40,UINT32_MAX);bad_room(at+12,2);}
 }
 // The v8 policy extension rejects foreign flags, invalid domains and missing order coverage.
 std::vector<uint8_t>session_bytes;CHECK(encore::read_file((base+"/session/opening.encsession").c_str(),session_bytes,1024*1024,error));
 auto offset_of=[&](const std::string&needle){auto it=std::search(session_bytes.begin(),session_bytes.end(),needle.begin(),needle.end());CHECK(it!=session_bytes.end());return size_t(it-session_bytes.begin());};
 const auto skill_at=offset_of("lifeUpA"),object_at=offset_of("Ninten's House/Present2");
 auto bad_session=[&](size_t at,uint32_t value){auto bytes=session_bytes;put(bytes,at,value);put(bytes,16,encore::crc32(bytes.data()+24,bytes.size()-24));CHECK(!session.load(bytes.data(),bytes.size(),error));CHECK(session.skill_acquisitions().size()==1&&session.skill_acquisitions()[0].skill_id=="lifeUpA");};
 bad_session(skill_at-8,33);bad_session(skill_at-4,0);bad_session(object_at+std::strlen("Ninten's House/Present2"),99);bad_session(object_at+std::strlen("Ninten's House/Present2"),1);
 auto foreign_skill_flag=session_bytes;foreign_skill_flag[skill_at+7+4]=':';put(foreign_skill_flag,16,encore::crc32(foreign_skill_flag.data()+24,foreign_skill_flag.size()-24));CHECK(!session.load(foreign_skill_flag.data(),foreign_skill_flag.size(),error));
 auto unknown_order=session_bytes;const std::string order_needle="lifeUpA";auto last=std::find_end(unknown_order.begin(),unknown_order.end(),order_needle.begin(),order_needle.end());CHECK(last!=unknown_order.end());*last='x';put(unknown_order,16,encore::crc32(unknown_order.data()+24,unknown_order.size()-24));CHECK(!session.load(unknown_order.data(),unknown_order.size(),error));
 RoomData old_room;FieldEquipmentData old_menu;NativeSessionData old_session;
 CHECK(old_room.load_file("tests/fixtures/pre-basement-opening.encroom",error));CHECK(old_menu.load_file("tests/fixtures/pre-basement-menu.encfield",error));CHECK(old_session.load_file("tests/fixtures/pre-basement-session.encsession",error));
 CHECK(old_menu.view().psi_skill_count()==1&&old_menu.view().psi_skill(0).id=="telepathy");
 std::printf("Basement: %u checks; bat/full/empty, diary event %u frames, skill/target/healing, equip/save/LOAD and negative operands\n",checks,frames);
}
