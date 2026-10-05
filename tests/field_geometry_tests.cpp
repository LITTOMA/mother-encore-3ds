#include "encore/field_geometry.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
// Manual only. No development or PR workflow runs this source automatically.
using namespace encore::upstream;
namespace {
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void set(std::vector<uint8_t>& b,size_t i,uint32_t v){for(unsigned j=0;j<4;++j)b[i+j]=uint8_t(v>>(j*8));}
void checksum(std::vector<uint8_t>& b){uint32_t c=~0u;for(size_t i=296;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0u);}set(b,24,~c);}
}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};assert(bytes.size()>296);
    FieldIdentity expected;expected.scene_id=u32(bytes.data()+36);std::memcpy(expected.upstream_commit.data(),bytes.data()+40,20);std::memcpy(expected.source_sha256.data(),bytes.data()+60,32);
    FieldGeometryView resource;std::string error;assert(resource.load(bytes.data(),bytes.size(),expected,error));assert(!resource.scene_admitted());assert(resource.owner_count()==902&&resource.shape_count()==941&&resource.node_count()==2033);
    size_t circles=0,disabled=0,empty=0;for(uint32_t i=0;i<resource.shape_count();++i){auto s=resource.shape(i);circles+=s.kind==2;disabled+=(s.flags&1)!=0;empty+=s.kind==0;}assert(circles==444&&disabled==158&&empty==1);
    auto reject=[&](std::vector<uint8_t> bad){assert(!resource.load(bad.data(),bad.size(),expected,error));assert(resource.owner_count()==902);};
    auto bad=bytes;set(bad,8,2);reject(bad);bad=bytes;bad[60]^=1;reject(bad);bad=bytes;bad.back()^=1;reject(bad);bad=bytes;bad.pop_back();reject(bad);
    bad=bytes;auto shape_offset=u32(bad.data()+128+4*24+12);set(bad,shape_offset+4,902);checksum(bad);reject(bad);
    bad=bytes;auto geometry_offset=u32(bad.data()+128+5*24+12);set(bad,geometry_offset,99);checksum(bad);reject(bad);
    bad=bytes;auto node_offset=u32(bad.data()+128+2*24+12);set(bad,node_offset+112+8,2033);checksum(bad);reject(bad);
    std::vector<uint32_t> owners{12345};assert(!resource.owners_for_layer(~0u,0,owners,error));assert(owners==std::vector<uint32_t>{12345});
}
