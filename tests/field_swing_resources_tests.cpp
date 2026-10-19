#include "encore/field_swing_resources.hpp"
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
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Swing resources %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b.at(p))|uint32_t(b.at(p+1))<<8|uint32_t(b.at(p+2))<<16|uint32_t(b.at(p+3))<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b.at(p+i)=uint8_t(v>>(i*8));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
std::vector<uint8_t>read(const std::string&p){std::vector<uint8_t>b;CHECK(encore::read_file(p.c_str(),b,16*1024*1024,error));return b;}
void write(const std::string&p,const std::vector<uint8_t>&b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
size_t row(const std::vector<uint8_t>&b,ResourceRole role){size_t at=60;for(uint32_t i=0;i<get(b,52);++i){if(get(b,at)==uint32_t(role))return at;at+=20+get(b,at+16);}return b.size();}
void empty(const FieldSwingResources&r){CHECK(!r.valid()&&!r.swing().valid()&&!r.graphics().valid()&&!r.action().valid()&&!r.entry().valid());}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1];auto cat=read(root+"/system/native.encresources");ResourceCatalog catalog,legacy,unprepared;CHECK(catalog.load(cat.data(),cat.size(),error));CHECK(legacy.load_file(argv[2],error));
 FieldSwingResources owner;empty(owner);CHECK(!owner.load(unprepared,root.c_str(),error));empty(owner);CHECK(!owner.load(catalog,nullptr,error));empty(owner);CHECK(!owner.load(legacy,root.c_str(),error));empty(owner);
 CHECK(owner.load(catalog,root.c_str(),error));CHECK(owner.valid()&&owner.graphics().bind(owner.swing(),error)&&owner.action().bind(owner.swing(),error)&&owner.entry().bind(owner.action(),owner.swing(),error));
 CHECK(owner.swing().clips().size()==8&&owner.graphics().frames().size()==40&&owner.entry().clips().size()==56);
 const auto*identity=&owner.swing();CHECK(!owner.load(catalog,root.c_str(),error)&&owner.valid()&&identity==&owner.swing());
 for(ResourceRole role:{ResourceRole::FieldSwing,ResourceRole::FieldSwingGraphics,ResourceRole::FieldSwingAction,ResourceRole::FieldSwingEntry}){
  const auto path=root+"/"+catalog.path(role);const auto original=read(path);const auto at=row(cat,role);CHECK(at<cat.size());
  CHECK(std::filesystem::remove(path));{FieldSwingResources bad;CHECK(!bad.load(catalog,root.c_str(),error));empty(bad);}write(path,original);
  auto changed=original;changed.back()^=1;write(path,changed);{FieldSwingResources bad;CHECK(!bad.load(catalog,root.c_str(),error));empty(bad);}
  // Reseal both layers: file integrity may pass while its typed schema fails.
  changed=original;put(changed,8,2);seal(changed);write(path,changed);auto updated=cat;put(updated,at+12,encore::crc32(changed.data(),changed.size()));seal(updated);ResourceCatalog typed;CHECK(typed.load(updated.data(),updated.size(),error));{FieldSwingResources bad;CHECK(!bad.load(typed,root.c_str(),error));empty(bad);}write(path,original);
  if(role==ResourceRole::FieldSwingAction||role==ResourceRole::FieldSwingEntry){
   changed=original;
   if(role==ResourceRole::FieldSwingAction){put(changed,get(changed,84),99);seal(changed);FieldSwingActionData valid;CHECK(valid.load(changed.data(),changed.size(),error)&&!valid.bind(owner.swing(),error));}
   else{changed.at(get(changed,132))='i';seal(changed);FieldSwingEntryData valid;CHECK(valid.load(changed.data(),changed.size(),error)&&!valid.bind(owner.action(),owner.swing(),error));}
   write(path,changed);updated=cat;put(updated,at+12,encore::crc32(changed.data(),changed.size()));seal(updated);ResourceCatalog rebound;CHECK(rebound.load(updated.data(),updated.size(),error));FieldSwingResources bad;CHECK(!bad.load(rebound,root.c_str(),error));empty(bad);write(path,original);
  }
  CHECK(owner.valid()&&identity==&owner.swing()&&owner.entry().bind(owner.action(),owner.swing(),error));
 }
 FieldSwingEntry player;CHECK(player.initialize(owner.entry(),owner.action(),owner.swing(),owner.entry().idle(),{0,1},0,"",error));SwingMotionFlags flags;SwingActionContext context;context.direction={0,1};context.skill_available=true;bool accepted=false;CHECK(player.input(owner.action().strings()[2],true,context,flags,accepted,error)&&accepted);
 for(unsigned i=0;i<50&&player.attacking();++i)CHECK(player.advance(1.0/60,error)&&player.flush(flags,error));
 CHECK(!player.attacking()&&player.rebase(owner.entry().motion_states()[1],{1,0},.02f,error)&&player.action().starts()==1);
 std::printf("Swing resource owner: %u checks; four real packets, typed catalog integrity, cross-binding rejection, immutable publication and action execution; metadata only\n",checks);
}
