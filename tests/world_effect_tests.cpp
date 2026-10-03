#include "encore/world_effect.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t o){return uint32_t(b[o])|(uint32_t(b[o+1])<<8)|(uint32_t(b[o+2])<<16)|(uint32_t(b[o+3])<<24);}
static void put(std::vector<uint8_t>&b,size_t o,uint32_t x){for(unsigned i=0;i<4;++i)b[o+i]=uint8_t(x>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
static bool near(float a,float b){return std::abs(a-b)<.000001f;}
int main(int argc,char**argv){
 std::string error;WorldEffectData data;
 if(argc==12&&std::strcmp(argv[1],"--compose")==0){
  CHECK(data.load_file(argv[2],error));WorldEffectKernel kernel;const auto width=uint32_t(std::strtoul(argv[3],nullptr,10)),height=uint32_t(std::strtoul(argv[4],nullptr,10));CHECK(kernel.prepare(data.view(),width,height,error));
  WorldEffectSample sample;sample.active=true;sample.alpha=std::strtof(argv[10],nullptr);sample.modulate={std::strtof(argv[6],nullptr),std::strtof(argv[7],nullptr),std::strtof(argv[8],nullptr),std::strtof(argv[9],nullptr)};
  std::vector<uint32_t>pixels(size_t(width)*height);CHECK(kernel.compose(std::strtof(argv[5],nullptr),sample,pixels.data(),pixels.size()));
  FILE*f=std::fopen(argv[11],"wb");CHECK(f);for(auto c:pixels){uint8_t rgba[]={uint8_t(c),uint8_t(c>>8),uint8_t(c>>16),uint8_t(c>>24)};CHECK(std::fwrite(rgba,1,4,f)==4);}CHECK(std::fclose(f)==0);return 0;
 }
 if(argc==4&&std::strcmp(argv[1],"--timing")==0){
  CHECK(data.load_file(argv[2],error));WorldEffect effect;CHECK(effect.initialize(data.view()));CHECK(effect.appear());
  std::vector<uint8_t>native;CHECK(encore::read_file(argv[3],native,65536,error));CHECK(native.size()==140*36);float max_error=0;
  for(unsigned i=0;i<140;++i){if(i==100)CHECK(effect.disappear());CHECK(effect.advance(1.0/60));const auto sample=effect.sample();const auto*p=native.data()+i*36;
   uint32_t active=0;std::memcpy(&active,p+32,4);CHECK(sample.active==bool(active));float alpha;std::memcpy(&alpha,p+28,4);CHECK(sample.alpha==alpha);
   if(active){float color[4];std::memcpy(color,p+12,16);const float actual[]={sample.modulate.r,sample.modulate.g,sample.modulate.b,sample.modulate.a};
    for(unsigned j=0;j<4;++j){max_error=std::max(max_error,std::abs(color[j]-actual[j]));if(color[j]!=actual[j])std::fprintf(stderr,"timing mismatch frame%u channel%u native %.9g cpp %.9g\n",i,j,color[j],actual[j]);CHECK(color[j]==actual[j]);}
    double time;std::memcpy(&time,p+4,8);CHECK(sample.animation_time==time);
   }
  }
  std::printf("WorldEffect native timing: 140 source AnimationPlayer/SceneTreeTween frames; exact color, alpha, clock and finished state; max error %.9g\n",max_error);return 0;
 }
 if(argc!=2)return 2;
 std::vector<uint8_t>b;CHECK(encore::read_file(argv[1],b,128*1024,error));CHECK(data.load(b.data(),b.size(),error));auto v=data.view();
 CHECK(v.settings().texture_width==16&&v.settings().texture_height==10);CHECK(v.key_count()==6&&v.palette_count()==2);CHECK(v.settings().cycle_duration==1.2f);CHECK(v.settings().pixel_snap_uv_epsilon==.00001f);
 for(size_t n=0;n<b.size();++n){CHECK(!data.load(b.data(),n,error));}
 CHECK(data.view().key_count()==6);
 for(auto mutation:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,3},{24,3},{28,2},{32,0},{52,1},{64,0},{68,0},{72,2},{80+12,0},{96+8,257},{112+8,161}}){auto bad=b;put(bad,mutation.first,mutation.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 const uint32_t settings=get(b,68),keys=get(b,84),pixels=get(b,116);
 for(auto mutation:std::vector<std::pair<size_t,uint32_t>>{{settings,0},{settings+4,257},{settings+8,7},{settings+12,1},{settings+24,0},{settings+32,0x7fc00000},{settings+36,0x40000000},{settings+68,0},{settings+72,0x3f800000},{keys,0x3f800000},{keys+4,0},{keys+8,0x7fc00000},{keys+24,0},{pixels,0xffffffff}}){auto bad=b;put(bad,mutation.first,mutation.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 auto bad=b;bad[16]^=1;CHECK(!data.load(bad.data(),bad.size(),error));bad=b;bad.push_back(0);put(bad,12,uint32_t(bad.size()));fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().key_count()==6);
 auto copy=b;CHECK(data.load(copy.data(),copy.size(),error));std::fill(copy.begin(),copy.end(),0);v=data.view();CHECK(v.settings().texture_width==16);CHECK(!WorldEffectView{}.valid());CHECK(WorldEffectView{}.texels()==nullptr);
 WorldEffect effect;CHECK(!effect.appear());CHECK(!effect.advance(.1));CHECK(effect.initialize(v));CHECK(!effect.active());CHECK(effect.appear());CHECK(effect.sample().alpha==0);CHECK(effect.advance(.25));CHECK(near(effect.sample().alpha,.5));CHECK(effect.advance(.25));CHECK(effect.sample().alpha==1);CHECK(effect.disappear());CHECK(effect.advance(.25));CHECK(near(effect.sample().alpha,.5));CHECK(effect.advance(.25));CHECK(!effect.active()&&effect.sample().alpha==0);
 CHECK(effect.appear());CHECK(effect.advance(.1));auto sample=effect.sample();CHECK(near(sample.modulate.r,(v.key(0).color.r+v.key(1).color.r)/2));CHECK(effect.advance(1.1));CHECK(near(effect.sample().modulate.r,v.key(0).color.r));CHECK(!effect.advance(-1));CHECK(!effect.advance(std::numeric_limits<double>::infinity()));
 CHECK(effect.disappear());CHECK(effect.advance(.125));CHECK(near(effect.sample().alpha,.75));CHECK(effect.appear());CHECK(effect.sample().alpha==0);CHECK(effect.advance(.5));effect.reset();CHECK(!effect.active());
 WorldEffectKernel expanded,source;CHECK(source.prepare(v,320,180,error));CHECK(expanded.prepare(v,400,240,error));WorldEffectSample white{true,1,{1,1,1,1},0};std::vector<uint32_t>a(320*180),c(400*240);
 for(unsigned i=0;i<120;++i){const float time=float(i)*.137f;CHECK(source.compose(time,white,a.data(),a.size()));CHECK(expanded.compose(time,white,c.data(),c.size()));for(unsigned y=0;y<180;++y)for(unsigned x=0;x<320;++x)CHECK(a[y*320+x]==c[(y+30)*400+x+40]);}
 const auto saved=c;CHECK(!expanded.compose(std::numeric_limits<float>::infinity(),white,c.data(),c.size()));CHECK(c==saved);CHECK(!expanded.compose(0,white,c.data(),1));CHECK(c==saved);CHECK(!expanded.prepare(v,0,240,error));CHECK(!expanded.compose(0,white,c.data(),c.size()));CHECK(expanded.prepare(v,400,240,error));
 auto start=std::chrono::steady_clock::now();uint64_t hash=0;const unsigned frames=300;
 for(unsigned i=0;i<frames;++i){CHECK(expanded.compose(float(i)/60,white,c.data(),c.size()));hash+=c[i%c.size()];}
 auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
 std::printf("WorldEffect: %u checks, strict parser rollback, fade/color clocks, repeat lifecycle, 6912000 center-crop pixels; 400x240 %u frames %.3f us/frame, hash %llu (host only)\n",checks,frames,elapsed/frames,(unsigned long long)hash);
}
