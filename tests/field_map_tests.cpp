// Manual suite: intentionally not run by automatic developer workflows.
#include "encore/field_map.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
using namespace encore::upstream;
namespace {
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>& b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(8*i));}
void checksum(std::vector<uint8_t>& b){uint32_t c=~0u;for(size_t i=128;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0u);}put(b,24,~c);}
}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});assert(bytes.size()>560);
    FieldIdentity id;id.scene_id=u32(bytes.data()+36);std::copy_n(bytes.data()+40,20,id.upstream_commit.begin());std::copy_n(bytes.data()+60,32,id.source_sha256.begin());
    FieldMapView view;std::string error;assert(view.load(bytes.data(),bytes.size(),id,error));assert(view.map_count()&&view.cell_count()&&!view.scene_admitted());const auto count=view.cell_count();
    auto reject=[&](std::vector<uint8_t> bad,bool fix=true){if(fix)checksum(bad);assert(!view.load(bad.data(),bad.size(),id,error));assert(view.cell_count()==count);};
    auto bad=bytes;put(bad,8,2);reject(bad);bad=bytes;put(bad,32,2);reject(bad);bad=bytes;bad[40]^=1;reject(bad);bad=bytes;bad[60]^=1;reject(bad);bad=bytes;bad.back()^=1;reject(bad,false);
    bad=bytes;put(bad,128+3*24+12,27);reject(bad);
    const auto canvas=u32(bytes.data()+128+8*24+4);bad=bytes;put(bad,canvas+8,0);reject(bad);
    const auto draw=u32(bytes.data()+128+4*24+4);bad=bytes;put(bad,draw+8,0xffffffff);reject(bad);
    const auto polygon=u32(bytes.data()+128+5*24+4);bad=bytes;put(bad,polygon+8,4);reject(bad);
    bad=bytes;put(bad,polygon+12,1);reject(bad);
    const auto texture=u32(bytes.data()+128+7*24+4);bad=bytes;put(bad,texture+12,1025);reject(bad);
    const auto chunk=u32(bytes.data()+128+10*24+4);bad=bytes;put(bad,chunk+8,0);reject(bad);
    const auto spatial=u32(bytes.data()+128+12*24+4);bad=bytes;put(bad,spatial,0);reject(bad);
    bad=bytes;put(bad,124,0xffffffff);reject(bad);
    const auto transform=u32(bytes.data()+128+17*24+4);bad=bytes;put(bad,transform+16,0x7f800000);reject(bad);
    bad=bytes;put(bad,polygon+36,1);reject(bad);
    const auto strings=u32(bytes.data()+128+4),characters=u32(bytes.data()+128+24+4);const auto path=view.texture(0).path;const auto path_offset=u32(bytes.data()+strings+path*8);
    bad=bytes;bad[characters+path_offset]='.';reject(bad);
    auto wrong=id;wrong.source_sha256[0]^=1;assert(!view.load(bytes.data(),bytes.size(),wrong,error));assert(!view.load(bytes.data(),415,id,error));
    std::vector<uint32_t> indices{123};assert(!view.collect_draws({{-100000,-100000},{100000,100000}},{},1000000,indices,error));assert(indices==std::vector<uint32_t>{123});
    FieldMapGateQuery visible=[](uint32_t){return FieldMapGateState::Visible;};
    assert(!view.collect_draws({{-100000,-100000},{100000,100000}},visible,0,indices,error));assert(indices==std::vector<uint32_t>{123});
    for(uint32_t i=0;i<view.polygon_count();++i){auto p=view.polygon(i);if(p.kind!=2)continue;auto g=view.local_geometry(p.geometry);auto n=view.concave_node(g.bvh_first);std::vector<uint32_t> pairs;const FieldMapRect area{{n.bounds.minimum.x-1,n.bounds.minimum.y-1},{n.bounds.maximum.x+1,n.bounds.maximum.y+1}};assert(view.concave_segments(i,area,g.leaf_count,pairs,error));assert(pairs.size()==g.leaf_count);for(uint32_t j=0;j<g.leaf_count;++j)assert(pairs[j]==view.concave_leaf(g.leaf_first+j));assert(!view.concave_segments(i,area,0,pairs,error));}
    for(uint32_t i=0;i<view.texture_count();++i){auto t=view.texture(i);if(t.frames<=1||t.frame)continue;FieldMapAnimationState state;assert(view.start_animation(i,state,error));assert(view.advance_animation(state,100,error)&&state.frame==0);const auto limit=1.f/t.fps+t.delay;assert(view.advance_animation(state,limit,error)&&state.frame==0);assert(view.advance_animation(state,limit*.01f,error)&&state.frame==1);assert(!view.advance_animation(state,-1,error));state.time=std::numeric_limits<float>::max();const auto previous=state;assert(!view.advance_animation(state,std::numeric_limits<float>::max(),error));assert(state.frame==previous.frame&&state.time==previous.time);}
}
