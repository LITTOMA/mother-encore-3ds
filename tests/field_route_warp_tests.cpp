// Synthetic same-scene route; actual frozen Podunk metadata. No new map claim.
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Field warp %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void number(std::vector<uint8_t>&b,size_t at,float n){uint32_t v;std::memcpy(&v,&n,4);put(b,at,v);}
std::vector<uint8_t>with_warp(const std::vector<uint8_t>&legacy){
 const unsigned stride[]={1,24,84,36,4};std::vector<std::vector<uint8_t>>sections(5);
 for(unsigned i=0;i<5;++i){auto at=get(legacy,64+i*16+4),n=get(legacy,64+i*16+8);sections[i].assign(legacy.begin()+at,legacy.begin()+at+n*stride[i]);}
 const char*name="SchemaFixture/internal-door";auto off=uint32_t(sections[0].size());sections[0].insert(sections[0].end(),name,name+std::strlen(name));
 auto route=std::vector<uint8_t>(sections[2].begin()+84,sections[2].begin()+168);uint32_t identity=0;for(size_t i=0;i<sections[2].size();i+=84)identity=std::max(identity,get(sections[2],i));put(route,0,identity+1);put(route,4,2);put(route,8,2);put(route,12,off);put(route,16,uint32_t(std::strlen(name)));number(route,20,400);number(route,24,500);number(route,28,0);number(route,32,0);sections[2].insert(sections[2].end(),route.begin(),route.end());
 // Preserve every actual scene/asset owner; the additional warp is a test fixture only.
 auto b=std::vector<uint8_t>(legacy.begin(),legacy.begin()+64);b.resize(144);put(b,20,get(legacy,20));put(b,52,5);
 for(unsigned i=0;i<5;++i){while(b.size()%4)b.push_back(0);auto n=uint32_t(sections[i].size()/stride[i]);put(b,64+i*16,i+1);put(b,68+i*16,n?uint32_t(b.size()):0);put(b,72+i*16,n);put(b,76+i*16,stride[i]);b.insert(b.end(),sections[i].begin(),sections[i].end());}
 put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));return b;
}
}
int main(int argc,char**argv){
 CHECK(argc==4);RoomData room;FieldMapData map;WorldLinksData links;std::vector<uint8_t>bytes;
 CHECK(room.load_file(argv[1],error));CHECK(map.load_file(argv[2],error));CHECK(encore::read_file(argv[3],bytes,65536,error));bytes=with_warp(bytes);CHECK(links.load(bytes.data(),bytes.size(),error));
 const auto warp=links.route_count()-1;FieldScene field;CHECK(!field.warp_route(warp,error));std::vector<bool>flags(room.view().flag_count(),false);if(!flags.empty())flags[0]=true;
 CHECK(field.prepare(room.view(),map.view(),links,flags,{318,273},{1,0},{400,240},error));SourceRandom random(17);field.world.attach_random(random);const auto rng=random.state();const auto pos=field.world.player().position;const auto revision=field.world.story_revision();
 CHECK(!field.warp_route(99,error)&&!field.warp_route(0,error)&&!field.warp_route(1,error));CHECK(field.phase()==FieldPhase::Transition&&field.world.player().position.x==pos.x&&field.world.player().position.y==pos.y&&random.state()==rng&&field.world.story_revision()==revision);
 CHECK(field.warp_route(warp,error));CHECK(field.world.player().position.x==400&&field.world.player().position.y==500&&field.world.player().direction.x==1&&field.world.player().direction.y==0);CHECK(field.phase()==FieldPhase::Transition&&field.world.house_paused());CHECK(random.state()==rng&&random.raw_draw_count()==0&&field.world.story_revision()==revision);if(!flags.empty())CHECK(field.world.story_flag_at(0));
 CHECK(field.finish_transition());CHECK(!field.warp_route(warp,error));CHECK(field.world.pause_for_house());field.abort_transition("fixture unsupported boundary");CHECK(field.idle_frame(1.0/60,true));CHECK(field.phase()==FieldPhase::Walking&&field.world.player().position.x==400&&field.world.player().position.y==500&&!field.world.house_paused());CHECK(random.state()==rng&&field.world.story_revision()==revision);
 std::printf("Field route warp: %u checks; actual metadata with synthetic route, atomic rejection, preserved direction/flags/RNG and new safe return point; host only\n",checks);
}
