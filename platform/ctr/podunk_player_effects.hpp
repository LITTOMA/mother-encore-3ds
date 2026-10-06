#pragma once
#include "encore/field_native_timer.hpp"
#include "encore/player_effects.hpp"
namespace encore::ctr {
// The process' existing Sprite/ResourceLoader/AnimationPlayer owners. Every
// operation must act on the same native Node state and ObjectDB. There is no
// fallback Sprite copy, synthetic animation_finished, or constant Ready.
class PodunkPlayerEffectOwners {
public:
  virtual ~PodunkPlayerEffectOwners() = default;
  virtual bool load_packed_scene(const upstream::FieldNodeRecipeData &,
                                 const upstream::GlobalYamlValue &,
                                 upstream::FieldObjectId &, std::string &) = 0;
  virtual bool admit_factory(const upstream::FieldNodeRecipeData &,
                             const upstream::GlobalYamlValue &,
                             std::string &) const = 0;
  virtual bool duplicate_sprite(upstream::FieldObjectId,
                                upstream::FieldObjectId &, std::string &) = 0;
  virtual bool copy_sprite(upstream::FieldObjectId, upstream::FieldObjectId,
                           const std::vector<std::string> &, std::string &) = 0;
  virtual bool party_size(size_t &, std::string &) const = 0;
  virtual bool play(upstream::FieldObjectId, std::string_view,
                    std::string &) = 0;
  virtual bool deferred(const upstream::FieldDeferredMessage &,
                        std::string &) = 0;
  virtual bool
  connect_finished(upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, bool,
                   std::function<bool(std::string_view, upstream::FieldObjectId,
                                      std::string &)>,
                   std::string &) = 0;
};
class PodunkPlayerEffectsHost final : public upstream::PlayerEffectsNative {
public:
  bool initialize(std::shared_ptr<const upstream::PlayerEffectsData>,
                  std::shared_ptr<const upstream::PlayerInitializationData>,
                  upstream::FieldGlobalRegistry &, upstream::SourceRandom &,
                  PodunkPlayerEffectOwners &, std::string &);
  // Actual source ResourceLoader phase, prior to onready literal assignment.
  bool load_preloads(std::string &);
  bool ready(upstream::FieldObjectId player, upstream::FieldObjectId creator,
             std::string &);
  bool dynamic_script_ready(upstream::FieldObjectId, std::string &);
  bool timeout(upstream::FieldObjectId, std::string &);
  bool bind_timer(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                  upstream::FieldNodeBinding &, std::string &);
  bool native_timer_ready(upstream::FieldObjectId, std::string &);
  bool timer_process(upstream::FieldObjectId, upstream::FieldTreePhase, float,
                     bool, std::string &);
  bool release_timer(upstream::FieldObjectId, std::string &);
  upstream::PlayerEffectsRuntime &core() { return core_; }
  bool preload(uint32_t, const upstream::FieldNodeRecipeData &,
               const upstream::GlobalYamlValue &, upstream::FieldObjectId &,
               std::string &) override;
  bool instance(uint32_t, upstream::FieldNodeTreeRuntime &,
                upstream::FieldObjectId &, std::string &) override;
  bool duplicate_sprite(upstream::FieldObjectId, upstream::FieldObjectId &,
                        std::string &) override;
  bool copy_sprite(upstream::FieldObjectId, upstream::FieldObjectId,
                   const std::vector<std::string> &, std::string &) override;
  bool global_position(upstream::FieldObjectId, upstream::Vec2 &,
                       std::string &) const override;
  bool set_global_position(upstream::FieldObjectId, upstream::Vec2,
                           std::string &) override;
  bool current_scene(upstream::FieldObjectId &, std::string &) const override;
  bool party_size(size_t &, std::string &) const override;
  bool timer_wait(upstream::FieldObjectId, double, std::string &) override;
  bool timer_start(upstream::FieldObjectId, std::string &) override;
  bool timer_stop(upstream::FieldObjectId, std::string &) override;
  bool animation_play(upstream::FieldObjectId, std::string_view,
                      std::string &) override;
  bool
  connect_finished(upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, std::string_view,
                   upstream::FieldObjectId, bool,
                   std::function<bool(std::string_view, upstream::FieldObjectId,
                                      std::string &)>,
                   std::string &) override;

private:
  std::shared_ptr<const upstream::PlayerEffectsData> data_;
  std::shared_ptr<const upstream::PlayerInitializationData> player_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldNativeTimers *timers_ = nullptr;
  upstream::FieldNativeTimers timers_owner_;
  PodunkPlayerEffectOwners *owners_ = nullptr;
  std::array<upstream::FieldObjectId, 2> resources_{};
  upstream::PlayerEffectsRuntime core_;
};
} // namespace encore::ctr
