#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/field_scene.hpp"
#include "encore/audio_data.hpp"
#include "encore/native_session.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Published Mayor %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Host:FieldStoryHost,MickHost {
 bool can_start(FieldStoryView,uint32_t,std::string&e)override{e.clear();return true;}
 bool validate_flag(std::string_view,std::string&)override{return false;}bool validate_item(std::string_view,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}bool play_sound(std::string_view,std::string&)override{return false;}
};
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];auto file=[&](const char*p){return root+"/"+p;};
 ResourceCatalog catalog;WorldLinksData links;RoomData opening;ItemData items;AudioBank audio;NativeSessionData session;
 CHECK(catalog.load_file(file("system/native.encresources").c_str(),error));CHECK(links.load_file(file("world/world.enclinks").c_str(),error));CHECK(links.bind_catalog(catalog,error));CHECK(links.capability()==4&&links.scene_count()==3&&links.route_count()==6);
 CHECK(opening.load_file(file("world/house/opening.encroom").c_str(),error));CHECK(items.load_file(file("ui/opening.encitems").c_str(),error));CHECK(audio.load_file(file("sound/banks/opening.encaudio").c_str(),error));
 CHECK(session.load_file(file("session/opening.encsession").c_str(),error));
 const auto entry=links.route(links.find_route(3));CHECK(entry.from==2&&entry.to==3);FieldSceneResources metadata;
 CHECK(metadata.load(catalog,links,entry.to,root.c_str(),opening.view(),items.view(),error));CHECK(metadata.valid()&&metadata.room().scene().stable_id==entry.to);CHECK(metadata.npc_count()==5&&metadata.story().programme_count()==4&&metadata.steps().valid());
 auto before=metadata.room().scene().stable_id;CHECK(!metadata.load(catalog,links,2,root.c_str(),opening.view(),items.view(),error));CHECK(metadata.room().scene().stable_id==before);
 Host host;
 for(unsigned cycle=0;cycle<3;++cycle){
  FieldScene scene;std::vector<bool>flags(metadata.room().flag_count(),false);
  CHECK(scene.prepare(metadata.room(),metadata.map(),links,flags,entry.destination,entry.direction,{400,240},error));
  for(uint32_t i=0;i<metadata.npc_count();++i)CHECK(scene.bind_npc(metadata.npc(i),host,error));
  CHECK(scene.npcs_ready());CHECK(scene.bind_story(metadata.story(),items.view(),host,error));CHECK(scene.bind_steps(metadata.steps(),audio,session.defaults().run_sound,error));CHECK(scene.finish_transition());
  for(unsigned tick=0;tick<120;++tick){WalkInput input;CHECK(scene.before_physics(input));CHECK(scene.world.advance(input));CHECK(scene.after_physics());CHECK(scene.world.idle_frame(1.0/60));CHECK(scene.idle_frame(1.0/60,false));CHECK(scene.world.end_scene_frame());}
  CHECK(scene.phase()==FieldPhase::Walking);CHECK(scene.world.stage()==OpeningStage::Walking);
 }
 // Begin at the actual incoming door poses, then approach by ordinary physics.
 // Direct placement inside an NPC interaction rectangle would miss a blocked desk.
 for(uint32_t route_id:{3u,5u}){
  const auto incoming=links.route(links.find_route(route_id));FieldScene scene;std::vector<bool>flags(metadata.room().flag_count(),false);
  CHECK(scene.prepare(metadata.room(),metadata.map(),links,flags,incoming.destination,incoming.direction,{400,240},error));
  for(uint32_t i=0;i<metadata.npc_count();++i)CHECK(scene.bind_npc(metadata.npc(i),host,error));
  CHECK(scene.bind_story(metadata.story(),items.view(),host,error));CHECK(scene.finish_transition());
  const float y=route_id==3?-20.f:-560.f;unsigned tick=0;
  for(;tick<300&&scene.world.player().position.y>y;++tick){WalkInput input{0,-1};CHECK(scene.before_physics(input));CHECK(scene.world.advance(input));CHECK(scene.after_physics());CHECK(scene.world.idle_frame(1.0/60));CHECK(scene.idle_frame(1.0/60,false));CHECK(scene.world.end_scene_frame());}
  CHECK(tick<300);CHECK(scene.phase()==FieldPhase::Walking);CHECK(scene.try_interaction(true,error));CHECK(scene.world.pending_dialogue_id()==(route_id==3?2000u:2001u));
 }
 for(uint32_t id:{4u,5u,6u}){auto route=links.route(links.find_route(id));CHECK(route.from==3);CHECK(route.to==(id==4?2u:3u));if(id!=4)CHECK(route.music_fade==0);}
 std::printf("Published Mayor: %u checks; actual catalog/resources, source entry and internal routes, five actors, inherited run sound and repeated scene preparation; no emulator/hardware claim\n",checks);
}
