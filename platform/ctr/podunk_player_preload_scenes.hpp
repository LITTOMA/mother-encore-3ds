#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/player_preload_scenes.hpp"
namespace encore::ctr {
// Complete immutable source PackedScene resource. No instance or Ready proof.
class PodunkPlayerPreloadSceneResource final
    : public upstream::FieldGlobalSourceResource {
public:
  const char *resource_class() const override { return "PackedScene"; }
  upstream::FieldGlobalExternalBinding binding() const override {
    return binding_;
  }
  const upstream::PlayerPreloadScene &scene() const { return *scene_; }
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }
  bool state(upstream::FieldGlobalExternalState &,
             std::string &) const override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool persist_append(upstream::FieldObjectId, std::string &) override;
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &) override;

private:
  friend class PodunkPlayerPreloadScenes;
  upstream::FieldGlobalExternalBinding binding_{};
  std::shared_ptr<const upstream::PlayerPreloadScene> scene_;
  const upstream::FieldGlobalRegistry *registry_ = nullptr;
};
class PodunkPlayerPreloadScenes {
public:
  bool prepare(const upstream::PlayerPreloadScenesData &,
               const upstream::PlayerInitializationData &,
               upstream::FieldGlobalRegistry &, std::string &);
  bool resolve(const upstream::GlobalYamlValue &, upstream::FieldObjectId &,
               std::string &);
  const upstream::PlayerPreloadScenesData *data() const { return data_; }
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }

private:
  const upstream::PlayerPreloadScenesData *data_ = nullptr;
  const upstream::PlayerInitializationData *player_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> source_ir_{};
  std::map<std::string, const PodunkPlayerPreloadSceneResource *> owners_;
};
} // namespace encore::ctr
