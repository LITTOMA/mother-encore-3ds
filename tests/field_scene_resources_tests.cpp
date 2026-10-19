#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Field owner %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];auto path=[&](const char*p){return root+"/"+p;};ResourceCatalog catalog;WorldLinksData links;RoomData domain;ItemData items;
 CHECK(catalog.load_file(path("system/native.encresources").c_str(),error));CHECK(links.load_file(path("world/world.enclinks").c_str(),error));CHECK(domain.load_file(path("world/house/opening.encroom").c_str(),error));CHECK(items.load_file(path("ui/opening.encitems").c_str(),error));WorldSceneResources refs;CHECK(links.resolve_scene(2,catalog,refs,error));
 FieldSceneResources owner;CHECK(!owner.valid()&&!owner.room().valid()&&!owner.map().valid()&&!owner.npc(0).valid());CHECK(owner.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error));CHECK(owner.valid()&&owner.room().scene().stable_id==2&&owner.map().scene_id()==2&&owner.story().valid());CHECK(owner.npc_count()==refs.npcs.size()&&owner.mick().valid());for(unsigned i=0;i<owner.npc_count();++i)CHECK(owner.npc(i).motion_only());CHECK(!owner.npc(owner.npc_count()).valid());CHECK(owner.zones().zones().size()==owner.music().regions().size());
 CHECK(owner.npc_count()==3&&owner.npc(2).owner_id()==4&&owner.npc(2).interaction_policy());
 const auto scene=owner.room().scene().stable_id;const auto source=std::string(owner.bindings().scene.source);CHECK(!owner.load(catalog,links,999,nullptr,{}, {},error));CHECK(owner.room().scene().stable_id==scene&&owner.bindings().scene.source==source);
 {FieldSceneResources bad;CHECK(!bad.load(catalog,links,1,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());CHECK(!bad.load(catalog,links,999,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());CHECK(!bad.load(catalog,links,2,nullptr,domain.view(),items.view(),error)&&!bad.valid());CHECK(!bad.load(catalog,links,2,root.c_str(),{},items.view(),error)&&!bad.valid());CHECK(!bad.load(catalog,links,2,root.c_str(),domain.view(),{},error)&&!bad.valid());}
 FieldSceneResources independent;
 {ResourceCatalog transient;WorldLinksData transient_links;CHECK(transient.load_file(path("system/native.encresources").c_str(),error));CHECK(transient_links.load_file(path("world/world.enclinks").c_str(),error));CHECK(independent.load(transient,transient_links,2,root.c_str(),domain.view(),items.view(),error));}
 CHECK(independent.bindings().scene.source==source&&independent.room().scene().stable_id==2&&independent.story().valid()&&independent.npc(0).valid());
 std::vector<uint8_t>output{1,2,3};const auto prior=output;CHECK(!catalog.read(ResourceRole::FieldMap,refs.scene.room_role,root.c_str(),output,error)&&output==prior);CHECK(catalog.read(ResourceRole::FieldRoom,refs.scene.room_role,root.c_str(),output,error));CHECK(!output.empty());
 std::vector<uint8_t>bytes;CHECK(encore::read_file(path("system/native.encresources").c_str(),bytes,128*1024,error));size_t at=60;for(unsigned i=0;i<get(bytes,52);++i){if(get(bytes,at)==refs.scene.room_role){put(bytes,at+12,get(bytes,at+12)^1);break;}at+=20+get(bytes,at+16);}sign(bytes);ResourceCatalog corrupt;CHECK(corrupt.load(bytes.data(),bytes.size(),error));FieldSceneResources bad;CHECK(!bad.load(corrupt,links,2,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());CHECK(owner.room().scene().stable_id==scene&&owner.story().valid());
 std::printf("Field owner: %u checks; immutable views, independent catalog lifetime, selected resource CRC and rejection rollback; host only\n",checks);
}
