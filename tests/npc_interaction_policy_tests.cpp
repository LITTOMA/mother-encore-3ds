#include "encore/mick_treats.hpp"
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace encore::upstream;
namespace{
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"NPC policy %u: %s\n",checks,why);std::exit(1);}}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned j=0;j<4;++j)b[at+j]=uint8_t(v>>(j*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
struct Host final:MickHost{
 bool validate_flag(std::string_view,std::string&)override{return false;}
 bool validate_item(std::string_view,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}
 bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}
 bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}
 bool play_sound(std::string_view,std::string&)override{return false;}
};
struct World final:MickWorld{
 bool seen=false;
 bool probe(Vec2,Vec2,bool&clear)override{clear=true;return true;}
 bool slide(Vec2 p,Vec2,Vec2&out)override{out=p;return true;}
 bool player_near(Vec2,Vec2)override{return false;}
 bool player_in_view(Vec2,float)override{return seen;}
 bool shove_player(Vec2,Vec2,bool)override{return false;}
};
struct Case{MickData data;MickRuntime motion;World world;};
struct Action{unsigned actor=0,frame=0;std::string name;};
}
int main(int argc,char**argv){
 check(argc==9,"three policy resources, native states/actions and source Room/Map/Links required");std::string error;Host host;std::array<Case,8>cases;
 for(unsigned i=0;i<8;++i){
  auto&c=cases[i];check(c.data.load_file(argv[i<3?1:2],error),error.c_str());check(c.data.view().interaction_policy()&&c.data.view().return_delay()==1&&c.data.view().has_dialogue(),"source policy fields");
  if(i==6){std::vector<uint8_t>b;check(encore::read_file(argv[2],b,65536,error),error.c_str());b[28]|=1;check(!c.data.load(b.data(),b.size(),error),"wandering policy explicitly rejected; no claimed native replay");continue;}
  check(c.motion.initialize(c.data.view(),host,error),error.c_str());check(c.motion.restore_motion_pose(c.motion.position(),{1,0},error),"fixture begins at source dialogue-facing boundary");
 }
 std::vector<Action>actions;{std::ifstream file(argv[5]);Action a;while(file>>a.actor>>a.frame>>a.name){check(a.actor<8&&a.frame<180,"action domain");actions.push_back(a);}check(file.eof()&&!actions.empty(),"complete source action fixture");}
 std::ifstream reference(argv[4]);check(bool(reference),"native reference file");unsigned compared=0,unsupported=0;
 for(unsigned frame=0;frame<180;++frame){
  for(const auto&a:actions)if(a.frame==frame&&a.actor!=6){auto&c=cases[a.actor];const auto p=c.motion.position();
   if(a.name=="stop")check(c.motion.end_interaction(error),error.c_str());
   else if(a.name=="begin"){check(c.motion.begin_interaction(error),error.c_str());c.motion.face_toward({p.x+1,p.y});}
   else if(a.name=="face")c.motion.face_toward({p.x+1,p.y});
   else if(a.name=="enter")c.world.seen=true;
   else if(a.name=="exit")c.world.seen=false;
   else check(a.name=="other_exit"&&a.actor==7,"only the player's overlap feeds native observe_player");
  }
  for(unsigned i=0;i<8;++i){unsigned f=0,id=0;float x=0,y=0;std::string looking,paused,talking,mute;
   check(bool(reference>>f>>id>>x>>y>>looking>>paused>>talking>>mute)&&f==frame&&id==i,"all native rows in order");
   if(i==6){++unsupported;continue;}
   auto&c=cases[i];const auto p=c.motion.position();check(c.motion.simulate(1.f/60,{p.x+1,p.y},{0,0},{400,240},false,c.world,error),error.c_str());
   check(c.motion.idle_frame(1.f/60,error),error.c_str());auto direction=c.motion.facing();check(direction.x==x&&direction.y==y,"original native timer/facing outcome at this frame");++compared;
  }
 }
 {std::string extra;check(!(reference>>extra)&&reference.eof(),"no extra native frames");}
 check(compared==1260&&unsupported==180,"seven cases replayed; wander case rejected, never silently executed");
 {
  MickData data;check(data.load_file(argv[1],error),error.c_str());std::vector<uint8_t>base;check(encore::read_file(argv[1],base,65536,error),error.c_str());
  auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().interaction_policy()&&data.view().return_delay()==1,"admission failure preserves current owner");};
  for(unsigned cap:{3u,4u,6u,8u}){auto b=base;put(b,20,cap);reject(b,"downgrade or unknown capability");}
  for(unsigned delay:{0u,30001u,0xffffffffu}){auto b=base;put(b,60,delay);reject(b,"invalid source policy duration");}
  for(unsigned directions:{0u,3u,9u}){auto b=base;put(b,56,directions);reject(b,"invalid direction policy");}
  {auto b=base;put(b,28,16);reject(b,"unknown policy feature");}
  for(size_t size=0;size<base.size();++size)check(!data.load(base.data(),size,error),"all policy truncations reject");
  MickRuntime runtime;check(runtime.initialize(data.view(),host,error),error.c_str());
  for(unsigned i=0;i<64;++i)check(runtime.end_interaction(error),"bounded independent source timer registration");check(!runtime.end_interaction(error),"capacity fails explicitly");
  check(!runtime.idle_frame(-.1,error)&&!runtime.idle_frame(std::nan(""),error),"invalid timer deltas rejected");
 }
 {
  MickData silent;check(silent.load_file(argv[3],error)&&!silent.view().has_dialogue(),"original office Abbott has no dialogue");MickRuntime runtime;check(runtime.initialize(silent.view(),host,error),error.c_str());check(!runtime.begin_interaction(error),"no invented dialogue admission");
  RoomData room;FieldMapData map;WorldLinksData links;FieldScene scene;
  check(room.load_file(argv[6],error)&&map.load_file(argv[7],error)&&links.load_file(argv[8],error),error.c_str());
  const auto at=silent.view().actor().interact_center;
  check(scene.prepare(room.view(),map.view(),links,std::vector<bool>(room.view().flag_count(),false),at,{0,-1},{400,240},error)&&scene.bind_npc(silent.view(),host,error),error.c_str());
  check(scene.finish_transition(),"source metadata interaction fixture ready");check(scene.try_interaction(true,error)&&scene.phase()==FieldPhase::Walking&&!scene.world.house_paused(),"disabled source interact area cannot enter unported-dialogue stop");
 }
 std::printf("NPC interaction policy: %u checks; %u native facing rows match; %u unsupported wandering rows rejected by admission; no sprite/dialogue/hardware claim\n",checks,compared,unsupported);
}
