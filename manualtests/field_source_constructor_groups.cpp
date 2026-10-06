// Manual structural test only: no original script/Ready/gameplay approval.
#include "encore/field_global_constructor.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace encore::upstream;
template<size_t N> bool hex(const char *s,std::array<uint8_t,N> &out) {
  std::string text(s);if(text.size()!=N*2)return false;
  auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  for(size_t i=0;i<N;++i){int a=digit(text[i*2]),b=digit(text[i*2+1]);if(a<0||b<0)return false;out[i]=uint8_t(a*16+b);}return true;
}
int main(int argc,char **argv) {
  if(argc!=5)return 2;
  FieldIdentity identity;char *end=nullptr;auto scene=std::strtoul(argv[2],&end,10);
  if(!end||*end||!scene||scene>UINT32_MAX||!hex(argv[3],identity.upstream_commit)||!hex(argv[4],identity.source_sha256))return 2;
  identity.scene_id=uint32_t(scene);FieldGlobalConstructorData source;std::string error;
  if(!source.load_file(argv[1],identity,error)){std::cerr<<error;return 1;}
  FieldNodeTreeRuntime tree;FieldNodeTreeHost host;FieldObjectId next=1;uint64_t name=1;
  host.allocate_object=[&](FieldObjectId &out,std::string&){out=next++;return true;};
  host.allocate_fast_name=[&](uint64_t &out,std::string&){out=name++;return true;};
  host.construct_source=[&](FieldObjectId id,const FieldNodeDescriptor&,const FieldIdentity&,std::string &e){return tree.add_group(id,"idle_process_internal",e);};
  host.bind=[](FieldObjectId,const FieldNodeDescriptor&,FieldNodeBinding&,std::string &e){e="This structural fixture grants no source binding/Ready";return false;};
  host.dispatch=[](FieldObjectId,const FieldNodeBinding&,FieldTreePhase,std::string &e){e="This structural fixture grants no native/script lifecycle";return false;};
  host.deferred=[](const FieldDeferredMessage&,std::string&){return false;};
  host.object_exists=[&](FieldObjectId id){return id&&id<next;};
  host.input_registration=[](FieldObjectId,uint32_t,bool,std::string&){return false;};
  host.external_pause_process=[](FieldObjectId){return false;};
  host.release=[](FieldObjectId,const FieldNodeBinding&,std::string&){return false;};
  if(!tree.initialize_recipe(source.recipe(),host,error)){std::cerr<<error;return 1;}
  for(const auto &record:source.recipe().records()) {
    const auto *node=tree.state(tree.source_object(record.id));
    if(!node||std::count(node->groups.begin(),node->groups.end(),"idle_process_internal")!=1)return 1;
    for(const auto &g:record.groups)if(std::count(node->groups.begin(),node->groups.end(),g)!=1)return 1;
  }
  auto root=tree.root();bool before=(tree.state(root)->flags&8)!=0;
  if(!tree.set_behind_parent(root,!before,error)||(bool(tree.state(root)->flags&8)==before))return 1;
  FieldObjectId plain=0;for(const auto &record:source.recipe().records())if(!(record.flags&1))plain=tree.source_object(record.id);
  if(!plain||tree.set_behind_parent(plain,true,error)||tree.set_behind_parent(0,true,error))return 1;
  return 0;
}
