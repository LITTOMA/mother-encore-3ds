#include "encore/field_scene.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/localization.hpp"
#include "encore/battle_round_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Field interaction %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Admission:FieldStoryHost {bool allowed=true;uint32_t selected=UINT32_MAX;bool can_start(FieldStoryView,uint32_t programme,std::string&e)override{selected=programme;if(!allowed){e="Controlled denied interaction";return false;}e.clear();return true;}};
struct MotionHost:MickHost {
 bool validate_flag(std::string_view,std::string&)override{return false;}
 bool validate_item(std::string_view,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}
 bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}
 bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}
 bool play_sound(std::string_view,std::string&)override{return false;}
};
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];RoomData room;FieldMapData map;WorldLinksData links;ItemData items;FieldStoryData story,legacy;MickData npc;HouseData house;BattleData font;LocaleCatalog catalog;NewGameSetupData names;NativeSessionData session;
 auto path=[&](const char*p){return root+"/"+p;};
 CHECK(room.load_file(path("world/podunk/podunk.encroom").c_str(),error));CHECK(map.load_file(path("world/podunk/podunk.encmap").c_str(),error));CHECK(links.load_file(path("world/world.enclinks").c_str(),error));CHECK(items.load_file(path("ui/opening.encitems").c_str(),error));CHECK(story.load_file(path("world/podunk/story.encstory").c_str(),error));CHECK(npc.load_file(path("world/podunk/npcs/lindgren.encmick").c_str(),error));CHECK(house.load_file(path("world/house/opening.enchouse").c_str(),error));CHECK(font.load_file(path("battle/lamp/opening.encbattle").c_str(),error));CHECK(catalog.load_file(path("localization/localization.enclocale").c_str(),error));CHECK(names.load_file(path("ui/opening.encnewgame").c_str(),error));CHECK(session.load_file(path("session/opening.encsession").c_str(),error));
 CHECK(legacy.load_file("tests/fixtures/field_story_v1.encstory",error));CHECK(legacy.view().interaction_count()==0&&legacy.view().programme_count()==2);CHECK(legacy.view().bind(room.view(),map.view(),items.view(),error));
 const auto view=story.view();CHECK(view.interaction_count()==2);CHECK(view.bind(room.view(),map.view(),items.view(),error));
 std::vector<uint8_t>bytes;CHECK(encore::read_file(path("world/podunk/story.encstory").c_str(),bytes,128*1024,error));const auto at=get(bytes,64+5*16+4);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,1},{20,3},{at,0},{at,63},{at+4,0},{at+16,4},{at+20,1},{at+8,UINT32_MAX},{at+24+4,0}}){auto bad=bytes;put(bad,edit.first,edit.second);sign(bad);FieldStoryData candidate;const bool loaded=candidate.load(bad.data(),bad.size(),error);CHECK(!loaded||!candidate.view().bind(room.view(),map.view(),items.view(),error));}
 for(size_t n:{size_t(63),size_t(145),bytes.size()-1}){FieldStoryData candidate;CHECK(!candidate.load(bytes.data(),n,error));}
 RoomData home;BattleRoundData lamp;CHECK(home.load_file(path("world/house/opening.encroom").c_str(),error));CHECK(lamp.load_file(path("battle/lamp/opening.encround").c_str(),error));auto saved=session.defaults();bool help_found=false;
 for(auto&flag:saved.flags)if(flag.id=="pippis_mom_help"){help_found=true;flag.value=true;}CHECK(help_found);CHECK(validate_native_session_snapshot(session,home.view(),house.view(),lamp.view(),items.view(),saved,error));std::vector<uint8_t>save_bytes;CHECK(encode_session_save(saved,session.compatibility(),save_bytes,error));SessionSnapshot restored;CHECK(decode_session_save(save_bytes.data(),save_bytes.size(),session.compatibility(),restored,error));CHECK(validate_native_session_snapshot(session,home.view(),house.view(),lamp.view(),items.view(),restored,error));help_found=false;for(const auto&flag:restored.flags)if(flag.id=="pippis_mom_help")help_found=flag.value;CHECK(help_found);
 LocaleSelection locale;CHECK(locale.bind(catalog,error));auto snapshot=session.defaults();snapshot.characters[0].nickname="Alex[Pippi]";auto pippi=session.startup_characters()[3];pippi.nickname="Peep";snapshot.characters.push_back(pippi);
 for(const auto code:{"en","zh_Hans_CN"}){
  CHECK(locale.select(code,error));for(uint32_t id:{1010u,1011u,1012u,1013u,1014u,1020u}){const auto text=view.dialogue(id,code);LocalizedHouseSpan span;CHECK(format_field_story_span(view.text(text.body),names,snapshot,locale,span,error));CHECK(!span.segments.empty());}
  LocalizedHouseSpan rich;CHECK(format_field_story_span("[@][color][Ninten][/color][WAIT@][DELAY][Pippi]",names,snapshot,locale,rich,error));CHECK(rich.segments.size()==2&&rich.segments[0].flags==3);CHECK(rich.segments[0].tokens[1].text=="Alex[Pippi]");CHECK(rich.segments[1].tokens[0].kind==8&&rich.segments[1].tokens[1].text=="Peep");
  for(const auto bad:{"[Unknown]","[WAIT@][","[color]x","[/color]","[color][color]x","[color]x[WAIT@]y[/color]"}){const auto prior=rich.segments.size();CHECK(!format_field_story_span(bad,names,snapshot,locale,rich,error));CHECK(rich.segments.size()==prior);}
 }
 CHECK(locale.select("en",error));snapshot.characters[0].nickname="Alex";
 SourceRandom random(991);HousePresentation box;CHECK(box.begin(house.view(),font.view(),random));
 LocalizedHouseSpan rich;CHECK(format_field_story_span("[@]A[WAIT@][color]B[/color][DELAY]C",names,snapshot,locale,rich,error));rich.speaker="Mom";rich.bullet=">";CHECK(box.present_external(rich,"voice"));CHECK(box.dialogue_pose().bullet==">");CHECK(box.advance_chrome(1));box.input(false,true);
 for(unsigned i=0;i<300&&!box.dialogue_stopped();++i)CHECK(box.advance_dialogue(1.0/60));CHECK(box.dialogue_stopped()&&!box.dialogue_finished());const auto stopped=box.visible_characters();for(unsigned i=0;i<100;++i)CHECK(box.advance_dialogue(1.0/60));CHECK(box.visible_characters()==stopped);
 auto invalid=rich;invalid.segments[0].tokens[0].kind=999;CHECK(!box.present_external(invalid));CHECK(box.visible_characters()==stopped);invalid=rich;invalid.segments[1].tokens[0].text="ffffffX";CHECK(!box.present_external(invalid));
 box.input(true,false);box.input(false,true);for(unsigned i=0;i<300&&!box.dialogue_finished();++i)CHECK(box.advance_dialogue(1.0/60));CHECK(box.dialogue_finished());bool colored=false;for(const auto&line:box.dialogue_pose().lines)for(auto color:line.colors)colored|=color!=0xffffffffu;CHECK(colored);
 const auto oldlines=box.dialogue_pose().lines.size();CHECK(box.present_external(rich,"voice"));CHECK(box.dialogue_pose().lines.size()>oldlines);box.close_story_dialogue();CHECK(box.advance_chrome(1));
 std::vector<bool>flags(room.view().flag_count(),false);for(uint32_t i=0;i<flags.size();++i){const auto id=room.view().string(room.view().flag(i).name_string);flags[i]=id=="beat_dog_bridge"||id=="pippis_mom_call";}
 // The emulator route must face the talker after its lateral approach. At
 // y283 the right-facing ray and foot hull do not reach the source-restored
 // actor (640,304); ordinary downward movement must establish contact.
 {
  Admission route_host;MotionHost route_motion;FieldScene route;auto call_flags=flags;
  for(uint32_t i=0;i<call_flags.size();++i)if(room.view().string(room.view().flag(i).name_string)=="pippis_mom_call")call_flags[i]=false;
  CHECK(route.prepare(room.view(),map.view(),links,call_flags,{510,272},{1,0},{400,240},error));CHECK(route.bind_npc(npc.view(),route_motion,error));CHECK(route.bind_story(view,items.view(),route_host,error));CHECK(route.finish_transition());CHECK(route.world.begin_house_program(1));
  unsigned tick=0;for(;tick<2400&&route.world.stage()!=OpeningStage::Walking;++tick){WalkInput input;CHECK(route.before_physics(input)&&route.world.advance(input)&&route.after_physics()&&route.world.idle_frame(1.0/60));if(route.world.story_input_allowed())CHECK(route.world.finish_story_dialogue());}
  CHECK(tick<2400&&route.world.story_flag("pippis_mom_call"));WalkInput input;CHECK(route.before_physics(input));CHECK(route.actor_pose(1).position.x==640&&route.actor_pose(1).position.y==304);
  CHECK(route.world.warp_same_scene({640,283},{1,0}));CHECK(route.try_interaction(true,error)&&route.world.stage()==OpeningStage::Walking&&route_host.selected==UINT32_MAX);
  CHECK(route.world.warp_same_scene({640,288},{0,1}));CHECK(route.try_interaction(true,error)&&route.world.stage()==OpeningStage::ScriptRunning&&route_host.selected==2);
 }
 Admission host;MotionHost motion;FieldScene field;CHECK(field.prepare(room.view(),map.view(),links,flags,{640,292},{0,-1},{400,240},error));CHECK(field.bind_npc(npc.view(),motion,error));CHECK(field.bind_story(view,items.view(),host,error));CHECK(field.finish_transition());field.world.attach_random(random);
 CHECK(!field.world.face_interaction({INFINITY,0}));CHECK(field.world.face_interaction({643,272}));CHECK(field.world.player().direction.x>0&&field.world.player().direction.y<0);CHECK(!field.world.set_house_direction({.2f,-.9f}));CHECK(field.world.warp_same_scene({640,292},{0,-1}));
 host.allowed=false;const auto before=random.state();const auto direction=field.actor_pose(1).direction;CHECK(field.try_interaction(true,error));CHECK(field.phase()==FieldPhase::Unsupported&&field.world.stage()==OpeningStage::Walking);CHECK(random.state()==before&&!field.world.story_flag("pippis_mom_help"));CHECK(field.actor_pose(1).direction.x==direction.x&&field.actor_pose(1).direction.y==direction.y);CHECK(field.idle_frame(1.0/60,true));host.allowed=true;
 for(unsigned round=0;round<2;++round){const bool started=field.try_interaction(true,error);if(!started)std::fprintf(stderr,"Interaction: %s / %s\n",field.error().c_str(),field.world.error());CHECK(started);CHECK(host.selected==uint32_t(round?3:2));CHECK(field.world.stage()==OpeningStage::ScriptRunning);unsigned lines=0;uint32_t previous=UINT32_MAX;
  for(unsigned tick=0;tick<2400&&field.world.stage()!=OpeningStage::Walking;++tick){WalkInput input;CHECK(field.before_physics(input));CHECK(field.world.advance(input));CHECK(field.after_physics());CHECK(field.world.idle_frame(1.0/60));const auto id=field.world.pending_dialogue_id();CHECK(field.world.set_story_talking(id!=UINT32_MAX));if(id!=UINT32_MAX&&id!=previous){previous=id;++lines;CHECK(view.dialogue(id,"en").id==id);for(unsigned idle=0;idle<20;++idle)CHECK(field.world.idle_frame(1.0/60));CHECK(field.world.pending_dialogue_id()==id);}
   if(field.world.story_input_allowed())CHECK(field.world.finish_story_dialogue());
  }
  CHECK(lines==unsigned(round?1:5));CHECK(field.world.stage()==OpeningStage::Walking&&field.world.story_flag("pippis_mom_help"));WalkInput input;CHECK(field.before_physics(input));CHECK(!field.world.actor_bound(1));CHECK(field.actor_pose(1).position.x==640&&field.actor_pose(1).position.y==272);CHECK(field.world.warp_same_scene({640,292},{0,-1}));
 }
 std::printf("Field interaction: %u checks; source help/repeat, rich WAIT/color/delay, old capability, malformed admission and actor restoration; host only\n",checks);
}
