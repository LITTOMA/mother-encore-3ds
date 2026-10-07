#include "house_return_interact_programme.hpp"

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
}
bool HouseReturnInteractProgrammeBridge::prepare(HouseReturnInteractDialog&source,
    FreshHouseState&house,FieldNodeTreeRuntime&tree,FieldGlobalRegistry&registry,
    std::string&e){
 if(source_||house_||tree_||registry_||registry.poisoned()||
    tree.object_domain()!=registry.kernel()||!house.world.healthy()||
    house.house.world_owner()!=&house.world)
  return reject(e,"House Interact bridge requires its fixed actual House/ObjectDB");
 // The source script prepares after this borrower. No native lifecycle or
 // global selection executes here; actual invocation is checked on use.
 source_=&source;house_=&house;tree_=&tree;registry_=&registry;
 e.clear();return true;
}
bool HouseReturnInteractProgrammeBridge::convert(const HouseReturnInteractProgramme&r,
    HouseSourceInteractProgramme&out,std::string&e)const{
 if(!source_||!house_||!tree_||!registry_||r.source!=source_||r.house!=house_||
    r.tree!=tree_||r.registry!=registry_||r.runtime!=&source_->runtime()||
    source_->tree()!=tree_||r.thoughts||!r.data||r.dialogue.empty())
  return reject(e,"House Interact bridge rejected a foreign source invocation");
 out={this,r.runtime,r.data,r.tree,r.tree_data,r.registry,r.reentry,r.doors,
      r.text,r.object,r.source_id,r.programme,r.dialogue,false};
 e.clear();return true;
}
bool HouseReturnInteractProgrammeBridge::observe(const HouseSourceInteractProgramme&r,
    HouseSourceInteractProgramme&out,std::string&e)const{
 if(r.caller!=this||!source_||!house_||r.source!=&source_->runtime()||
    r.tree!=tree_||r.registry!=registry_||r.thoughts)
  return reject(e,"House Interact observation requires its actual caller owner");
 HouseReturnInteractProgramme actual;
 return source_->programme_input(r.object,r.dialogue,actual,e)&&convert(actual,out,e);
}
bool HouseReturnInteractProgrammeBridge::admit(const HouseReturnInteractProgramme&r,
    std::string&e)const{
 HouseSourceInteractProgramme source;
 return convert(r,source,e)&&house_->house.admit_source_interact_programme(source,e);
}
bool HouseReturnInteractProgrammeBridge::request(const HouseReturnInteractProgramme&r,
    std::string&e){
 HouseSourceInteractProgramme source;
 return convert(r,source,e)&&house_->house.request_source_interact_programme(source,e);
}
}
