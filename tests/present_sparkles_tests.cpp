#include "encore/present_sparkles.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace encore::upstream;
namespace {
void check(bool ok,const char*s){if(!ok){std::cerr<<s<<'\n';std::exit(1);}}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void fix(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}put(b,16,~c);}
}
int main(int argc,char**argv){check(argc==2,"Actual Sparkles binary required");std::string e;PresentSparklesData data;check(data.load_file(argv[1],e),e.c_str());std::vector<uint8_t>b;check(encore::read_file(argv[1],b,256*1024,e),e.c_str());const auto pin=data.reviewed_commit();
 for(size_t n=0;n<b.size();++n)check(!data.load(b.data(),n,e),"truncated Sparkles rejected");
 for(const auto row:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,2},{52,1},{64,0},{72,0},{84,0},{92,0},{96,0x7fc00000},{104,0},{108,0x7f800000}}){auto bad=b;put(bad,row.first,row.second);fix(bad);check(!data.load(bad.data(),bad.size(),e),"malformed source parameter/version rejected");check(data.valid()&&data.reviewed_commit()==pin,"failed admission is atomic");}
 SourceRandom actual(77),oracle(77);const auto expected=std::min(uint32_t(oracle.rand_range(data.random_low(),data.random_high())),uint32_t(data.frames().size()-1));PresentSparklesRuntime runtime;check(runtime.ready(data,actual,e),e.c_str());check(runtime.frame_index()==expected&&actual.state()==oracle.state()&&actual.raw_draw_count()==oracle.raw_draw_count(),"source Ready shares identical RNG draws and truncation/clamp");const auto count=actual.raw_draw_count();check(!runtime.ready(data,actual,e)&&actual.raw_draw_count()==count,"duplicate Ready rejects before draw");
 check(runtime.set_opened(true,e),e.c_str());const auto frame=runtime.frame_index();const auto timeout=runtime.timeout();check(!runtime.visible()&&!runtime.playing(),"opened parent stops and hides");check(runtime.idle_frame(double(.1f),true,e)&&runtime.timeout()==timeout&&runtime.frame_index()==frame,"stopped child preserves source frame/timeout");check(runtime.set_opened(false,e)&&runtime.visible()&&runtime.playing(),"unopened parent shows and plays");
 const float duration=float(1.0/double(float(data.speed()*data.speed_scale())));check(runtime.timeout()==duration,"resume resets source timeout");check(runtime.idle_frame(duration,true,e),e.c_str());check(runtime.frame_index()==frame&&runtime.timeout()==0,"exact duration consumes time without premature boundary frame change");check(runtime.idle_frame(0,true,e)&&runtime.frame_index()==frame,"empty idle cannot advance a frame");check(runtime.idle_frame(double(.001f),false,e)&&runtime.frame_index()==frame,"no pending engine update preserves frame");check(runtime.idle_frame(double(.001f),true,e)&&runtime.frame_index()==(frame+1)%data.frames().size(),"next nonempty update performs source boundary advance");
 check(!runtime.idle_frame(-.1,true,e)&&!runtime.idle_frame(NAN,true,e),"invalid source clock rejected");
 bool observed_clamp=false;for(uint64_t seed=0;seed<1000&&!observed_clamp;++seed){SourceRandom x(seed),y(seed);const auto value=uint32_t(y.rand_range(data.random_low(),data.random_high()));if(value<data.frames().size())continue;PresentSparklesRuntime child;check(child.ready(data,x,e),e.c_str());check(child.frame_index()==data.frames().size()-1&&x.state()==y.state(),"out-of-frame source draw clamps to final frame rather than modulo");check(child.set_opened(true,e)&&!child.playing(),"persisted opened parent still follows child Ready RNG first");observed_clamp=true;}check(observed_clamp,"real source range exercises frame clamp");
 std::cout<<"Manual original Sparkles parser, shared RNG, source clamp, stop/play and float idle-boundary cases\n";
}
