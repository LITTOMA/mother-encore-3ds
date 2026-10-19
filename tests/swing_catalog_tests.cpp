#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Swing catalog %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(i*8));}
void seal(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>blob,legacy;CHECK(encore::read_file(argv[1],blob,128*1024,error));CHECK(encore::read_file(argv[2],legacy,128*1024,error));
 ResourceCatalog data;CHECK(data.load(blob.data(),blob.size(),error));CHECK(get(blob,20)==12);
 CHECK(uint32_t(ResourceRole::FieldProps)==44&&uint32_t(ResourceRole::FieldSwing)==45&&uint32_t(ResourceRole::FieldSwingEntry)==48);
 CHECK(data.path(ResourceRole::FieldSwing)=="world/fixture/FieldSwing.encswing");CHECK(data.path(ResourceRole::FieldSwingGraphics)=="world/fixture/FieldSwingGraphics.encgraphics");
 CHECK(data.path(ResourceRole::FieldSwingAction)=="world/fixture/FieldSwingAction.encaction");CHECK(data.path(ResourceRole::FieldSwingEntry)=="world/fixture/FieldSwingEntry.encentry");
 auto reject=[&](const std::vector<uint8_t>&b){CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.path(ResourceRole::FieldSwing)=="world/fixture/FieldSwing.encswing");};
 for(size_t n=0;n<blob.size();++n){CHECK(!data.load(blob.data(),n,error));CHECK(data.valid()&&!data.path(ResourceRole::FieldSwingEntry).empty());}
 for(uint32_t cap:{0u,1u,10u,11u,13u}){auto b=blob;put(b,20,cap);reject(b);}
 {auto b=legacy;put(b,20,12);reject(b);}
 for(size_t at=60,i=0;i<get(blob,52);++i){const auto length=get(blob,at+16);if(get(blob,at+4)>=45){
  for(auto offset:std::vector<size_t>{0,4,8,16}){auto b=blob;put(b,at+offset,offset==0?1025u:offset==4?49u:0u);seal(b);reject(b);}
  {auto b=blob;b[at+20+length-1]='x';seal(b);reject(b);}
  {auto b=blob;b.erase(b.begin()+at,b.begin()+at+20+length);put(b,52,get(b,52)-1);seal(b);reject(b);}
 }at+=20+length;}
 CHECK(data.load(legacy.data(),legacy.size(),error));CHECK(data.valid()&&data.path(ResourceRole::FieldSwing).empty()&&data.path(ResourceRole::FieldSwingEntry).empty());
 CHECK(data.load(blob.data(),blob.size(),error));CHECK(!data.path(ResourceRole::FieldMenus).empty());
 std::printf("Swing catalog: %u checks; capability12 singleton group, legacy capability11, all truncations and rejected partial/unknown/identity/type paths; fixture only\n",checks);
}
