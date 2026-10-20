// Real bat animation, physics samples, Root Break and source getshovel.
// Placement and prepared sound service are explicit doubles; no emulator QA.
#include "encore/field_scene_resources.hpp"
#include "encore/field_scene.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Prop contact scene %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
struct Host:MickHost,FieldStoryHost,StoryKeyItemHost,FieldPropContactEffects {
 FieldScene&scene;const FieldPropsData&source;unsigned grants=0,starts=0,stops=0,prepared=0;bool admit=true,audio_admit=true;std::vector<uint32_t>voices;
 Host(FieldScene&s,const FieldPropsData&d):scene(s),source(d){}
 bool validate_flag(std::string_view name,std::string&e)override{const auto r=scene.world.content();for(uint32_t i=0;i<r.flag_count();++i)if(r.string(r.flag(i).name_string)==name){e.clear();return true;}e="Unknown source flag";return false;}
 bool flag(std::string_view name,bool&value,std::string&e)override{if(!validate_flag(name,e))return false;value=scene.world.story_flag(name);return true;}
 bool validate_item(std::string_view,std::string&e)override{e.clear();return true;}
 bool validate_key_item(std::string_view name,uint32_t doses,std::string&e)override{e.clear();return name=="BatOld"&&doses==1;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{++grants;return false;}
 bool set_flag(std::string_view name,bool value,std::string&e)override{return validate_flag(name,e)&&scene.world.set_story_flag(name,value,false);}
 bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}
 bool play_sound(std::string_view,std::string&e)override{e.clear();return true;}
 bool can_start(FieldStoryView,uint32_t,std::string&e)override{e=admit?"":"Explicit missing programme presentation";return admit;}
 bool matches(const FieldPropsData&data,std::string&e)const override{e.clear();return data.contact_bound()&&data.props().size()==source.props().size();}
 bool prepare_hits(const FieldPropsData&data,const std::vector<uint32_t>&indices,std::string&e)override{if(!matches(data,e)||!audio_admit){e="Explicit unavailable prepared sound voice";return false;}voices=indices;++prepared;return true;}
 void commit_hit(uint32_t i)override{CHECK(!voices.empty()&&voices.front()==i);voices.erase(voices.begin());++starts;}
 void stop_deleted(uint32_t i)override{CHECK(i<scene.props().states().size()&&!scene.props().states()[i].alive);++stops;}
};
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];auto file=[&](const char*n){return root+"/"+n;};ResourceCatalog catalog;WorldLinksData links;RoomData house;ItemData items;FieldSceneResources data;FieldSwingResources action;
 CHECK(catalog.load_file(file("system/native.encresources").c_str(),error)&&links.load_file(file("world/world.enclinks").c_str(),error));CHECK(house.load_file(file("world/house/opening.encroom").c_str(),error)&&items.load_file(file("ui/opening.encitems").c_str(),error));CHECK(data.load(catalog,links,2,root.c_str(),house.view(),items.view(),error)&&action.load(catalog,root.c_str(),error));CHECK(data.props().contact_bound());const auto r=data.room();
 uint32_t target=kFieldNone;for(uint32_t i=0;i<data.props().props().size();++i)if(data.props().props()[i].callback!=kFieldNone)target=i;CHECK(target!=kFieldNone);const auto&prop=data.props().props()[target];const Vec2 start{prop.position.x,prop.position.y+24};const auto duration=data.props().clips().clip(data.props().policy().broken)->duration;
 std::vector<bool>flags(r.flag_count(),false);for(uint32_t i=0;i<flags.size();++i){const auto name=r.string(r.flag(i).name_string);flags[i]=name=="bat"||name=="cem_roots_grown";}
 auto bind=[&](FieldScene&scene,Host&host,const std::vector<bool>&bits){
  CHECK(scene.prepare(r,data.map(),links,bits,start,{0,-1},{400,240},error));CHECK(scene.world.bind_player_action(action,error)&&scene.world.bind_story_key_items(host,error));CHECK(scene.bind_mick(data.mick(),host,error));for(uint32_t i=0;i<data.npc_count();++i)CHECK(scene.bind_npc(data.npc(i),host,error));CHECK(scene.bind_story(data.story(),items.view(),host,error));
  CHECK(!scene.bind_prop_contacts(data.props(),action.swing(),host,error)&&!scene.props().ready()); // Object lifecycle missing: atomic refusal.
  CHECK(scene.bind_objects(data.objects(),error));FieldSwingData missing;CHECK(!scene.bind_prop_contacts(data.props(),missing,host,error)&&!scene.props().ready());CHECK(scene.bind_prop_contacts(data.props(),action.swing(),host,error));CHECK(!scene.bind_prop_contacts(data.props(),action.swing(),host,error));CHECK(scene.finish_transition()&&scene.idle_frame(0,false));
 };
 auto tick=[&](FieldScene&scene){WalkInput input;CHECK(scene.before_physics(input));CHECK(scene.world.advance(input)&&scene.after_physics());CHECK(scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());};
 auto swing=[&](FieldScene&scene){bool accepted=false;CHECK(scene.world.player_action_input(action.action().strings()[2],true,true,false,false,accepted,error)&&accepted);};
 // The same source Area is a normal body trigger too. check_start persists
 // during a player/menu pause, and a body exit cancels its next idle pass.
 for(unsigned mode=0;mode<4;++mode){
  FieldScene scene;Host host(scene,data.props());bind(scene,host,flags);const auto area=data.map().boundary(prop.callback);
  CHECK(scene.world.pause_for_house()&&scene.world.warp_same_scene(area.center,{1,0})&&scene.world.unpause_from_house());WalkInput input;CHECK(scene.before_physics(input)&&scene.world.advance(input)&&scene.after_physics()&&!scene.world.cutscene_active());
  if(mode==1){CHECK(scene.world.pause_for_house()&&scene.world.warp_same_scene(start,{0,-1})&&scene.world.unpause_from_house());input={};CHECK(scene.before_physics(input)&&scene.world.advance(input)&&scene.after_physics());}
  if(mode==2)CHECK(scene.world.pause_for_house());
  if(mode==3)scene.battle_to_overworld();
  CHECK(scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());CHECK(scene.world.cutscene_active()==(mode==0));
  if(mode==2){CHECK(scene.world.unpause_from_house()&&scene.idle_frame(0,false)&&scene.world.cutscene_active());}
  CHECK(scene.props().states()[target].alive&&host.starts==0);
 }
 for(unsigned hold:{5u,18u}){
  FieldScene scene;Host host(scene,data.props());bind(scene,host,flags);CHECK(scene.objects().state(1)->alive);swing(scene);unsigned hit_tick=0,deleted_tick=0;
  for(unsigned t=1;t<160&&!deleted_tick;++t){tick(scene);if(host.starts&&!hit_tick)hit_tick=t;if(hit_tick&&scene.props().states()[target].alive)CHECK(!scene.world.cutscene_active());if(!scene.props().states()[target].alive){deleted_tick=t;CHECK(!scene.world.cutscene_active());}}
  CHECK(hit_tick&&deleted_tick&&host.starts==1&&host.prepared==1&&host.stops==1);CHECK(double(deleted_tick-hit_tick+1)/60>=double(duration)-1./60);CHECK(scene.props().clones().size()==1&&!scene.props().states()[target].alive);CHECK(!scene.world.story_flag("got_shovel"));
  tick(scene);CHECK(scene.world.cutscene_active()); // check_start runs on the next idle, not in the Root callback.
  unsigned texts=0,held=0;uint32_t previous=kFieldNone;bool flag_before_delete=false;
  for(unsigned t=0;t<8000&&!scene.world.story_completed();++t){tick(scene);const auto id=scene.world.pending_dialogue_id();if(id!=kFieldNone&&id!=previous){CHECK(id>=3200&&id<=3204&&data.story().dialogue(id,"en").id==id);previous=id;held=0;++texts;}if(id==3200){CHECK(scene.world.story_flag("got_shovel")&&scene.objects().state(1)->alive);flag_before_delete=true;}if(scene.world.story_input_allowed()&&++held>=hold)CHECK(scene.world.finish_story_dialogue());}
  CHECK(scene.world.story_completed()&&texts==5&&flag_before_delete&&host.grants==0&&host.starts==1);CHECK(scene.world.stage()==OpeningStage::Walking&&!scene.world.house_paused()&&!scene.objects().state(1)->alive&&!scene.actor_visible(3));const auto p=scene.world.player().position;CHECK(p.x==1408&&p.y==4720&&scene.world.player().direction.x==-1&&scene.world.player().direction.y==0);
  std::vector<bool>saved;for(uint32_t i=0;i<r.flag_count();++i)saved.push_back(scene.world.story_flag_at(i));FieldScene again;Host again_host(again,data.props());bind(again,again_host,saved);CHECK(again.props().states()[target].alive&&!again.objects().state(1)->alive&&!again.actor_visible(3));swing(again);for(unsigned t=0;t<160;++t)tick(again);CHECK(!again.props().states()[target].alive&&again_host.starts==1&&again_host.stops==1&&!again.world.cutscene_active()&&again_host.grants==0);
 }
 // A source programme or audio reservation failure precedes Root mutation.
 for(bool missing_audio:{false,true}){FieldScene scene;Host host(scene,data.props());bind(scene,host,flags);if(missing_audio)host.audio_admit=false;else host.admit=false;swing(scene);bool declined=false;for(unsigned t=0;t<160&&!declined;++t){WalkInput input;if(!scene.before_physics(input)){declined=true;break;}CHECK(scene.world.advance(input)&&scene.after_physics());CHECK(scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());}CHECK(declined&&scene.props().clones().empty()&&scene.props().states()[target].alive&&host.starts==0&&!scene.world.story_flag("got_shovel"));}
 std::printf("Field prop contact scene: %u checks; source Bat contact, Break, delayed check_start, full five-text programme, two confirmation durations, exact return/reentry and preflight refusal; sound is a prepared-service double, placement artificial, no PCM/GPU/emulator/hardware claim\n",checks);
}
