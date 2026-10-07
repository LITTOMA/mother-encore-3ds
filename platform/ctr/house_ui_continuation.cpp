#include "house_ui_continuation.hpp"
#include <algorithm>
#include "encore/global_yaml_caches.hpp"
#include "encore/field_global_data.hpp"
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
      !b.object || b.family != 0x454e0064 || b.capability != d->capability() ||
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
  key_open_=data_->key_initial_open();
  if(data_->dialogue_continuation()){
    const auto &policy=data_->business_policy();
    if(policy.widgets.size()!=3||!policy.initial_timer_null||s.commands->phase()!=FieldEquipmentPhase::Closed)
      return fail(e,"House continued UI closed-widget source/models boundary rejected");
    key_open_=policy.widgets[0].initial_open;cash_open_=policy.widgets[1].initial_open;
    party_showing_=policy.widgets[2].initial_open;party_info_timer_=0;business_imported_=true;
  }
  source_cutscene_observed_ = true;
  story_generation_ = sources_.world->story_generation();
  outcome_cursor_ = sources_.outcome->events().size();
  e.clear();
  return true;
}
bool HouseUiContinuation::models(std::string &e) const {
  if (reentry_failed_ || reentry_committing_ || !data_ || !registry_ || registry_->poisoned() || !sources_.world ||
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
  out.stable_canvas = stable_canvas_;
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
bool HouseUiContinuation::source_stack_empty(FieldObjectId id,bool &out,std::string &e)const{
  if(!actual(id,e)||!native_ready_||!data_->stack_continuation()||!stack_state(e))return false;
  // is_stack_empty reads only the actual Array. A real DialogueBox may already
  // be closing after _dialogue_box was cleared; it remains on the source stack.
  if(!ui_stack_.empty()){out=false;e.clear();return true;}
  bool battle=false;
  if(!source_is_in_battle(id,battle,e))return false;
  if(battle||sources_.commands->phase()!=FieldEquipmentPhase::Closed||
     (sources_.dialogue->dialogue_active()&&!current_dialogue_)||sources_.house->story_pending())
    return fail(e,"House UI stack has an active owner without its actual registered source node");
  out=true;e.clear();return true;
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
  const bool registered_dialogue=dialogue_life_&&(current_dialogue_||!ui_stack_.empty());
  if ((story || dialogue) && !registered_dialogue && !cutscene_ &&
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
bool HouseUiContinuation::assign_stable_canvas(FieldObjectId id, std::string &e) {
  if(!source_continuation_canvas_admitted(e)||!global_||!id)return false;
  const auto *ns=registry_->data();auto tree=registry_->tree_owner(id);
  const auto *record=ns&&ns->canvas_recipe().valid()?ns->canvas_recipe().record(ns->canvas_recipe().identity().scene_id):nullptr;
  const auto *node=tree?tree->state(id):nullptr;const auto *desc=tree?tree->descriptor(id):nullptr;
  const auto *parent=node?tree->state(node->parent):nullptr;FieldIdentity identity{};
  if(!record||!node||!desc||!parent||!node->alive||!node->bound||!node->inside||
     node->parent!=registry_->current_scene()||node->ready_notified!=parent->ready_notified||
     !tree->object_identity(id,identity)||!same(identity,ns->canvas_recipe().identity())||
     desc->id!=record->id||desc->native_class!=record->native_class||
     desc->script!=record->script||desc->script_sha!=record->script_sha)
    return fail(e,"Continued UI Canvas is not the actual checked native recipe child");
  std::shared_ptr<const GlobalLoadObjectArray> persistent;
  if(!global_->array(FieldGlobalMemberRole::Persistent,persistent,e)||!persistent||
     std::count(persistent->values.begin(),persistent->values.end(),id)!=1)
    return fail(e,"Continued UI Canvas was not appended to the same real global persistent Array");
  stable_canvas_=id;e.clear();return true;
}
bool HouseUiContinuation::rebind_scene(const OpeningWorld &w,
                                       const HouseRuntime &h,
                                       const HousePresentation &p,
                                       std::string &e) {
  if (!actual(binding_.object, e) || !w.healthy() ||
      &w!=sources_.world || &h!=sources_.house || &p!=sources_.dialogue)
    return fail(e, "House UI scene replacement requires a checked House reentry ticket");
  e.clear();
  return true;
}
bool HouseUiContinuation::checked_house_reentry(const HouseUiReentryInput &in,
                                                bool committing,
                                                std::string &e) const {
  if (!actual(binding_.object,e) || !native_ready_ || !global_ ||
      in.registry!=registry_ || !in.source || !in.source_tree || !in.source_tree->valid() ||
      !in.doors || !in.door_runtime || !in.old_house ||
      !in.next_house || in.old_house==in.next_house || !in.old_tree ||
      !in.next_tree || in.old_tree==in.next_tree || !in.old_root ||
      !in.next_root || in.old_root==in.next_root || !in.random ||
      !in.uid_ledger || !in.borrowers ||
      sources_.world!=&in.old_house->world ||
      sources_.house!=&in.old_house->house ||
      sources_.dialogue!=&in.old_house->presentation)
    return fail(e,"UI House reentry requires the same live source owners/Registry");
  if (in.door_runtime->data()!=in.doors ||
      in.door_runtime->phase()!=FieldDoorPhase::Deferred ||
      in.door_runtime->active_door()!=in.source->door_id() ||
      !in.door_runtime->source_ready(in.source->door_id()))
    return fail(e,"UI House reentry is outside the actual source Door deferred commit");
  if (!in.source->matches(*in.doors,in.next_house->world.content(),in.house_data,e) ||
      !in.source->matches(*in.doors,in.old_house->world.content(),in.house_data,e) ||
      !same(in.old_identity,in.doors->identity()) ||
      !same(in.next_identity,in.source_tree->identity()) ||
      in.next_identity.upstream_commit!=in.source->identity().upstream_commit ||
      in.next_identity.source_sha256!=in.source->identity().source_sha256 ||
      in.source_tree->source_scene()!=in.source->target_scene() ||
      in.old_identity.upstream_commit!=data_->identity().upstream_commit ||
      in.next_identity.upstream_commit!=data_->identity().upstream_commit ||
      in.old_tree->object_domain()!=registry_->kernel() ||
      in.next_tree->object_domain()!=registry_->kernel() ||
      registry_->tree_owner(in.old_root)!=in.old_tree ||
      registry_->tree_owner(in.next_root)!=in.next_tree)
    return fail(e,"UI House reentry full old/new source identity/domain differs");
  FieldIdentity old_id{},next_id{};
  const auto *old=in.old_tree->state(in.old_root);
  const auto *next=in.next_tree->state(in.next_root);
  const auto *desc=in.next_tree->descriptor(in.next_root);
  const auto &nodes=in.source->native_nodes();
  std::array<uint8_t,32> source_sha{};
  if (!in.source_tree->source_hash(in.source->target_scene(),source_sha) ||
      source_sha!=in.source->identity().source_sha256 || nodes.empty() ||
      nodes.front().id!=in.next_identity.scene_id)
    return fail(e,"UI House reentry complete tree target source proof differs");
  if (!old || !next || !desc || nodes.empty() || !old->alive || old->queued ||
      (!committing&&(!old->inside||!old->ready_notified)) ||
      !next->alive || next->queued || next->inside || next->ready_notified ||
      !next->ready_first || next->parent ||
      in.old_tree->lifecycle_pending() || in.next_tree->lifecycle_pending() ||
      !in.old_tree->object_identity(in.old_root,old_id) ||
      !in.next_tree->object_identity(in.next_root,next_id) ||
      !same(old_id,in.old_identity) || !same(next_id,in.next_identity) ||
      desc->id!=nodes.front().id || desc->native_class!=nodes.front().native_class ||
      desc->script!=nodes.front().script || desc->script_sha!=nodes.front().script_sha ||
      next->name!=in.source->target_root_name() ||
      (!committing&&registry_->current_scene()!=in.old_root) ||
      (committing&&registry_->current_scene()!=in.old_root&&
                   registry_->current_scene()!=in.next_root))
    return fail(e,"UI House reentry must precede old deletion/new Enter and mapped Ready");
  if (!in.next_house->world.healthy() ||
      in.next_house->world.stage()!=OpeningStage::Walking ||
      !in.next_house->scene_ready_pending() ||
      in.next_house->house.phase()!=HousePhase::Idle ||
      in.next_house->house.story_pending() ||
      in.next_house->presentation.dialogue_active() ||
      in.next_house->world.cutscene_active() || sources_.house->story_pending() ||
      sources_.dialogue->dialogue_active() || sources_.world->cutscene_active() ||
      awaiting_entry_ || cutscene_ || !source_cutscene_observed_ ||
      current_dialogue_ || !ui_stack_.empty() || !on_screen_enemies_.empty() ||
      !source_business_closed(e))
    return fail(e,"UI House reentry rejects pending dialogue/story/battle/native widget work");
  bool battle=false;FieldObjectId talker=0;
  if (!source_is_in_battle(binding_.object,battle,e) || battle ||
      !source_current_talker(talker,e) || talker)
    return fail(e,"UI House reentry has an active battle or source talker");
  if (!stable_canvas_ || stable_canvas_!=registry_->stable_canvas())
    return fail(e,"UI House reentry lost its same persistent Canvas ObjectID");
  auto canvas_tree=registry_->tree_owner(stable_canvas_);
  const auto *canvas=canvas_tree?canvas_tree->state(stable_canvas_):nullptr;
  const auto *canvas_desc=canvas_tree?canvas_tree->descriptor(stable_canvas_):nullptr;
  const auto &recipe=registry_->data()->canvas_recipe();
  const auto *record=recipe.record(recipe.identity().scene_id);
  FieldIdentity canvas_id{};
  if (!canvas || !canvas_desc || !record || !canvas->alive || canvas->queued ||
      !canvas->bound || canvas->inside || canvas->parent ||
      canvas->ready_notified || canvas->ready_first ||
      canvas_tree->object_domain()!=registry_->kernel() ||
      !canvas_tree->object_identity(stable_canvas_,canvas_id) ||
      !same(canvas_id,recipe.identity()) || canvas_desc->id!=record->id ||
      canvas_desc->native_class!=record->native_class ||
      canvas_desc->script!=record->script || canvas_desc->script_sha!=record->script_sha)
    return fail(e,"UI House reentry Canvas source/lifecycle differs");
  std::shared_ptr<const GlobalLoadObjectArray> persistent;
  if (!global_->array(FieldGlobalMemberRole::Persistent,persistent,e) || !persistent ||
      std::count(persistent->values.begin(),persistent->values.end(),stable_canvas_)!=1)
    return fail(e,"UI House reentry changed the actual persistent Array");
  e.clear();return true;
}
bool HouseUiContinuation::checked_reentry_borrowers(const HouseUiReentryInput &in,
    bool rebound,HouseUiReentryBorrowState &out,std::string &e) const {
  HouseUiReentryBorrowState next;
  const auto *printer=rebound?&in.next_house->presentation:&in.old_house->presentation;
  if (!in.borrowers->observe(in,*this,rebound,next,e))return false;
  if (!next.complete || next.registry!=registry_ || next.printer!=printer ||
      next.dialogue_script!=dialogue_script_ || next.random!=in.random ||
      next.uid_ledger!=in.uid_ledger || next.pending_ui_callbacks ||
      next.pending_native_callbacks)
    return fail(e,"UI House reentry has unknown/pending native callbacks or stale printer borrowers");
  std::vector<FieldObjectId> observed;
  for(auto id:next.dialogue_objects){
    auto tree=registry_->tree_owner(id);const auto *node=tree?tree->state(id):nullptr;
    if(!id || !registry_->object_exists(id) || !node || !node->alive || node->queued ||
       tree->object_domain()!=registry_->kernel() ||
       std::find(observed.begin(),observed.end(),id)!=observed.end())
      return fail(e,"UI House reentry dialogue ObjectIDs have unknown/pending native ownership");
    observed.push_back(id);
  }
  if (dialogue_script_ && (!dialogue_life_ || !dialogue_recipe_ ||
      !dialogue_script_->admit(*dialogue_life_,*dialogue_recipe_,
          rebound?in.next_house->presentation:
                  const_cast<HousePresentation&>(in.old_house->presentation),e)))
    return fail(e,"UI House reentry dialogue script did not retain its actual source/printer");
  out=next;e.clear();return true;
}
bool HouseUiContinuation::prepare_house_reentry(const HouseUiReentryInput &in,
    HouseUiReentryTicket &out,std::string &e) const {
  if (out.valid())return fail(e,"UI House reentry ticket is already prepared");
  if (!checked_house_reentry(in,false,e))return false;
  HouseUiReentryTicket next;
  next.input_=in;
  next.random_state_=in.random->state();next.random_draws_=in.random->raw_draw_count();
  next.uids_=*in.uid_ledger;
  if (!checked_reentry_borrowers(in,false,next.borrowers_,e))return false;
  if (in.random->state()!=next.random_state_ ||
      in.random->raw_draw_count()!=next.random_draws_ || *in.uid_ledger!=next.uids_)
    return fail(e,"UI House reentry admission modified live entropy/UID ledger");
  next.owner_=this;next.ui_=binding_.object;next.canvas_=stable_canvas_;
  next.story_generation_=story_generation_;next.outcome_cursor_=outcome_cursor_;
  next.dialogue_event_=dialogue_at_source_event_;next.story_event_=story_at_source_event_;
  out=std::move(next);e.clear();return true;
}
bool HouseUiContinuation::commit_house_reentry(HouseUiReentryTicket &ticket,
                                               std::string &e) {
  if (ticket.owner_!=this || ticket.ui_!=binding_.object || ticket.canvas_!=stable_canvas_ ||
      ticket.story_generation_!=story_generation_ || ticket.outcome_cursor_!=outcome_cursor_ ||
      ticket.dialogue_event_!=dialogue_at_source_event_ || ticket.story_event_!=story_at_source_event_)
    return fail(e,"UI House reentry ticket does not own the unchanged actual continuation");
  const auto &in=ticket.input_;HouseUiReentryBorrowState before;
  if (!checked_house_reentry(in,true,e) || !checked_reentry_borrowers(in,false,before,e))return false;
  if (before.dialogue_objects!=ticket.borrowers_.dialogue_objects ||
      in.random->state()!=ticket.random_state_ ||
      in.random->raw_draw_count()!=ticket.random_draws_ || *in.uid_ledger!=ticket.uids_)
    return fail(e,"UI House reentry source instances/entropy changed after preparation");
  // An actual owner may have partially changed its borrows on failure. Poison
  // this continuation before any later callback can dereference a mixed scene.
  reentry_committing_=true;
  if (!in.borrowers->rebind(in,*this,e)){
    reentry_committing_=false;reentry_failed_=true;
    if(e.empty())e="UI House reentry concrete borrower commit failed";
    return false;
  }
  HouseUiReentryBorrowState after;
  if (!checked_reentry_borrowers(in,true,after,e) ||
      after.dialogue_objects!=before.dialogue_objects ||
      in.random->state()!=ticket.random_state_ ||
      in.random->raw_draw_count()!=ticket.random_draws_ || *in.uid_ledger!=ticket.uids_){
    reentry_committing_=false;reentry_failed_=true;
    if(e.empty())e="UI House reentry borrower commit changed source instances/entropy";
    return false;
  }
  sources_.world=&in.next_house->world;sources_.house=&in.next_house->house;
  sources_.dialogue=&in.next_house->presentation;
  reentry_committing_=false;ticket=HouseUiReentryTicket{};
  e.clear();return true;
}
bool HouseUiContinuation::bind_source_global(FieldGlobalConstructorRuntime &g,std::string &e){
  const auto *ns=registry_?registry_->data():nullptr;
  const auto *owner=registry_?registry_->external_object(g.owner()):nullptr;
  if(global_||!actual(binding_.object,e)||!native_ready_||!data_->dialogue_continuation()||
     !ns||!g.data()||!g.data()->valid()||!owner||
     g.owner()!=registry_->autoload_object(ns->global_autoload())||
     owner->binding().source.script!=g.data()->owner_source()||
     g.data()->identity().upstream_commit!=data_->identity().upstream_commit)
    return fail(e,"UI dialogue bridge requires the same actual source global body");
  global_=&g;e.clear();return true;
}
bool HouseUiContinuation::source_continuation_canvas_admitted(std::string &e)const{
  if(!actual(binding_.object,e)||!native_ready_||!data_->dialogue_continuation()||
     !global_||stable_canvas_||registry_->stable_canvas()||current_dialogue_||!ui_stack_.empty()||
     sources_.commands->phase()!=FieldEquipmentPhase::Closed||sources_.dialogue->dialogue_active()||
     sources_.world->cutscene_active()||sources_.house->story_pending()||cutscene_)
    return fail(e,"UI continuation Canvas requires its actual imported ordinary boundary");
  e.clear();return true;
}
bool HouseUiContinuation::bind_dialogue_sources(const FieldDialogueLifecycleData &life,
    const FieldNodeRecipeData &recipe,PodunkDialogueRootScript &script,HousePresentation &printer,std::string &e){
  std::array<uint8_t,32> scene{},body{},abstract{};
  if(dialogue_life_||!actual(binding_.object,e)||!native_ready_||!global_||
     !data_->dialogue_continuation()||&printer!=sources_.dialogue||!life.valid()||!recipe.valid()||
     life.commit()!=data_->identity().upstream_commit||recipe.identity().upstream_commit!=life.commit()||
     life.factory_ir_sha()!=recipe.ir_sha256()||life.factory_scene_id()!=recipe.identity().scene_id||
     life.factory_node_count()!=recipe.records().size()||life.scene()!=data_->dialogue_policy().dialogue_scene||
     recipe.source_scene()!=life.scene()||!data_->source_hash(life.scene(),scene)||
     scene!=recipe.identity().source_sha256||scene!=life.scene_sha()||
     !data_->source_hash(data_->dialogue_policy().dialogue_script,body)||
     !life.source_hash(data_->dialogue_policy().dialogue_script,abstract)||body!=abstract||
     !data_->source_hash(data_->dialogue_policy().abstract_script,body)||
     !life.source_hash(data_->dialogue_policy().abstract_script,abstract)||body!=abstract||
     !script.admit(life,recipe,printer,e))
    return fail(e,"UI dialogue bridge rejected the actual recipe/script/lifecycle owners");
  dialogue_life_=&life;dialogue_recipe_=&recipe;dialogue_script_=&script;
  e.clear();return true;
}
bool HouseUiContinuation::source_dialogue_parent(FieldObjectId id,std::string &e)const{
  if(!actual(binding_.object,e)||!native_ready_||!global_||!stable_canvas_||id!=stable_canvas_||
     registry_->stable_canvas()!=id||!registry_->object_exists(id))
    return fail(e,"UI dialogue parent is not the actual persistent continued Canvas");
  auto tree=registry_->tree_owner(id);const auto *node=tree?tree->state(id):nullptr;
  const auto *desc=tree?tree->descriptor(id):nullptr;FieldIdentity identity{};
  const auto &recipe=registry_->data()->canvas_recipe();const auto *record=recipe.record(recipe.identity().scene_id);
  if(!node||!desc||!record||!node->alive||!node->bound||!node->inside||!node->ready_notified||
     node->parent!=registry_->current_scene()||!tree->object_identity(id,identity)||
     !same(identity,recipe.identity())||desc->id!=record->id||desc->native_class!=record->native_class||
     desc->script!=record->script||desc->script_sha!=record->script_sha)
    return fail(e,"UI dialogue Canvas actual Tree/source/native lifecycle is incomplete");
  std::shared_ptr<const GlobalLoadObjectArray> persistent;
  if(!global_->array(FieldGlobalMemberRole::Persistent,persistent,e)||!persistent||
     std::count(persistent->values.begin(),persistent->values.end(),id)!=1)
    return fail(e,"UI dialogue Canvas lost its same source persistent Array membership");
  e.clear();return true;
}
bool HouseUiContinuation::dialogue_object(FieldObjectId id,bool entered,std::string &e)const{
  if(!dialogue_life_||!dialogue_recipe_||!dialogue_script_||!id||!registry_->object_exists(id))
    return fail(e,"UI stack entry has no actual checked DialogueBox script owner");
  auto tree=registry_->tree_owner(id);const auto *n=tree?tree->state(id):nullptr;
  const auto *d=tree?tree->descriptor(id):nullptr;
  const auto *record=dialogue_recipe_->record(dialogue_recipe_->identity().scene_id);
  FieldIdentity identity{};PodunkDialogueScriptState source;
  if(!n||!d||!record||!n->alive||(n->inside&&!n->bound)||!tree->object_identity(id,identity)||
     !same(identity,dialogue_recipe_->identity())||d->id!=record->id||
     d->native_class!=record->native_class||d->script!=record->script||d->script_sha!=record->script_sha||
     !dialogue_script_->state(id,source,e)||source.object!=id||!source.constructed||
     !same(source.identity,identity)||source.printer!=sources_.dialogue||source.entered!=n->inside||
     (source.ready&&!n->ready_notified)||
     (n->inside&&(n->parent!=stable_canvas_||registry_->tree_owner(stable_canvas_)!=tree))||
     (!n->inside&&n->parent)||
     (entered&&(!n->inside||!n->ready_notified||!source.ready)))
    return fail(e,"UI stack DialogueBox actual native/script/ObjectDB lifecycle differs");
  e.clear();return true;
}
bool HouseUiContinuation::stack_state(std::string &e)const{
  if(!data_||(!data_->dialogue_continuation()&&(!ui_stack_.empty()||current_dialogue_)))
    return fail(e,"UI stack requires its checked dialogue source capability");
  for(auto id:ui_stack_)if(!dialogue_object(id,false,e))return false;
  if(current_dialogue_&&!dialogue_object(current_dialogue_,false,e))return false;
  e.clear();return true;
}
bool HouseUiContinuation::source_add_ui(FieldObjectId id,bool add_child,std::string &e){
  if(!actual(binding_.object,e)||!native_ready_||!source_dialogue_parent(stable_canvas_,e)||
     !dialogue_object(id,false,e))return false;
  auto tree=registry_->tree_owner(id);const auto *n=tree->state(id);
  if(add_child&&(n->parent||n->inside||n->queued))
    return fail(e,"Source UI deferred add requires the actual detached DialogueBox");
  // Original add_ui mutates the Array before call_deferred, including when
  // add_child=false. It does not wait for Ready or silently deduplicate nodes.
  ui_stack_.insert(ui_stack_.begin(),id);
  if(add_child){FieldDeferredMessage m;m.object=stable_canvas_;m.kind=FieldDeferredKind::Call;
    m.member=data_->dialogue_policy().add_child_method;m.args={FieldObjectRef{id}};
    if(!registry_->enqueue(std::move(m),e))return false;}
  e.clear();return true;
}
bool HouseUiContinuation::source_remove_ui(FieldObjectId id,std::string &e){
  if(!actual(binding_.object,e)||!native_ready_||!data_->dialogue_continuation())return false;
  auto found=std::find(ui_stack_.begin(),ui_stack_.end(),id);
  if(found==ui_stack_.end()){e.clear();return true;}
  if(!dialogue_object(id,false,e))return false;
  // The checked DialogueBox and its base have no close() method: close_item
  // selects actual queue_free, after erasing exactly the first Array entry.
  auto tree=registry_->tree_owner(id);ui_stack_.erase(found);
  return tree->queue_free(id,e);
}
bool HouseUiContinuation::checked_dialogue_step(const FieldDialogueStep &s,std::string &e)const{
  if(!dialogue_life_)return fail(e,"UI dialogue source method was not bound");
  const auto found=std::find_if(dialogue_life_->steps().begin(),dialogue_life_->steps().end(),
    [&](const auto &v){return v.stage==s.stage&&v.op==s.op&&v.role==s.role&&v.value==s.value&&v.text==s.text;});
  if(found==dialogue_life_->steps().end())return fail(e,"UI dialogue step is outside its checked source lifecycle");
  e.clear();return true;
}
bool HouseUiContinuation::source_dialogue_step(const FieldDialogueStep &s,FieldObjectId id,std::string &e){
  if(!actual(binding_.object,e)||!native_ready_||!checked_dialogue_step(s,e)||
     !source_dialogue_parent(stable_canvas_,e)||!dialogue_object(id,false,e))return false;
  using O=FieldDialogueOp;
  switch(s.op){
  case O::StoreDialogue:current_dialogue_=id;break;
  case O::PauseMenuInactive:
    if(sources_.commands->phase()!=FieldEquipmentPhase::Closed)
      return fail(e,"Dialogue opening with an active commands menu requires its actual close owner");
    break;
  case O::StackPush:return source_add_ui(id,false,e);
  case O::UiCutscene:
    if(s.stage==FieldDialogueStage::Ready&&!dialogue_object(id,true,e))return false;
    return source_set_cutscene(binding_.object,s.value!=0,e);
  case O::ClearDialogue:
    if(current_dialogue_!=id)return fail(e,"Source dialogue completion does not own the current UI field");
    current_dialogue_=0;break;
  case O::RemoveUi:return source_remove_ui(id,e);
  default:return fail(e,"Dialogue UI business action requires its actual independent native owner");
  }
  e.clear();return true;
}
bool HouseUiContinuation::source_dialogue_global_step(const FieldDialogueStep &s,FieldObjectId id,
    FieldObjectId talker,std::string &e){
  if(!actual(binding_.object,e)||!global_||!checked_dialogue_step(s,e)||!dialogue_object(id,true,e))return false;
  if(s.op==FieldDialogueOp::GlobalCutscene)
    return global_->set_boolean(FieldGlobalMemberRole::InCutscene,s.value!=0,e);
  if(s.op==FieldDialogueOp::SetTalker){
    if((s.value==0&&talker)||(talker&&(!registry_->tree_owner(talker)||!registry_->object_exists(talker))))
      return fail(e,"Source dialogue talker is not an actual source Node");
    return global_->set_object(FieldGlobalMemberRole::Talker,talker,e);
  }
  return fail(e,"Source global dialogue operation requires its actual signal/business owner");
}
bool HouseUiContinuation::source_business_closed(std::string &e)const{
  if(!actual(binding_.object,e)||!native_ready_||!business_imported_||
     !data_->dialogue_continuation()||key_open_||cash_open_||party_showing_||party_info_timer_||
     sources_.commands->phase()!=FieldEquipmentPhase::Closed)
    return fail(e,"House UI actual imported closed guards are not closed/supported");
  e.clear();return true;
}
bool HouseUiContinuation::source_close_closed_widget(uint32_t role,std::string &e){
  if(!actual(binding_.object,e)||!native_ready_||!business_imported_||!role||role>data_->business_policy().widgets.size())
    return fail(e,"UI closed-widget actual source body absent");
  const auto &source=data_->business_policy().widgets[role-1];
  if(source.role!=role||source.initial_open) return fail(e,"UI source closed guard policy rejected");
  if((role==1&&key_open_)||(role==2&&cash_open_)||(role==3&&(party_showing_||party_info_timer_)))
    return fail(e,"UI opened widget/timer requires its complete actual native animation owner");
  e.clear();return true;
}
bool HouseUiContinuation::source_current_talker(FieldObjectId &out,std::string &e)const{
  FieldObjectId value=0;
  if(!actual(binding_.object,e)||!global_||!global_->object(FieldGlobalMemberRole::Talker,value,e))return false;
  if(value&&(!registry_->object_exists(value)||!registry_->tree_owner(value)))
    return fail(e,"UI current_talker does not reference the same actual source Node");
  out=value;e.clear();return true;
}
bool HouseUiContinuation::source_dialogue_state(HouseUiDialogueState &out,std::string &e)const{
  if(!actual(binding_.object,e)||!native_ready_||!source_dialogue_parent(stable_canvas_,e)||!stack_state(e))return false;
  HouseUiDialogueState next;next.stable_canvas=stable_canvas_;next.current_dialogue=current_dialogue_;next.stack=ui_stack_;
  if(!source_current_talker(next.talker,e)||!source_is_in_cutscene(binding_.object,next.cutscene,e))return false;
  if(current_dialogue_){auto tree=registry_->tree_owner(current_dialogue_);const auto *n=tree->state(current_dialogue_);
    PodunkDialogueScriptState script;if(!dialogue_script_->state(current_dialogue_,script,e))return false;
    next.current_inside=n->inside;next.current_ready=n->ready_notified&&script.ready;}
  out=std::move(next);e.clear();return true;
}
bool HouseUiContinuation::source_update_key_indicator(
    const FieldGlobalDataRuntime&globaldata,std::string_view region,std::string&e){
  if(!actual(binding_.object,e)||!native_ready_||!data_->key_indicator()||region.empty())
    return fail(e,"House key indicator lacks actual live UI/region/source policy");
  FieldGlobalDataMemberState keys;
  if(!globaldata.read_global_member(data_->key_member(),keys,e))return false;
  if(!keys.value||keys.value->kind!=6)
    return fail(e,"House key indicator actual globaldata.keys is not a Dictionary");
  auto value=keys.value->get(region);int64_t count=data_->key_default_count();
  if(value){if(value->kind!=2)return fail(e,"Regional key count is not an integer");count=value->integer;}
  if(count>0||key_open_)
    return fail(e,"KeyNumber open/animated close needs its actual native UI owner");
  // Source KeyNumber.close returns immediately when the same continued UI
  // has never opened. The running House has no key-count drawing owner;
  // this admitted branch preserves that real closed state, with no tween.
  e.clear();return true;
}
bool HouseUiContinuation::source_clear_on_screen_enemies(std::string&e){
 bool in_battle=false;
 if(!actual(binding_.object,e)||!native_ready_||!data_->key_indicator()||
    sources_.world->content().string(sources_.world->content().scene().source_scene_string)!=data_->house_scene()||
    !source_is_in_battle(binding_.object,in_battle,e)||in_battle||!on_screen_enemies_.empty())
   return fail(e,"House source enemy clearing lacks its actual completed empty owner");
 on_screen_enemies_.clear();e.clear();return true;
}
} // namespace encore::ctr
