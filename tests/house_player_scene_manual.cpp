// Manual negative cases only; not registered or run by this source slice.
// Supply actual retained/constructed owners, never synthetic Ready receipts.
#include "../platform/ctr/house_return_player_scene_owner.hpp"
#include "../platform/ctr/podunk_player_scene_services.hpp"

namespace encore::ctr::manual {
bool house_player_mixed_route(PodunkPlayerSceneInput real_podunk,
    HouseReturnPlayerSceneOwner&house,std::string&e){
  if(!real_podunk.consumers||!real_podunk.scripts){
    e="manual mixed-route case requires the actual prepared Podunk inputs";return false;
  }
  real_podunk.house=&house;PodunkPlayerSceneServices candidate;std::string rejected;
  if(candidate.prepare(real_podunk,rejected)||candidate.sources()){
    e="manual mixed-route case accepted both scenes or retained failed inputs";return false;
  }
  e.clear();return true;
}
bool house_player_foreign_rebind(PodunkPlayerSceneServices&same,
    HouseReturnPlayerSceneOwner&house,upstream::FieldNodeTreeRuntime&foreign,
    upstream::FieldNodeTreeRuntime&next,std::string&e){
  const auto*before=same.tree();std::string rejected;
  if(same.rebind_house(house,foreign,next,{},rejected)||same.tree()!=before){
    e="manual foreign/empty transfer accepted or changed the retained callback owner";return false;
  }
  e.clear();return true;
}
bool house_player_unscoped_prompt(HouseReturnPlayerSceneOwner&house,
    upstream::FieldObjectId actual_prompt,std::string_view actual_method,std::string&e){
  HouseButtonPromptSourceCall out;out.caller=123;std::string rejected;
  if(house.observe(actual_prompt,actual_method,out,rejected)||out.caller!=123){
    e="manual Prompt observation manufactured a caller scope or changed failed output";return false;
  }
  upstream::PlayerColliderInfo collision;collision.name="preserve";
  if(house.collider_info(0,collision,rejected)||collision.name!="preserve"){
    e="manual unknown collider was admitted or replaced failed output";return false;
  }
  e.clear();return true;
}
}
