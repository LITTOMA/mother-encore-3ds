#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>
#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
// Fail only optional arrays: the baseline's vector allocations use ordinary new.
// Successful allocations/deallocations remain the normal C++ array-new pair.
static int fail_array=-1,array_calls=0;
void* operator new[](std::size_t bytes,const std::nothrow_t&)noexcept{
    const int call=array_calls++;
    if(fail_array>=0&&call==fail_array)return nullptr;
    try{return ::operator new[](bytes);}catch(...){return nullptr;}
}
namespace {
size_t checks=0,path_case=0;uint64_t pixels=0,mapped_bytes=0;
void require(bool ok,const char* text){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
using Kernel=encore::RegionBackgroundKernel;
std::vector<Kernel::Layer> config(const gpu_span::Content& c,const std::vector<gpu_span::Image>& images){std::vector<Kernel::Layer> out;for(unsigned n=0;n<2;++n)out.push_back(gpu_span::layer<Kernel>(c.data.view().background(n),images[n],c.palette[n]));return out;}
std::vector<encore::ScalarBackgroundKernel::Layer> scalar_config(const gpu_span::Content& c,const std::vector<gpu_span::Image>& images){std::vector<encore::ScalarBackgroundKernel::Layer> out;for(unsigned n=0;n<2;++n)out.push_back(gpu_span::layer<encore::ScalarBackgroundKernel>(c.data.view().background(n),images[n],c.palette[n]));return out;}
uint32_t gpu(uint32_t c){return ((c&255)<<24)|((c&0xff00)<<8)|((c>>8)&0xff00)|(c>>24);}
void compare(Kernel& candidate,encore::ScalarBackgroundKernel& scalar,uint32_t w,uint32_t h,const std::vector<float>& times,bool mapped_check=true,bool original_dispatch=false){
 const size_t count=size_t(w)*h;const uint32_t tw=w<=512?512:1024,th=h<=256?256:1024;const size_t texture_count=size_t(tw)*th;
 std::vector<uint32_t> a(count),b(count),offsets(count),mapped(texture_count+2),expected(texture_count);std::string error;
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){const uint32_t m=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);offsets[size_t(y)*w+x]=((y/8)*(tw/8)+x/8)*64+m;}
 require(candidate.prepare_mapped_output(offsets.data(),count,texture_count,error),"layout preparation");
 for(float t:times){const bool ao=scalar.compose(t,0x81234567,a.data()),bo=candidate.compose(t,0x81234567,b.data());require(ao==bo,"scalar success parity");if(ao){require(a==b,"scalar pixels");pixels+=count;}
  const auto linear_stats=candidate.region_stats();
  if(original_dispatch&&t==0)require(bo&&linear_stats.samples>0,"original time-zero actually uses guarded region path");
  if(original_dispatch&&std::abs(t)==1000000.f)require(bo&&linear_stats.samples==0,"original large time actually uses scalar fallback");
  if(!mapped_check)continue;
  std::fill(mapped.begin(),mapped.end(),0xa5a5a5a5);std::fill(expected.begin(),expected.end(),0xa5a5a5a5);
  const bool ok=candidate.compose_mapped(t,0x81234567,mapped.data()+1,texture_count);
  if(ok){require(ao,"mapped success implies scalar success");for(size_t i=0;i<count;++i)expected[offsets[i]]=gpu(a[i]);require(std::equal(expected.begin(),expected.end(),mapped.begin()+1),"mapped all words including padding");mapped_bytes+=texture_count*4;}
  else require(std::all_of(mapped.begin(),mapped.end(),[](uint32_t v){return v==0xa5a5a5a5;}),"mapped failure untouched");
  require(mapped.front()==0xa5a5a5a5&&mapped.back()==0xa5a5a5a5,"mapped sentinels");
  const auto mapped_stats=candidate.region_stats();uint32_t time_bits;std::memcpy(&time_bits,&t,sizeof(t));
  std::printf("PATH case=%zu width=%u height=%u time_bits=%08x linear=%u mapped=%u ready=%u linear_samples=%llu linear_skipped=%llu mapped_samples=%llu mapped_skipped=%llu\n",path_case++,unsigned(w),unsigned(h),unsigned(time_bits),unsigned(bo),unsigned(ok),unsigned(candidate.region_fast_path()),(unsigned long long)linear_stats.samples,(unsigned long long)linear_stats.skipped,(unsigned long long)mapped_stats.samples,(unsigned long long)mapped_stats.skipped);
 }
 std::fill(offsets.begin(),offsets.end(),0);require(candidate.prepare_mapped_output(offsets.data(),count,1,error),"duplicate layout preparation");
 require(scalar.compose(1.25f,0,a.data()),"duplicate scalar");std::fill(mapped.begin(),mapped.end(),0xa5a5a5a5);
 const bool duplicate=candidate.compose_mapped(1.25f,0,mapped.data()+1,1);
 if(duplicate)require(mapped[1]==gpu(a.back())&&mapped[0]==0xa5a5a5a5&&mapped[2]==0xa5a5a5a5,"duplicate last-index semantics");
 std::printf("DUPLICATE case=%zu width=%u height=%u mapped=%u\n",path_case++,unsigned(w),unsigned(h),unsigned(duplicate));
 // The borrowed offsets expire at return; invalidate before destroying them.
 require(!candidate.prepare_mapped_output(nullptr,0,0,error),"layout invalidation");
}
}
int main(){
 gpu_span::Content content;std::string error;require(content.load("romfs",error),error.c_str());const auto layers=config(content,content.source);
 for(const auto& size:{std::pair<uint32_t,uint32_t>{400,240},{320,180},{65,33}}){
  Kernel k;encore::ScalarBackgroundKernel oracle;require(k.prepare(layers,size.first,size.second,error),error.c_str());require(k.region_fast_path(),"original source validates diagonal regions");require(k.preparation_status()==Kernel::PreparationStatus::Ready,"ready status");require(oracle.prepare(content.scalar_layers,size.first,size.second,error),error.c_str());
  std::vector<float> times={0,-0.f,1.25f,60.f,1000.f,1000000.f,-1000000.f,std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};uint32_t rng=0x8f2185u;
  // Exercise both sides of the existing guarded/scalar phase boundary. The
  // row-constant hoist must retain every float operation and fallback choice.
  for(const auto& layer:layers)for(float speed:{layer.speed_x,layer.compression_speed_y})if(speed!=0){
   for(float sign:{-1.f,1.f}){const float boundary=sign*4096.f/std::abs(speed);
    times.push_back(std::nextafter(boundary,-std::numeric_limits<float>::infinity()));times.push_back(boundary);times.push_back(std::nextafter(boundary,std::numeric_limits<float>::infinity()));
   }
  }
  for(unsigned i=0;i<96;++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;times.push_back(i<48?float(i)/60:float(int32_t(rng))/123456);}
  compare(k,oracle,size.first,size.second,times,true,true);Kernel moved(std::move(k));require(moved.region_fast_path(),"move retains prepared ownership");compare(moved,oracle,size.first,size.second,{1.25f});
  moved.clear();require(!moved.region_fast_path()&&moved.prepared_bytes()==0,"clear releases optional and baseline storage");
  require(!moved.prepare(layers,0,size.second,error)&&!moved.region_fast_path(),"invalid base preparation fails closed");
 }
 // Every optional nothrow allocation, including last-row failure, must preserve
 // the already-prepared exact baseline and permit a later successful prepare.
 {Kernel k;encore::ScalarBackgroundKernel oracle;require(oracle.prepare(content.scalar_layers,65,33,error),error.c_str());array_calls=0;fail_array=-1;require(k.prepare(layers,65,33,error)&&k.region_fast_path(),"allocation census");const int total=array_calls;require(total==12,"bounded optional allocation count");
  for(int failure=0;failure<total;++failure){array_calls=0;fail_array=failure;require(k.prepare(layers,65,33,error),"optional allocation failure is not baseline failure");fail_array=-1;require(!k.region_fast_path()&&k.preparation_status()==Kernel::PreparationStatus::AllocationFailure,"allocation fallback status");compare(k,oracle,65,33,{0,1.25f,1000.f});require(k.prepare(layers,65,33,error)&&k.region_fast_path(),"recover after optional allocation failure");}
  std::printf("PASS all %d optional allocation failure points\n",total);
 }
 for(unsigned mode=0;mode<14;++mode){auto images=content.source;
  for(auto& im:images){if(mode==1)im.indices[im.indices.size()/2]^=1;if(mode==2)std::fill(im.indices.begin(),im.indices.end(),0);if(mode==3)for(size_t i=0;i<im.indices.size();++i)im.indices[i]=uint8_t((i+i/im.width)&1);}
  auto lc=config(content,images);auto sc=scalar_config(content,images);const auto mutate=[&](auto& c){for(auto& l:c){switch(mode){case 4:l.barrel=false;break;case 5:l.amplitude_x*=-1;l.compression_amplitude_y*=-1;break;case 6:l.frequency_x*=8;l.compression_frequency_y*=8;break;case 7:l.move_x=-17;l.move_y=33;l.ping_pong_speed_y=0;break;case 8:l.compression_amplitude_y*=0.001f;l.amplitude_x*=0.001f;break;case 9:l.repeat=false;break;case 10:l.width=37;l.height=51;break;case 11:l.effect_scale=0.1f;break;case 12:l.amplitude_x*=32;l.compression_amplitude_y*=64;break;case 13:l.amplitude_x*=100000;l.compression_amplitude_y*=100000;break;default:break;}}};mutate(lc);mutate(sc);
  Kernel k;encore::ScalarBackgroundKernel oracle;require(k.prepare(lc,65,33,error),error.c_str());require(oracle.prepare(sc,65,33,error),error.c_str());if(mode==3)require(!k.region_fast_path()&&k.preparation_status()==Kernel::PreparationStatus::ResourceLimit,"component-count cap");compare(k,oracle,65,33,{-80.71f,0.f,1.25123f,1011.97f});
 }
 {auto images=content.source;for(auto& im:images){im.width=1024;im.height=256;im.indices.resize(size_t(im.width)*im.height);for(size_t i=0;i<im.indices.size();++i)im.indices[i]=uint8_t((i+i/im.width)&1);}
  Kernel k;encore::ScalarBackgroundKernel oracle;const auto lc=config(content,images);require(k.prepare(lc,65,33,error),error.c_str());require(!k.region_fast_path()&&k.preparation_status()==Kernel::PreparationStatus::ResourceLimit,"source-pixel cap");require(oracle.prepare(scalar_config(content,images),65,33,error),error.c_str());compare(k,oracle,65,33,{0,1.25f,1000.f});}
 std::printf("ALL REGION TESTS PASS checks=%zu pixels=%llu mapped_bytes=%llu\n",checks,(unsigned long long)pixels,(unsigned long long)mapped_bytes);
}
