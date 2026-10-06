// Manual-only structural fixtures. No source gameplay or Ready is simulated.
#include "encore/field_node_tree.hpp"
#include <cassert>
using namespace encore::upstream;
void field_node_source_constructor_manual() {
 FieldIdentity identity;identity.scene_id=7;identity.upstream_commit[0]=1;identity.source_sha256[0]=2;
 FieldNodeDescriptor descriptor;descriptor.id=7;descriptor.path=".";descriptor.index=-1;
 descriptor.native_class="Node";descriptor.script="fixture/source.gd";descriptor.script_sha[0]=3;
 descriptor.local=descriptor.world={{{1,0},{0,1},{0,0}}};
 descriptor.modulate=descriptor.self_modulate={1,1,1,1};
 std::string error;FieldObjectId allocated=0;size_t constructors=0,paths=0;
 FieldNodeTreeRuntime tree;FieldNodeTreeHost host;
 host.allocate_object=[&](FieldObjectId &out,std::string&){out=++allocated;return true;};
 host.allocate_fast_name=[](uint64_t&,std::string&){return false;};
 host.bind=[&](FieldObjectId,const FieldNodeDescriptor &source,FieldNodeBinding &out,std::string&){
  out.identity=identity;out.stable_id=source.id;out.class_index=source.class_index;
  out.family=1;out.capability=1;out.script_sha=source.script_sha;out.native_class=source.native_class;return true;
 };
 host.dispatch=[&](FieldObjectId,const FieldNodeBinding&,FieldTreePhase phase,std::string &e){
  if(phase!=FieldTreePhase::PathChanged){e="Fixture has no Enter or Ready consumer";return false;}
  ++paths;return true;
 };
 host.deferred=[](const FieldDeferredMessage&,std::string&){return false;};
 host.object_exists=[](FieldObjectId){return false;};
 host.input_registration=[](FieldObjectId,uint32_t,bool,std::string&){return false;};
 host.external_pause_process=[](FieldObjectId){return false;};
 host.release=[](FieldObjectId,const FieldNodeBinding&,std::string&){return false;};
 assert(!tree.initialize_source_node(identity,descriptor,host,error)&&!allocated);
 host.construct_source=[&](FieldObjectId id,const FieldNodeDescriptor&,const FieldIdentity&,std::string&){
  const auto *state=tree.state(id);assert(state&&state->name.empty()&&!state->parent&&!state->owner&&state->children.empty()&&!state->inside&&!state->ready_notified);
  ++constructors;return true;
 };
 auto malformed=descriptor;malformed.name="invented";
 assert(!tree.initialize_source_node(identity,malformed,host,error)&&!allocated);
 malformed=descriptor;malformed.native_class="UnknownNode";
 assert(!tree.initialize_source_node(identity,malformed,host,error)&&!allocated);
 malformed=descriptor;malformed.parent=1;
 assert(!tree.initialize_source_node(identity,malformed,host,error)&&!allocated);
 assert(tree.initialize_source_node(identity,descriptor,host,error)&&allocated==1&&constructors==1);
 assert(tree.root()==1&&tree.source_object(7)==1&&tree.state(1)->name.empty());
 assert(!tree.set_name(1,"bad/path",error)&&!paths);
 assert(tree.set_name(1,"source_name",error)&&paths==1&&tree.state(1)->name=="source_name");
 assert(!tree.state(1)->inside&&!tree.state(1)->ready_notified);
 FieldNodeTreeRuntime rejected;auto rejecting=host;
 rejecting.construct_source=[](FieldObjectId,const FieldNodeDescriptor&,const FieldIdentity&,std::string &e){e="Unknown source constructor";return false;};
 assert(!rejected.initialize_source_node(identity,descriptor,rejecting,error));
 const auto after_failure=allocated;
 assert(!rejected.initialize_source_node(identity,descriptor,rejecting,error)&&allocated==after_failure);
}
