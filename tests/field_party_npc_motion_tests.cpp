#include "encore/mick_treats.hpp"
#include "encore/content.hpp"
#include "encore/field_scene.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Party NPC check %u: %s\n",checks,why);std::exit(1);}}
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
 bool seen=true;
 bool probe(Vec2,Vec2,bool&clear)override{clear=true;return true;}
 bool slide(Vec2 p,Vec2,Vec2&out)override{out=p;return true;}
 bool player_near(Vec2,Vec2)override{return false;}
 bool player_in_view(Vec2,float)override{return seen;}
 bool shove_player(Vec2,Vec2,bool)override{return false;}
};
}
int main(int argc,char**argv){
 check(argc==4,"Pippi candidate, existing NPC and Mick required");std::string error;std::vector<uint8_t>bytes;
 check(encore::read_file(argv[1],bytes,65536,error),error.c_str());MickData data;
 check(data.load(bytes.data(),bytes.size(),error),error.c_str());auto view=data.view();
 check(view.motion_only()&&view.owner_id()==6&&view.direction_count()==8,"independent eight-direction NPC owner");
 check(!view.wanders()&&view.stares()&&!view.animated_idle(),"source Pippi motion modes");
 Host host;World world;MickRuntime npc;check(npc.initialize(view,host,error),error.c_str());check(host.effects==0,"motion does not borrow dialogue or inventory effects");
 const Vec2 spawn=npc.position();
 // Exact source PartyMember Idle frames, including diagonal blend points.
 const Vec2 targets[]={{0,1},{-1,0},{1,0},{0,-1},{-1,1},{1,1},{-1,-1},{1,-1},{1,.5f},{1,.1f}};
 const unsigned frames[]={1,11,21,31,41,51,61,71,51,21};
 for(unsigned i=0;i<10;++i){
  check(npc.simulate(1.f/60,{spawn.x+targets[i].x,spawn.y+targets[i].y},{-400,-800},{400,240},false,world,error),error.c_str());
  check(npc.sheet_frame()==frames[i],"source closest eight-direction point, including shallow diagonals");
  check(npc.position().x==spawn.x&&npc.position().y==spawn.y,"stationary NPC does not move");
 }
 world.seen=false;npc.face_toward({spawn.x-2,spawn.y-1});check(npc.sheet_frame()==61,"direct interaction facing selects diagonal");
 check(npc.restore_motion_pose(spawn,{0,-1},error)&&npc.sheet_frame()==31,"pose restore retains eight-direction sheet");
 check(!npc.restore_motion_pose(spawn,{},error),"invalid restoration fails closed");
 auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().owner_id()==6&&data.view().direction_count()==8,"failed admission preserves old owner");};
 for(uint32_t cap:{3u,4u,5u,7u}){auto b=bytes;put(b,20,cap);reject(b,"downgrade or unknown capability");}
 for(uint32_t features:{1u,3u,8u}){auto b=bytes;put(b,28,features);reject(b,"unreviewed wandering or feature");}
 for(uint32_t directions:{0u,4u,9u}){auto b=bytes;put(b,56,directions);reject(b,"wrong eight-direction policy");}
 {auto b=bytes;put(b,60,1);reject(b,"reserved policy word");}
 {auto b=bytes;put(b,28,8);reject(b,"unknown motion feature");}
 {auto b=bytes;put(b,24,0);reject(b,"missing owner");}
 {auto b=bytes;put(b,section(b,4)+12,0);reject(b,"motion cannot bind items");}
 {auto b=bytes;put(b,section(b,4)+180,0);reject(b,"overlapping clip spans");}
 {auto b=bytes;b[section(b,8)+7*8+2]=0;reject(b,"missing one direction");}
 {auto b=bytes;b[section(b,8)]=1;reject(b,"wrong animation role");}
 {auto b=bytes;b[section(b,8)+2]=8;reject(b,"direction out of range");}
 {auto b=bytes;b[section(b,8)+4]=255;b[section(b,8)+5]=255;reject(b,"sheet frame out of range");}
 {auto b=bytes;b[section(b,8)+6]=0;b[section(b,8)+7]=0;reject(b,"zero duration");}
 {auto b=bytes;b[32]^=1;reject(b,"wrong source pin");}
 for(size_t n=0;n<bytes.size();++n){check(!data.load(bytes.data(),n,error),"all truncations rejected");check(data.view().owner_id()==6,"truncation preserves old owner");}
 for(int arg=2;arg<=3;++arg){MickData old;check(old.load_file(argv[arg],error),error.c_str());check(old.view().direction_count()==4,"legacy motion/explicit Mick keep four directions");}
 {
  RoomData room;FieldMapData map;WorldLinksData links;FieldScene scene;MickData aliased;
  check(room.load_file("romfs/world/podunk/podunk.encroom",error)&&map.load_file("romfs/world/podunk/podunk.encmap",error)&&links.load_file("romfs/world/world.enclinks",error),error.c_str());
  check(scene.prepare(room.view(),map.view(),links,std::vector<bool>(room.view().flag_count(),false),{560,260},{0,1},{400,240},error),error.c_str());
  auto b=bytes;put(b,24,2);sign(b);check(aliased.load(b.data(),b.size(),error),error.c_str());
  check(!scene.bind_npc(aliased.view(),host,error)&&error=="Field NPC replacement direction profile differs","eight-direction motion cannot borrow a four-direction replacement profile");
 }
 check(host.effects==0,"no unsupported game effect executed");
 std::printf("Party NPC motion: %u checks; actual source Pippi candidate; no Room replacement, dialogue, renderer or hardware claim\n",checks);
}
