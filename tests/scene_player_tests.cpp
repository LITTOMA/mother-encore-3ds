#include "encore/world.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Scene player %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool transients(const WalkState&a,const WalkState&b){return same(a.velocity,b.velocity)&&a.speed==b.speed&&a.crouch==b.crouch&&a.tap_run==b.tap_run&&a.running==b.running&&a.substantial==b.substantial&&a.walking==b.walking&&a.previous_toggle==b.previous_toggle&&a.animation==b.animation&&a.moved_signals==b.moved_signals;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
std::unique_ptr<OpeningWorld> prepared(RoomView room,Vec2 p={520,397},Vec2 d={0,-1}){
 auto w=std::make_unique<OpeningWorld>();std::vector<bool>flags(room.flag_count(),false),reviewed(room.flag_count(),true);
 CHECK(w->initialize_restored(room,flags,reviewed,p,d));CHECK(w->set_party_leader("ninten"));CHECK(w->pause_for_house());return w;
}
}
int main(int argc,char**argv){
 CHECK(argc==4);RoomData house,mayor;CHECK(house.load_file(argv[1],error)&&mayor.load_file(argv[2],error));
 // Independently generated native source outputs. Movement fields here are
 // opaque transport values, not a claim that arbitrary speeds are playable.
 std::ifstream stream(argv[3]);CHECK(bool(stream));std::string line;unsigned rows=0;Vec2 previous{0,1};
 while(std::getline(stream,line)){if(line.empty()||line[0]=='#')continue;
  std::istringstream row(line);unsigned index,tap,running,substantial,toggle,shaker;Vec2 p,d,input;float speed;Vec2 velocity,offset;std::string run_sound;
  CHECK(bool(row>>index>>p.x>>p.y>>input.x>>input.y>>d.x>>d.y>>tap>>running>>speed>>velocity.x>>velocity.y>>substantial>>toggle>>run_sound>>offset.x>>offset.y>>shaker));
  CHECK(index==rows++);WalkState old;old.position={-3,11};old.direction=previous;old.speed=speed;old.velocity=velocity;old.tap_run=tap;old.running=running;old.substantial=substantial;old.previous_toggle=toggle;old.moved_signals=17;old.animation=MotionAnimation::Run;
  WalkState result;
  CHECK(relocate_walk(old,p,input,result));CHECK(same(result.position,p)&&same(result.direction,d)&&transients(old,result));previous=d;
  WalkState preserved=result;
  for(const auto invalid: {Vec2{.25f,1},Vec2{2,0},Vec2{std::numeric_limits<float>::infinity(),0},Vec2{0,std::numeric_limits<float>::quiet_NaN()}}){
   CHECK(!relocate_walk(old,p,invalid,result));CHECK(same(result.position,preserved.position)&&same(result.direction,preserved.direction)&&transients(result,preserved));
  }
  CHECK(!relocate_walk(old,{std::numeric_limits<float>::infinity(),0},{},result));auto bad=old;bad.speed=0;CHECK(!relocate_walk(bad,p,{},result));bad=old;bad.animation=MotionAnimation(255);CHECK(!relocate_walk(bad,p,{},result));bad=old;bad.velocity.x=std::numeric_limits<float>::quiet_NaN();CHECK(!relocate_walk(bad,p,{},result));bad=old;bad.direction={};CHECK(!relocate_walk(bad,p,{},result));
 }
 CHECK(rows==6);
 auto active=prepared(house.view(),{-100,-100});CHECK(active->unpause_from_house());CHECK(active->advance({0,0,true})&&active->advance({0,0,false}));CHECK(active->advance({0,0,false}));CHECK(active->player().tap_run&&active->player().running);
 CHECK(active->pause_for_house());CHECK(active->shake_house_camera(2,4,{1,0}));CHECK(active->idle_frame(1.0/60));
 const auto*identity=active->persistent_player();const auto*camera=&active->cutscene_camera();const auto*walk=&active->player();
 for(unsigned i=0;i<6;++i){
  auto next=prepared(i%2?house.view():mayor.view());const auto old=active->player();const auto offset=active->cutscene_camera().display_offset();
  const Vec2 p=i%2?Vec2{520,397}:Vec2{8,9};const Vec2 direction=i%3?Vec2{1,0}:Vec2{};
  CHECK(!next->adopt_scene_player(*next,p,direction,error));CHECK(active->healthy()&&next->healthy());
  CHECK(!next->adopt_scene_player(*active,{std::numeric_limits<float>::infinity(),0},direction,error));CHECK(active->healthy()&&next->healthy()&&active->persistent_player()==identity);
  CHECK(next->adopt_scene_player(*active,p,direction,error));
  CHECK(!active->healthy()&&active->retains_existing_player());CHECK(next->healthy()&&next->persistent_player()==identity&&&next->player()==walk&&&next->cutscene_camera()==camera);
  CHECK(same(next->player().position,p)&&same(next->player().direction,i%3?direction:old.direction)&&transients(old,next->player()));
  CHECK(next->house_paused()&&next->cutscene_camera().is_shaking()&&same(next->cutscene_camera().display_offset(),offset));
  CHECK(!active->advance({1,0})&&!active->idle_frame(1.0/60)&&!active->unpause_from_house()&&!active->set_house_direction({-1,0})&&!active->shake_house_camera(1,1,{1,0})&&!active->set_party_leader("replacement"));
  CHECK(!active->initialize(house.view()));CHECK(next->persistent_player()->party_leader()=="ninten");
  active.reset();CHECK(next->persistent_player()==identity&&next->cutscene_camera().is_shaking());
  CHECK(next->advance({0,0,true,true}));CHECK(next->player().previous_toggle);active=std::move(next);
 }
 auto next=prepared(house.view());CHECK(active->unpause_from_house());CHECK(!next->adopt_scene_player(*active,{520,397},{},error));CHECK(active->pause_for_house());
 CHECK(next->unpause_from_house());CHECK(!next->adopt_scene_player(*active,{520,397},{},error));CHECK(next->pause_for_house());
 CHECK(next->set_party_leader("other"));CHECK(!next->adopt_scene_player(*active,{520,397},{},error));CHECK(next->set_party_leader("ninten"));
 auto shared=next->retain_player();CHECK(!next->adopt_scene_player(*active,{520,397},{},error));shared={};
 CHECK(active->cutscene_camera().is_shaking());CHECK(next->healthy()&&active->healthy());
 // Validly parsed but incompatible player metadata cannot replace either owner.
 std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,kMaxRoomBytes,error));const auto room=house.view();const auto profile=room.actor_profile(room.actor_instance(room.scene().player_instance_index).profile_index);
 bytes[room.section_offset(RoomSection::Resource)+size_t(profile.primary_resource)*56+24]^=1;put(bytes,52,0);put(bytes,52,encore::crc32(bytes.data(),bytes.size()));RoomData changed;CHECK(changed.load(bytes.data(),bytes.size(),error));
 auto incompatible=prepared(changed.view());CHECK(!incompatible->adopt_scene_player(*active,{520,397},{},error));CHECK(active->persistent_player()==identity&&active->healthy()&&incompatible->healthy());
 OpeningWorld unprepared;CHECK(!unprepared.adopt_scene_player(*active,{520,397},{},error));
 std::printf("Scene player: %u checks; six native transport outcomes, same object/camera across prepared rooms, old callback rejection and failed handoff preservation; shared core only\n",checks);
}
