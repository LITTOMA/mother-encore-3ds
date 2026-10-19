// Actual shipping cemetery source programme. Artificial placement, shared core only.
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Cemetery %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));}
struct Admission:FieldStoryHost {unsigned calls=0;bool can_start(FieldStoryView,uint32_t,std::string&e)override{++calls;e.clear();return true;}};
struct Props:StoryPropHost {bool admit=true;unsigned calls=0;OpeningWorld*world=nullptr;std::vector<uint32_t>order;
 bool validate_grow(uint32_t id,std::string&e)const override{if(!admit||id<3||id>7){e="fixture missing prop owner";return false;}return true;}
 bool grow(uint32_t id,std::string&e)override{CHECK(world&&world->story_flag("cem_roots_grown"));++calls;order.push_back(id);return validate_grow(id,e);}};
struct Motion:MickHost {
 bool validate_flag(std::string_view,std::string&)override{return false;}bool validate_item(std::string_view,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}bool play_sound(std::string_view,std::string&)override{return false;}
};
}
int main(){
 RoomData room;FieldMapData map;WorldLinksData links;FieldPropsData props;FieldStoryData story;ItemData items;std::vector<uint8_t>bytes;
 CHECK(encore::read_file("romfs/world/podunk/podunk.encroom",bytes,16*1024*1024,error)&&room.load(bytes.data(),bytes.size(),error));
 CHECK(map.load_file("romfs/world/podunk/podunk.encmap",error)&&links.load_file("romfs/world/world.enclinks",error));
 CHECK(props.load_file("romfs/world/podunk/podunk.encprops",error)&&story.load_file("romfs/world/podunk/story.encstory",error)&&items.load_file("romfs/ui/opening.encitems",error));
 CHECK(get(bytes,36)==12&&room.view().actor_instance_count()==6&&room.view().program_count()==6);
 uint32_t programme=UINT32_MAX;for(uint32_t i=0;i<room.view().program_count();++i)if(room.view().string(room.view().program(i).source_path_string)=="Podunk/cutscenes/graveyard_zombie_roots")programme=i;
 CHECK(programme==5);const auto p=room.view().program(programme);uint32_t erase=UINT32_MAX,grow=UINT32_MAX;
 for(uint32_t i=0;i<p.command_count;++i){const auto c=room.view().command(p.first_command+i);if(c.opcode==47)erase=p.first_command+i;if(c.opcode==6)grow=c.target_index;}
 CHECK(erase!=UINT32_MAX&&grow!=UINT32_MAX);
 const auto identity=room.view().bytes();const auto binding=room.view().section_offset(RoomSection::Binding)+grow*32;
 const auto command=room.view().section_offset(RoomSection::Command)+erase*48;
 auto reject=[&](std::vector<uint8_t>b){sign(b);CHECK(!room.load(b.data(),b.size(),error)&&room.view().bytes()==identity);};
 for(uint32_t cap:{11u,13u}){auto b=bytes;put(b,36,cap);reject(b);}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{binding+4,11},{binding+8,0},{binding+8,65536},{binding+12,0},{binding+16,1},{binding+24,1},{command+2,0},{command+8,0},{command+12,1},{command+24,1}}){auto b=bytes;put(b,edit.first,edit.second);reject(b);}
 {auto b=bytes;const auto first=room.view().section_offset(RoomSection::Command);for(uint32_t i=0;i<room.view().command_count();++i){const auto c=room.view().command(i);if(c.opcode==6){put(b,first+i*48,0xffff0019);put(b,first+i*48+8,UINT32_MAX);}}reject(b);}
 OpeningWorld unbound;CHECK(unbound.initialize(room.view()));SourceRandom random(17);unbound.attach_random(random);const auto initial=random.state();
 CHECK(!unbound.validate_story_props(programme,error)&&!unbound.begin_house_program(programme));CHECK(unbound.stage()==OpeningStage::Walking&&!unbound.story_flag("cem_roots_grown")&&random.state()==initial&&unbound.action_trace().empty());
 Props host;host.world=&unbound;host.admit=false;CHECK(!unbound.bind_story_props(host,error));host.admit=true;CHECK(unbound.bind_story_props(host,error));CHECK(!unbound.bind_story_props(host,error));
 host.admit=false;CHECK(!unbound.begin_house_program(programme)&&unbound.action_trace().empty());host.admit=true;CHECK(unbound.begin_house_program(programme));
 for(unsigned i=0;i<1300&&!unbound.story_completed();++i){CHECK(unbound.advance({}));CHECK(unbound.idle_frame(1.0/60));}
 CHECK(unbound.story_completed()&&host.calls==5&&host.order==std::vector<uint32_t>({3,4,6,7,5}));CHECK(!unbound.instance_visible(4)&&!unbound.instance_visible(5)&&!unbound.actor_bound(4)&&!unbound.actor_bound(5));
 std::vector<bool>flags(room.view().flag_count(),false);FieldScene scene;Admission admission;
 CHECK(scene.prepare(room.view(),map.view(),links,flags,{264,4576},{0,1},{400,240},error));CHECK(scene.bind_props(props,error));CHECK(scene.bind_story(story.view(),items.view(),admission,error));
 Motion mh;std::vector<MickData>npcs(5);const char*names[]={"lindgren","bridge-dog","iggy","zombie1","zombie2"};
 for(unsigned i=0;i<5;++i){const auto path=std::string("romfs/world/podunk/npcs/")+names[i]+".encmick";CHECK(npcs[i].load_file(path.c_str(),error)&&scene.bind_npc(npcs[i].view(),mh,error));}
 CHECK(scene.npcs_ready()&&scene.actor_visible(4)&&scene.actor_visible(5));CHECK(scene.finish_transition());
 CHECK(scene.after_physics()&&scene.world.cutscene_active()&&admission.calls==1);std::set<uint32_t>phrases;unsigned rumble=0;
 for(unsigned tick=0;tick<1400&&!scene.world.story_completed();++tick){WalkInput input;CHECK(scene.before_physics(input));CHECK(scene.world.advance(input));CHECK(scene.after_physics());CHECK(scene.world.idle_frame(1.0/60));CHECK(scene.idle_frame(1.0/60,false));phrases.insert(scene.world.phrase());
  for(const auto&e:scene.take_prop_events()){CHECK(e.kind==FieldPropEventKind::Rumble&&scene.world.story_flag("cem_roots_grown"));++rumble;}}
 CHECK(scene.world.story_completed()&&scene.world.stage()==OpeningStage::Walking&&phrases.size()==11&&rumble==5);
 CHECK(!scene.actor_visible(4)&&!scene.actor_visible(5));CHECK(scene.world.story_flag("cem_roots_grown"));
 for(uint32_t i=2;i<7;++i){const auto&s=scene.props().states()[i];const auto pose=s.animation.pose();CHECK(s.alive&&!s.queued&&s.grow_waiters==0&&pose.visible&&pose.body&&pose.hitbox&&pose.interaction);}
 for(unsigned i=0;i<30;++i){CHECK(scene.after_physics()&&scene.world.idle_frame(1.0/60)&&scene.idle_frame(1.0/60,false));}CHECK(admission.calls==1);
 for(uint32_t i=0;i<flags.size();++i)flags[i]=scene.world.story_flag_at(i);
 FieldScene restored;CHECK(restored.prepare(room.view(),map.view(),links,flags,{264,4576},{0,1},{400,240},error)&&restored.bind_props(props,error)&&restored.bind_story(story.view(),items.view(),admission,error));CHECK(restored.finish_transition()&&restored.after_physics());
 CHECK(!restored.world.cutscene_active()&&!restored.actor_visible(4)&&!restored.actor_visible(5)&&admission.calls==1);
 for(uint32_t i=2;i<7;++i){const auto a=scene.props().states()[i].animation.pose(),b=restored.props().states()[i].animation.pose();CHECK(a.frame==b.frame&&a.visible==b.visible&&a.body==b.body&&a.hitbox==b.hitbox&&a.interaction==b.interaction);}
 std::printf("Cemetery roots: %u checks; shipping source programme, five ordered deferred Grow calls after flag, two actor erasures, 11 phrases, repeated contact and reconstructed scene; artificial placement, no emulator/hardware claim\n",checks);
}
