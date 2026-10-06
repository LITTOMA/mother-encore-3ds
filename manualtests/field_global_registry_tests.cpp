// Explicitly manual: parser negative cases. Not run by automatic development.
#include "encore/field_global_registry.hpp"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream f(argv[1],std::ios::binary);
 std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});
 if(b.size()<128)return 3;
 FieldIdentity id;id.scene_id=uint32_t(b[36])|(uint32_t(b[37])<<8)|(uint32_t(b[38])<<16)|(uint32_t(b[39])<<24);
 std::copy_n(b.data()+40,20,id.upstream_commit.begin());std::copy_n(b.data()+60,32,id.source_sha256.begin());
 FieldGlobalRegistryData d;std::string e;assert(d.load(b.data(),b.size(),id,e));assert(!d.scene_admitted());
 auto bad=b;bad[0]^=1;assert(!d.load(bad.data(),bad.size(),id,e));
 bad=b;bad[28]^=1;assert(!d.load(bad.data(),bad.size(),id,e));
 bad=b;bad.back()^=1;assert(!d.load(bad.data(),bad.size(),id,e));
 assert(!d.load(b.data(),127,id,e));
 auto foreign=id;foreign.scene_id^=1;assert(!d.load(b.data(),b.size(),foreign,e));
 FieldGlobalRegistry registry;assert(!registry.initialize(d,{},e));
 std::puts("manual registry parser checks passed");
}
