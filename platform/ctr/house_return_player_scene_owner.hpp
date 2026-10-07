#pragma once
#include "house_return_button_prompt_native.hpp"
#include "house_return_npc_runtime.hpp"
#include "podunk_house_continuation.hpp"
#include "podunk_player_host.hpp"

namespace encore::ctr {
struct HouseReturnPlayerSceneInput {
  const upstream::HouseReturnSources *sources=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FieldGlobalRegistry *registry=nullptr;
  HouseReturnNpcRuntime *npcs=nullptr;
  HouseReturnInteractDialog *interact=nullptr;
  HouseReturnButtonPromptNative *prompts=nullptr;
  PodunkPlayerHost *player=nullptr;
  PodunkHouseContinuation *continuation=nullptr;
};
// Actual House source receivers only. Shared Player business remains in the
// existing SceneServices; this owner does not create a Player, frame or clock.
class HouseReturnPlayerSceneOwner final:public HouseButtonPromptSourceOwner {
public:
  HouseReturnPlayerSceneOwner()=default;
  HouseReturnPlayerSceneOwner(const HouseReturnPlayerSceneOwner&)=delete;
  HouseReturnPlayerSceneOwner&operator=(const HouseReturnPlayerSceneOwner&)=delete;
  bool prepare(HouseReturnPlayerSceneInput,std::string&);
  const upstream::FieldNodeTreeRuntime*tree()const{return in_.tree;}
  const upstream::FieldGlobalRegistry*registry()const{return in_.registry;}
  const PodunkPlayerHost*player()const{return in_.player;}
  bool borrowed(std::string&)const;
  bool current_scene_area(bool&,std::string&)const;
  bool respawn_path(std::string&,std::string&)const;
  bool collider_info(upstream::FieldObjectId,upstream::PlayerColliderInfo&,
                     std::string&)const;
  bool turn_player(upstream::FieldObjectId,bool party,std::string&);
  bool interact(upstream::FieldObjectId,bool thoughts,std::string&);
  bool press_prompt(upstream::FieldObjectId,std::string&);
  bool update_key_indicator(std::string&);
  // Only press_prompt's real synchronous source call produces this receipt.
  // Exported field assignments and other force methods remain unadmitted.
  bool observe(upstream::FieldObjectId,std::string_view,
               HouseButtonPromptSourceCall&,std::string&)const override;
private:
  struct Press;
  HouseReturnPlayerSceneInput in_{};
  bool prepared_=false;
  const Press *press_=nullptr;
  bool current_root(std::string&)const;
  bool live_player(std::string&)const;
  bool actual(upstream::FieldObjectId,const upstream::FieldNodeDescriptor*&,
              const upstream::FieldNodeState*&,std::string&)const;
};
}
