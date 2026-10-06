// Manual-only independent binary capability cases. Never gameplay approval.
#include "encore/field_node_recipe.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
std::vector<uint8_t> bytes(const char *path){std::ifstream f(path,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
FieldIdentity identity(const std::vector<uint8_t> &b){assert(b.size()>128);FieldIdentity out;for(unsigned i=0;i<4;++i)out.scene_id|=uint32_t(b[36+i])<<(8*i);std::copy_n(b.data()+40,20,out.upstream_commit.begin());std::copy_n(b.data()+60,32,out.source_sha256.begin());return out;}
void reseal(std::vector<uint8_t> &b){uint32_t c=~0u;for(size_t i=128;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0u);}c=~c;for(unsigned i=0;i<4;++i)b[20+i]=uint8_t(c>>(8*i));}
}
void field_node_recipe_capabilities_manual(const char *cap5,const char *cap6){
 auto old=bytes(cap5);const auto old_identity=identity(old);assert(old[28]==5);
 FieldNodeRecipeData data;std::string error;
 assert(data.load(old.data(),old.size(),old_identity,error)&&data.classes().size()==46);
 auto malformed=old;malformed[28]=6;
 assert(!data.load(malformed.data(),malformed.size(),old_identity,error)&&data.classes().size()==46);
 auto current=bytes(cap6);const auto current_identity=identity(current);assert(current[28]==6);
 assert(data.load(current.data(),current.size(),current_identity,error)&&data.classes().size()==47&&data.classes()[46]=="AnimationTree"&&!data.scene_admitted());
 for(const auto &r:data.records())if(r.class_index==46)assert(!(r.flags&1));
 malformed=current;malformed[28]=7;
 assert(!data.load(malformed.data(),malformed.size(),current_identity,error)&&data.classes().size()==47);
 malformed=current;const std::string name="AnimationTree";
 const auto where=std::search(malformed.begin()+128,malformed.end(),name.begin(),name.end());assert(where!=malformed.end());
 *where='X';reseal(malformed);
 assert(!data.load(malformed.data(),malformed.size(),current_identity,error)&&data.classes()[46]=="AnimationTree");
 malformed=current;malformed[28]=5;
 assert(!data.load(malformed.data(),malformed.size(),current_identity,error));
}
