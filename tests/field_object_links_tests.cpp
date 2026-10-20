#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace encore::upstream;
namespace{unsigned checks=0;std::string error;
void check(bool value,const char*why){++checks;if(!value){std::fprintf(stderr,"Object links %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
void fix(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
void write(const std::string&name,const std::vector<uint8_t>&b){std::ofstream out(name,std::ios::binary);CHECK(bool(out));out.write(reinterpret_cast<const char*>(b.data()),b.size());CHECK(bool(out));}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];auto path=[&](const char*p){return root+"/"+p;};std::vector<uint8_t>cb,lb;CHECK(encore::read_file(path("system/native.encresources").c_str(),cb,128*1024,error)&&encore::read_file(path("world/world.enclinks").c_str(),lb,64*1024,error));
 ResourceCatalog catalog,legacy;WorldLinksData links,old_links;CHECK(catalog.load(cb.data(),cb.size(),error)&&links.load(lb.data(),lb.size(),error));CHECK(word(cb,20)==13&&links.capability()==5&&links.bind_catalog(catalog,error));CHECK(legacy.load_file(path("system/legacy.encresources").c_str(),error)&&old_links.load_file(path("world/legacy.enclinks").c_str(),error)&&old_links.capability()==4&&old_links.bind_catalog(legacy,error));CHECK(!old_links.bind_catalog(catalog,error)&&!links.bind_catalog(legacy,error));
 WorldSceneResources refs;CHECK(links.resolve_scene(2,catalog,refs,error)&&refs.objects==1032&&refs.props==1031);CHECK(catalog.path(ResourceRole::FieldObjects,refs.objects)=="world/podunk/podunk.encobjects");CHECK(links.resolve_scene(3,catalog,refs,error)&&!refs.objects&&refs.steps==1030);
 auto reject_catalog=[&](std::vector<uint8_t>b){fix(b);CHECK(!catalog.load(b.data(),b.size(),error)&&catalog.path(ResourceRole::FieldObjects,1032)=="world/podunk/podunk.encobjects");};
 for(uint32_t cap:{0u,1u,9u,10u,11u,12u,14u}){auto b=cb;put(b,20,cap);reject_catalog(b);}
 size_t object=0;for(size_t at=60,i=0;i<word(cb,52);++i){if(word(cb,at+4)==49)object=at;at+=20+word(cb,at+16);}CHECK(object>0);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{object,49},{object,1023},{object+4,50},{object+8,0},{object+16,0}}){auto b=cb;put(b,edit.first,edit.second);reject_catalog(b);}
 {std::vector<uint8_t>b;CHECK(encore::read_file(path("system/legacy.encresources").c_str(),b,128*1024,error));put(b,20,13);reject_catalog(b);} // canonical13 needs an actual object role
 const size_t row=word(lb,64+6*16+4);auto reject_links=[&](std::vector<uint8_t>b){fix(b);CHECK(!links.load(b.data(),b.size(),error)&&links.capability()==5);};
 for(uint32_t cap:{0u,1u,2u,3u,4u,6u}){auto b=lb;put(b,20,cap);reject_links(b);}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{52,6},{64+6*16+8,0},{64+6*16+12,4},{row,0},{row,1},{row,999},{row+4,0},{row+4,49},{row+4,1023},{row+4,1031}}){auto b=lb;put(b,edit.first,edit.second);reject_links(b);}
 {auto b=lb;b.insert(b.end(),lb.begin()+row,lb.begin()+row+8);put(b,12,uint32_t(b.size()));put(b,64+6*16+8,2);reject_links(b);}
 {auto b=lb;put(b,row+4,1027);fix(b);WorldLinksData wrong;CHECK(wrong.load(b.data(),b.size(),error)&&!wrong.bind_catalog(catalog,error));}
 RoomData domain;ItemData items;CHECK(domain.load_file(path("world/house/opening.encroom").c_str(),error)&&items.load_file(path("ui/opening.encitems").c_str(),error));FieldSceneResources owner;CHECK(owner.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error)&&owner.valid()&&owner.objects().drawing_bound()&&owner.props().valid());CHECK(owner.objects().ready_state()==(uint64_t(owner.mick().actor().rng_lo)|(uint64_t(owner.mick().actor().rng_hi)<<32)));CHECK(!owner.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error));
 {auto b=lb;put(b,row,3);fix(b);WorldLinksData wrong;CHECK(wrong.load(b.data(),b.size(),error)&&wrong.bind_catalog(catalog,error));FieldSceneResources bad;CHECK(!bad.load(catalog,wrong,2,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());}
 const std::string texture=root+"/"+owner.objects().texture();std::vector<uint8_t>raw;CHECK(encore::read_file(texture.c_str(),raw,1024*1024,error));auto corrupt=raw;corrupt.back()^=1;write(texture,corrupt);{FieldSceneResources bad;CHECK(!bad.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());}write(texture,raw);CHECK(owner.valid()&&owner.objects().scene_id()==2);
 CHECK(std::remove(texture.c_str())==0);{FieldSceneResources bad;CHECK(!bad.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error)&&!bad.valid());}write(texture,raw);{FieldSceneResources again;CHECK(again.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error)&&again.objects().drawing_bound());}
 std::printf("Field object links: %u checks; catalog13/role49 and links5 typed owners, old12/4 reading, missing/wrong/version/duplicate/texture rejection before publication; synthetic texture, no GPU claim\n",checks);
}
