#include "house_ui_continuation.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool HouseUiContinuation::initialize(
    std::shared_ptr<const HouseUiContinuationData> d,
    const FieldGlobalRegistryData &ns, FieldGlobalRegistry &r,
    PodunkNativeRoot &root, FieldObjectSignals &bus,
    FieldGlobalExternalBinding b, HouseUiContinuationSources s,
    std::string &e) {
  const auto a =
      std::find_if(ns.autoloads().begin(), ns.autoloads().end(),
                   [&](const auto &v) { return v.id == ns.ui_autoload(); });
  if (data_ || !d || !d->valid() || !ns.valid() || a == ns.autoloads().end() ||
      !b.object || b.family != 0x454e0064 || b.capability != 1 ||
      b.source.role != 3 || b.source.stable_id != a->id ||
      !same(b.source.identity, ns.identity()) ||
      d->identity().upstream_commit != ns.identity().upstream_commit ||
      b.source.name != a->name || b.source.native_class != a->native_class ||
      b.source.source != a->path || b.source.script != a->script ||
      b.source.source_sha != a->source_sha ||
      b.source.script_sha != a->script_sha ||
      b.source.script != d->source_script() ||
      b.source.script_sha != d->identity().source_sha256 ||
      root.kernel_object() != r.kernel() ||
      root.viewport_object() != r.root() || bus.registry() != &r || !s.battle ||
      !s.outcome || !s.commands || !s.world || !s.house || !s.dialogue)
    return fail(e, "House UI continuation actual namespace/source/session "
                   "owners rejected");
  // Import only a verified ordinary existing House boundary. No new game,
  // cold default LOAD, UI constructor or RNG draw is executed here.
  if (!s.world->healthy() || s.world->stage() != OpeningStage::Walking ||
      (s.house->phase() != HousePhase::Idle &&
       s.house->phase() != HousePhase::SceneDoorPending) ||
      s.house->story_pending() ||
      s.dialogue->dialogue_active() || s.world->cutscene_active())
    return fail(
        e, "House UI continuation requires a live completed House boundary");
  std::array<uint8_t, 32> proof{};
  if (!ns.source_hash(d->source_script(), proof) ||
      proof != d->identity().source_sha256 || !s.commands->content().valid())
    return fail(e, "House UI continuation original source/menu owner absent");
  data_ = std::move(d);
  registry_ = &r;
  root_ = &root;
  signals_ = &bus;
  binding_ = std::move(b);
  sources_ = s;
  cutscene_ = data_->fields()[2].initial;
  source_cutscene_observed_ = true;
  story_generation_ = sources_.world->story_generation();
  outcome_cursor_ = sources_.outcome->events().size();
  e.clear();
  return true;
}
bool HouseUiContinuation::models(std::string &e) const {
  if (!data_ || !registry_ || registry_->poisoned() || !sources_.world ||
      !sources_.world->healthy() || !sources_.battle || !sources_.outcome ||
      !sources_.commands || !sources_.commands->content().valid() ||
      !sources_.house || !sources_.dialogue ||
      sources_.house->phase() == HousePhase::Error ||
      sources_.house->phase() == HousePhase::Unsupported ||
      sources_.battle->phase() == BattleEntryPhase::Error ||
      sources_.outcome->phase() == BattleOutcomePhase::Error)
    return fail(
        e, "House UI actual running session is unavailable or unsupported");
  return true;
}
bool HouseUiContinuation::actual(FieldObjectId id, std::string &e) const {
  if (!models(e) || !id || id != binding_.object ||
      registry_->external_object(id) != this || !inside_ ||
      parent_ != registry_->root())
    return fail(e, "House UI actual ObjectDB identity/Enter boundary rejected");
  return true;
}
bool HouseUiContinuation::state(FieldGlobalExternalState &out,
                                std::string &e) const {
  if (!data_)
    return fail(e, "House UI continuation was not constructed");
  out = {};
  out.name = binding_.source.name;
  out.parent = parent_;
  out.inside = inside_;
  // This continuation does not execute source UiManager._ready or claim the
  // seven UI factories/stable Canvas. Native inside is real; Ready stays
  // pending.
  out.ready = false;
  out.ui_before_canvas = false;
  e.clear();
  return true;
}
bool HouseUiContinuation::stage_parent(FieldObjectId p, std::string &e) {
  if (!data_ || inside_ || (p && parent_) || (p && p != registry_->root()) ||
      (p && std::count(root_->viewport().children.begin(),
                       root_->viewport().children.end(), binding_.object) != 1))
    return fail(
        e, "House UI native parent must follow actual root child insertion");
  parent_ = p;
  e.clear();
  return true;
}
bool HouseUiContinuation::native_notification(FieldTreePhase p,
                                              std::string &e) {
  if (!data_)
    return fail(e, "House UI native notification before construction");
  if (p == FieldTreePhase::Parented) {
    if (parent_ != registry_->root() || inside_)
      return fail(e, "House UI parented native boundary rejected");
  } else if (p == FieldTreePhase::Unparented) {
    if (inside_)
      return fail(e, "House UI native unparent before exit");
    parent_ = 0;
  } else if (p != FieldTreePhase::ChildMoved &&
             p != FieldTreePhase::PathChanged)
    return fail(e, "House UI native notification outside checked continuation");
  e.clear();
  return true;
}
bool HouseUiContinuation::enter(FieldObjectId p, std::string &e) {
  if (!data_ || inside_ || p != parent_ || p != registry_->root() ||
      registry_->external_object(binding_.object) != this ||
      !root_->viewport().inside ||
      std::count(root_->viewport().children.begin(),
                 root_->viewport().children.end(), binding_.object) != 1)
    return fail(e, "House UI actual native root Enter boundary rejected");
  inside_ = true;
  if (!root_->node_notification(binding_.object, FieldTreePhase::NodeAdded,
                                e) ||
      !root_->node_notification(binding_.object, FieldTreePhase::ChildEntered,
                                e))
    return false;
  e.clear();
  return true;
}
bool HouseUiContinuation::ready(std::string &e) {
  return fail(
      e,
      "House UI continuation does not execute whole original UiManager Ready");
}
bool HouseUiContinuation::adopt_continuation_ready(std::string &e) {
  // initialize already checked the completed, running House boundary. Import
  // that owner once after its actual native Node entered this same Viewport;
  // do not run UiManager._ready or manufacture its seven UI children.
  if (native_ready_ || !actual(binding_.object, e) ||
      !root_->viewport().ready || !root_->viewport().ready_notified ||
      !source_cutscene_observed_ || cutscene_ ||
      sources_.world->story_generation() != story_generation_ ||
      sources_.house->story_pending() ||
      sources_.dialogue->dialogue_active() || sources_.world->cutscene_active())
    return fail(e, "House UI native continuation boundary changed before import");
  native_ready_ = true;
  e.clear();
  return true;
}
bool HouseUiContinuation::exit(std::string &e) {
  if (!actual(binding_.object, e) ||
      !root_->node_notification(binding_.object, FieldTreePhase::NodeRemoved,
                                e) ||
      !root_->node_notification(binding_.object, FieldTreePhase::ChildExiting,
                                e))
    return false;
  inside_ = false;
  native_ready_ = false;
  e.clear();
  return true;
}
bool HouseUiContinuation::source_is_in_battle(FieldObjectId id, bool &out,
                                              std::string &e) const {
  if (!actual(id, e))
    return false;
  const auto &events = sources_.outcome->events();
  out = sources_.battle->phase() != BattleEntryPhase::Idle &&
        std::find(events.begin(), events.end(),
                  BattleRewardEvent::BattleEnded) == events.end();
  e.clear();
  return true;
}
bool HouseUiContinuation::source_is_pause_menu_active(FieldObjectId id,
                                                      bool &out,
                                                      std::string &e) const {
  if (!actual(id, e))
    return false;
  const auto phase = sources_.commands->phase();
  out = phase != FieldEquipmentPhase::Closed &&
        phase != FieldEquipmentPhase::PauseClosing &&
        !sources_.commands->items_suspended() &&
        !sources_.commands->psi_suspended();
  e.clear();
  return true;
}
bool HouseUiContinuation::source_is_in_cutscene(FieldObjectId id, bool &out,
                                                std::string &e) const {
  if (!actual(id, e) || !source_cutscene_observed_)
    return fail(e,
                "House UI actual source cutscene field has not been observed");
  // A newly running source story cannot silently inherit the ordinary boundary
  // value. Its real UI source setter/DialogueBox-ready/done must be dispatched.
  const bool story =
      sources_.world->cutscene_active() || sources_.house->story_executing();
  const bool dialogue = sources_.dialogue->dialogue_active();
  if ((story || dialogue) && !cutscene_ &&
      (story_generation_ != sources_.world->story_generation() ||
       story != story_at_source_event_ ||
       dialogue != dialogue_at_source_event_))
    return fail(e, "House UI active story has no actual source cutscene event");
  out = cutscene_;
  e.clear();
  return true;
}
bool HouseUiContinuation::source_set_cutscene(FieldObjectId id, bool v,
                                              std::string &e) {
  if (!actual(id, e))
    return false;
  cutscene_ = v;
  source_cutscene_observed_ = true;
  story_generation_ = sources_.world->story_generation();
  story_at_source_event_ =
      sources_.world->cutscene_active() || sources_.house->story_executing();
  dialogue_at_source_event_ = sources_.dialogue->dialogue_active();
  e.clear();
  return true;
}
bool HouseUiContinuation::signal_declaration(FieldObjectId id,
                                             std::string_view name,
                                             uint32_t &arity,
                                             std::string &e) const {
  if (!actual(id, e))
    return false;
  auto s = std::find_if(data_->signals().begin(), data_->signals().end(),
                        [&](const auto &v) { return v.name == name; });
  if (s == data_->signals().end())
    return fail(e, "House UI source signal is outside checked declarations");
  arity = s->arity;
  e.clear();
  return true;
}
bool HouseUiContinuation::battle_will_begin(std::string &e) {
  bool active = false;
  if (!source_is_in_battle(binding_.object, active, e) || active ||
      sources_.commands->visible())
    return fail(
        e, "House UI original battle-start boundary has no idle live owners");
  if (awaiting_entry_)
    return fail(e, "House UI battle-start source signal already delivered");
  awaiting_entry_ = true;
  return signals_->emit(binding_.object, data_->signal(2)->name, {}, e);
}
bool HouseUiContinuation::battle_entry_committed(std::string &e) {
  if (!actual(binding_.object, e) || !awaiting_entry_ ||
      sources_.battle->phase() == BattleEntryPhase::Idle ||
      sources_.outcome->phase() != BattleOutcomePhase::Idle ||
      !sources_.outcome->events().empty())
    return fail(
        e, "House UI actual battle Entry/reset Outcome source commit rejected");
  awaiting_entry_ = false;
  outcome_cursor_ = 0;
  e.clear();
  return true;
}
bool HouseUiContinuation::observe_battle_events(std::string &e) {
  if (!actual(binding_.object, e))
    return false;
  const auto &events = sources_.outcome->events();
  // Existing begin_battle resets the SAME Outcome object before Entry::begin.
  // Reset preserves object ownership, while the new event vector starts empty.
  if (events.size() < outcome_cursor_)
    return fail(
        e, "House UI Outcome stream reset without actual source entry commit");
  while (outcome_cursor_ < events.size()) {
    const auto ev = events[outcome_cursor_++];
    if (ev == BattleRewardEvent::ReturnStarted) {
      if (!signals_->emit(binding_.object, data_->signal(3)->name, {}, e))
        return false;
    }
  }
  e.clear();
  return true;
}
bool HouseUiContinuation::deferred(const FieldDeferredMessage &m,
                                   std::string &e) {
  if (!actual(m.object, e) || m.kind != FieldDeferredKind::Call)
    return fail(e, "House UI actual method target/kind rejected");
  const auto *setter = data_->method(4);
  if (m.member == setter->name && m.args.size() == 1 &&
      std::holds_alternative<bool>(m.args.front()))
    return source_set_cutscene(m.object, std::get<bool>(m.args.front()), e);
  // Return-valued getters use the typed methods above, never a dropped Variant.
  return fail(e, "House UI method requires unsupported full UI owner or typed "
                 "return endpoint");
}
bool HouseUiContinuation::persist_append(FieldObjectId, std::string &e) {
  return fail(e, "House UI does not own source global persistent Array");
}
bool HouseUiContinuation::assign_stable_canvas(FieldObjectId, std::string &e) {
  return fail(e,
              "House UI continuation has no original stableCanvas constructor");
}
bool HouseUiContinuation::rebind_scene(const OpeningWorld &w,
                                       const HouseRuntime &h,
                                       const HousePresentation &p,
                                       std::string &e) {
  if (!actual(binding_.object, e) || !w.healthy())
    return fail(e, "House UI continuation destination ownership rejected");
  sources_.world = &w;
  sources_.house = &h;
  sources_.dialogue = &p;
  e.clear();
  return true;
}
} // namespace encore::ctr
