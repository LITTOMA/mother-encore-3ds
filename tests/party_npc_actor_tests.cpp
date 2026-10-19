#include "encore/actor_actions.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/field_scene.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Party actor %u: %s\n",checks,why);std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(i*8));}
size_t section(const std::vector<uint8_t>&b,unsigned kind){return get(b,128+(kind-1)*24+4);}
void sign(std::vector<uint8_t>&b){put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));}
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
}
int main(int argc,char**argv){
 check(argc==5,"original candidate Room, Map, Links and Pippi motion required");std::string error;std::vector<uint8_t>bytes;RoomData data;
 check(encore::read_file(argv[1],bytes,1024*1024,error)&&data.load(bytes.data(),bytes.size(),error),error.c_str());const auto room=data.view();
 check(room.actor_profile_count()==6&&room.actor_instance_count()==6&&room.actor_profile(5).execution_kind==4,"source Mayor roster with a distinct PartyMember NPC replacement");
 check(room.actor_profile(5).direction_count==8&&room.actor_profile(5).animation_binding_count==24,"eight Idle/Walk/Talk directions admitted");
 ActorActionState actor;check(initialize_actor(actor,room,5,{-216,-600},{0,1}),"initialize actual source Pippi replacement");check(actor.directional_animation&&actor.frame==1,"source south Idle");
 const Vec2 dirs[]={{0,1},{-1,0},{1,0},{0,-1},{-1,1},{1,1},{-1,-1},{1,-1}};
 const unsigned frames[]={1,11,21,31,41,51,61,71};
 for(unsigned i=0;i<8;++i){check(actor_turn(actor,dirs[i]),"turn source PartyMember Actor");check(actor_physics_step(actor)&&actor_idle_step(actor),"directional physics and idle");check(actor.frame==frames[i],"Actor retains source diagonal PartyMember Idle");}
 check(actor_turn(actor,{0,1})&&actor_set_talking(actor,true),"enable talk");std::set<uint16_t>talk_frames;
 for(unsigned i=0;i<60;++i){check(actor_physics_step(actor)&&actor_idle_step(actor),"source Talk progression");if(i>1)talk_frames.insert(actor.frame);}
 check(actor.playing_motion==5&&talk_frames.size()>1,"PartyMember Talk animates rather than borrowing player movement");
 check(actor_set_talking(actor,false),"stop talk");for(unsigned i=0;i<60;++i)check(actor_physics_step(actor)&&actor_idle_step(actor),"source Talk-to-Idle end transition");check(actor.playing_motion==0&&actor.frame==1,"source return to Idle");
 const ActorPathEntry move{ActorPathEntryKind::Move,{-200,-584},0};check(actor_move_path(actor,&move,1,64,ActorMoveType::Position,false,4),"source diagonal Walk command");
 std::set<uint16_t>walk_frames;for(unsigned i=0;i<50;++i){check(actor_physics_step(actor)&&actor_idle_step(actor),"walk progression");walk_frames.insert(actor.frame);}
 check(actor.position.x==-200&&actor.position.y==-584&&walk_frames.size()>1,"source Actor arrives and animates");
 auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().actor_profile(5).execution_kind==4,"rejection keeps admitted Room");};
 for(unsigned cap:{7u,8u,9u,11u}){auto b=bytes;put(b,36,cap);reject(b,"new NPC cannot be downgraded or use unknown capability");}
 const auto profile=section(bytes,12)+5*80,instance=section(bytes,13)+5*40;
 {auto b=bytes;b[profile+4]=5;reject(b,"unknown executor");}
 {auto b=bytes;b[profile+68]=4;reject(b,"wrong direction count");}
 {auto b=bytes;b[instance+8]=1;reject(b,"NPC cannot become Player binding");}
 {auto b=bytes;b[profile+24]=23;reject(b,"missing directional binding");}
 {auto b=bytes;const auto stride=get(b,128+25*24+12);const size_t at=section(b,26)+room.actor_profile(5).animation_binding_first*stride;check(stride==8&&at+3<b.size(),"bounded animation-binding corruption fixture");b[at+3]=8;reject(b,"direction out of range");}
 for(size_t n=0;n<bytes.size();n+=31)check(!data.load(bytes.data(),n,error),"truncation rejected");
 {
  FieldMapData map;WorldLinksData links;MickData motion;Host host;FieldScene scene;
  check(map.load_file(argv[2],error)&&links.load_file(argv[3],error)&&motion.load_file(argv[4],error),error.c_str());
  check(scene.prepare(room,map.view(),links,std::vector<bool>(room.flag_count(),false),{8,9},{0,-1},{400,240},error),error.c_str());
  check(scene.bind_npc(motion.view(),host,error),error.c_str());check(scene.actor_visible(5)&&scene.actor_pose(5).frame==1,"actual Pippi motion binds to its eight-direction replacement");
  check(!scene.npcs_ready(),"unbound remaining Mayor owners cannot be published as a complete scene");
 }
 std::printf("Party NPC Actor: %u checks; original source roster/animations, new Room capability and atomic negative paths; no Mayor dialogue, GPU or hardware claim\n",checks);
}
