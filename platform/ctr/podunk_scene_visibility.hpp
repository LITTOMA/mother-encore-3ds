#pragma once
#include "encore/field_visibility.hpp"
#include "podunk_native_root.hpp"
#include "podunk_scene_loop.hpp"
#include <set>
namespace encore::upstream {class HouseReturnSources;}
namespace encore::ctr {
class HouseReturnNpcRuntime;
// Borrow the unique native AnimationPlayer state. Enabler set_active does not
// stop/reset/seek an animation or introduce a second animation clock.
struct PodunkVisibilityNativeOwners {
  std::function<bool(upstream::FieldObjectId,bool,std::string&)>animation_active;
};
class PodunkSceneVisibility final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::FieldVisibilityData&,
               const upstream::FieldNodeTreeData&,
               upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
               upstream::FieldObjectSignals&,PodunkNativeRoot&,
               PodunkSceneScripts&,upstream::FieldSceneConsumers,
               PodunkVisibilityNativeOwners,std::string&);
  // Exact returned House closure. The existing spatial/enabler algorithm and
  // same native Viewport are retained; no Podunk script router is borrowed.
  // If the source Enabler tracks AnimationPlayers, their actual native owner
  // must own those same objects and consume set_active, without a new clock.
  bool prepare_house(const upstream::HouseReturnSources&,
               upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
               upstream::FieldObjectSignals&,PodunkNativeRoot&,
               HouseReturnNpcRuntime&,PodunkSceneNativeMechanism*animation_players,
               std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const override;
  bool owns(upstream::FieldObjectId)const override;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                 const upstream::FieldIdentity&,std::string&)override;
  bool bind(upstream::FieldObjectId,upstream::FieldNodeBinding&,std::string&)override;
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,float,bool,bool,
             std::string&)override;
  bool deferred(const upstream::FieldDeferredMessage&,std::string&)override;
  bool release(upstream::FieldObjectId,std::string&)override;
  // Connect original PackedScene callbacks once the actual full factory has
  // allocated its nodes, before entering it. Bus callbacks dispatch through
  // handles_method/deferred in the same actual Registry method router.
  bool finish_factory(std::string&);
  bool handles_method(const upstream::FieldDeferredMessage&)const;
  bool signal_declaration(upstream::FieldObjectId,std::string_view,uint32_t&,
                          std::string&)const;
  bool is_on_screen(upstream::FieldObjectId,bool&,std::string&)const;
  // Invoke exactly at the real Viewport World2D update boundary. This reads
  // its actual current world rectangle; there is no private timer or dt.
  bool update_world(uint64_t actual_frame,std::string&);
  bool remove_viewport(std::string&);
private:
  struct Range {int32_t x0=0,y0=0,x1=-1,y1=-1;};
  struct Instance {
    const upstream::FieldVisibilityRecord*source=nullptr;
    upstream::FieldNodeBinding binding;
    Range cells;upstream::FieldMapRect world_rect{};
    bool inside=false,on_screen=false,enabler_visible=false;
    std::map<upstream::FieldObjectId,uint32_t>tracked;
  };
  bool actual(upstream::FieldObjectId,Instance*&,std::string&);
  bool range(const upstream::FieldMapRect&,Range&,std::string&)const;
  bool world_rect(upstream::FieldObjectId,upstream::FieldMapRect&,std::string&);
  bool change_cells(upstream::FieldObjectId,const Range&,bool,std::string&);
  bool transform_changed(upstream::FieldObjectId,std::string&);
  bool enabler_enter(upstream::FieldObjectId,Instance&,std::string&);
  bool enabler_exit(upstream::FieldObjectId,Instance&,std::string&);
  bool enable(Instance&,bool,std::string&);
  bool change_tracked(const Instance&,upstream::FieldObjectId,uint32_t,bool,
                      std::string&);
  bool screen(upstream::FieldObjectId,bool,std::string&);
  bool source_callback(const upstream::FieldVisibilityConnection&,
                       upstream::FieldObjectId,std::string&);
  bool house_npc_receipt(uint32_t,upstream::FieldObjectId,bool ready,std::string&)const;
  bool disconnect_source(upstream::FieldObjectId,std::string&);
  bool fail(std::string&,const char*);
  const upstream::FieldVisibilityData*data_=nullptr;
  const upstream::FieldNodeTreeData*source_=nullptr;
  upstream::FieldNodeTreeRuntime*tree_=nullptr;
  upstream::FieldGlobalRegistry*registry_=nullptr;
  upstream::FieldObjectSignals*signals_=nullptr;PodunkNativeRoot*viewport_=nullptr;
  PodunkSceneScripts*scripts_=nullptr;upstream::FieldSceneConsumers consumers_;
  const upstream::HouseReturnSources*house_sources_=nullptr;
  HouseReturnNpcRuntime*house_npcs_=nullptr;
  PodunkSceneNativeMechanism*house_animations_=nullptr;
  PodunkVisibilityNativeOwners native_;
  std::map<upstream::FieldObjectId,Instance>instances_;
  std::map<uint32_t,upstream::FieldObjectId>objects_;
  std::map<uint64_t,std::map<upstream::FieldObjectId,uint32_t>>cells_;
  std::set<upstream::FieldObjectId>visible_,parents_;
  std::set<size_t>connections_;
  std::map<std::pair<upstream::FieldObjectId,std::string>,size_t>methods_;
  upstream::FieldMapRect viewport_rect_{};Range viewport_cells_{};
  uint64_t frame_=0;bool frame_seen_=false,viewport_registered_=false,
      changed_=false,finished_=false,updating_=false,failed_=false;
};
}
