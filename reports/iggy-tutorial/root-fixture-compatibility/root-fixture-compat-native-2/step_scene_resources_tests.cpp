// Metadata-only schema fixture; no original Mayor scene or audible output claim.
#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Step owner %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(n>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
size_t row(const std::vector<uint8_t>&b,uint32_t id){size_t at=60;for(unsigned i=0;i<get(b,52);++i){if(get(b,at)==id)return at;at+=20+get(b,at+16);}return at;}
std::vector<uint8_t>read(const std::string&p){std::vector<uint8_t>b;CHECK(encore::read_file(p.c_str(),b,16*1024*1024,error));return b;}
void write(const std::string&p,const std::vector<uint8_t>&b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1],legacy_root=argv[2];
 CHECK(std::filesystem::path(root).lexically_normal().string().find("build")!=std::string::npos);
 auto path=[&](const char*p){return root+"/"+p;};
 auto cat=read(path("system/native.encresources")),links=read(path("world/world.enclinks"));ResourceCatalog catalog;WorldLinksData world;RoomData domain;ItemData items;
 CHECK(get(cat,20)==10&&get(links,20)==3);CHECK(catalog.load(cat.data(),cat.size(),error));CHECK(catalog.verify_files(root.c_str(),error));CHECK(world.load(links.data(),links.size(),error));CHECK(world.bind_catalog(catalog,error));
 CHECK(domain.load_file(path("world/house/opening.encroom").c_str(),error));CHECK(items.load_file(path("ui/opening.encitems").c_str(),error));
 WorldSceneResources r;CHECK(world.resolve_scene(1,catalog,r,error)&&!r.steps);CHECK(world.resolve_scene(2,catalog,r,error)&&r.steps==1030);CHECK(catalog.path(ResourceRole::FieldSteps).empty());CHECK(catalog.ids(ResourceRole::FieldSteps)==std::vector<uint32_t>{1030});
 FieldSceneResources owner;CHECK(!owner.steps().valid());CHECK(owner.load(catalog,world,2,root.c_str(),domain.view(),items.view(),error));auto steps=owner.steps();CHECK(steps.valid()&&steps.scene_id()==2&&steps.sound_count()==2&&steps.area_count()==2);CHECK(!owner.load(catalog,world,1,root.c_str(),domain.view(),items.view(),error));CHECK(owner.steps().bytes_identity()==steps.bytes_identity());
 const auto cr=row(cat,1030);CHECK(cr+20<cat.size());
 for(auto e:std::vector<std::pair<size_t,uint32_t>>{{20,11},{20,9},{cr,43},{cr,1023},{cr+4,42},{cr+4,44}}){auto bad=cat;put(bad,e.first,e.second);sign(bad);CHECK(!catalog.load(bad.data(),bad.size(),error));CHECK(catalog.path(ResourceRole::FieldSteps,1030)=="world/fixture/source.encsteps");}
 {auto bad=cat;bad[cr+20+get(bad,cr+16)-1]='x';sign(bad);CHECK(!catalog.load(bad.data(),bad.size(),error));}
 const auto assets=get(links,64+3*16+4);CHECK(get(links,assets)==2&&get(links,assets+32)==1030);
 for(auto e:std::vector<std::pair<size_t,uint32_t>>{{20,4},{20,2},{assets+32,0},{52,3}}){auto bad=links;put(bad,e.first,e.second);sign(bad);CHECK(!world.load(bad.data(),bad.size(),error));CHECK(world.capability()==3&&world.bind_catalog(catalog,error));}
 for(uint32_t id:{42u,99999u}){auto bad=links;put(bad,assets+32,id);sign(bad);WorldLinksData candidate;CHECK(candidate.load(bad.data(),bad.size(),error));CHECK(!candidate.bind_catalog(catalog,error));WorldSceneResources unchanged;unchanged.steps=17;CHECK(!candidate.resolve_scene(2,catalog,unchanged,error)&&unchanged.steps==17);}
 {auto bad=links;put(bad,assets+32,0);put(bad,20,2);sign(bad);WorldLinksData old;CHECK(old.load(bad.data(),bad.size(),error));CHECK(!old.bind_catalog(catalog,error));}
 {auto old=read("tests/fixtures/world-links-cap1.enclinks");WorldLinksData prior;CHECK(prior.load(old.data(),old.size(),error));CHECK(!prior.bind_catalog(catalog,error));}
 const auto step_path=path("world/fixture/source.encsteps");const auto packet=read(step_path);
 // File integrity rejection occurs before the steps parser and preserves both
 // the prepared scene owner and the caller's existing output vector.
 {auto bad=packet;bad.back()^=1;write(step_path,bad);std::vector<uint8_t>out{1,2,3};const bool rejected=!catalog.read(ResourceRole::FieldSteps,1030,root.c_str(),out,error);FieldSceneResources candidate;const bool failed=!candidate.load(catalog,world,2,root.c_str(),domain.view(),items.view(),error);write(step_path,packet);CHECK(rejected&&out==std::vector<uint8_t>({1,2,3}));CHECK(failed&&!candidate.valid()&&!candidate.steps().valid());CHECK(owner.steps().bytes_identity()==steps.bytes_identity());}
 // A checksum-correct, valid steps resource for another scene must still fail
 // its consumer join; this is an explicitly corrupted negative fixture only.
 {auto bad=packet;put(bad,24,3);sign(bad);FieldStepsData parsed;CHECK(parsed.load(bad.data(),bad.size(),error));auto changed=cat;put(changed,cr+12,encore::crc32(bad.data(),bad.size()));sign(changed);ResourceCatalog candidate_catalog;CHECK(candidate_catalog.load(changed.data(),changed.size(),error));write(step_path,bad);FieldSceneResources candidate;const bool failed=!candidate.load(candidate_catalog,world,2,root.c_str(),domain.view(),items.view(),error);const auto reason=error;write(step_path,packet);CHECK(failed&&!candidate.valid());CHECK(reason=="Field stepping scene identity differs");CHECK(owner.steps().scene_id()==2);}
 FieldSceneResources independent;{ResourceCatalog transient;WorldLinksData transient_links;CHECK(transient.load(cat.data(),cat.size(),error));CHECK(transient_links.load(links.data(),links.size(),error));CHECK(independent.load(transient,transient_links,2,root.c_str(),domain.view(),items.view(),error));}CHECK(independent.steps().valid()&&independent.steps().scene_id()==2);
 ResourceCatalog legacy;WorldLinksData old;FieldSceneResources previous;CHECK(legacy.load_file((legacy_root+"/system/native.encresources").c_str(),error));CHECK(old.load_file((legacy_root+"/world/world.enclinks").c_str(),error));CHECK(old.bind_catalog(legacy,error));CHECK(previous.load(legacy,old,2,legacy_root.c_str(),domain.view(),items.view(),error));CHECK(previous.steps().valid()==bool(previous.bindings().steps)&&previous.props().valid()&&previous.room().valid());CHECK(catalog.verify_files(root.c_str(),error));
 std::printf("Step scene resources: %u checks; catalog10/links3, legacy ownership, typed joins, integrity and failed-load rollback; metadata fixture only\n",checks);
}
