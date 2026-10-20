#include "encore/field_scene.hpp"
#include "encore/dungeon_rooms.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Live field camera %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void real(std::vector<uint8_t>&b,size_t at,float v){uint32_t raw;std::memcpy(&raw,&v,4);put(b,at,raw);}
}
int main(int argc,char**argv){
 CHECK(argc==4);const std::string dir=argv[1];RoomData room;WorldLinksData links;DungeonRoomsData rooms;std::vector<uint8_t>source;
 CHECK(room.load_file((dir+"/mayor-initial-story.encroom").c_str(),error)&&links.load_file((dir+"/mayor-actor-routes.enclinks").c_str(),error)&&rooms.load_file(argv[2],error));
 CHECK(encore::read_file((dir+"/mayor-area.encmap").c_str(),source,8*1024*1024,error));
 const auto cameras=get(source,64+19*16+4),count=get(source,64+19*16+8);CHECK(count>0);std::ifstream input(argv[3]);CHECK(input);std::string line;unsigned seen=0;
 while(std::getline(input,line)){
  unsigned vi,action,index,active,signals,inside,visited;int current;std::array<int,4>limits;std::istringstream row(line);
  CHECK(bool(row>>vi>>action>>index>>active>>current>>signals>>inside>>visited>>limits[0]>>limits[1]>>limits[2]>>limits[3]));if(action!=2||active!=1)continue;
  // Select only each original room's isolated post-idle observation. Later
  // overlap/ABA cases belong to DungeonRoomState, not FieldScene's old policy.
  // Translator retains the room index in current; isolated source pass occurs
  // before any overlap sequence and visits each room once per viewport.
  const unsigned ordinal=seen%18;if(seen>=36||vi!=seen/18||index!=ordinal||current!=int(ordinal))continue;CHECK(ordinal<rooms.rooms().size());
  const auto&r=rooms.rooms()[ordinal];auto bytes=source;
  for(uint32_t i=0;i<count;++i){const size_t at=cameras+i*56;real(bytes,at,r.center.x);real(bytes,at+4,r.center.y);real(bytes,at+8,r.extents.x);real(bytes,at+12,r.extents.y);real(bytes,at+16,0);real(bytes,at+20,0);real(bytes,at+24,r.size.x);real(bytes,at+28,r.size.y);real(bytes,at+32,0);real(bytes,at+36,0);put(bytes,at+40,0);put(bytes,at+44,0);put(bytes,at+48,0);}
  put(bytes,16,encore::crc32(bytes.data()+32,bytes.size()-32));FieldMapData map;CHECK(map.load(bytes.data(),bytes.size(),error));FieldScene field;
  const Vec2 viewport=vi?Vec2{400,240}:Vec2{320,180};std::vector<bool>flags(room.view().flag_count(),false);
  CHECK(field.prepare(room.view(),map.view(),links,flags,r.center,{0,1},viewport,error)&&field.finish_transition()&&field.after_physics());
  const auto actual=field.camera_origin(r.center,viewport);
  const Vec2 expected{std::floor(std::clamp(r.center.x-viewport.x*.5f,float(limits[0]),float(limits[2])-viewport.x)),std::floor(std::clamp(r.center.y-viewport.y*.5f,float(limits[1]),float(limits[3])-viewport.y))};
  if(actual.x!=expected.x||actual.y!=expected.y)std::fprintf(stderr,"viewport%u room%u actual(%g,%g),native(%g,%g)\n",vi,ordinal,actual.x,actual.y,expected.x,expected.y);
  CHECK(actual.x==expected.x&&actual.y==expected.y);++seen;
 }
 CHECK(input.eof()&&seen==36);std::printf("Live field camera: %u checks;36 native isolated room limits projected through prepared FieldScene at original/adapted viewport; controlled map fixture, not live dungeon\n",checks);
}
