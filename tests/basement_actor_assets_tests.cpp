#include "encore/basement_actor_assets.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
#define CHECK(x)do{if(!(x)){std::fprintf(stderr,"Manual basement actor check failed %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static uint32_t word(const std::vector<uint8_t>&b,size_t i){return b[i]|uint32_t(b[i+1])<<8|uint32_t(b[i+2])<<16|uint32_t(b[i+3])<<24;}
static void put(std::vector<uint8_t>&b,size_t i,uint32_t v){for(unsigned k=0;k<4;++k)b[i+k]=uint8_t(v>>(k*8));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
int main(int argc,char**argv){
 const std::string path=std::string(argc>1?argv[1]:"romfs/data")+"/house.encbasmanim";std::string error;BasementActorData data;CHECK(data.load_file(path.c_str(),error));CHECK(data.resources().size()==2&&data.animations().size()==3);CHECK(data.resources()[0].width==512&&data.resources()[0].height==448);CHECK(data.resources()[0].position.y+data.resources()[0].offset.y==-4);CHECK(!data.present_sound().empty()&&std::abs(data.present_sound_stop()-.333333f)<1e-6);
 for(const auto&a:data.animations()){CHECK(data.frame(a.id,0)==a.keys.front().frame);CHECK(data.frame(a.id,a.length)==a.keys.back().frame);BasementActorPlayback play;CHECK(begin_basement_actor(data,a.id,play,error));for(unsigned i=0;i<10000;++i)CHECK(advance_basement_actor(data,.1f,play,error));CHECK(play.visible&&play.elapsed==a.length&&data.frame(play.animation_id,play.elapsed)==a.keys.back().frame);const auto old=play;CHECK(!advance_basement_actor(data,-1,play,error)&&play.elapsed==old.elapsed);CHECK(!begin_basement_actor(data,9999,play,error)&&play.animation_id==old.animation_id);}
 std::vector<uint8_t>raw;CHECK(encore::read_file(path.c_str(),raw,1024*1024,error));const auto pin=data.reviewed_commit();for(size_t n=0;n<raw.size();++n)CHECK(!data.load(raw.data(),n,error));for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,2},{28,5},{52,1},{64,99},{68,0},{76,3}}){auto b=raw;put(b,entry.first,entry.second);fix(b);CHECK(!data.load(b.data(),b.size(),error));}auto b=raw;put(b,word(b,64+2*16+4)+8,999);fix(b);CHECK(!data.load(b.data(),b.size(),error));b=raw;put(b,word(b,64+3*16+4)+4,99999);fix(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.reviewed_commit()==pin);std::puts("Manual basement special frame/hold/negative binary checks passed");
}
