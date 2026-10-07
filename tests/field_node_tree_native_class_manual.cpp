#include "encore/field_node_tree.hpp"
#include "manual_require.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&v,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)v.at(at+i)=uint8_t(value>>(8*i));}
void reseal(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=128;i<v.size();++i){c^=v[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}put(v,20,~c);}
bool hex(std::string_view s,uint8_t*out,size_t n){
 if(s.size()!=2*n)return false;
 for(size_t i=0;i<n;++i){unsigned v=0;for(unsigned j=0;j<2;++j){const char c=s[2*i+j];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:99;if(d>15)return false;v=16*v+d;}out[i]=uint8_t(v);}return true;
}
}
// Explicit manual regression only; no automatic build/test registration.
// Uses an actual checked tree pack and independent expected identity arguments.
int main(int argc,char**argv){
 if(argc!=5){std::cerr<<"tree-pack scene-id pin source-sha\n";return 2;}
 FieldIdentity identity;identity.scene_id=uint32_t(std::strtoul(argv[2],nullptr,10));
 MANUAL_REQUIRE(hex(argv[3],identity.upstream_commit.data(),20));
 MANUAL_REQUIRE(hex(argv[4],identity.source_sha256.data(),32));
 std::ifstream f(argv[1],std::ios::binary);MANUAL_REQUIRE(bool(f));
 std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(f)),{});
 FieldNodeTreeData owner;std::string error;
 MANUAL_REQUIRE(owner.load(bytes.data(),bytes.size(),identity,error));
 for(const auto&node:owner.records()){
  MANUAL_REQUIRE(node.class_index<owner.classes().size());
  MANUAL_REQUIRE(!node.native_class.empty()&&node.native_class==owner.classes()[node.class_index]);
 }
 const auto count=owner.records().size();const auto root=owner.records().front().native_class;
 size_t at=128;
 auto text=[&]{MANUAL_REQUIRE(at+4<=bytes.size());const auto n=word(bytes.data()+at);at+=4;MANUAL_REQUIRE(n<=bytes.size()-at);at+=n;};
 text();MANUAL_REQUIRE(at+4<=bytes.size());const auto classes=word(bytes.data()+at);at+=4;
 const auto first_class=at+4;
 for(uint32_t i=0;i<classes;++i)text();
 MANUAL_REQUIRE(at+20<=bytes.size());
 auto bad=bytes;put(bad,at+16,classes);reseal(bad);
 MANUAL_REQUIRE(!owner.load(bad.data(),bad.size(),identity,error));
 MANUAL_REQUIRE(owner.valid()&&owner.records().size()==count&&owner.records().front().native_class==root);
 bad=bytes;MANUAL_REQUIRE(first_class<bad.size());bad[first_class]='?';reseal(bad);
 MANUAL_REQUIRE(!owner.load(bad.data(),bad.size(),identity,error));
 MANUAL_REQUIRE(owner.valid()&&owner.records().size()==count&&owner.records().front().native_class==root);
}
