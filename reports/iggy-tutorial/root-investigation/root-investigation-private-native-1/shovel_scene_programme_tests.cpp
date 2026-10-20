// Complete source programme through the real scene/NPC/object owners.
// Artificial post-teaching placement; Root contact and physical audio excluded.
#include "encore/field_scene_resources.hpp"
#include "encore/field_scene.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace{unsigned checks=0;std::string error;
void check(bool value,const char*why){++checks;if(!value){std::fprintf(stderr,"Shovel scene %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Host:MickHost,FieldStoryHost,StoryKeyItemHost,FieldPropHost {
 FieldScene&scene;unsigned grants=0,callbacks=0;
 explicit Host(FieldScene&s):scene(s){}
 bool validate_flag(std::string_view name,std::string&e)override{const auto r=scene.world.content();for(uint32_t i=0;i<r.flag_count();++i)if(r.string(r.flag(i).name_string)==name){e.clear();return true;}e="Unknown source scene flag";return false;}
 bool flag(std::string_view name,bool&value,std::string&e)override{if(!validate_flag(name,e))return false;value=scene.world.story_flag(name);return true;}
 bool validate_item(std::string_view,std::string&e)override{e.clear();return true;}
 bool validate_key_item(std::string_view name,uint32_t doses,std::string&e)override{e.clear();return name=="BatOld"&&doses==1;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{++grants;return false;}
 bool set_flag(std::string_view name,bool value,std::string&e)override{if(!validate_flag(name,e))return false;return scene.world.set_story_flag(name,value,false);}
 bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}
 bool play_sound(std::string_view,std::string&e)override{e.clear();return true;}
 bool can_start(FieldStoryView,uint32_t,std::string&e)override{e.clear();return true;}
 bool can_call(uint32_t callback,std::string&e)override{const auto path=scene.map().text(scene.map().boundary(callback).path);for(uint32_t i=0;i<scene.story().programme_count();++i){const auto p=scene.story().programme(i);if(scene.story().text(p.boundary)==path)return scene.world.validate_story_props(p.programme,e);}e="Unbound fixture callback";return false;}
 bool call(uint32_t callback,uint32_t prop,std::string&e)override{CHECK(scene.props().states().at(prop).alive);if(!can_call(callback,e))return false;++callbacks;return true;} // Acknowledged seam only; no Root check_start implementation claim.
};
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];auto file=[&](const char*n){return root+"/"+n;};ResourceCatalog catalog;WorldLinksData links;RoomData house;ItemData items;FieldSceneResources data;FieldSwingResources action;
 CHECK(catalog.load_file(file("system/native.encresources").c_str(),error)&&links.load_file(file("world/world.enclinks").c_str(),error));CHECK(house.load_file(file("world/house/opening.encroom").c_str(),error)&&items.load_file(file("ui/opening.encitems").c_str(),error));CHECK(data.load(catalog,links,2,root.c_str(),house.view(),items.view(),error)&&action.load(catalog,root.c_str(),error));
 const auto r=data.room();CHECK(r.bytes()[36]==14&&r.program_count()==10&&data.story().programme_count()==8&&data.objects().drawing_bound());uint32_t programme=UINT32_MAX;
 for(uint32_t i=0;i<data.story().programme_count();++i){const auto p=data.story().programme(i);if(data.story().text(p.source)=="Podunk/cutscenes/gravedigger_getshovel")programme=p.programme;}CHECK(programme==7);
 std::vector<bool>flags(r.flag_count(),false);for(uint32_t i=0;i<flags.size();++i){const auto name=r.string(r.flag(i).name_string);flags[i]=name=="bat"||name=="cem_roots_grown";}
 auto bind=[&](FieldScene&scene,Host&host,const std::vector<bool>&bits){
  CHECK(scene.prepare(r,data.map(),links,bits,{1432,4719},{0,-1},{400,240},error));CHECK(scene.world.bind_player_action(action,error)&&scene.world.bind_story_key_items(host,error));CHECK(scene.bind_mick(data.mick(),host,error));
  for(uint32_t i=0;i<data.npc_count();++i)CHECK(scene.bind_npc(data.npc(i),host,error));CHECK(scene.bind_story(data.story(),items.view(),host,error)&&scene.bind_props(data.props(),error)&&scene.bind_objects(data.objects(),error));CHECK(scene.npcs_ready()&&scene.finish_transition()&&scene.idle_frame(0,false));
 };
 Vec2 result{};
 for(unsigned hold:{5u,18u}){
  FieldScene scene;Host host(scene);bind(scene,host,flags);CHECK(scene.objects().state(1)->alive&&scene.objects().state(1)->visible&&scene.actor_visible(3));uint32_t root=UINT32_MAX;for(uint32_t i=0;i<data.props().props().size();++i)if(data.props().props()[i].callback!=kFieldNone)root=i;CHECK(root!=UINT32_MAX&&scene.world.validate_story_props(programme,error));
  // Establish the real post-break collider state without simulating the
  // not-yet-connected bat contact or production callback dispatcher.
  CHECK(host.can_call(data.props().props()[root].callback,error)&&scene.hit_prop(root,error));for(unsigned i=0;i<8;++i)CHECK(scene.world.idle_frame(.1)&&scene.idle_frame(.1,false)&&scene.world.end_scene_frame());CHECK(scene.flush_props(host,error)&&host.callbacks==1&&!scene.props().states()[root].alive);CHECK(scene.world.begin_house_program(programme));unsigned texts=0,held=0;uint32_t previous=UINT32_MAX;bool flag_while_visible=false;
  for(unsigned tick=0;tick<8000&&!scene.world.story_completed();++tick){
   WalkInput input;CHECK(scene.before_physics(input)&&scene.world.advance(input)&&scene.after_physics());CHECK(scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());
   const auto id=scene.world.pending_dialogue_id();if(id!=UINT32_MAX&&id!=previous){CHECK(id>=3200&&id<=3204&&data.story().dialogue(id,"en").id==id);previous=id;held=0;++texts;}
   if(id==3200){CHECK(scene.world.story_flag("got_shovel")&&scene.objects().state(1)->alive&&scene.objects().state(1)->visible);flag_while_visible=true;}
   if(scene.world.story_input_allowed()&&++held>=hold)CHECK(scene.world.finish_story_dialogue());
  }
  CHECK(scene.world.story_completed()&&texts==5&&flag_while_visible&&host.grants==0);CHECK(scene.world.stage()==OpeningStage::Walking&&!scene.world.house_paused()&&scene.world.story_flag("got_shovel")&&scene.world.story_flag("returned_shovel"));CHECK(!scene.objects().state(1)->alive&&!scene.actor_visible(3));const auto p=scene.world.player().position;CHECK(p.x==1408&&p.y==4720&&scene.world.player().direction.x==-1&&scene.world.player().direction.y==0);if(hold==5)result=p;else CHECK(p.x==result.x&&p.y==result.y);
  std::vector<bool>saved;for(uint32_t i=0;i<r.flag_count();++i)saved.push_back(scene.world.story_flag_at(i));FieldScene again;Host again_host(again);bind(again,again_host,saved);CHECK(!again.objects().state(1)->alive&&!again.actor_visible(3));const auto revision=again.world.story_revision();CHECK(!again.world.begin_house_program(programme)&&again.world.healthy()&&again.world.story_revision()==revision&&again_host.grants==0);
 }
 std::printf("Formal shovel scene: %u checks; two manual text durations, bound real NPC/player/object owners, live flag before deferred deletion, no invented item grant, exact control return and suppressed reentry; artificial placement, no Root/audio/GPU/emulator/hardware claim\n",checks);
}
