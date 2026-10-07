#pragma once
#include "house_return_npc_runtime.hpp"
#include "podunk_house_continuation.hpp"
#include "podunk_player_host.hpp"

namespace encore::ctr {
struct HouseReturnUiContextInput {
  const upstream::HouseReturnSources *sources=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FreshHouseState *house=nullptr;
  PodunkHouseContinuation *continuation=nullptr;
  PodunkMickSession *session=nullptr;
  PodunkPlayerHost *player=nullptr;
  HouseReturnDialogue *dialogue=nullptr;
  bool actual_debug_build=false;
  HouseReturnNpcRuntime *npc_source=nullptr;
};
// Borrows the retained UI/Global/Player and the destination House. No input,
// programme lease, timer, dialogue stack or source state is owned here. Keep
// this fixed-address owner alive until all NPC callbacks/deletes have completed.
class HouseReturnUiContext final {
public:
  HouseReturnUiContext()=default;
  HouseReturnUiContext(const HouseReturnUiContext&)=delete;
  HouseReturnUiContext&operator=(const HouseReturnUiContext&)=delete;
  HouseReturnUiContext(HouseReturnUiContext&&)=delete;
  HouseReturnUiContext&operator=(HouseReturnUiContext&&)=delete;
  // Stage before factory construction; this grants no destination Ready.
  // The live endpoints check the completed receiver/tree transfer themselves.
  bool prepare(HouseReturnUiContextInput,std::string&);
  // Installs only the UI/class/membership endpoints. Native sprite/timer and
  // actual source signal admission remain with their concrete owners.
  bool install(HouseReturnNpcPorts&,std::string&);
  bool context(upstream::FieldObjectId,upstream::FieldNpcContext&,std::string&)const;
  bool close_commands(std::string&);
  bool party_player(upstream::FieldObjectId,bool&,std::string&)const;
  bool persistent(upstream::FieldObjectId,bool&,std::string&)const;
  bool telepathy(upstream::FieldObjectId,bool,std::string&)const;
private:
  HouseReturnUiContextInput in_{};
  upstream::FieldGlobalRegistry *registry_=nullptr;
  upstream::FieldGlobalConstructorRuntime *global_=nullptr;
  HouseUiContinuation *ui_=nullptr;
  bool prepared_=false,installed_=false;
  bool domains(std::string&)const;
  bool live_ui(std::string&,upstream::FieldObjectId ready_npc=0)const;
  bool npc(upstream::FieldObjectId,bool entered,std::string&)const;
  bool player(upstream::FieldObjectId&,std::string&,upstream::FieldObjectId ready_npc=0)const;
};
} // namespace encore::ctr
