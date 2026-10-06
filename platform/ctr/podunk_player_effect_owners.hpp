#pragma once
#include "encore/field_global_constructor.hpp"
#include "encore/field_object_signals.hpp"
#include "player_effects_renderer.hpp"
#include "podunk_player_animation.hpp"
#include "podunk_player_effects.hpp"
#include "podunk_player_resources.hpp"
#include <tuple>

namespace encore::ctr {
// Owns actual source PackedScenes and their dynamic native effect nodes. The
// existing Tree owns all lifecycle and the existing Animation owner owns time.
class PodunkConcretePlayerEffectOwners final : public PodunkPlayerEffectOwners {
public:
  PodunkConcretePlayerEffectOwners();
  ~PodunkConcretePlayerEffectOwners();
  bool initialize(const upstream::PlayerEffectsData &,
                  const upstream::PlayerInitializationData &,
                  const upstream::PlayerResourcesData &,
                  upstream::FieldGlobalRegistry &,
                  upstream::FieldGlobalConstructorRuntime &,
                  upstream::FieldObjectSignals &, PodunkPlayerResources &,
                  PodunkPlayerAnimation &, std::string &);
  bool bind_scripts(PodunkPlayerEffectsHost &, std::string &);
  bool accepts(const upstream::FieldIdentity &) const;
  const upstream::PlayerEffectsData *data() const { return data_; }
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }
  bool owns(upstream::FieldObjectId) const;
  bool construct(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                 const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool phase(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
             upstream::FieldTreePhase, float delta, bool paused, std::string &);
  bool draw(upstream::FieldObjectId, upstream::Vec2 camera,
            std::string &) const;
  bool release(upstream::FieldObjectId, std::string &);
  bool collect_retired(std::string &);
  bool handles_callback(const upstream::FieldDeferredMessage &) const;
  bool signal_declared(upstream::FieldObjectId, std::string_view,
                       uint32_t &arguments, std::string &) const;
  bool load_packed_scene(const upstream::FieldNodeRecipeData &,
                         const upstream::GlobalYamlValue &,
                         upstream::FieldObjectId &, std::string &) override;
  bool admit_factory(const upstream::FieldNodeRecipeData &,
                     const upstream::GlobalYamlValue &,
                     std::string &) const override;
  bool duplicate_sprite(upstream::FieldObjectId, upstream::FieldObjectId &,
                        std::string &) override;
  bool copy_sprite(upstream::FieldObjectId, upstream::FieldObjectId,
                   const std::vector<std::string> &, std::string &) override;
  bool party_size(size_t &, std::string &) const override;
  bool play(upstream::FieldObjectId, std::string_view, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool
  connect_finished(upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, bool,
                   std::function<bool(std::string_view, upstream::FieldObjectId,
                                      std::string &)>,
                   std::string &) override;

private:
  struct Endpoint;
  struct Instance;
  struct Node {
    uint32_t kind = 0;
    upstream::FieldObjectId root = 0;
    upstream::FieldNodeTreeRuntime *tree = nullptr;
    upstream::FieldNodeDescriptor descriptor;
    bool animation_constructed = false;
  };
  struct Finished {
    upstream::FieldObjectId bind = 0;
    bool one_shot = false;
    std::function<bool(std::string_view, upstream::FieldObjectId,
                       std::string &)>
        call;
  };
  bool live(std::string &) const;
  bool kind(const upstream::FieldNodeRecipeData &,
            const upstream::GlobalYamlValue &, uint32_t &, std::string &) const;
  Node *node(upstream::FieldObjectId, std::string &);
  const Node *node(upstream::FieldObjectId, std::string &) const;
  const upstream::PlayerEffectsData *data_ = nullptr;
  const upstream::PlayerInitializationData *player_ = nullptr;
  const upstream::PlayerResourcesData *resource_data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldGlobalConstructorRuntime *global_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkPlayerResources *resources_ = nullptr;
  PodunkPlayerAnimation *animation_ = nullptr;
  PodunkPlayerEffectsHost *scripts_ = nullptr;
  std::array<upstream::FieldObjectId, 2> scenes_{};
  std::map<upstream::FieldObjectId, Node> nodes_;
  std::map<std::pair<upstream::FieldNodeTreeRuntime *, uint32_t>,
           upstream::FieldObjectId>
      constructing_;
  std::map<upstream::FieldObjectId, std::unique_ptr<Instance>> instances_;
  std::map<
      std::tuple<upstream::FieldObjectId, std::string, upstream::FieldObjectId>,
      Finished>
      finished_;
  std::vector<upstream::FieldObjectId> retired_, retired_roots_;
};
} // namespace encore::ctr
