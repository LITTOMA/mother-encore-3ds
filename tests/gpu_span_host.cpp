#include <cstdlib>
#include <chrono>
#include "tests/gpu_span_common.hpp"
static void require(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
int main(){
    std::string error;gpu_span::Content content;require(content.load("romfs",error),error.c_str());
    uint64_t total=0;std::vector<gpu_span::Span> spans;spans.reserve(gpu_span::max_spans);
    for(const auto& size:{std::pair<uint32_t,uint32_t>{400,240},{320,180}}){
        const auto w=size.first,h=size.second;std::vector<uint32_t> expected(size_t(w)*h),actual(expected.size()),rebuilt(expected.size());
        encore::FrozenBackgroundKernel candidate;encore::ScalarBackgroundKernel oracle;
        require(candidate.prepare(content.layers,w,h,error),error.c_str());require(oracle.prepare(content.scalar_layers,w,h,error),error.c_str());
        for(float t:{0.f,1.25f,60.f,1000.f}){
            require(oracle.compose(t,gpu_span::clear_color,expected.data()),"oracle compose");
            require(candidate.compose(t,gpu_span::clear_color,actual.data()),"candidate compose");
            require(actual==expected,"CPU oracle parity");
            require(gpu_span::encode(actual.data(),w,h,spans),"span encode");
            gpu_span::reconstruct(spans,w,rebuilt);require(rebuilt==expected,"span reconstruction");total+=expected.size();
            std::printf("PASS w=%u h=%u t=%.2f spans=%zu\n",w,h,double(t),spans.size());
        }
        for(size_t i=0;i<actual.size();++i)actual[i]=uint32_t(i&1);
        require(!gpu_span::encode(actual.data(),w,h,spans),"bounded span capacity must reject adversarial pixels");
        require(spans.size()==gpu_span::max_spans,"capacity boundary");
    }
    std::printf("ALL HOST CHECKS PASS pixels=%llu; GPU untested\n",(unsigned long long)total);
}
