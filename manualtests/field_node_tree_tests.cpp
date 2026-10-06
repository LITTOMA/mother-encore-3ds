// Explicit manual-only engine-structure cases. Not an upstream gameplay run.
#include "encore/field_node_tree.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void reseal(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=128;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);}put(b,20,~c);}
}
void field_node_tree_manual(const char*pack,const FieldIdentity&id){
 std::ifstream f(pack,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});
 FieldNodeTreeData d;std::string error;assert(d.load(b.data(),b.size(),id,error));
 const auto original=d.records().size();assert(original&&d.record(id.scene_id));
 for(auto offset:{size_t(8),size_t(24),size_t(28),size_t(124)}){
  auto bad=b;put(bad,offset,word(bad,offset)+1);assert(!d.load(bad.data(),bad.size(),id,error));assert(d.records().size()==original);
 }
 auto bad=b;bad.pop_back();assert(!d.load(bad.data(),bad.size(),id,error));
 size_t first=128+4+word(b,128);auto classes=word(b,first);first+=4;
 for(uint32_t i=0;i<classes;++i)first+=4+word(b,first);
 bad=b;put(bad,first+4,id.scene_id);reseal(bad);assert(!d.load(bad.data(),bad.size(),id,error));
 bad=b;put(bad,first+20,0);reseal(bad);assert(!d.load(bad.data(),bad.size(),id,error));
 bad=b;put(bad,first+28,1u<<31);reseal(bad);assert(!d.load(bad.data(),bad.size(),id,error));
 bad=b;put(bad,first+36,1u<<31);reseal(bad);assert(!d.load(bad.data(),bad.size(),id,error));
 // There is deliberately no blanket Ready callback in this manual host.
 FieldNodeTreeHost host;uint64_t counter=0;size_t dispatches=0;
 host.allocate_object=[&](FieldObjectId&out,std::string&){out=++counter;return true;};
 host.allocate_fast_name=[&](uint64_t&out,std::string&){out=++counter;return true;};
 host.bind=[](FieldObjectId,const FieldNodeDescriptor&,FieldNodeBinding&,std::string&e){e="Manual host has no typed native/script adapter";return false;};
 host.dispatch=[&](FieldObjectId,const FieldNodeBinding&,FieldTreePhase,std::string&){++dispatches;return false;};
 host.deferred=[](const FieldDeferredMessage&,std::string&){return false;};
 host.object_exists=[](FieldObjectId){return false;};
 host.input_registration=[](FieldObjectId,uint32_t,bool,std::string&){return false;};
 host.external_pause_process=[](FieldObjectId){return false;};
 host.release=[](FieldObjectId,const FieldNodeBinding&,std::string&){return false;};
 auto split_host=host;
 FieldNodeTreeRuntime tree;assert(tree.initialize(d,std::move(host),error));assert(tree.object_count()==original);
 FieldObjectId target=0;assert(tree.get_node(tree.root(),".",target,error)&&target==tree.root());
 assert(!tree.get_node(tree.root(),"",target,error));assert(!tree.get_node(tree.root(),"%unreviewed",target,error));
 assert(!tree.get_node(tree.root(),"/root",target,error));assert(!tree.set_owner(tree.root(),tree.root(),error));
 assert(!tree.enter(error)&&tree.lifecycle_pending()&&!dispatches);
 assert(!tree.resume_lifecycle(error)&&!dispatches);
 // Source startup cannot skip a missing native owner by issuing Ready first,
 // retrying Enter, or switching the incomplete split traversal to enter().
 FieldNodeTreeRuntime split;assert(split.initialize(d,std::move(split_host),error));
 assert(!split.ready_entered_branch(error)&&!split.lifecycle_pending());
 assert(!split.enter_branch_only(error)&&split.lifecycle_pending()&&!dispatches);
 assert(!split.ready_entered_branch(error)&&!dispatches);
 assert(!split.enter_branch_only(error)&&!dispatches);
 assert(!split.enter(error)&&!dispatches);
 assert(!split.resume_lifecycle(error)&&split.lifecycle_pending()&&!dispatches);
 assert(!tree.process(false,false,error));assert(!d.scene_admitted());
 FieldDeferredMessage unknown;unknown.object=1;unknown.kind=static_cast<FieldDeferredKind>(3);
 assert(!tree.enqueue(unknown,error));unknown.kind=FieldDeferredKind::Set;unknown.member="x";
 assert(!tree.enqueue(unknown,error));
}
