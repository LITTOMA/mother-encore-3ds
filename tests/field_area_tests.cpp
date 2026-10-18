#include "encore/field_scene.hpp"
#include "encore/field_menus.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"AreaRoom %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string fixtures=argv[1],romfs=argv[2];FieldMapData map,legacy;RoomData room;WorldLinksData links;std::vector<uint8_t>bytes;
 CHECK(encore::read_file((fixtures+"/mayor-area.encmap").c_str(),bytes,8*1024*1024,error));CHECK(map.load(bytes.data(),bytes.size(),error));const auto v=map.view();CHECK(v.capabilities()==5&&v.count(FieldMapSection::Area)==1);const auto a=v.area();
 CHECK(v.text(a.source)=="res://Maps/podunk/Mayors Office.tscn");CHECK(v.text(a.region)=="Podunk"&&v.text(a.map_item)=="MapPodunk");CHECK(v.text(a.map_override)=="Merrysville"&&v.text(a.map_override_item)=="MapMerrysville");CHECK(a.map_offset.x==110&&a.map_offset.y==240&&a.map_divisor==16&&a.sub_area&&a.background==0x1c1c1cff);
 CHECK(legacy.load_file((fixtures+"/mayor-static-actor.encmap").c_str(),error));CHECK(legacy.view().count(FieldMapSection::Area)==0&&legacy.view().area().map_divisor==0);
 CHECK(room.load_file((fixtures+"/mayor-initial-story.encroom").c_str(),error));CHECK(links.load_file((fixtures+"/mayor-actor-routes.enclinks").c_str(),error));
 const auto r=room.view();std::vector<bool>flags(r.flag_count(),false);uint32_t visit=kRoomNoIndex,guard=kRoomNoIndex;
 for(uint32_t i=0;i<r.flag_count();++i){const auto name=r.string(r.flag(i).name_string);if(name==v.text(a.visit_flag))visit=i;if(name==v.text(a.party_guard_flag))guard=i;}
 CHECK(visit!=kRoomNoIndex&&guard!=kRoomNoIndex);
 for(bool already:{false,true}){flags[visit]=already;FieldScene scene;CHECK(scene.prepare(r,v,links,flags,{8,-63},{0,1},{400,240},error));CHECK(scene.world.story_flag(v.text(a.visit_flag)));CHECK(!scene.world.story_flag(v.text(a.party_guard_flag)));CHECK(scene.world.story_revision()==1);CHECK(flags[visit]==already);CHECK(scene.finish_transition());CHECK(scene.world.advance({}));}
 flags[guard]=true;FieldScene unsupported;CHECK(!unsupported.prepare(r,v,links,flags,{8,-63},{0,1},{400,240},error));CHECK(error.find("flying")!=error.npos);flags[guard]=false;
 const auto area=get(bytes,64+21*16+4);auto reject=[&](std::vector<uint8_t>bad){sign(bad);CHECK(!map.load(bad.data(),bad.size(),error));CHECK(map.view().bytes_identity()==v.bytes_identity());CHECK(map.view().area().background==a.background);};
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,4},{20,6},{28,1},{52,21},{64+21*16,23},{64+21*16+8,0},{64+21*16+12,84},{area,UINT32_MAX},{area+8,UINT32_MAX},{area+12,0},{area+64,0},{area+64,0x7fc00000},{area+68,0},{area+72,2},{area+76,1}}){auto bad=bytes;put(bad,change.first,change.second);reject(std::move(bad));}
 for(size_t n=0;n<bytes.size();n+=31){CHECK(!map.load(bytes.data(),n,error));CHECK(map.view().bytes_identity()==v.bytes_identity());}CHECK(!map.load(bytes.data(),bytes.size()-1,error));
 {auto bad=bytes;put(bad,area+40,get(bytes,area+8));put(bad,area+44,get(bytes,area+12));sign(bad);FieldMapData unknown;CHECK(unknown.load(bad.data(),bad.size(),error));FieldScene scene;CHECK(!scene.prepare(r,unknown.view(),links,flags,{8,-63},{0,1},{400,240},error));CHECK(error.find("unregistered")!=error.npos);}
 std::ifstream native(fixtures+"/area-state.tsv");CHECK(native.good());std::string line;unsigned cases=0;
 while(std::getline(native,line)){if(line.empty()||line[0]=='#')continue;int sub,old,visited,signals,party;Vec2 p,expected;std::istringstream row(line);CHECK(bool(row>>sub>>old>>p.x>>p.y>>expected.x>>expected.y>>visited>>signals>>party));auto controlled=a;controlled.sub_area=sub;const auto point=field_area_map_position(controlled,p);CHECK(point.x==expected.x&&point.y==expected.y);flags[visit]=old;FieldScene scene;CHECK(scene.prepare(r,v,links,flags,{8,-63},{0,1},{400,240},error));CHECK(scene.world.story_flag(v.text(a.visit_flag))==bool(visited));CHECK(scene.world.story_revision()==unsigned(signals));CHECK(!party&&!scene.world.story_flag(v.text(a.party_guard_flag)));++cases;}CHECK(cases==16);
 FieldMenusData menus;CHECK(encore::read_file((romfs+"/ui/field.encmenus").c_str(),bytes,4*1024*1024,error));CHECK(menus.load(bytes.data(),bytes.size(),error));FieldMenusState state;state.area_map=true;state.area_map_item=std::string(v.text(a.map_item));state.area_map_position={a.map_offset.x,a.map_offset.y};
 for(Vec2 pos:std::vector<Vec2>{{8,-63},{8,-519},{-216,-600},{1000,1000}}){state.world_x=pos.x;state.world_y=pos.y;const auto point=field_menu_map_position(menus,state);CHECK(point[0]==110&&point[1]==240);}
 std::printf("AreaRoom: %u checks; source metadata, staged Ready flag, single-party boundary, fixed map location and parser negatives; no GPU/hardware claim\n",checks);
}
