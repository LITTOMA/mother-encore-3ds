#pragma once
#include "encore/battle_entry.hpp"
#include "encore/battle_outcome.hpp"
#include "encore/field_equipment_menu.hpp"
#include "encore/house_presentation.hpp"
#include "encore/house_runtime.hpp"
#include "encore/house_ui_continuation.hpp"
#include "podunk_native_root.hpp"
namespace encore::ctr {
// Borrow the running House session. These are its existing objects, not copies.
struct HouseUiContinuationSources {
  const upstream::BattleEntry *battle = nullptr;
  const upstream::BattleOutcome *outcome = nullptr;
  const upstream::FieldEquipmentMenu *commands = nullptr;
  const upstream::OpeningWorld *world = nullptr;
  const upstream::HouseRuntime *house = nullptr;
  const upstream::HousePresentation *dialogue = nullptr;
};
class HouseUiContinuation final : public upstream::FieldGlobalExternalObject,
                                  public PodunkExternalNodeLifecycle {
public:
  bool initialize(std::shared_ptr<const upstream::HouseUiContinuationData>,
                  const upstream::FieldGlobalRegistryData &,
                  upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
                  upstream::FieldObjectSignals &,
                  upstream::FieldGlobalExternalBinding,
                  HouseUiContinuationSources, std::string &);
  upstream::FieldGlobalExternalBinding binding() const override {
    return binding_;
  }
  bool state(upstream::FieldGlobalExternalState &,
             std::string &) const override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool persist_append(upstream::FieldObjectId, std::string &) override;
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &) override;
  bool stage_parent(upstream::FieldObjectId, std::string &) override;
  bool native_notification(upstream::FieldTreePhase, std::string &) override;
  bool enter(upstream::FieldObjectId, std::string &) override;
  bool ready(std::string &) override;
  bool exit(std::string &) override;
  bool adopt_continuation_ready(std::string &) override;
  bool continuation_native_ready() const override { return native_ready_; }
  bool source_is_in_battle(upstream::FieldObjectId, bool &,
                           std::string &) const;
  bool source_is_pause_menu_active(upstream::FieldObjectId, bool &,
                                   std::string &) const;
  bool source_is_in_cutscene(upstream::FieldObjectId, bool &,
                             std::string &) const;
  bool source_set_cutscene(upstream::FieldObjectId, bool, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  // Invoke immediately before the existing entry commits, at source
  // ov_to_battle. It observes actual current Entry/Outcome, never constructs a
  // second battle.
  bool battle_will_begin(std::string &);
  bool battle_entry_committed(std::string &);
  // Call at the existing Outcome event boundary. Previous session events are
  // retained as observed; only new real ReturnStarted events emit battle_to_ov.
  bool observe_battle_events(std::string &);
  // Rebind only after actual destination ownership has replaced the House
  // scene. The same UI, Battle, Menu, ObjectDB and signal connections survive.
  bool rebind_scene(const upstream::OpeningWorld &,
                    const upstream::HouseRuntime &,
                    const upstream::HousePresentation &, std::string &);
  bool full_source_ready() const { return false; }

private:
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool models(std::string &) const;
  std::shared_ptr<const upstream::HouseUiContinuationData> data_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_{};
  HouseUiContinuationSources sources_{};
  upstream::FieldObjectId parent_ = 0;
  bool inside_ = false, cutscene_ = false;
  bool source_cutscene_observed_ = false, awaiting_entry_ = false;
  bool native_ready_ = false;
  uint32_t story_generation_ = 0;
  bool dialogue_at_source_event_ = false, story_at_source_event_ = false;
  size_t outcome_cursor_ = 0;
};
} // namespace encore::ctr
