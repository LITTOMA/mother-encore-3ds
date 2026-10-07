#pragma once
#include "podunk_dialogue_lifecycle_ports.hpp"
#include "podunk_house_door_fade.hpp"
#include "podunk_named_sfx.hpp"
#include "encore/blackbars.hpp"
#include "encore/field_scene_data.hpp"

namespace encore::ctr {
struct PodunkDialogueBusinessInput {
  const upstream::HouseUiContinuationData *data = nullptr;
  const upstream::FieldDialogueLifecycleData *life = nullptr;
  const upstream::FieldSceneData *scene = nullptr;
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  upstream::FieldGlobalConstructorRuntime *global = nullptr;
  upstream::FieldGlobalDataRuntime *global_data = nullptr;
  HouseUiContinuation *ui = nullptr;
  PodunkDialogueRootOwner *root_script = nullptr;
  PodunkDialogueHost *dialogue = nullptr;
  PodunkPlayerHost *player = nullptr;
  upstream::Blackbars *bars = nullptr;
  PodunkHouseDoorFade *fade = nullptr;
  PodunkNamedSfx *sounds = nullptr;
};
// Bounded continuation of the actual imported closed House widgets. Their
// open branches remain explicit failures. Blackbars and Fade borrow the same
// rendered bodies; this owner does not create another UI tree or frame clock.
class PodunkDialogueBusinessNative final : public PodunkDialogueBusiness {
public:
  bool prepare(PodunkDialogueBusinessInput, std::string &);
  bool idle(uint64_t epoch, float delta, std::string &);
  const upstream::FieldGlobalRegistry *registry() const override {
    return in_.registry;
  }
  bool observe(upstream::FieldObjectId, upstream::FieldDialogueObservation &,
               std::string &) const override;
  bool admit(const upstream::FieldDialogueStep &,
             const upstream::FieldProgrammeContext &, std::string &) const override;
  bool manager(const upstream::FieldDialogueStep &, upstream::FieldObjectId,
               std::string &) override;
  bool global(const upstream::FieldDialogueStep &, upstream::FieldObjectId,
              upstream::FieldObjectId, std::string &) override;
  bool close_sound(std::string &) override;
  bool restore_telepathy(std::string &) override;
  bool set_telepathy_effect(bool, upstream::FieldObjectId, std::string &);
  bool return_camera(bool offset, double duration, std::string &) override;
private:
  bool checked_step(const upstream::FieldDialogueStep &, std::string &) const;
  bool current_camera(upstream::FieldGameCameraRuntime *&, uint32_t &,
                      std::string &) const;
  PodunkDialogueBusinessInput in_{};
};
} // namespace encore::ctr
