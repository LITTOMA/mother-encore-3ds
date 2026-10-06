#include "encore/field_node_recipe.hpp"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <cstring>
using namespace encore::upstream;
// Manual-only source: not run by development or automatic CI.
int main(int argc,char**argv){
 assert(argc==2);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(file)),{});assert(bytes.size()>128);FieldIdentity id;id.scene_id=uint32_t(bytes[36])|uint32_t(bytes[37])<<8|uint32_t(bytes[38])<<16|uint32_t(bytes[39])<<24;std::copy_n(bytes.data()+40,20,id.upstream_commit.begin());std::copy_n(bytes.data()+60,32,id.source_sha256.begin());FieldNodeRecipeData data;std::string error;assert(data.load(bytes.data(),bytes.size(),id,error));assert(data.records().size()==47&&!data.scene_admitted());auto bad=bytes;bad[8]=2;assert(!data.load(bad.data(),bad.size(),id,error));bad=bytes;bad[28]=99;assert(!data.load(bad.data(),bad.size(),id,error));bad=bytes;bad.back()^=1;assert(!data.load(bad.data(),bad.size(),id,error));auto foreign=id;foreign.source_sha256[0]^=1;assert(!data.load(bytes.data(),bytes.size(),foreign,error));assert(!data.load(bytes.data(),127,id,error));return 0;
}
