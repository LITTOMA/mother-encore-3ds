#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
#include <cassert>
#include <iostream>
int main(){gpu_span::Content content;std::string error;assert(content.load("romfs",error));std::vector<encore::BackgroundKernel::Layer> layers;for(unsigned i=0;i<2;++i)layers.push_back(gpu_span::layer<encore::BackgroundKernel>(content.data.view().background(i),content.source[i],content.palette[i]));
 struct Counter {size_t count=0,limit=0;};
 for(size_t limit:{size_t(0),size_t(50),size_t(500),size_t(3000)}){Counter count{0,limit};encore::PreparationControl stop{[](void* p){auto& c=*static_cast<Counter*>(p);return c.count++>=c.limit;},&count};encore::RegionBackgroundKernel kernel;assert(!kernel.prepare(layers,400,240,error,&stop));assert(!kernel.region_fast_path());}
 encore::RegionBackgroundKernel kernel;assert(kernel.prepare(layers,400,240,error));assert(kernel.prepare_spans());Counter count{0,20};encore::PreparationControl stop{[](void* p){auto& c=*static_cast<Counter*>(p);return c.count++>=c.limit;},&count};assert(!kernel.prepare_certificates(SIZE_MAX,&stop));assert(kernel.certificate_bytes()==0&&kernel.region_fast_path());
 std::vector<uint32_t> before(400*240),after(before.size());assert(kernel.compose(2.375f,0,before.data()));assert(kernel.prepare_certificates());assert(kernel.compose(2.375f,0,after.data()));assert(before==after);std::cout<<"PASS cancellation in coordinates, blocks, regions and certificates; retained dynamic proof pixels match completed certificate pixels\n";
}
