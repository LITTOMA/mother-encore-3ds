#pragma once
#include "encore/battle_entry.hpp"
#include "encore/battle_outcome.hpp"
#include "encore/field_equipment_menu.hpp"
#include "encore/house_presentation.hpp"
#include "encore/house_runtime.hpp"
#include "encore/house_ui_continuation.hpp"
#include "podunk_native_root.hpp"
#include "podunk_dialogue_host.hpp"
#include "house_ui_reentry.hpp"
#include "encore/field_global_constructor.hpp"
namespace encore::upstream { class FieldGlobalDataRuntime; }
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
struct HouseUiDialogueState {
  upstream::FieldObjectId stable_canvas=0, current_dialogue=0, talker=0;
  std::vector<upstream::FieldObjectId> stack;
  bool current_inside=false, current_ready=false, cutscene=false;
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
  // Identity-preserving compatibility check only. Scene replacement requires
  // the checked source Door reentry ticket below.
  bool rebind_scene(const upstream::OpeningWorld &,
                    const upstream::HouseRuntime &,
                    const upstream::HousePresentation &, std::string &);
  // Read-only preflight and pointer-only commit at the real source Door
  // deferred boundary after persistent detach/destination instance, before
  // old House destruction and destination mapped Ready.
  // Tickets do not grant House/UI Ready and are consumed only on success.
  bool prepare_house_reentry(const HouseUiReentryInput &,
                            HouseUiReentryTicket &, std::string &) const;
  bool commit_house_reentry(HouseUiReentryTicket &, std::string &);
  bool source_stack_empty(upstream::FieldObjectId,bool&,std::string&)const;
  bool source_clear_on_screen_enemies(std::string &);
  bool source_update_key_indicator(const upstream::FieldGlobalDataRuntime &,
                                   std::string_view region,std::string &);
  // Borrow the actual source global body before reifying its continued Canvas.
  bool bind_source_global(upstream::FieldGlobalConstructorRuntime &,std::string &);
  bool source_continuation_canvas_admitted(std::string &) const override;
  bool bind_dialogue_sources(const upstream::FieldDialogueLifecycleData &,
                            const upstream::FieldNodeRecipeData &,
                            PodunkDialogueRootScript &,upstream::HousePresentation &,std::string &);
  bool source_dialogue_parent(upstream::FieldObjectId,std::string &) const;
  bool source_add_ui(upstream::FieldObjectId,bool add_child,std::string &);
  bool source_remove_ui(upstream::FieldObjectId,std::string &);
  bool source_dialogue_step(const upstream::FieldDialogueStep &,
                            upstream::FieldObjectId,std::string &);
  bool source_dialogue_global_step(const upstream::FieldDialogueStep &,
                                   upstream::FieldObjectId,upstream::FieldObjectId talker,std::string &);
  bool source_dialogue_state(HouseUiDialogueState &,std::string &) const;
  bool source_current_talker(upstream::FieldObjectId &,std::string &) const;
  bool source_close_closed_widget(uint32_t,std::string &);
  bool source_business_closed(std::string &) const;
  bool full_source_ready() const { return false; }

private:
  bool checked_house_reentry(const HouseUiReentryInput &, bool committing,
                             std::string &) const;
  bool checked_reentry_borrowers(const HouseUiReentryInput &, bool rebound,
                                HouseUiReentryBorrowState &, std::string &) const;
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool models(std::string &) const;
  bool dialogue_object(upstream::FieldObjectId,bool entered,std::string &) const;
  bool stack_state(std::string &) const;
  bool checked_dialogue_step(const upstream::FieldDialogueStep &,std::string &) const;
  upstream::FieldGlobalConstructorRuntime *global_=nullptr;
  const upstream::FieldDialogueLifecycleData *dialogue_life_=nullptr;
  const upstream::FieldNodeRecipeData *dialogue_recipe_=nullptr;
  PodunkDialogueRootScript *dialogue_script_=nullptr;
  upstream::FieldObjectId stable_canvas_=0,current_dialogue_=0;
  std::vector<upstream::FieldObjectId> ui_stack_;
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
  bool key_open_=false,cash_open_=false,party_showing_=false,business_imported_=false;
  upstream::FieldObjectId party_info_timer_=0;
  std::vector<upstream::FieldObjectId> on_screen_enemies_;
  uint32_t story_generation_ = 0;
  bool dialogue_at_source_event_ = false, story_at_source_event_ = false;
  size_t outcome_cursor_ = 0;
  bool reentry_committing_ = false, reentry_failed_ = false;
};
} // namespace encore::ctr
