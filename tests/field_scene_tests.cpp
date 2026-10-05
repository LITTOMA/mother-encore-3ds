#include "encore/field_data.hpp"
#include "encore/field_runtime.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using namespace encore::upstream;
namespace {
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void put(std::vector<uint8_t>& b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
void checksum(std::vector<uint8_t>& b){uint32_t crc=0xffffffff;for(size_t i=128;i<b.size();++i){crc^=b[i];for(int j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}put(b,20,crc^0xffffffff);}
}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});assert(bytes.size()>272);
    FieldIdentity identity;identity.scene_id=u32(bytes.data()+36);std::copy_n(bytes.data()+48,20,identity.upstream_commit.begin());std::copy_n(bytes.data()+68,32,identity.source_sha256.begin());
    FieldData data;std::string error;assert(data.load(bytes.data(),bytes.size(),identity,error));assert(data.grass_count()>0&&data.pending_count()>0&&!data.scene_admitted());
    const auto original=data.identity();const auto count=data.grass_count();
    auto reject=[&](std::vector<uint8_t> bad,bool crc=true){if(crc)checksum(bad);assert(!data.load(bad.data(),bad.size(),identity,error));assert(data.grass_count()==count&&data.identity().source_sha256==original.source_sha256);};
    auto bad=bytes;put(bad,8,2);reject(bad);
    bad=bytes;put(bad,28,2);reject(bad);
    bad=bytes;put(bad,32,0);reject(bad);
    bad=bytes;bad[48]^=1;reject(bad);
    bad=bytes;bad[68]^=1;reject(bad);
    bad=bytes;bad.back()^=1;reject(bad,false);
    bad=bytes;put(bad,128+3*24+16,55);reject(bad);
    const auto grass_offset=u32(bytes.data()+128+3*24+4);
    bad=bytes;put(bad,grass_offset+24,0);reject(bad);
    bad=bytes;put(bad,grass_offset+16,u32(bad.data()+grass_offset+16)^1);reject(bad);
    bad=bytes;put(bad,grass_offset+56,u32(bad.data()+grass_offset));reject(bad);
    bad=bytes;put(bad,grass_offset+56+12,u32(bad.data()+grass_offset+12));reject(bad);
    const auto profile_offset=u32(bytes.data()+128+2*24+4);
    bad=bytes;put(bad,profile_offset+64,0);put(bad,profile_offset+68,0);reject(bad);
    const auto pending_offset=u32(bytes.data()+128+5*24+4);
    bad=bytes;put(bad,pending_offset+16,1);reject(bad);
    auto wrong=identity;wrong.source_sha256[0]^=1;assert(!data.load(bytes.data(),bytes.size(),wrong,error));
    assert(!data.load(bytes.data(),127,identity,error));
    FieldRuntime field;assert(field.prepare_grass_slice(data,error));assert(!field.activate_scene(error));
    SourceRandom actual(42),expected(42);const auto g=data.grass(0);const auto p=data.profile(g.profile_index);
    const auto before=actual.state();assert(!field.execute_grass_ready(g.ready_ordinal+1,actual,error));assert(actual.state()==before);
    expected.seed(g.seed);auto texture=p.texture_first+expected.randi()%g.grass_types;bool flip=expected.randi()%2==1;
    assert(field.execute_grass_ready(g.ready_ordinal,actual,error));assert(actual.state()==expected.state());assert(!field.execute_grass_ready(g.ready_ordinal,actual,error));
    assert(field.screen_entered(g.stable_id,error));auto first=field.grass_instance(g.stable_id);assert(first);
    assert(field.body_entered(first,100,g.position.x,true,error));assert(!field.body_entered(first,100,g.position.x,true,error));
    assert(field.physics_tick(1.f/60,error));assert(field.idle_tick(1./60,error));
    std::vector<FieldGrassDraw> draws;field.grass_draws(draws);assert(draws.size()==1&&draws[0].texture_index==texture&&draws[0].flip_h==flip&&draws[0].frame==p.frames[2]);
    assert(field.body_exited(first,100,error));for(int i=0;i<4;++i)assert(field.idle_tick(p.idle_delay/4,error)); // exact zero does not timeout yet.
    assert(field.screen_exited(g.stable_id,error));assert(field.screen_entered(g.stable_id,error));auto second=field.grass_instance(g.stable_id);assert(first!=second);
    field.grass_draws(draws);assert(draws.size()==2);field.flush_deferred();field.grass_draws(draws);assert(draws.size()==1&&draws[0].instance_id==second);
    assert(!field.body_entered(first,101,0,true,error));assert(!field.idle_tick(std::nan(""),error));assert(!field.physics_tick(-1,error));
}
