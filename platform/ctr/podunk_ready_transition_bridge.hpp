#pragma once
#include "encore/field_npc_world.hpp"
#include "podunk_scene_consumers.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
struct PodunkReadyTransitionInput {
  const upstream::FieldSceneSources *sources = nullptr;
  PodunkHouseContinuation *continuation = nullptr;
  PodunkPlayerHost *player = nullptr;
  const PodunkPlayerSources *player_sources = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  upstream::FieldMapSpace *map = nullptr;
  PodunkSceneNative *native = nullptr;
  upstream::FieldMapGateQuery map_gates;
  std::function<bool(uint32_t, upstream::FieldObjectId &, std::string &)>
      source;
};
// The real Jump RayCast native cache and normal source transition mutations.
// No Player/party query runs while the destination factories are being bound.
class PodunkReadyTransitionBridge final : public PodunkSceneNativeMechanism {
public:
  bool prepare(PodunkReadyTransitionInput, std::string &);
  bool apply(upstream::FieldPlayerTransitionsHost &, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool emission(upstream::FieldObjectId, std::string_view, size_t,
                std::string &) const;

private:
  struct Ray {
    upstream::FieldNpcWorldRay source;
    upstream::FieldObjectId hit = 0;
    bool entered = false, ready = false;
  };
  bool actual(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool context(upstream::FieldTransitionContext &, std::string &);
  bool cached(const upstream::FieldTransitionDescriptor &,
              const upstream::FieldTransitionActor &, uint32_t &,
              std::string &);
  bool command(const upstream::FieldTransitionCommand &, std::string &);
  bool flag(upstream::PlayerMotionField, bool &, std::string &) const;
  bool vector(upstream::PlayerMotionField, upstream::Vec2 &,
              std::string &) const;
  PodunkReadyTransitionInput input_{};
  std::map<upstream::FieldObjectId, Ray> rays_;
  bool applied_ = false;
};
} // namespace encore::ctr
