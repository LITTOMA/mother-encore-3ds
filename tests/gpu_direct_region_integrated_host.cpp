#include <cstdlib>
#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
void require(bool v,const char* text){if(!v){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
int main(){
 gpu_span::Content c;std::string error;require(c.load("romfs",error),error.c_str());
 std::vector<encore::BackgroundKernel::Layer> layers;for(unsigned n=0;n<2;++n)layers.push_back(gpu_span::layer<encore::BackgroundKernel>(c.data.view().background(n),c.source[n],c.palette[n]));
 uint64_t pixels=0;
 for(auto wh:{std::pair<uint32_t,uint32_t>{400,240},{320,180}}){
  encore::RegionBackgroundKernel k;encore::ScalarBackgroundKernel oracle;
  const auto w=wh.first,h=wh.second;require(k.prepare(layers,w,h,error)&&k.prepare_spans(),"optional span preparation");require(oracle.prepare(c.scalar_layers,w,h,error),"oracle prepare");
  std::vector<gpu_span::Span> spans(gpu_span::max_spans);std::vector<uint32_t> expected(size_t(w)*h),actual(expected.size()),baseline(expected.size());
  for(float t:{0.f,1.25f,60.f,1000.f}){size_t count=0;require(k.generate_spans(t,spans.data(),spans.size(),count),"generate");
   for(size_t i=0;i<count;++i){const auto& s=spans[i];require(s.width&&s.x+s.width<=w&&s.y<h,"bounds");std::fill_n(actual.data()+size_t(s.y)*w+s.x,s.width,s.color);}
   require(oracle.compose(t,0,expected.data())&&k.compose(t,0,baseline.data()),"CPU compose");require(actual==expected&&baseline==expected,"direct and unchanged CPU match oracle");pixels+=actual.size();
   require(!k.generate_spans(t,spans.data(),1,count)&&count==0,"cap empties batch");require(k.compose(t,0,baseline.data())&&baseline==expected,"cap retains CPU fallback");
  }
  size_t count=999;require(!k.generate_spans(1000000.f,spans.data(),spans.size(),count)&&count==0,"large TIME fallback");
  require(!k.generate_spans(0,static_cast<gpu_span::Span*>(nullptr),spans.size(),count)&&count==0,"null storage");
  k.clear();require(!k.generate_spans(0,spans.data(),spans.size(),count)&&count==0,"clear invalidates GPU output");
 }
 std::printf("PASS integrated sparse output and CPU fallback pixels=%llu\n",(unsigned long long)pixels);
}
