#include "encore/cutscene_camera.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Camera binding %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
void number(std::vector<uint8_t>&b,size_t at,double n){uint64_t bits;std::memcpy(&bits,&n,8);for(unsigned i=0;i<8;++i)b[at+i]=uint8_t(bits>>(8*i));}
}
int main(int argc,char**argv){
 CHECK(argc==3);RoomData stable;CHECK(stable.load_file(argv[1],error));const auto room=stable.view();const auto position=room.scene().spawn;const auto duration=room.rule_f64(RoomRuleKey::CameraDefaultShakeSeconds);
 CutsceneCamera baseline,owned;CHECK(baseline.initialize(room,position));CHECK(baseline.shake(2,duration,{1,0}));
 {RoomData temporary;CHECK(temporary.load_file(argv[1],error));CHECK(owned.initialize(temporary.view(),position));CHECK(owned.shake(2,duration,{1,0}));}
 // The original metadata buffer has been destroyed before every operation.
 for(unsigned i=0;i<120;++i){Vec2 p{position.x+float(i),position.y};CHECK(baseline.relocate_player(p)&&owned.relocate_player(p));CHECK(baseline.physics_frame()&&owned.physics_frame());CHECK(baseline.idle_frame(1.0/60)&&owned.idle_frame(1.0/60));CHECK(same(baseline.center(),owned.center())&&same(baseline.display_offset(),owned.display_offset())&&baseline.is_shaking()==owned.is_shaking());}
 CHECK(baseline.shake(2,duration,{1,0})&&owned.shake(2,duration,{1,0}));CHECK(baseline.physics_frame()&&owned.physics_frame());const auto offset=owned.display_offset();
 {RoomData next;CHECK(next.load_file(argv[2],error));CHECK(owned.rebind_scene(next.view(),next.view().scene().spawn,{400,240}));CHECK(same(owned.display_offset(),offset)&&owned.is_shaking());}
 for(unsigned i=0;i<120;++i){CHECK(baseline.physics_frame()&&owned.physics_frame());CHECK(baseline.idle_frame(1.0/60)&&owned.idle_frame(1.0/60));CHECK(same(baseline.display_offset(),owned.display_offset())&&baseline.is_shaking()==owned.is_shaking());}
 auto unchanged=owned.center();CHECK(!owned.rebind_scene({},position,{400,240})&&same(owned.center(),unchanged));CHECK(!owned.rebind_scene(room,position,{0,240})&&same(owned.center(),unchanged));CHECK(!owned.rebind_scene(room,{std::numeric_limits<float>::infinity(),0},{400,240})&&same(owned.center(),unchanged));
 CHECK(!owned.initialize({},position)&&same(owned.center(),unchanged));CHECK(!owned.initialize(room,position,{400,0})&&same(owned.center(),unchanged));
 CutsceneCamera unprepared;CHECK(!unprepared.rebind_scene(room,position,{400,240}));
 auto actor=owned;CHECK(actor.change_to_actor(9,position));const auto actor_center=actor.center();CHECK(!actor.rebind_scene(room,position,{400,240})&&actor.follows_actor()&&same(actor.center(),actor_center));
 auto moving=owned;CHECK(moving.move_to(position,1));CHECK(!moving.rebind_scene(room,position,{400,240})&&moving.is_moving());
 std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,kMaxRoomBytes,error));bool changed=false;
 for(uint32_t i=0;i<room.count(RoomSection::Rule);++i)if(room.rule(i).key==RoomRuleKey::CameraShakeWeight){number(bytes,room.section_offset(RoomSection::Rule)+size_t(i)*16+8,.25);changed=true;}
 CHECK(changed);put(bytes,52,0);put(bytes,52,encore::crc32(bytes.data(),bytes.size()));RoomData different;CHECK(different.load(bytes.data(),bytes.size(),error));CHECK(!owned.rebind_scene(different.view(),position,{400,240})&&same(owned.center(),unchanged));
 std::printf("Camera binding: %u checks; owned data after room destruction, source-compatible scene limits preserve shakers, invalid/moving/actor/mismatched rule rejection; host only\n",checks);
}