#pragma once
#include "encore/field_tint.hpp"
#include "encore/house_return_sources.hpp"
#include "house_return_kinematic_native.hpp"
#include "podunk_scene_native.hpp"

namespace encore::ctr {
struct HouseReturnTintInput {
  const upstream::HouseReturnSources *sources=nullptr;
  const upstream::FieldTintData *data=nullptr;
  upstream::FieldNodeTreeRuntime *tree=nullptr;
  upstream::FieldGlobalRegistry *registry=nullptr;
  upstream::FieldObjectSignals *signals=nullptr;
  PodunkSceneNative *canvas=nullptr;
  HouseReturnKinematicNative *kinematic=nullptr;
};
// CharacterTint only. Uses the existing core's ordered target cache and the
// SAME actual ObjectDB signal graph. Canvas self_modulate does not tint children.
// Fixed address: scene dispatch borrows this owner through actual deletion.
class HouseReturnTintRuntime final {
public:
  HouseReturnTintRuntime()=default;
  HouseReturnTintRuntime(const HouseReturnTintRuntime&)=delete;
  HouseReturnTintRuntime&operator=(const HouseReturnTintRuntime&)=delete;
  static bool admit_source(const upstream::FieldTintData&,
      const upstream::HouseReturnSources&,std::string&);
  bool prepare(HouseReturnTintInput,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
      const upstream::FieldIdentity&,std::string&);
  // Source receipt only; caller must first validate the actual native Node.
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool source_phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&)const;
  bool handles_method(const upstream::FieldDeferredMessage&)const;
  bool dispatch(const upstream::FieldDeferredMessage&,std::string&);
  // Actual source caller endpoints; no timer/Ready/input/colour synthesis.
  bool set_tint(upstream::FieldObjectId,upstream::FieldTintColor,std::string&);
  bool connect_tint(upstream::FieldObjectId,upstream::FieldObjectId,std::string&);
  // After actual native/Tree/ObjectDB deletion. Exit retains connections/cache.
  bool release_deleted(upstream::FieldObjectId,std::string&);
  const upstream::FieldTintRuntime&runtime()const{return runtime_;}
private:
  struct Instance {uint32_t source=0;bool bound=false,entered=false;};
  HouseReturnTintInput in_{};
  upstream::FieldTintRuntime runtime_;
  std::map<upstream::FieldObjectId,Instance>instances_;
  // Factory-owned ObjectIDs for nullable lookup/dead-cache proofs, not copies
  // of source fields, target caches, connections or scheduler state.
  std::map<uint32_t,upstream::FieldObjectId>targets_;
  std::vector<upstream::FieldObjectId>emitting_;
  bool prepared_=false,factory_=false,poisoned_=false;
  size_t callback_depth_=0;
  bool borrows(std::string&)const;
  bool actual(upstream::FieldObjectId,uint32_t&,std::string&)const;
  bool source(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool target(upstream::FieldObjectId,const upstream::FieldTintTarget&,std::string&)const;
  bool resolve(uint32_t,const upstream::FieldTintDescriptor&,
      const upstream::FieldTintTarget&,bool&,uint32_t&,std::string&);
  bool self_modulate(uint32_t,upstream::FieldTintColor,std::string&);
  bool reachable(upstream::FieldObjectId,upstream::FieldObjectId,
      std::vector<upstream::FieldObjectId>&,bool&,std::string&)const;
};
} // namespace encore::ctr
