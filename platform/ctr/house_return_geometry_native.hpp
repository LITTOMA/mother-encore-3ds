#pragma once
#include "encore/house_return_sources.hpp"
#include "podunk_player_physics_world.hpp"
#include <set>

namespace encore::ctr {
// Owns only these native subclasses. Script owners, NPC KinematicBody2D and
// Canvas/TileMap owners execute their own construction and notifications.
class HouseReturnGeometryNative {
public:
  using SourceDispatch=std::function<bool(const upstream::FieldDeferredMessage&,std::string&)>;
  bool prepare(const upstream::HouseReturnSources&,upstream::FieldNodeTreeRuntime&,
      upstream::FieldGlobalRegistry&,upstream::FieldObjectSignals&,
      upstream::FieldGeometrySpace&,PodunkPlayerPhysicsWorld&,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
      const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  // Called by the actual other native ancestor's transform/deletion callback.
  // These observations do not bind it or supply its Enter/Ready/Exit.
  bool observe_transform(upstream::FieldObjectId,std::string&);
  bool observe_deleted(upstream::FieldObjectId,std::string&);
  bool replacement_updates(std::vector<upstream::FieldGeometryNodeUpdate>&,std::string&);
  // The target keeper retains the sources, new Tree and already emptied old
  // Tree object for this owner and PhysicsWorld receipt's entire lifetime.
  bool bind_replaced_space(const upstream::FieldNodeTreeRuntime& old_tree,
      upstream::FieldObjectId old_root,upstream::FieldObjectId house_root,std::string&);
  bool activate_monitors(std::string&);
  bool physics_admitted(std::string&)const;
  bool set_disabled(upstream::FieldObjectId,bool,std::string&);
  bool shape_disabled(upstream::FieldObjectId,bool&,std::string&)const;
  bool owner_rid(upstream::FieldObjectId,upstream::FieldPhysicsRid&,std::string&)const;
  bool set_collision(upstream::FieldObjectId,uint32_t layer,uint32_t mask,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,std::string&)const;
  // A Room/Ladder concrete owner supplies its real method consumer. Presence
  // grants no constructor/lifecycle admission; calls require an actual Area
  // emission frame, same ObjectDB receiver and the checked source script.
  bool bind_script_receiver(upstream::FieldObjectId,SourceDispatch,std::string&);
  bool dispatch_source(const upstream::FieldDeferredMessage&,std::string&);
  bool source_object(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool release_deleted(upstream::FieldObjectId,std::string&);
private:
  enum class Kind {StaticBody,Area,Shape};
  struct Instance {
    Kind kind=Kind::Shape;
    uint32_t source=0,node=UINT32_MAX,owner=UINT32_MAX,shape=UINT32_MAX;
    upstream::FieldObjectId shape_parent=0;
    upstream::FieldGeometryOwner properties{};
    upstream::FieldGeometryTransform cached_shape_transform{};
    upstream::FieldPhysicsRid rid{};
    bool bound=false,entered=false,ready=false,deleted=false,disabled=false,monitored=false;
  };
  struct Receiver {std::array<uint8_t,32>sha{};SourceDispatch dispatch;};
  bool actual(upstream::FieldObjectId,const upstream::FieldNodeDescriptor*&,
      const upstream::FieldNodeState*&,std::string&)const;
  bool synchronize(upstream::FieldObjectId,Instance&,std::string&);
  bool source_geometry(uint32_t,uint32_t&,std::string&)const;
  bool live_space(std::string&)const;
  const upstream::HouseReturnSources*sources_=nullptr;
  upstream::FieldNodeTreeRuntime*tree_=nullptr;
  upstream::FieldGlobalRegistry*registry_=nullptr;
  upstream::FieldObjectSignals*signals_=nullptr;
  upstream::FieldGeometrySpace*space_=nullptr;
  PodunkPlayerPhysicsWorld*world_=nullptr;
  const upstream::FieldNodeTreeRuntime*old_tree_=nullptr;
  upstream::FieldObjectId old_root_=0,root_=0;
  std::map<upstream::FieldObjectId,Instance>instances_;
  std::map<uint32_t,upstream::FieldObjectId>source_objects_,deleted_nodes_;
  std::map<upstream::FieldObjectId,Receiver>receivers_;
  bool finished_=false,replaced_=false,monitors_active_=false,poisoned_=false;
};
}
