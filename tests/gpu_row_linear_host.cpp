#include <chrono>
#include <cstdlib>
#include "tests/gpu_span_common.hpp"
#include "encore/row_linear_background_kernel.hpp"
using Kernel=encore::RowLinearBackgroundKernel;
void require(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
double ms(std::chrono::steady_clock::time_point t){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();}
int main(){
 encore::upstream::BattleData data;std::string error;require(data.load_file("romfs/data/pillow-entry.encbattle",error),error.c_str());const auto v=data.view();
 gpu_span::Image src[2],pal[2];std::vector<encore::BackgroundKernel::Layer> layers;std::vector<encore::ScalarBackgroundKernel::Layer> scalar;
 for(unsigned n=0;n<2;++n){const auto b=v.background(n);const auto r=v.resource(b.resource);require(gpu_span::image("romfs/"+std::string(v.string(r.path)),r,src[n]),"image");layers.push_back(gpu_span::layer<encore::BackgroundKernel>(b,src[n],pal[n]));scalar.push_back(gpu_span::layer<encore::ScalarBackgroundKernel>(b,src[n],pal[n]));}
 uint64_t pixels=0;
 for(auto wh:{std::pair<uint32_t,uint32_t>{400,240},{320,180}}){const auto w=wh.first,h=wh.second;Kernel candidate;encore::ScalarBackgroundKernel oracle;require(candidate.prepare(layers,w,h)&&oracle.prepare(scalar,w,h,error),"prepare");std::vector<gpu_span::Span> spans(32768);std::vector<uint32_t> expected(size_t(w)*h),actual(expected.size());
  for(float t:{0.f,0.5f,1.25f,60.f,1000.f,-80.71f}){size_t count=0;auto tick=std::chrono::steady_clock::now();require(candidate.generate_spans(t,spans.data(),spans.size(),count),"generate");const double cost=ms(tick);require(oracle.compose(t,0x81234567u,expected.data()),"oracle");for(size_t i=0;i<count;++i)std::fill_n(actual.data()+size_t(spans[i].y)*w+spans[i].x,spans[i].width,spans[i].color);if(actual!=expected){for(size_t i=0;i<actual.size();++i)if(actual[i]!=expected[i]){std::fprintf(stderr,"first %zu (%zu,%zu) got %08x expected %08x\n",i,i%w,i/w,actual[i],expected[i]);break;}require(false,"pixels");}pixels+=expected.size();std::printf("ROW_LINEAR w=%u h=%u t=%.3f spans=%zu evaluations=%llu ms=%.3f bytes=%zu PASS\n",w,h,double(t),count,(unsigned long long)candidate.sample_evaluations(),cost,candidate.prepared_bytes());}
  size_t count=1;require(!candidate.generate_spans(0,spans.data(),1,count)&&count==0,"cap resets count");require(!candidate.generate_spans(INFINITY,spans.data(),spans.size(),count)&&count==0,"nonfinite");require(!candidate.generate_spans(100000000.f,spans.data(),spans.size(),count)&&count==0,"coordinate bound");candidate.clear();require(!candidate.ready()&&!candidate.generate_spans(0,spans.data(),spans.size(),count),"clear");
 }
 auto unsupported=layers;unsupported[0].barrel=true;Kernel k;require(!k.prepare(unsupported,400,240),"barrel rejected");unsupported=layers;unsupported[1].amplitude_y=1;unsupported[1].frequency_y=1;require(!k.prepare(unsupported,400,240),"Y dependence rejected");std::printf("PASS pixels=%llu\n",(unsigned long long)pixels);
}
