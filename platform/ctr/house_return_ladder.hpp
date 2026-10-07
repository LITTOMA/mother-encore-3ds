#pragma once
#include "encore/house_return_ladder.hpp"
#include "encore/field_object_signals.hpp"
#include "podunk_player_host.hpp"
namespace encore::ctr {
// Two actual PackedScene connections + exact PartyObject callbacks. Native
// Area lifecycle/monitoring stays with the real target world owner.
class HouseReturnLadder {
public:
 bool prepare(const upstream::HouseReturnLadderData&,
              const upstream::FieldNodeTreeData&,
              const upstream::FieldGeometryView&,const PodunkPlayerSources&,
              upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
              upstream::FieldObjectSignals&,upstream::FieldGeometrySpace&,
              PodunkPlayerHost&,std::string&);
 bool construct(upstream::FieldObjectId,std::string&);
 // Call after complete PackedScene child/owner construction, before Enter.
 // These persisted connections are not an invented script _init/_ready.
 bool connect_source(std::string&);
 // Call after actual same-space House world replacement; not during staging.
 bool bind_geometry(std::string&);
 bool dispatch(const upstream::FieldDeferredMessage&,std::string&);
 bool source_constructed(upstream::FieldObjectId)const;
 // No script_phase(Ready) exists: the reviewed Ladder defines no lifecycle
 // methods. The actual native Area owner must still execute every phase.
 bool release_deleted(std::string&);
private:
 bool actual(std::string&)const;
 const upstream::HouseReturnLadderData*data_=nullptr;
 const upstream::FieldNodeTreeData*nodes_=nullptr;
 const upstream::FieldGeometryView*geometry_=nullptr;
 upstream::FieldNodeTreeRuntime*tree_=nullptr;
 upstream::FieldGlobalRegistry*registry_=nullptr;
 upstream::FieldObjectSignals*signals_=nullptr;
 upstream::FieldGeometrySpace*space_=nullptr;
 PodunkPlayerHost*player_=nullptr;
 std::array<uint8_t,32>ir_{};
 upstream::FieldObjectId object_=0;
 bool connected_=false,geometry_bound_=false,poisoned_=false;
};
}
