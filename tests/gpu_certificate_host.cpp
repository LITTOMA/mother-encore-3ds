#include <chrono>
#include <cstdlib>
#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
using Kernel=encore::RegionBackgroundKernel;
void require(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
double ms(std::chrono::steady_clock::time_point t){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();}
int main(){gpu_span::Content content;std::string error;require(content.load("romfs",error),error.c_str());
 std::vector<Kernel::Layer> layers;for(unsigned n=0;n<2;++n)layers.push_back(gpu_span::layer<Kernel>(content.data.view().background(n),content.source[n],content.palette[n]));
 for(auto wh:{std::pair<uint32_t,uint32_t>{400,240},{320,180}}){
  Kernel base,candidate;encore::ScalarBackgroundKernel oracle;const auto w=wh.first,h=wh.second;
  require(base.prepare(layers,w,h,error)&&base.prepare_spans()&&candidate.prepare(layers,w,h,error)&&candidate.prepare_spans()&&oracle.prepare(content.scalar_layers,w,h,error),"prepare");
  require(!candidate.prepare_certificates(0)&&candidate.certificate_bytes()==0,"preflight memory cap");
  auto tick=std::chrono::steady_clock::now();require(candidate.prepare_certificates(),"certificates ready");double prep=ms(tick);
  require(candidate.certificate_bytes()<=36*1024*1024/10,"3.6MiB cap");
  std::vector<gpu_span::Span> a(8192),b(8192);std::vector<uint32_t> expected(size_t(w)*h),actual(expected.size());
  for(float time:{0.f,1.25f,60.f,1000.f,-80.71f}){
   size_t ac=0,bc=0;tick=std::chrono::steady_clock::now();require(base.generate_spans(time,a.data(),a.size(),ac),"base generate");const double before=ms(tick);tick=std::chrono::steady_clock::now();require(candidate.generate_spans(time,b.data(),b.size(),bc),"cert generate");const double after=ms(tick);
   require(candidate.certificate_frame_used(),"cert actually dispatched");require(oracle.compose(time,0,expected.data()),"oracle");
   for(size_t i=0;i<bc;++i)std::fill_n(actual.data()+size_t(b[i].y)*w+b[i].x,b[i].width,b[i].color);
   require(actual==expected,"cert exact pixels");
   require(ac==bc,"maximal span count");for(size_t i=0;i<ac;++i)require(a[i].x==b[i].x&&a[i].y==b[i].y&&a[i].width==b[i].width&&a[i].color==b[i].color,"identical output spans");
   std::printf("CERT w=%u h=%u t=%.2f bytes=%zu prep_ms=%.3f base_ms=%.3f cert_ms=%.3f base_samples=%llu cert_samples=%llu spans=%zu PASS\n",w,h,double(time),candidate.certificate_bytes(),prep,before,after,(unsigned long long)base.region_stats().samples,(unsigned long long)candidate.region_stats().samples,bc);
  }
  size_t count=12;require(!candidate.generate_spans(0,b.data(),1,count)&&count==0,"cap empty");require(!candidate.generate_spans(1000000.f,b.data(),b.size(),count)&&count==0,"unguarded fallback");
 }
 std::puts("PASS certificate envelope finite matrix, source sampling unchanged");
}
