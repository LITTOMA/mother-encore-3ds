// Source Root A investigation; read-only data and artificial placement.
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
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Prop investigation scene %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
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
 CHECK(argc==3);const std::string root=argv[1],fixture=argv[2];auto file=[&](const char*n){return root+"/"+n;};ResourceCatalog catalog;WorldLinksData links;RoomData house,room;ItemData items;FieldSceneResources data;FieldSwingResources action;FieldStoryData metadata;
 CHECK(catalog.load_file(file("system/native.encresources").c_str(),error)&&links.load_file(file("world/world.enclinks").c_str(),error));CHECK(house.load_file(file("world/house/opening.encroom").c_str(),error)&&items.load_file(file("ui/opening.encitems").c_str(),error));CHECK(data.load(catalog,links,2,root.c_str(),house.view(),items.view(),error)&&action.load(catalog,root.c_str(),error));CHECK(room.load_file((fixture+"/podunk.encroom").c_str(),error)&&metadata.load_file((fixture+"/story.encstory").c_str(),error));const auto r=room.view();const auto story=metadata.view();CHECK(story.bind(r,data.map(),items.view(),error)&&story.bind_props(data.props(),error));CHECK(story.programme_count()==10&&story.text_count()==70&&story.prop_dialogue().when_false==8&&story.prop_dialogue().when_true==9);uint32_t target=UINT32_MAX;for(uint32_t i=0;i<data.props().props().size();++i)if(data.props().props()[i].callback!=kFieldNone)target=i;CHECK(target!=UINT32_MAX);const auto prop=data.props().props()[target];
 auto bind=[&](FieldScene&scene,Host&host,bool bat,Vec2 direction,bool admit){
  std::vector<bool>flags(r.flag_count(),false);for(uint32_t i=0;i<r.flag_count();++i){const auto name=r.string(r.flag(i).name_string);flags[i]=name=="cem_roots_grown"||(bat&&name=="bat");}
  CHECK(scene.prepare(r,data.map(),links,flags,{prop.interact_center.x,prop.interact_center.y+7},direction,{400,240},error)&&scene.world.bind_player_action(action,error));CHECK(scene.bind_mick(data.mick(),host,error));for(uint32_t i=0;i<data.npc_count();++i)CHECK(scene.bind_npc(data.npc(i),host,error));CHECK(scene.bind_story(story,items.view(),host,error)&&scene.bind_objects(data.objects(),error)&&scene.bind_prop_contacts(data.props(),action.swing(),host,error));CHECK(scene.finish_transition()&&scene.world.warp_same_scene(scene.world.player().position,direction)&&scene.idle_frame(0,false));host.admit=admit;CHECK(scene.props().states()[target].visible&&scene.props().states()[target].animation.pose().interaction);
 };
 for(bool bat:{false,true})for(unsigned hold:{5u,180u}){
  FieldScene scene;Host host(scene,data.props());bind(scene,host,bat,{0,-1},true);const auto position=scene.world.player().position;const auto revision=scene.world.story_revision();const bool opened=scene.try_interaction(true,error);if(!opened||scene.world.story_program_index()!=(bat?9u:8u)||scene.world.pending_dialogue_id()!=(bat?3301u:3300u))std::fprintf(stderr,"investigation debug opened=%d phase=%u stage=%u programme=%u dialogue=%u position=%g,%g direction=%g,%g policy=%u,%u ray=%g,%g,%g root=%g,%g extents=%g,%g\n",opened,unsigned(scene.phase()),unsigned(scene.world.stage()),scene.world.story_program_index(),scene.world.pending_dialogue_id(),scene.world.player().position.x,scene.world.player().position.y,scene.world.player().direction.x,scene.world.player().direction.y,story.prop_dialogue().when_false,story.prop_dialogue().when_true,story.prop_dialogue().ray_origin.x,story.prop_dialogue().ray_origin.y,story.prop_dialogue().ray_length,prop.interact_center.x,prop.interact_center.y,prop.interact_extents.x,prop.interact_extents.y);CHECK(opened&&scene.world.story_program_index()==(bat?9u:8u)&&scene.world.pending_dialogue_id()==(bat?3301u:3300u));CHECK(!scene.world.actor_bound(r.scene().player_instance_index));
  for(unsigned i=0;i<hold;++i){WalkInput input{1,0,false,false,false};CHECK(scene.before_physics(input)&&scene.world.advance(input)&&scene.after_physics()&&scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());CHECK(scene.world.pending_dialogue_id()==(bat?3301u:3300u)&&scene.world.player().position.x==position.x&&scene.world.player().position.y==position.y);}
  CHECK(scene.world.finish_story_dialogue());for(unsigned i=0;i<10&&!scene.world.story_completed();++i)CHECK(scene.world.idle_frame(1./60)&&scene.idle_frame(1./60,false)&&scene.world.end_scene_frame());CHECK(scene.world.story_completed()&&!scene.world.house_paused()&&scene.world.story_revision()==revision&&host.grants==0&&host.starts==0&&scene.props().states()[target].alive);for(unsigned i=0;i<8;++i){WalkInput input{1,0,false,false,false};CHECK(scene.before_physics(input)&&scene.world.advance(input)&&scene.after_physics());}CHECK(scene.world.player().position.x>position.x);
 }
 {FieldScene scene;Host host(scene,data.props());bind(scene,host,false,{0,-1},true);CHECK(scene.world.warp_same_scene({prop.interact_center.x,prop.interact_center.y+prop.interact_extents.y+2},{0,-1}));CHECK(scene.try_interaction(true,error)&&scene.world.story_program_index()==4&&scene.world.pending_dialogue_id()==3000);}
 {FieldScene scene;Host host(scene,data.props());bind(scene,host,false,{0,1},true);CHECK(scene.world.warp_same_scene({prop.interact_center.x,prop.interact_center.y+22},{0,1}));CHECK(scene.try_interaction(true,error)&&scene.world.story_program_index()!=8&&scene.world.story_program_index()!=9);}
 {FieldScene scene;Host host(scene,data.props());bind(scene,host,false,{0,-1},true);CHECK(scene.world.warp_same_scene({prop.interact_center.x-prop.interact_extents.x-2,prop.interact_center.y-6},{1,0}));CHECK(scene.try_interaction(true,error)&&scene.world.pending_dialogue_id()==UINT32_MAX&&scene.world.stage()==OpeningStage::Walking);}
 {FieldScene scene;Host host(scene,data.props());bind(scene,host,false,{0,-1},false);const auto revision=scene.world.story_revision();CHECK(scene.try_interaction(true,error)&&scene.phase()==FieldPhase::Unsupported&&scene.world.house_paused()&&scene.world.story_revision()==revision);CHECK(scene.idle_frame(0,true)&&scene.phase()==FieldPhase::Walking&&!scene.world.house_paused());}
 {FieldScene scene;Host host(scene,data.props());bind(scene,host,false,{0,-1},true);CHECK(scene.world.pause_for_house());CHECK(scene.try_interaction(true,error)&&scene.world.pending_dialogue_id()==UINT32_MAX);CHECK(scene.world.unpause_from_house());CHECK(scene.hit_prop(target,error));CHECK(!scene.props().states()[target].animation.pose().interaction);CHECK(scene.try_interaction(true,error)&&scene.world.pending_dialogue_id()==UINT32_MAX);}
 std::printf("Prop investigation scene: %u checks; checked source ray, both bat branches, manual holds, facing, pause, Break-disabled area and failed-admission recovery. Artificial source placement and prepared sound double; no current emulator/hardware claim.\n",checks);
}
