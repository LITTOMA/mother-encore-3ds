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
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Prop owner %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
std::vector<uint8_t>read(const std::string&p){std::vector<uint8_t>b;CHECK(encore::read_file(p.c_str(),b,16*1024*1024,error));return b;}
size_t catrow(const std::vector<uint8_t>&b,uint32_t id){size_t at=60;for(uint32_t i=0;i<get(b,52);++i){if(get(b,at)==id)return at;at+=20+get(b,at+16);}return b.size();}
void write(const std::filesystem::path&p,const std::vector<uint8_t>&b){std::filesystem::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string source=argv[1],out=argv[2];auto file=[&](const char*p){return source+"/"+p;};auto cat=read(file("system/native.encresources")),links=read(file("world/world.enclinks"));ResourceCatalog catalog;WorldLinksData world;RoomData domain;ItemData items;
 CHECK(get(cat,20)==13&&get(links,20)==5);CHECK(catalog.load(cat.data(),cat.size(),error)&&world.load(links.data(),links.size(),error)&&world.bind_catalog(catalog,error));CHECK(domain.load_file(file("world/house/opening.encroom").c_str(),error)&&items.load_file(file("ui/opening.encitems").c_str(),error));WorldSceneResources r;
 CHECK(world.resolve_scene(2,catalog,r,error)&&r.props==1031);CHECK(catalog.ids(ResourceRole::FieldProps)==std::vector<uint32_t>{1031});WorldSceneResources other;CHECK(world.resolve_scene(3,catalog,other,error)&&!other.props);other.props=17;CHECK(!world.resolve_scene(99,catalog,other,error)&&other.props==17);
 FieldSceneResources owner;CHECK(owner.load(catalog,world,2,source.c_str(),domain.view(),items.view(),error));CHECK(owner.props().rendering_bound()&&owner.props().props().size()==13&&owner.props().policy().clone_after==1068);const auto*identity=owner.props().props().data();CHECK(!owner.load(catalog,world,3,source.c_str(),domain.view(),items.view(),error)&&owner.props().props().data()==identity);
 WorldLinksData old;auto prior_links=read("tests/fixtures/world-links-cap3.enclinks");CHECK(old.load(prior_links.data(),prior_links.size(),error));CHECK(!old.bind_catalog(catalog,error));FieldSceneResources unowned;CHECK(!unowned.load(catalog,old,2,source.c_str(),domain.view(),items.view(),error)&&!unowned.valid());ResourceCatalog prior;auto prior_cat=read("tests/fixtures/catalog-cap10.encresources");CHECK(prior.load(prior_cat.data(),prior_cat.size(),error));CHECK(!world.bind_catalog(prior,error));
 const auto cr=catrow(cat,1031);const auto pr=get(links,64+5*16+4),assets=get(links,64+3*16+4);CHECK(cr<cat.size()&&get(links,pr)==2&&get(links,pr+4)==1031);
 for(auto m:std::vector<std::pair<size_t,uint32_t>>{{20,10},{20,11},{20,12},{20,14},{cr,44},{cr,1023},{cr+4,43}}){auto b=cat;put(b,m.first,m.second);sign(b);CHECK(!catalog.load(b.data(),b.size(),error));CHECK(catalog.path(ResourceRole::FieldProps,1031)=="world/podunk/podunk.encprops");}
 for(auto m:std::vector<std::pair<size_t,uint32_t>>{{20,3},{20,4},{20,6},{52,5},{64+5*16+8,0},{64+5*16+12,4},{pr,1},{pr,99},{pr+4,0},{pr+4,44},{assets+36+32,0}}){auto b=links;put(b,m.first,m.second);sign(b);CHECK(!world.load(b.data(),b.size(),error));CHECK(world.capability()==5&&world.bind_catalog(catalog,error));}
 for(uint32_t id:{1030u,99999u}){auto b=links;put(b,pr+4,id);sign(b);WorldLinksData bad;CHECK(bad.load(b.data(),b.size(),error)&&!bad.bind_catalog(catalog,error));WorldSceneResources unchanged;unchanged.props=17;CHECK(!bad.resolve_scene(2,catalog,unchanged,error)&&unchanged.props==17);}
 for(size_t n=0;n<links.size();++n)CHECK(!world.load(links.data(),n,error)&&world.capability()==5);for(size_t n=0;n<cat.size();++n)CHECK(!catalog.load(cat.data(),n,error)&&catalog.valid());
 const auto root=std::filesystem::weakly_canonical(out),build=std::filesystem::weakly_canonical(std::filesystem::path(source).parent_path()/"build");const auto relative=std::filesystem::relative(root,build);CHECK(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..");std::vector<std::filesystem::path>created;
 auto copy=[&](ResourceRole role,uint32_t id){if(!id)return;std::vector<uint8_t>b;CHECK(catalog.read(role,id,source.c_str(),b,error));auto p=root/catalog.path(role,id);write(p,b);created.push_back(p);};
 copy(ResourceRole::FieldRoom,r.scene.room_role);copy(ResourceRole::FieldMap,r.scene.map_role);copy(ResourceRole::FieldStory,r.story);copy(ResourceRole::FieldMusic,r.music);copy(ResourceRole::FieldMusicZones,r.zones);copy(ResourceRole::FieldAudio,r.audio);copy(ResourceRole::MickTreats,r.mick);for(auto id:r.npcs)copy(ResourceRole::FieldNpc,id);copy(ResourceRole::FieldProps,r.props);copy(ResourceRole::FieldSteps,r.steps);copy(ResourceRole::FieldObjects,r.objects);
 const auto object_texture=owner.objects().texture();write(root/object_texture,read(source+"/"+object_texture));created.push_back(root/object_texture);
 const auto pp=root/catalog.path(ResourceRole::FieldProps,1031);const auto packet=read(pp.string());FieldSceneResources candidate;CHECK(candidate.load(catalog,world,2,out.c_str(),domain.view(),items.view(),error));
 {auto b=packet;b.back()^=1;write(pp,b);FieldSceneResources bad;CHECK(!bad.load(catalog,world,2,out.c_str(),domain.view(),items.view(),error)&&!bad.valid());write(pp,packet);CHECK(owner.props().props().data()==identity);}
 for(unsigned mode=0;mode<7;++mode){auto b=packet;const auto callback_row=get(b,116)+11*get(b,124);
  if(mode==0)b=read("tests/fixtures/source-field-props.encprops");
  else if(mode==1){const auto policy=get(b,84);put(b,policy,3);sign(b);}
  else if(mode==2){const auto policy=get(b,84);put(b,policy+92,8);sign(b);}
  else if(mode==3){put(b,callback_row+24,42);sign(b);} // Correct Area kind, wrong source path.
  else if(mode==4){put(b,callback_row+28,UINT32_MAX);sign(b);}
  else if(mode==5){put(b,callback_row+28,0);sign(b);}
  else{auto at=get(b,68)+get(b,callback_row+28)-1;while(b.at(at))++at;b.at(at)='x';sign(b);}
  auto changed=cat;put(changed,cr+8,uint32_t(b.size()));put(changed,cr+12,encore::crc32(b.data(),b.size()));sign(changed);ResourceCatalog checked;CHECK(checked.load(changed.data(),changed.size(),error));write(pp,b);FieldSceneResources bad;CHECK(!bad.load(checked,world,2,out.c_str(),domain.view(),items.view(),error)&&!bad.valid());write(pp,packet);}
 for(const auto&p:created)CHECK(std::filesystem::remove(p));
 std::printf("Prop scene ownership: %u checks; catalog13/links5/props3 with checked object companion, legacy readers, required joins, wrong identities, truncation and failed candidate rollback; metadata only, no quest/audio/hardware claim\n",checks);
}
