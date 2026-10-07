#pragma once
#include "house_return_dialogue_native.hpp"
#include "house_return_ray_native.hpp"
#include "podunk_global_data_host.hpp"
#include "encore/field_global_constructor.hpp"

namespace encore::ctr {
// Endpoints whose native resources are not owned by the NPC script. Every
// endpoint is mandatory and receives the actual ObjectDB receiver. A target
// must supply the checked House Sprite/Timer/UI consumers, never default success.
struct HouseReturnNpcPorts {
  std::function<bool(upstream::FieldObjectId,upstream::FieldNpcContext&,std::string&)> context;
  std::function<bool(upstream::FieldObjectId,upstream::FieldNpcPresentation,
      const upstream::FieldNpcDescriptor&,const upstream::FieldNpcInstance&,std::string&)> sprite;
  // ReturnDirection uses the retained ONE SceneTreeTimer list and its real
  // waiter receipt; the other operations address the actual WanderTimer node.
  std::function<bool(upstream::FieldObjectId,upstream::FieldObjectId,
      upstream::FieldNpcTimer,double,uint64_t,std::string&)> timer;
  // Connect the actual source FlagsChanged/tree-exiting/visibility receivers;
  // packed Scene connections remain with their reviewed native resource owner.
  std::function<bool(upstream::FieldObjectId,std::string&)> connect_source;
  std::function<bool(std::string&)> close_commands;
  std::function<bool(upstream::FieldObjectId,bool,std::string&)> telepathy;
  // Proves a real source call or real Timer/Area/Visibility signal receiver,
  // including deferred provenance where no emission frame remains open.
  std::function<bool(const upstream::FieldDeferredMessage&,
      const upstream::FieldNpcWorldCallback&,std::string&)> admit_callback;
  std::function<bool(upstream::FieldObjectId,bool&,std::string&)> party_player;
  std::function<bool(upstream::FieldObjectId,bool&,std::string&)> persistent;
  bool actual_debug_build=false;
};
struct HouseReturnNpcInput {
  const upstream::HouseReturnSources *sources=nullptr;
  const upstream::FieldNpcData *data=nullptr;
  const upstream::FieldNpcWorldData *world_data=nullptr;
  const upstream::FieldDoorData *doors=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FieldGlobalRegistry *registry=nullptr;
  upstream::FieldObjectSignals *signals=nullptr;
  upstream::FieldGeometrySpace *space=nullptr;
  HouseReturnKinematicNative *kinematic=nullptr;
  HouseReturnGeometryNative *geometry=nullptr;
  HouseReturnRayNative *rays=nullptr;
  upstream::FieldGlobalConstructorRuntime *global=nullptr;
  PodunkGlobalDataHost *characters=nullptr;
  upstream::SourceRandom *random=nullptr;
  std::vector<uint32_t> *uid_ledger=nullptr;
  upstream::FreshHouseState *house=nullptr;
  upstream::HouseView text{};
  PodunkMickSession *session=nullptr;
  HouseReturnDialogue *dialogue=nullptr;
  HouseReturnDialogueNativeOwner *dialogue_native=nullptr;
  upstream::FieldObjectId player=0;
  HouseReturnNpcPorts ports;
};
// npc.gd only. Kinematic/Area/Ray/Timer/Sprite owners keep their actual native
// construction and notifications. No new VM, ObjectIDs, timer list or clock.
// Fixed address: FieldNpcRuntime callbacks borrow this owner until real delete.
class HouseReturnNpcRuntime final {
public:
  HouseReturnNpcRuntime()=default;
  HouseReturnNpcRuntime(const HouseReturnNpcRuntime&)=delete;
  HouseReturnNpcRuntime&operator=(const HouseReturnNpcRuntime&)=delete;
  HouseReturnNpcRuntime(HouseReturnNpcRuntime&&)=delete;
  HouseReturnNpcRuntime&operator=(HouseReturnNpcRuntime&&)=delete;
  bool prepare(HouseReturnNpcInput,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  // Native body allocation must already have run. This attaches the SAME
  // source core to that body; it grants no Enter/Ready.
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
      const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  bool source_phase(upstream::FieldObjectId,upstream::FieldTreePhase,float actual_delta,
      bool tree_paused,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&)const;
  bool handles_callback(const upstream::FieldDeferredMessage&)const;
  bool deferred(const upstream::FieldDeferredMessage&,std::string&);
  bool return_direction_timeout(upstream::FieldObjectId,uint64_t,std::string&);
  bool set_talking(upstream::FieldObjectId,bool,std::string&);
  bool stop_interaction(upstream::FieldObjectId,std::string&);
  bool recheck_flags(upstream::FieldObjectId,std::string&);
  bool has_dialog(upstream::FieldObjectId,bool,bool&,std::string&);
  bool interact(upstream::FieldObjectId,bool thoughts,std::string&);
  // After the actual World2D replacement, before its first query. Apply source
  // extended-interaction fields held by the real Ready instance, not defaults.
  bool activate_geometry(std::string&);
  bool release_deleted(upstream::FieldObjectId,std::string&);
  upstream::FieldNpcRuntime&runtime(){return runtime_;}
  const upstream::FieldNpcRuntime&runtime()const{return runtime_;}
  const upstream::FieldNodeTreeRuntime*tree()const{return in_.tree;}
  const upstream::HouseReturnSources*sources()const{return in_.sources;}
  const upstream::FieldGlobalRegistry*registry()const{return in_.registry;}
  const upstream::FreshHouseState*house()const{return in_.house;}
  const HouseReturnDialogueNativeOwner*native_dialogue()const{return in_.dialogue_native;}
  bool source_frame_closed()const{return !callback_depth_&&!source_call_;}
  // Only the live npc.gd invocation can produce this source request receipt.
  bool programme_input(upstream::FieldObjectId,uint32_t,bool,
      upstream::HouseSourceNpcProgramme&,std::string&)const;
private:
  struct Instance {uint32_t source=0;bool bound=false,entered=false;};
  HouseReturnNpcInput in_{};
  upstream::FieldNpcRuntime runtime_;
  std::map<upstream::FieldObjectId,Instance>instances_;
  bool prepared_=false,geometry_active_=false,poisoned_=false;
  size_t callback_depth_=0;
  uint32_t source_call_=0;
  bool thoughts_call_=false;
  bool actual(upstream::FieldObjectId,uint32_t&,std::string&)const;
  bool source(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool borrows(std::string&)const;
  bool result(bool,std::string&);
  bool invoke(upstream::FieldObjectId,bool,const std::function<bool()>&,std::string&);
  const upstream::FieldNpcDescriptor*descriptor(uint32_t)const;
  bool dialogue_row(uint32_t,const upstream::FieldNpcDialogue&)const;
  bool programme(uint32_t,std::string_view,uint32_t&,uint32_t&,std::string&)const;
  bool context(uint32_t,upstream::FieldNpcContext&,std::string&);
  bool present(uint32_t,upstream::FieldNpcPresentation,
      const upstream::FieldNpcDescriptor&,const upstream::FieldNpcInstance&,std::string&);
  bool interaction_shape(uint32_t,const upstream::FieldNpcInstance&,std::string&);
  upstream::FieldNpcHost host();
};
}
