// Codec/reference fixtures only. The synthetic third scene is not upstream gameplay.
#include "encore/world_links.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Scene resources %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void append(std::vector<uint8_t>&b,uint32_t v){const auto at=b.size();b.resize(at+4);put(b,at,v);}
void sign(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
size_t section(const std::vector<uint8_t>&b,unsigned k){return get(b,64+(k-1)*16+4);}
size_t catalog_row(const std::vector<uint8_t>&b,uint32_t id){size_t at=60;for(uint32_t i=0;i<get(b,52);++i){if(get(b,at)==id)return at;at+=20+get(b,at+16);}return at;}
void add_binding(std::vector<uint8_t>&b,uint32_t id,uint32_t role,const char*path){const auto base=catalog_row(b,role),at=catalog_row(b,UINT32_MAX);std::vector<uint8_t>r;for(auto v:{id,role,get(b,base+8),get(b,base+12),uint32_t(std::strlen(path))})append(r,v);r.insert(r.end(),path,path+std::strlen(path));b.insert(b.begin()+at,r.begin(),r.end());put(b,52,get(b,52)+1);put(b,20,9);sign(b);}
std::vector<uint8_t>extended_links(const std::vector<uint8_t>&legacy){
 const unsigned stride[]={1,24,84,36,4};std::vector<std::vector<uint8_t>>s(5);
 for(unsigned i=0;i<3;++i){const auto first=section(legacy,i+1);const auto count=get(legacy,64+i*16+8);s[i].assign(legacy.begin()+first,legacy.begin()+first+count*stride[i]);}
 auto text=[&](const char*value){auto at=uint32_t(s[0].size());s[0].insert(s[0].end(),value,value+std::strlen(value));return at;};const char*path="res://Maps/Schema Fixture.tscn";const auto off=text(path);
 for(auto v:{3u,off,uint32_t(std::strlen(path)),1024u,1025u,0u})append(s[1],v);
 auto route=std::vector<uint8_t>(s[2].begin(),s[2].begin()+84);put(route,0,3);put(route,4,2);put(route,8,3);s[2].insert(s[2].end(),route.begin(),route.end());put(route,0,4);put(route,4,3);s[2].insert(s[2].end(),route.begin(),route.end());
 for(auto v:{2u,42u,37u,38u,39u,35u,0u,2u,0u,3u,1026u,0u,0u,0u,0u,0u,0u,0u})append(s[3],v);
 append(s[4],320);append(s[4],321);
 auto b=std::vector<uint8_t>(legacy.begin(),legacy.begin()+64);b.resize(64+5*16);put(b,20,2);put(b,52,5);
 for(unsigned i=0;i<5;++i){const auto count=uint32_t(s[i].size()/stride[i]);while(b.size()%4)b.push_back(0);put(b,64+i*16,i+1);put(b,64+i*16+4,count?uint32_t(b.size()):0);put(b,64+i*16+8,count);put(b,64+i*16+12,stride[i]);b.insert(b.end(),s[i].begin(),s[i].end());}sign(b);return b;
}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>cat,links;CHECK(encore::read_file(argv[1],cat,128*1024,error));CHECK(encore::read_file(argv[2],links,64*1024,error));ResourceCatalog legacy;WorldLinksData world;
 CHECK(legacy.load(cat.data(),cat.size(),error));CHECK(world.load(links.data(),links.size(),error));CHECK(world.capability()==1);CHECK(world.bind_catalog(legacy,error));WorldSceneResources resolved;CHECK(world.resolve_scene(2,legacy,resolved,error));CHECK(resolved.story==42&&resolved.music==37&&resolved.npcs==std::vector<uint32_t>({320,321}));
 auto multi=cat;add_binding(multi,1024,31,"world/schema-fixture/room.encroom");add_binding(multi,1025,32,"world/schema-fixture/map.encmap");add_binding(multi,1026,42,"world/schema-fixture/story.encstory");ResourceCatalog extended;CHECK(extended.load(multi.data(),multi.size(),error));CHECK(extended.path(ResourceRole::FieldRoom,1024)=="world/schema-fixture/room.encroom");CHECK(extended.path(ResourceRole::FieldMap,1024).empty());CHECK(extended.ids(ResourceRole::FieldRoom).size()==2);CHECK(extended.path(ResourceRole::FieldRoom)==legacy.path(ResourceRole::FieldRoom));CHECK(!world.bind_catalog(extended,error));
 auto bytes=extended_links(links);CHECK(world.load(bytes.data(),bytes.size(),error));CHECK(world.capability()==2&&world.scene_count()==3&&world.route_count()==4);CHECK(world.bind_catalog(extended,error));CHECK(world.resolve_scene(3,extended,resolved,error));CHECK(resolved.scene.room_role==1024&&resolved.scene.map_role==1025&&resolved.story==1026&&resolved.npcs.empty());CHECK(world.route(3).from==world.route(3).to);CHECK(!world.bind_catalog(legacy,error));const auto before=resolved.scene.id;CHECK(!world.resolve_scene(99,extended,resolved,error)&&resolved.scene.id==before);
 const auto assets=section(bytes,4),refs=section(bytes,5);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,3},{52,4},{assets,1},{assets+32,1},{assets+24,3},{assets+28,129},{assets+12,0},{assets+36,2},{refs,0},{refs+4,320}}){auto bad=bytes;put(bad,edit.first,edit.second);sign(bad);CHECK(!world.load(bad.data(),bad.size(),error));CHECK(world.capability()==2&&world.scene_count()==3);}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{assets+4,1025},{assets+8,1024},{refs,1026},{assets+36+4,9999}}){auto bad=bytes;put(bad,edit.first,edit.second);sign(bad);WorldLinksData candidate;const bool loaded=candidate.load(bad.data(),bad.size(),error);CHECK(!loaded||!candidate.bind_catalog(extended,error));}
 for(size_t n:{size_t(0),size_t(111),size_t(143),bytes.size()-1}){CHECK(!world.load(bytes.data(),n,error));CHECK(world.capability()==2);}
 auto same=links;put(same,section(same,3)+8,get(same,section(same,3)+4));sign(same);CHECK(!world.load(same.data(),same.size(),error));
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,8},{20,10},{catalog_row(multi,1024),512},{catalog_row(multi,1024)+4,40}}){auto bad=multi;put(bad,edit.first,edit.second);sign(bad);CHECK(!extended.load(bad.data(),bad.size(),error));CHECK(extended.path(ResourceRole::FieldMap,1025)=="world/schema-fixture/map.encmap");}
 auto unused=multi;add_binding(unused,1030,42,"world/schema-fixture/unowned.encstory");ResourceCatalog unowned;CHECK(unowned.load(unused.data(),unused.size(),error));CHECK(!world.bind_catalog(unowned,error));
 std::printf("Scene resources: %u checks; legacy and multiple typed bindings, same-scene routes, ownership and malformed metadata; codec fixtures only\n",checks);
}
