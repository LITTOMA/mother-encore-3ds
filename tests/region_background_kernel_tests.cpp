#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>
#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
#include "encore/certified_texture_background_kernel.hpp"
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
uint32_t indexed_color(const Kernel::Layer& l,uint8_t index){
 require(index<l.source.palette_size,"source-index span palette bound");
 uint32_t c=l.source.palette[index];
 if(l.palette_shifting){const auto& p=l.palette_source;
  require(!l.palette_frames&&l.palette_fixed_row<p.height,"source-index fixed palette row");
  const uint32_t x=std::min(uint32_t(float(c&255u)/255.0f*p.width),p.width-1);
  c=(p.palette[p.pixels[size_t(l.palette_fixed_row)*p.width+x]]&0xffffffu)|(c&0xff000000u);
 }
 return c;
}
uint32_t indexed_blend(uint32_t b,uint32_t a,float opacity){
 const float alpha=float(b>>24)/255*opacity;uint32_t out=0;
 for(unsigned shift=0;shift<24;shift+=8){const float v=float((b>>shift)&255)*alpha+float((a>>shift)&255)*(1-alpha);out|=uint32_t(v+0.5f)<<shift;}
 return out|(uint32_t(255*alpha+float(a>>24)*(1-alpha)+0.5f)<<24);
}
bool equal_index_span(const Kernel::IndexSpan& a,const Kernel::IndexSpan& b){
 return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.index[0]==b.index[0]&&a.index[1]==b.index[1];
}
void empty_index_state(const Kernel& k){
 const auto s=k.region_stats();require(!s.samples&&!s.skipped&&!s.trig_calls&&!k.certificate_frame_used(),"failed source-index generation clears diagnostics");
}
// Manual sparse-source regression uses the checked original Doll images and
// an independent scalar oracle; these spans never replace upstream gameplay.
void compare_index_runs(Kernel& k,encore::ScalarBackgroundKernel& oracle,const std::vector<Kernel::Layer>& layers,uint32_t w,uint32_t h){
 const size_t area=size_t(w)*h;std::vector<Kernel::IndexSpan> spans(area+2),callback;
 callback.reserve(area);std::vector<uint32_t> expected(area),actual(area);
 size_t count=99;
 require(!k.generate_index_spans(0,spans.data()+1,area,count)&&count==0,"source-index output requires span preparation");empty_index_state(k);
 require(k.prepare_spans(),"source-index span preparation");
 for(bool certificates:{false,true}){
  if(certificates)require(k.prepare_certificates(),"source-index optional certificates");
  for(float t:{0.f,0.001f,1.25f,60.f,1000.f,-80.71f}){
   const Kernel::IndexSpan sentinel{65535,65535,65535,{255,255}};
   spans.front()=sentinel;spans.back()=sentinel;count=99;
   require(k.generate_index_spans(t,spans.data()+1,area,count)&&count&&count<=area,"source-index bounded generation");
   require(equal_index_span(spans.front(),sentinel)&&equal_index_span(spans.back(),sentinel),"source-index output sentinels");
   const auto statistics=k.region_stats();
   require(statistics.samples+statistics.skipped==2*area&&statistics.trig_calls<=2*statistics.samples,"source-index proof and trig statistics");
   require(!certificates||k.certificate_frame_used(),"source-index certificate dispatch");
   size_t cursor=0;
   for(size_t i=0;i<count;++i){const auto& s=spans[i+1];
    require(s.width&&s.x+s.width<=w&&s.y<h&&size_t(s.y)*w+s.x==cursor,"source-index ordered exact coverage");
    if(i){const auto& previous=spans[i];require(previous.y!=s.y||previous.index[0]!=s.index[0]||previous.index[1]!=s.index[1],"adjacent equal source-index pairs coalesced");}
    const auto color=indexed_blend(indexed_color(layers[1],s.index[1]),indexed_color(layers[0],s.index[0]),layers[1].opacity);
    std::fill_n(actual.data()+cursor,s.width,color);cursor+=s.width;
   }
   require(cursor==area&&oracle.compose(t,0x81234567u,expected.data())&&actual==expected,"source-index spans equal original scalar pixels");pixels+=area;
   callback.clear();
   require(k.generate_index_runs(t,[&](uint32_t x,uint32_t y,uint32_t width,uint8_t a,uint8_t b){
    require(callback.size()<area,"source-index callback bound");callback.push_back({uint16_t(x),uint16_t(y),uint16_t(width),{a,b}});return true;
   }),"zero-copy source-index runs");
   require(callback.size()==count,"callback and bounded span count agree");
   for(size_t i=0;i<count;++i)require(equal_index_span(callback[i],spans[i+1]),"callback and bounded span identities agree");
   size_t calls=0;
   require(!k.generate_index_runs(t,[&](uint32_t,uint32_t,uint32_t,uint8_t,uint8_t){++calls;return false;})&&calls==1,"callback refusal stops immediately");empty_index_state(k);
   count=99;require(!k.generate_index_spans(t,spans.data()+1,1,count)&&count==0,"source-index capacity discards partial batch");empty_index_state(k);
  }
 }
 for(float t:{1000000.f,-1000000.f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  count=99;require(!k.generate_index_spans(t,spans.data()+1,area,count)&&count==0,"source-index invalid or unguarded time fails closed");empty_index_state(k);
  size_t calls=0;require(!k.generate_index_runs(t,[&](uint32_t,uint32_t,uint32_t,uint8_t,uint8_t){++calls;return true;})&&calls==0,"invalid source-index frame emits nothing");empty_index_state(k);
 }
 count=99;require(!k.generate_index_spans(0,nullptr,area,count)&&count==0,"null source-index storage");empty_index_state(k);
 count=99;require(!k.generate_index_spans(0,spans.data()+1,0,count)&&count==0,"zero source-index capacity");empty_index_state(k);
}
void compare_native_bridge(const gpu_span::Content& content,const std::vector<Kernel::Layer>& layers){
 using Bridge=encore::CertifiedTextureBackgroundKernel;
 constexpr uint32_t w=400,h=240;constexpr size_t area=size_t(w)*h;
 Kernel kernel;Bridge bridge;encore::ScalarBackgroundKernel oracle;std::string error;
 require(kernel.prepare(layers,w,h,error)&&kernel.prepare_spans(),"native bridge original region preparation");
 require(oracle.prepare(content.scalar_layers,w,h,error)&&bridge.prepare(kernel,w,h),"native bridge preparation");
 const size_t representatives=(layers[0].source.palette_size+layers[1].source.palette_size)*(sizeof(uint32_t)+2*sizeof(float));
 require(bridge.ready()&&bridge.prepared_bytes()==representatives&&
  bridge.prepared_bytes()<=Bridge::preparation_upper_bound(layers,w,h),"native bridge retains only representatives, no 576k secant table");
 std::vector<Bridge::Strip> strips(area);std::vector<Kernel::IndexSpan> indices(area);
 std::vector<uint32_t> actual(area),expected(area);
 const auto empty_bridge=[](const Bridge::Stats& s){return !s.linear_pixels&&!s.scalar_pixels&&!s.exact_pixels&&
  !s.scalar_trig_calls&&!s.constant_pixels&&!s.merged_strips&&!s.region_pixels;};
 for(float t:{0.f,3.f}){
  size_t count=0,index_count=0;Bridge::Stats stats;
  require(kernel.generate_index_spans(t,indices.data(),indices.size(),index_count),"native bridge source-index reference");
  require(bridge.generate(kernel,t,strips.data(),strips.size(),count,stats)&&count==index_count,"native bridge run count");
  require(stats.linear_pixels==area&&stats.constant_pixels==area&&stats.region_pixels==area&&
   stats.scalar_trig_calls==kernel.region_stats().trig_calls,"native bridge truthful region diagnostics");
  size_t cursor=0;
  for(size_t i=0;i<count;++i){const auto& s=strips[i];const auto& reference=indices[i];
   require(s.height==1&&s.delta[0]==0&&s.delta[1]==0&&s.delta[2]==0&&s.delta[3]==0,"native bridge fixed texture coordinates");
   require(s.x==reference.x&&s.y==reference.y&&s.width==reference.width&&s.width&&
    s.x+s.width<=w&&s.y<h&&size_t(s.y)*w+s.x==cursor,"native bridge complete run coverage");
   uint8_t sampled[2];
   for(unsigned n=0;n<2;++n){const auto& source=layers[n].source;
    const float x=s.uv[n*2]*256,y=(1-s.uv[n*2+1])*256;
    require(std::isfinite(x)&&std::isfinite(y)&&x>=0&&y>=0&&x<source.width&&y<source.height,"native bridge bounded representative texel");
    sampled[n]=source.pixels[size_t(uint32_t(y))*source.width+uint32_t(x)];
    require(sampled[n]==reference.index[n],"native bridge samples original source-index pair");
   }
   const auto color=indexed_blend(indexed_color(layers[1],sampled[1]),indexed_color(layers[0],sampled[0]),layers[1].opacity);
   std::fill_n(actual.data()+cursor,s.width,color);cursor+=s.width;
  }
  require(cursor==area&&oracle.compose(t,0x81234567u,expected.data())&&actual==expected,"native bridge representative pixels equal scalar original");pixels+=area;
  count=99;stats={99,99,99,99,99,99,99};
  require(!bridge.generate(kernel,t,strips.data(),1,count,stats)&&count==0&&empty_bridge(stats),"native bridge capacity clears partial batch");
 }
 kernel.clear();size_t count=99;Bridge::Stats stats{99,99,99,99,99,99,99};
 require(!bridge.generate(kernel,0,strips.data(),strips.size(),count,stats)&&count==0&&empty_bridge(stats),"native bridge invalidated source owner fails closed");
 bridge.clear();require(!bridge.ready()&&!bridge.prepared_bytes(),"native bridge clear releases representatives");
 count=99;stats={99,99,99,99,99,99,99};
 require(!bridge.generate(kernel,0,strips.data(),strips.size(),count,stats)&&count==0&&empty_bridge(stats),"cleared native bridge resets output diagnostics");
}
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
 compare_native_bridge(content,layers);
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
  compare(k,oracle,size.first,size.second,times,true,true);compare_index_runs(k,oracle,layers,size.first,size.second);
  Kernel moved(std::move(k));require(moved.region_fast_path(),"move retains prepared ownership");compare(moved,oracle,size.first,size.second,{1.25f});
  moved.clear();require(!moved.region_fast_path()&&moved.prepared_bytes()==0,"clear releases optional and baseline storage");
  std::vector<Kernel::IndexSpan> cleared(size_t(size.first)*size.second);size_t cleared_count=99;
  require(!moved.generate_index_spans(0,cleared.data(),cleared.size(),cleared_count)&&cleared_count==0,"clear invalidates source-index spans");empty_index_state(moved);
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
