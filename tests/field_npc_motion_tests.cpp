#include "encore/mick_treats.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/field_scene.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"NPC check %u: %s\n",checks,why);std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
size_t section(const std::vector<uint8_t>&b,uint32_t k){return get(b,64+(k-1)*16+4);}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
struct Host final:MickHost {
 unsigned effects=0;
 bool validate_flag(std::string_view,std::string&)override{++effects;return false;}
 bool validate_item(std::string_view,std::string&)override{++effects;return false;}
 bool flag(std::string_view,bool&,std::string&)override{++effects;return false;}
 bool set_flag(std::string_view,bool,std::string&)override{++effects;return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{++effects;return false;}
 bool remove_key_item(std::string_view,std::string&)override{++effects;return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{++effects;return false;}
 bool play_sound(std::string_view,std::string&)override{++effects;return false;}
};
struct World final:MickWorld {
 bool probe(Vec2,Vec2,bool&clear)override{clear=true;return true;}
 bool slide(Vec2 p,Vec2 v,Vec2&out)override{out={p.x+v.x/60,p.y+v.y/60};return true;}
 bool player_near(Vec2,Vec2)override{return false;}
 bool player_in_view(Vec2,float)override{return true;}
 bool shove_player(Vec2,Vec2,bool)override{return false;}
};
}
int main(int argc,char**argv){
 check(argc==4,"Lindgren, dog and legacy Mick resources required");std::string error;
 for(int arg=1;arg<=2;++arg){std::vector<uint8_t>bytes;check(encore::read_file(argv[arg],bytes,1024*1024,error),error.c_str());MickData data;
  check(data.load(bytes.data(),bytes.size(),error),error.c_str());auto view=data.view();
  check(view.motion_only()&&view.owner_id()==uint32_t(arg+1),"explicit checked motion owner identity");
  check(view.count(MickSection::Programmes)==0&&view.count(MickSection::Commands)==0&&view.count(MickSection::Texts)==0,"motion resource never borrows Mick dialogue");
  Host host;World world;MickRuntime runtime;check(runtime.initialize(view,host,error),error.c_str());check(host.effects==0,"no dialogue or inventory validation/effects for a motion owner");
  const auto initial=runtime.position();std::set<uint32_t>frames;bool moved=false;
  for(unsigned tick=0;tick<240;++tick){check(runtime.simulate(1.f/60,{600,260},{350,150},{400,240},false,world,error),error.c_str());frames.insert(runtime.sheet_frame());auto at=runtime.position();moved|=at.x!=initial.x||at.y!=initial.y;check(std::abs(at.x-initial.x)<=view.actor().wander_radius/2+1&&std::abs(at.y-initial.y)<=view.actor().wander_radius/2+1,"source wander radius retained");}
  check(moved==view.wanders(),"only source-enabled wandering moves the actor");
  check(!view.animated_idle()||frames.size()>1,"Walk idle animation remains animated");
  auto paused=runtime.position();for(unsigned tick=0;tick<30;++tick)check(runtime.simulate(1.f/60,{600,260},{350,150},{400,240},true,world,error),"paused step");check(runtime.position().x==paused.x&&runtime.position().y==paused.y,"pause preserves position");
  check(runtime.restore_motion_pose(initial,{0,1},error)&&runtime.position().x==initial.x&&runtime.position().y==initial.y,"checked cutscene pose handoff");
  check(!runtime.start_talk(error)&&runtime.state()==MickProgramState::Failed&&host.effects==0,"unsupported NPC dialogue fails closed without copying Mick text");
  auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().owner_id()==view.owner_id(),"failed admission preserves current resource");};
  {auto b=bytes;put(b,20,5);reject(b,"unknown capability");}
  {auto b=bytes;put(b,20,3);reject(b,"motion-only cannot masquerade as legacy programme resource");}
  {auto b=bytes;put(b,24,0);reject(b,"missing owner");}
  {auto b=bytes;put(b,28,8);reject(b,"unknown feature bit");}
  {auto b=bytes;put(b,section(b,4)+12,0);reject(b,"motion-only cannot bind inventory");}
  {auto b=bytes;put(b,section(b,4)+176,0);reject(b,"missing idle clips");}
  {auto b=bytes;put(b,section(b,9)+28,1);reject(b,"reserved RNG content");}
 }
 std::vector<uint8_t>legacy;check(encore::read_file(argv[3],legacy,1024*1024,error),error.c_str());MickData original;
 check(original.load(legacy.data(),legacy.size(),error)&&!original.view().motion_only()&&original.view().wanders()&&original.view().stares(),"legacy Mick contract remains admitted");
 put(legacy,20,4);put(legacy,28,3);sign(legacy);check(!original.load(legacy.data(),legacy.size(),error),"programme-bearing pack cannot masquerade as motion-only");
 {
  RoomData room;FieldMapData map;WorldLinksData links;MickData mom,dog,iggy,z1,z2;Host host;
  check(room.load_file("romfs/world/podunk/podunk.encroom",error),error.c_str());
  check(map.load_file("romfs/world/podunk/podunk.encmap",error),error.c_str());
  check(links.load_file("romfs/world/world.enclinks",error),error.c_str());
  check(mom.load_file(argv[1],error)&&dog.load_file(argv[2],error),error.c_str());
  check(iggy.load_file("romfs/world/podunk/npcs/iggy.encmick",error)&&iggy.view().owner_id()==4&&iggy.view().interaction_policy(),"independent Iggy source motion policy");
  FieldScene scene;std::vector<bool>flags(room.view().flag_count(),false);
  check(scene.prepare(room.view(),map.view(),links,flags,{560,260},{0,1},{400,240},error),error.c_str());
  check(!scene.npcs_ready(),"unbound source actor cannot be committed by the platform");
  check(scene.bind_npc(mom.view(),host,error)&&scene.bind_npc(dog.view(),host,error),error.c_str());
  check(!scene.npcs_ready(),"new map actor must not be silently discarded by the legacy two-owner path");
  check(scene.bind_npc(iggy.view(),host,error),error.c_str());
  check(!scene.npcs_ready(),"cemetery source actors also require their independent owners");
  check(z1.load_file("romfs/world/podunk/npcs/zombie1.encmick",error)&&z2.load_file("romfs/world/podunk/npcs/zombie2.encmick",error),error.c_str());
  check(scene.bind_npc(z1.view(),host,error)&&scene.bind_npc(z2.view(),host,error),error.c_str());
  check(scene.npcs_ready(),"all map motion owners admitted");
  check(!scene.bind_npc(dog.view(),host,error),"duplicate live motion owner rejected");
  check(!scene.bind_npc(original.view(),host,error),"Mick programme cannot bind as a field motion owner");
  check(scene.actor_visible(1)&&scene.actor_visible(2)&&scene.actor_visible(3)&&!scene.actor_visible(0),"map conditions select all admitted NPC actors only");
  check(scene.finish_transition(),"start checked field owner");
  std::set<uint16_t>frames;
  for(unsigned tick=0;tick<120;++tick){WalkInput input;check(scene.before_physics(input),scene.error().c_str());check(scene.world.advance(input),scene.world.error());check(scene.after_physics(),scene.error().c_str());frames.insert(scene.actor_pose(2).frame);}
  check(frames.size()>1,"live dog pose animates in source y-sort owner");
  check(host.effects==0,"ambient owners never invoke inventory or borrowed dialogue effects");
  check(scene.world.warp_same_scene({640,286},{0,-1}),"place interaction test fixture");
  check(scene.try_interaction(true,error)&&scene.phase()==FieldPhase::Unsupported,"unmapped NPC conversation is an explicit development boundary");
  check(scene.idle_frame(1.0/60,true)&&scene.phase()==FieldPhase::Walking,"unmapped NPC conversation can return control");
  check(scene.world.set_story_flag("beat_dog_bridge",true,false)&&scene.refresh_conditions(error),error.c_str());
  check(!scene.actor_visible(2)&&scene.actor_visible(1),"source victory flag hides only the defeated source actor");
  check(scene.actor_visible(3)&&scene.world.set_story_flag("returned_shovel",true,false)&&scene.refresh_conditions(error),"Iggy source visibility flag remains independent");
  check(!scene.actor_visible(3)&&scene.actor_visible(1),"return flag hides Iggy without erasing the original conversation owner");
  // Multiple moving foot bodies coexist; excluding one's own foot must retain
  // every other actor obstacle for the same motion query.
  FieldCollision collision;std::vector<bool>layers(map.view().count(FieldMapSection::Layers),false),bodies(map.view().count(FieldMapSection::Bodies),false);
  check(collision.bind(map.view(),layers,bodies,error),error.c_str());
  collision.set_body({10,10},{2,2},0);collision.set_body({20,10},{2,2},1);
  std::vector<MotionObstacle>obstacles;check(collision.collect({0,0},{30,30},obstacles)&&obstacles.size()==2,"both live foot bodies participate");
  collision.set_excluded(1);obstacles.clear();check(collision.collect({0,0},{30,30},obstacles)&&obstacles.size()==1&&obstacles[0].minimum.x==8,"exclude only the moving actor");
  collision.set_excluded(UINT32_MAX);collision.set_body({20,10},{2,2},1,false);obstacles.clear();check(collision.collect({0,0},{30,30},obstacles)&&obstacles.size()==1,"deleted actor stops blocking immediately");
 }
 std::printf("Field NPC motion: %u checks; isolated source owners, no borrowed dialogue, legacy admission and negative paths\n",checks);
}
