#include "tests/transition_mask_test_common.hpp"
#include <chrono>
#include <cstdlib>
using namespace transition_mask_test;
static size_t checks=0;static uint64_t compared=0;
static void require(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
static void compare(const Image& image,Plan::Canvas canvas,uint32_t old_color,uint32_t new_color,unsigned pattern){
    Plan plan;std::string error;const auto begin=std::chrono::steady_clock::now();require(plan.prepare(image.source(),canvas,2*1024*1024,error),error.c_str());
    const double prepare_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    std::vector<uint32_t> background(size_t(canvas.width)*canvas.height),actual(background.size());
    for(uint32_t y=0;y<canvas.height;++y)for(uint32_t x=0;x<canvas.width;++x){const uint32_t n=(x/(pattern+1))+y*71;background[size_t(y)*canvas.width+x]=(n*0x75431u)^0xb39176ab;}
    auto source=encode(background,canvas.width,canvas.height);const auto resources=plan.resources(source.size());require(resources.maximum_output_spans!=0,"capacity bound");
    std::vector<Span> output(resources.maximum_output_spans);size_t high=0;double cost=0;
    for(uint32_t frame=0;frame<image.frames;++frame){size_t count=999;
        const auto tick=std::chrono::steady_clock::now();require(plan.compose(frame,old_color,new_color,source.data(),source.size(),output.data(),output.size(),count),"compose");cost+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-tick).count();
        require(plan.validate_background(output.data(),count),"complete output partition");high=std::max(high,count);auto expected=background;oracle(image,canvas,frame,old_color,new_color,expected);reconstruct(output.data(),count,canvas.width,actual);require(actual==expected,"exact full RGBA surface");compared+=actual.size();
        if(count>1){size_t failed=999;require(!plan.compose(frame,old_color,new_color,source.data(),source.size(),output.data(),count-1,failed)&&failed==0,"capacity fail closed");}
    }
    std::printf("MASK %ux%u frames=%u offset=%d,%d old=%08x new=%08x runs=%zu max_mask=%zu background=%zu bound=%zu observed=%zu cpu_bytes=%zu prepare_ms=%.3f compose_mean_ms=%.3f PASS\n",canvas.width,canvas.height,image.frames,canvas.offset_x,canvas.offset_y,old_color,new_color,resources.total_runs,resources.maximum_frame_runs,source.size(),resources.maximum_output_spans,high,resources.cpu_bytes,prepare_ms,cost/image.frames);
}
static void negatives(){
    Image image{2,2,1,{0xff000001u,0xff000002u},{0,1,1,0}};const Plan::Canvas canvas{2,2,0,0};Plan plan;std::string error;
    require(plan.prepare(image.source(),canvas,1024,error),"negative setup");const size_t bytes=plan.cpu_bytes();require(!plan.prepare(image.source(),canvas,bytes-1,error)&&!plan.ready(),"exact budget rejection");require(plan.prepare(image.source(),canvas,bytes,error),"exact budget admits");
    auto bad=image.source();bad.index_count--;require(!plan.prepare(bad,canvas,1024,error)&&!plan.ready(),"truncated input");bad=image.source();bad.palette_count=1;require(!plan.prepare(bad,canvas,1024,error),"bad palette index");
    bad=image.source();bad.palette=nullptr;require(!plan.prepare(bad,canvas,1024,error),"null palette");bad=image.source();bad.indices=nullptr;require(!plan.prepare(bad,canvas,1024,error),"null source");bad=image.source();bad.frames=257;require(!plan.prepare(bad,canvas,1024,error),"frame bound");
    require(!plan.prepare(image.source(),{1,2,0,0},1024,error),"canvas too small");require(!plan.prepare(image.source(),{1025,2,0,0},1024,error),"canvas bound");require(plan.prepare(image.source(),canvas,1024,error),"reset setup");
    std::vector<Span> source{{0,0,2,0,0xffff0101},{0,1,2,0,0xffff0202}},output(4);size_t count=99;
    auto rejected=[&](const Span* ptr,size_t n,const char* why){count=99;require(!plan.compose(0,0,0,ptr,n,output.data(),output.size(),count)&&count==0,why);};
    rejected(nullptr,source.size(),"null bg");rejected(source.data(),1,"incomplete bg");source[0].x=1;rejected(source.data(),2,"gap");source[0].x=0;source[0].width=3;rejected(source.data(),2,"overrun");source[0].width=0;rejected(source.data(),2,"zero width");source[0].width=2;source[1].y=0;rejected(source.data(),2,"overlap");source[1].y=1;
    require(!plan.compose(1,0,0,source.data(),2,output.data(),4,count)&&count==0,"invalid frame");require(!plan.compose(0,0,0,source.data(),2,source.data(),2,count)&&count==0,"alias");require(!plan.compose(0,0,0,source.data(),2,output.data(),0,count)&&count==0,"zero capacity");
    require(!plan.resources(1).maximum_output_spans,"invalid background capacity");plan.clear();require(!plan.ready()&&!plan.cpu_bytes()&&!plan.compose(0,0,0,source.data(),2,output.data(),4,count)&&count==0,"clear");
    image.palette={0,0x80127845};require(plan.prepare(image.source(),canvas,1024,error),"flat setup");require(!plan.requires_background(0,0xffffffff),"no opaque copy");require(plan.compose<Span,Span>(0,0xffffffff,0,nullptr,0,output.data(),4,count),"flat no bg needed");
}
static void cancellation(){
    Image image{2,3,2,{0xff000001u,0x80785634u},{0,1,1,0,1,0,1,0,0,1,0,1}};
    const Plan::Canvas canvas{2,3,0,0};Plan plain;std::string error;
    require(plain.prepare(image.source(),canvas,1024,error),"cancel baseline");
    const size_t row_checks=size_t(image.frames)*canvas.height;
    const size_t all_checks=1+(image.indices.size()+8191)/8192+2*row_checks+1;
    struct Counter {size_t calls=0,limit=0;};
    for(size_t limit=0;limit<all_checks;++limit){
        Plan plan;require(plan.prepare(image.source(),canvas,1024,error),"prepopulate cancelled plan");
        Counter counter{0,limit};encore::PreparationControl control{[](void* p){auto& c=*static_cast<Counter*>(p);return c.calls++>=c.limit;},&counter};
        require(!plan.prepare(image.source(),canvas,1024,error,&control),"cancel entry/index/count/fill/final checkpoints");
        require(error=="Transition mask preparation cancelled"&&!plan.ready()&&!plan.cpu_bytes()&&!plan.frames(),"cancel clears all ownership");
        require(!plan.resources(2).total_runs&&!plan.resources(2).maximum_frame_runs&&counter.calls==limit+1,"cancel clears reservations promptly");
        require(plan.prepare(image.source(),canvas,1024,error)&&error.empty(),"recover default call after cancellation");
    }
    Counter counter{0,all_checks};encore::PreparationControl control{[](void* p){auto& c=*static_cast<Counter*>(p);return c.calls++>=c.limit;},&counter};Plan controlled;
    require(controlled.prepare(image.source(),canvas,1024,error,&control)&&counter.calls==all_checks,"all exact row checkpoints without cancellation");
    require(controlled.cpu_bytes()==plain.cpu_bytes(),"control preserves reservation");
    std::vector<Span> background{{0,0,2,0,0xff000099},{0,1,2,0,0xff0000aa},{0,2,2,0,0xff0000bb}},a(6),b(6);
    for(uint32_t frame=0;frame<image.frames;++frame){size_t ac=0,bc=0;
        require(plain.compose(frame,0xff000001,0x81725394,background.data(),background.size(),a.data(),a.size(),ac)&&
                controlled.compose(frame,0xff000001,0x81725394,background.data(),background.size(),b.data(),b.size(),bc),"controlled compose");
        require(ac==bc&&!std::memcmp(a.data(),b.data(),ac*sizeof(Span)),"control preserves exact output spans");
    }
    Image large{257,33,1,{0},std::vector<uint8_t>(257*33)};Counter midway{0,2};
    encore::PreparationControl index_stop{[](void* p){auto& c=*static_cast<Counter*>(p);return c.calls++>=c.limit;},&midway};
    require(!controlled.prepare(large.source(),{257,33,0,0},1024*1024,error,&index_stop)&&midway.calls==3&&!controlled.ready()&&!controlled.cpu_bytes(),"mid-index chunk cancellation");
    std::printf("CANCELLATION PASS checkpoints=%zu index_chunk=8192 rows_checked_in_both_passes=1 final_commit_checked=1 success_exact=1\n",all_checks);
}
int main(){
    Image real;require(load("romfs/battle-preview/transition.bpx",real),"checked real mask");
    for(const auto canvas:{Plan::Canvas{320,180,0,0},Plan::Canvas{400,240,40,30}}){
        compare(real,canvas,0xffffffffu,0x82ff0000u,12);
        compare(real,canvas,0x0000ff00u,0x7d9f3473u,0); // equality wins even for alpha-zero old colors
    }
    Image synthetic;synthetic.width=17;synthetic.height=11;synthetic.frames=3;synthetic.palette={0x00012345,0x01123456,0x80785634,0xfe541289,0xff128745,0xffffffff};synthetic.indices.resize(17*11*3);
    for(size_t i=0;i<synthetic.indices.size();++i)synthetic.indices[i]=uint8_t((i*23+i/17)%synthetic.palette.size());
    for(const auto canvas:{Plan::Canvas{17,11,0,0},Plan::Canvas{25,19,4,4},Plan::Canvas{25,19,-7,9},Plan::Canvas{25,19,INT32_MIN,INT32_MAX}})
        for(uint32_t old:synthetic.palette)compare(synthetic,canvas,old,0x716b53a1u,3);
    negatives();cancellation();std::printf("PASS checks=%zu exact_RGBA_pixels=%llu no_GPU_claim\n",checks,(unsigned long long)compared);
}
