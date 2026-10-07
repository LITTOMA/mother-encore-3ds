#pragma once
#include "house_return_geometry_native.hpp"
#include "encore/field_npc.hpp"
#include "encore/movement.hpp"
#include "encore/podunk_player_kinematic.hpp"

namespace encore::ctr {
// Native KinematicBody2D only. The same checked House shape owner retains the
// CollisionShape2D; a concrete FieldNpcRuntime must execute npc.gd separately.
class HouseReturnKinematicNative {
public:
  struct AreaBodyPair {
    upstream::FieldObjectId area=0,body=0;
    upstream::FieldPhysicsRid body_rid{};
    uint32_t body_shape=0,area_shape=0;
  };
  bool prepare(const upstream::HouseReturnSources&,upstream::FieldNodeTreeRuntime&,
      upstream::FieldGlobalRegistry&,upstream::FieldObjectSignals&,
      upstream::FieldGeometrySpace&,PodunkPlayerPhysicsWorld&,
      HouseReturnGeometryNative&,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
      const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  // Forward the actual child's ChildEntered/ChildExiting event when its parent
  // is one of these bodies. Shape/Area native phases still belong to their owner.
  bool child_phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&)const;
  // The actual source runtime, not a callback-presence receipt. This validates
  // House scene/script fingerprints and its native geometry before any Ready.
  bool bind_source(upstream::FieldObjectId,upstream::FieldNpcRuntime&,
      uint32_t npc_id,std::string&);
  // Overlay these source-ID rows in the complete geometry replacement snapshot.
  bool replacement_updates(std::vector<upstream::FieldGeometryNodeUpdate>&,std::string&);
  bool bind_replaced_space(const upstream::FieldNodeTreeRuntime&old_tree,
      upstream::FieldObjectId old_root,upstream::FieldObjectId house_root,std::string&);
  // Requires real source Ready and completed native/tree/shape lifecycle.
  bool activate_sources(std::string&);
  bool begin_physics(uint64_t actual_epoch,float actual_delta,std::string&);
  bool move_and_slide(upstream::FieldObjectId,upstream::Vec2 velocity,float actual_delta,
      upstream::Vec2&world_position,upstream::Vec2&returned_velocity,std::string&);
  bool set_world_position(upstream::FieldObjectId,upstream::Vec2,std::string&);
  bool set_collision(upstream::FieldObjectId,uint32_t layer,uint32_t mask,std::string&);
  bool owner_rid(upstream::FieldObjectId,upstream::FieldPhysicsRid&,std::string&)const;
  bool body_shapes(upstream::FieldObjectId,std::vector<upstream::FieldGeometryContact>&,
      std::string&)const;
  // Read-only pair contribution for the ONE PhysicsWorld collector. Does not
  // emit, flush, hold monitor state, advance a clock or call physics_step.
  bool sample_area_pairs(std::vector<AreaBodyPair>&,std::string&)const;
  bool source_object(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool release_deleted(upstream::FieldObjectId,std::string&);
private:
  struct Instance {
    uint32_t source=0,node=0,owner=0,shape=0,npc_id=0;
    upstream::FieldGeometryOwner properties{};
    upstream::FieldPhysicsRid rid{};
    upstream::FieldNpcRuntime*script=nullptr;
    const upstream::FieldNpcData*script_data=nullptr;
    bool bound=false,entered=false,ready=false,deleted=false,active=false;
  };
  bool actual(upstream::FieldObjectId,const upstream::FieldNodeDescriptor*&,
      const upstream::FieldNodeState*&,std::string&)const;
  bool source_receipt(upstream::FieldObjectId,const Instance&,bool ready,
      std::string&)const;
  bool live_space(std::string&)const;
  bool admitted(upstream::FieldObjectId,const Instance&,std::string&)const;
  bool synchronize(upstream::FieldObjectId,Instance&,std::string&);
  upstream::FieldGeometryContact contact(upstream::FieldObjectId,const Instance&)const;
  const upstream::HouseReturnSources*sources_=nullptr;
  upstream::FieldNodeTreeRuntime*tree_=nullptr;
  upstream::FieldGlobalRegistry*registry_=nullptr;
  upstream::FieldObjectSignals*signals_=nullptr;
  upstream::FieldGeometrySpace*space_=nullptr;
  PodunkPlayerPhysicsWorld*world_=nullptr;
  HouseReturnGeometryNative*geometry_=nullptr;
  const upstream::FieldNodeTreeRuntime*old_tree_=nullptr;
  upstream::FieldObjectId old_root_=0,root_=0;
  std::map<upstream::FieldObjectId,Instance>instances_;
  std::map<uint32_t,upstream::FieldObjectId>source_objects_;
  uint64_t epoch_=0;float delta_=0;
  bool finished_=false,replaced_=false,activated_=false,poisoned_=false;
};
}
