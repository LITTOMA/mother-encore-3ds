#pragma once
#include "encore/field_npc_world.hpp"
#include "encore/field_sprite_bridge.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
struct PodunkNpcWorldPorts {
  // Actual UI/Programme owners provide these source methods. World operations
  // below are owned here and never forwarded to fabricated collision ports.
  upstream::FieldNpcHost source;
  bool actual_debug_build=false;
  upstream::PlayerInitializationBody *player = nullptr;
  upstream::FieldGlobalConstructorRuntime *global = nullptr;
  PodunkGlobalDataHost *characters = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  upstream::FieldMapGateQuery map_gates;
  // Source SceneTree.create_timer waiter: no private countdown/clock here.
  std::function<bool(upstream::FieldObjectId, double, uint64_t, std::string &)>
      return_timer;
  std::function<bool(upstream::FieldObjectId, bool &, std::string &)>
      persistent;
};
class PodunkSceneNpcWorld final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::FieldNpcWorldData &,
               const upstream::FieldNodeTreeData &,
               const upstream::FieldNpcData &, upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldGeometrySpace &,
               upstream::FieldMapSpace &, upstream::FieldNpcRuntime &,
               upstream::FieldSpriteRuntime &, PodunkSceneNative &,
               PodunkSceneTimers &, PodunkNpcWorldPorts, std::string &);
  upstream::FieldNpcHost source_host();
  bool activate_geometry(upstream::FieldSceneHost &, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  // Source Ready precedes native Ready on the same body; children already ran.
  bool native_entered(upstream::FieldObjectId, std::string &) const;
  bool native_ready(upstream::FieldObjectId, std::string &) const;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool handles_callback(const upstream::FieldDeferredMessage &) const;
  bool return_direction_timeout(uint32_t, uint64_t, std::string &);
  bool ray_enabled(upstream::FieldObjectId, bool, std::string &);
  bool ray_cast(upstream::FieldObjectId, upstream::Vec2, std::string &);
  bool ray_collider(upstream::FieldObjectId, upstream::FieldObjectId &,
                    std::string &) const;
  bool publish_position(uint32_t, upstream::Vec2, std::string &);

private:
  struct Native {
    upstream::FieldObjectId object = 0;
    uint32_t source = 0;
    upstream::FieldNpcWorldRay ray{};
    upstream::FieldObjectId hit = 0;
    bool is_ray = false, entered = false, ready = false, disabled = false;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool source(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool npc_present(uint32_t, upstream::FieldNpcPresentation,
                   const upstream::FieldNpcDescriptor &,
                   const upstream::FieldNpcInstance &, std::string &);
  bool timer(uint32_t, upstream::FieldNpcTimer, double, uint64_t,
             std::string &);
  bool move(uint32_t, upstream::Vec2, float, upstream::Vec2 &, upstream::Vec2 &,
            std::string &);
  bool disabled(uint32_t, bool, std::string &);
  bool callback(uint32_t, uint32_t, const upstream::FieldDeferredMessage &,
                std::string &);
  const upstream::FieldNpcWorldData *data_ = nullptr;
  const upstream::FieldNodeTreeData *nodes_ = nullptr;
  const upstream::FieldNpcData *npcs_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldGeometrySpace *space_ = nullptr;
  upstream::FieldMapSpace *map_ = nullptr;
  upstream::FieldNpcRuntime *runtime_ = nullptr;
  upstream::FieldSpriteRuntime *sprites_ = nullptr;
  PodunkSceneNative *native_ = nullptr;
  PodunkSceneTimers *timers_ = nullptr;
  PodunkNpcWorldPorts ports_;
  std::map<upstream::FieldObjectId, Native> instances_;
  bool geometry_active_ = false;
};
} // namespace encore::ctr
