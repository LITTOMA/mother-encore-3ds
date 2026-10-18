#include "encore/music_regions.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cmath>
using namespace encore::upstream;
namespace{unsigned checks=0;std::string error;void check(bool v,const char*why){++checks;if(!v){std::fprintf(stderr,"Door music %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}}
#define CHECK(x) check(bool(x),#x)
int main(int argc,char**argv){
 CHECK(argc==3);MusicRegionData data;CHECK(data.load_file(argv[1],error));std::vector<uint32_t>admitted;
 for(uint32_t i=0;i<data.regions().size();++i){const auto&r=data.regions()[i];if(!r.disabled&&(r.appear_flag.empty()||r.appear_flag!=r.disappear_flag))admitted.push_back(i);}
 CHECK(admitted.size()>=6);std::ifstream reference(argv[2]);CHECK(reference.good());
 for(unsigned count=0;count<7;++count)for(unsigned mode=0;mode<3;++mode){
  unsigned n,m,want;CHECK(bool(reference>>n>>m>>want)&&n==count&&m==mode);MusicRegionController controller;CHECK(controller.initialize(data,16,error)&&controller.attach_scene(1,error));
  for(unsigned j=0;j<count;++j){const auto&r=data.regions()[admitted[j]];MusicRegionContext context;context.flag=[&](std::string_view f){return f==r.appear_flag&&f!=r.disappear_flag;};CHECK(controller.enter(1,r.source_path,context,error));CHECK(controller.regions()[admitted[j]].registered);}
  // Modes1/2 are the source same-scene / disabled fade guards: no event.
  if(mode==0)CHECK(controller.stop_on_door(1,.8,error));unsigned actual=0;
  for(unsigned j=0;j<count;++j)if(controller.regions()[admitted[j]].registered)actual|=1u<<j;
  CHECK(actual==want);CHECK(controller.stop_on_door(999,.8,error));unsigned stale=0;for(unsigned j=0;j<count;++j)if(controller.regions()[admitted[j]].registered)stale|=1u<<j;CHECK(stale==actual);
 }
 {std::string extra;CHECK(!(reference>>extra)&&reference.eof());}
 MusicRegionController controller;MusicRegionContext context;const auto&r=data.regions()[admitted[0]];context.flag=[&](std::string_view f){return f==r.appear_flag&&f!=r.disappear_flag;};CHECK(controller.initialize(data,16,error)&&controller.attach_scene(2,error)&&controller.enter(2,r.source_path,context,error));const int voice=controller.regions()[admitted[0]].voice;CHECK(voice>=0);const auto generation=controller.voices()[voice].generation;
 for(double bad:{-1.0,61.0,std::nan("")}){CHECK(!controller.stop_on_door(2,bad,error));CHECK(controller.regions()[admitted[0]].registered&&controller.voices()[voice].generation==generation);}
 CHECK(controller.stop_on_door(2,.8,error));CHECK(!controller.regions()[admitted[0]].registered&&controller.regions()[admitted[0]].inside);CHECK(controller.voices()[voice].duration==.8&&controller.voices()[voice].curve==MusicRegionCurve::QuartIn);CHECK(controller.advance(.2,false,error));CHECK(controller.tree_exit(2,r.source_path,error));CHECK(controller.voices()[voice].duration==.8&&controller.voices()[voice].elapsed==.2);CHECK(controller.advance(.6,false,error));CHECK(!controller.voices()[voice].allocated);
 std::printf("Door music: %u checks; 21 native registration outcomes, source live-array erase order, explicit fade duration and no tree-exit restart; core only\n",checks);
}
