#pragma once
#include "house_return_kinematic_native.hpp"
#include "encore/field_npc_world.hpp"

namespace encore::ctr {
// Native RayCast2D cache and source methods, on the existing House World2D.
// NPC source lifecycle, physics frame dispatch and MessageQueue remain owned
// by their actual existing consumers. No ray query is run by a cache getter.
class HouseReturnRayNative {
public:
  struct Cache {
    bool collided=false;
    upstream::FieldObjectId against=0;
    uint32_t shape=0;
    upstream::Vec2 point{},normal{};
  };
  bool prepare(const upstream::HouseReturnSources&,const upstream::FieldNpcWorldData&,
      const upstream::FieldNpcData&,upstream::FieldNodeTreeRuntime&,
      upstream::FieldGlobalRegistry&,upstream::FieldObjectSignals&,
      upstream::FieldGeometrySpace&,PodunkPlayerPhysicsWorld&,
      HouseReturnKinematicNative&,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  const upstream::HouseReturnSources*sources()const{return sources_;}
  const upstream::FieldNpcWorldData*data()const{return data_;}
  const upstream::FieldNpcData*npc_data()const{return npcs_;}
  const upstream::FieldNodeTreeRuntime*tree()const{return tree_;}
  const upstream::FieldGlobalRegistry*registry()const{return registry_;}
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
      const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,bool tree_paused,std::string&);
  // Call after Geometry/Kinematic same-space binding, before any ray update.
  bool bind_replaced_space(const upstream::FieldNodeTreeRuntime&old_tree,
      upstream::FieldObjectId old_root,upstream::FieldObjectId house_root,std::string&);
  bool set_enabled(upstream::FieldObjectId,bool,std::string&);
  bool set_cast_to(upstream::FieldObjectId,upstream::Vec2,std::string&);
  bool set_collision_mask(upstream::FieldObjectId,uint32_t,std::string&);
  bool set_exclude_parent(upstream::FieldObjectId,bool,std::string&);
  bool set_collide_with(upstream::FieldObjectId,bool bodies,bool areas,std::string&);
  bool collider(upstream::FieldObjectId,upstream::FieldObjectId&,std::string&)const;
  bool cache(upstream::FieldObjectId,Cache&,std::string&)const;
  bool force_update(upstream::FieldObjectId,std::string&);
  bool deferred(const upstream::FieldDeferredMessage&,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&)const;
  bool source_object(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool release_deleted(upstream::FieldObjectId,std::string&);
private:
  struct Instance {
    upstream::FieldNpcWorldRay source{};
    upstream::FieldObjectId parent=0;
    upstream::FieldPhysicsRid parent_rid{};
    Cache cached{};
    bool bound=false,entered=false,ready=false,deleted=false;
  };
  bool actual(upstream::FieldObjectId,const upstream::FieldNodeDescriptor*&,
      const upstream::FieldNodeState*&,std::string&)const;
  bool live_space(std::string&)const;
  bool parent_receipt(upstream::FieldObjectId,Instance&,std::string&);
  bool update(upstream::FieldObjectId,Instance&,std::string&);
  const upstream::HouseReturnSources*sources_=nullptr;
  const upstream::FieldNpcWorldData*data_=nullptr;
  const upstream::FieldNpcData*npcs_=nullptr;
  upstream::FieldNodeTreeRuntime*tree_=nullptr;
  upstream::FieldGlobalRegistry*registry_=nullptr;
  upstream::FieldObjectSignals*signals_=nullptr;
  upstream::FieldGeometrySpace*space_=nullptr;
  PodunkPlayerPhysicsWorld*world_=nullptr;
  HouseReturnKinematicNative*kinematic_=nullptr;
  const upstream::FieldNodeTreeRuntime*old_tree_=nullptr;
  upstream::FieldObjectId old_root_=0,root_=0;
  std::map<upstream::FieldObjectId,Instance>instances_;
  std::map<uint32_t,upstream::FieldObjectId>source_objects_;
  bool finished_=false,replaced_=false,poisoned_=false;
};
}
