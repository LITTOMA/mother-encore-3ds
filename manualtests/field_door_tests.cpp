#include "encore/field_door.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
// Manual-only parser source; never included in automatic build/test targets.
// Caller supplies the trusted scene identity from its independently checked
// scene family. Mutations must preserve the already loaded valid resource.
void field_door_manual_parser_cases(const char*path,const FieldIdentity&id){
 std::ifstream f(path,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(!b.empty());
 FieldDoorData data;std::string e;assert(data.load(b.data(),b.size(),id,e));auto count=data.door_count();assert(count);
 for(size_t cut:{size_t(0),size_t(127),b.size()-1}){assert(!data.load(b.data(),cut,id,e));assert(data.door_count()==count);}
 for(size_t at:{size_t(0),size_t(8),size_t(20),size_t(24),size_t(28),size_t(32),size_t(40),size_t(128),size_t(140),b.size()-1}){auto bad=b;bad[at]^=1;assert(!data.load(bad.data(),bad.size(),id,e));assert(data.door_count()==count);}
 auto wrong=id;wrong.source_sha256[0]^=1;assert(!data.load(b.data(),b.size(),wrong,e));
 FieldDoorRuntime runtime;assert(!runtime.initialize(data,{},e));assert(!runtime.ready(data.door(0).id,e));assert(!runtime.fade_in_done(e));assert(!runtime.deferred_commit(e));assert(!runtime.tree_changed(e));assert(!runtime.fade_out_mostly_done(e));assert(!runtime.special_guest(data.door(0).id,e));
}
