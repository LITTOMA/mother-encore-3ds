// Manual primitive cases. These do not run or approve Podunk's script roster.
#include "encore/field_map_space.hpp"
#include <cassert>
using namespace encore::upstream;
void field_map_space_manual_cases(const FieldMapView&map,
    const FieldSceneActionsData&actions){
 FieldMapSpace space;std::string error;assert(space.configure(map,actions,error));
 uint32_t leaf=0,parent=0,original=0;
 for(const auto&binding:actions.bindings())if(binding.kind==1)
  for(const auto&object:binding.objects){const auto*r=actions.reference(object.source_id);
   if(r->kind==1&&r->parent!=binding.parent_id){leaf=r->id;parent=binding.parent_id;original=r->parent;break;}}
 assert(leaf&&parent&&original);assert(space.admit_reparent(leaf,parent,error));
 assert(!space.admit_reparent(leaf,0,error));assert(!space.remove_leaf(leaf,0,error));
 assert(!space.append_leaf(leaf,parent,1,error));
 assert(space.remove_leaf(leaf,original,error));assert(!space.remove_leaf(leaf,original,error));
 assert(!space.append_leaf(leaf,0,1,error));assert(space.append_leaf(leaf,parent,1,error));
 assert(!space.set_collision(leaf,0,1,error));assert(space.set_collision(leaf,2,0,error));
 uint32_t index=map.map_count();for(uint32_t i=0;i<map.map_count();++i)if(map.map(i).stable_id==leaf)index=i;
 assert(index<map.map_count()&&space.map(index).layer==0);
 std::vector<uint32_t>out{0xffffffffu};FieldMapGateQuery pending=[](uint32_t){return FieldMapGateState::Pending;};
 assert(!space.collect_draws({{-10000,-10000},{10000,10000}},pending,1000000,out,error));assert(out==std::vector<uint32_t>{0xffffffffu});
 std::vector<uint8_t>invalid(map.map_count(),0);invalid[0]=2;
 assert(!map.collect_draws_masked({{0,0},{100,100}},invalid,100,out,error));assert(out==std::vector<uint32_t>{0xffffffffu});
 assert(space.commit_deleted(parent,error));assert(!space.admit_reparent(leaf,parent,error));
}
