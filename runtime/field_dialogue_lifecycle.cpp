#include "encore/field_dialogue_lifecycle.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
bool FieldDialogueLifecycleRuntime::fail(std::string &e, const char *m) {
  error_ = m;
  e = error_;
  phase_ = FieldDialogueLifecyclePhase::Error;
  return false;
}
bool FieldDialogueLifecycleRuntime::initialize(
    const FieldDialogueLifecycleData &d, const FieldProgrammeData &p,
    const FieldNodeRecipeData &recipe, FieldNodeTreeRuntime &t,
    FieldNpcRuntime &n, FieldDialogueLifecycleHost h, std::string &e) {
  if (data_ || !d.valid() || !p.valid() || d.commit() != p.commit() ||
      !h.admit_factory || !h.admit_parent || !h.factory_created || !h.admit_step || !h.observe ||
      !h.pause_player || !h.unpause_player || !h.manager || !h.global ||
      !h.native || !h.play_animation || !h.close_sound ||
      !h.restore_telepathy || !h.return_camera || !h.connect_ready ||
      !h.connect_done || !h.connect_animation || !h.start_programme ||
      !h.bind_programme || !h.emit_done || !h.disconnect) {
    e = "Dialogue lifecycle checked source/typed native endpoints incomplete";
    return false;
  }
  for (uint32_t i = 0; i < p.program_count(); ++i)
    if (!d.supports(p, i, e))
      return false;
  std::array<uint8_t, 32> sha{};
  if (!p.source_hash(d.scene(), sha) || sha != d.scene_sha()) {
    e = "Dialogue lifecycle scene source and Programme provenance differ";
    return false;
  }
  if (!recipe.valid() || recipe.identity().scene_id != d.factory_scene_id() ||
      recipe.identity().upstream_commit != d.commit() ||
      recipe.identity().source_sha256 != d.scene_sha() ||
      recipe.source_scene() != d.scene() ||
      recipe.ir_sha256() != d.factory_ir_sha() ||
      recipe.records().size() != d.factory_node_count()) {
    e = "Dialogue lifecycle complete checked factory provenance differs";
    return false;
  }
  for (uint32_t role = 1; role <= 12; ++role) {
    const auto *ref = d.reference(role);
    const auto *node = ref ? recipe.record(ref->id) : nullptr;
    if (!node || node->path != d.node(role) ||
        node->native_class != ref->native_class || node->ready != ref->ready) {
      e = "Dialogue lifecycle checked factory native ref differs";
      return false;
    }
  }
  recipe_ = &recipe;
  data_ = &d;
  programmes_ = &p;
  tree_ = &t;
  npcs_ = &n;
  host_ = std::move(h);
  phase_ = FieldDialogueLifecyclePhase::Closed;
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::preflight(const FieldProgrammeContext &ctx,
                                              std::string &e) {
  if (!data_ || ctx.source_npc != programmes_->npc().id || !ctx.actor_object) {
    e = "Dialogue lifecycle original NPC context absent";
    return false;
  }
  auto *n = tree_->state(ctx.actor_object);
  if (!n || !n->alive || !n->inside || !n->ready_notified ||
      n->source != ctx.source_npc) {
    e = "Dialogue lifecycle actual source NPC Ready absent";
    return false;
  }
  if (!host_.admit_factory(*data_, e))
    return false;
  FieldDialogueObservation o;
  if (!host_.observe(0, o, e))
    return false;
  if (!o.stable_canvas || o.dialogue_present || o.actor_count ||
      o.queued_battle || o.set_respawn) {
    e = "Dialogue lifecycle concurrent/actor/battle/respawn source branch "
        "pending";
    return false;
  }
  if (!host_.admit_parent(o.stable_canvas, e))
    return false;
  for (const auto &s : data_->steps())
    if (!host_.admit_step(s, ctx, e))
      return false;
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::admit(const DialogueAction &a,
                                          const FieldProgrammeContext &ctx,
                                          std::string &e) {
  switch (a.kind) {
  case DialogueActionKind::BeginCutscene:
  case DialogueActionKind::StopInteraction:
  case DialogueActionKind::SetTalker:
  case DialogueActionKind::CutsceneEnded:
  case DialogueActionKind::DialogueDone:
    break;
  default:
    e = "Dialogue lifecycle opcode outside admitted source slice";
    return false;
  }
  if (phase_ != FieldDialogueLifecyclePhase::Closed &&
      phase_ != FieldDialogueLifecyclePhase::Removed &&
      !(phase_ == FieldDialogueLifecyclePhase::Closing && done_seen_)) {
    e = "Dialogue lifecycle source UI owner busy";
    return false;
  }
  return preflight(ctx, e);
}
bool FieldDialogueLifecycleRuntime::owned(std::string &e) const {
  if (!data_ || !object_) {
    e = "Dialogue lifecycle actual source owner absent";
    return false;
  }
  const auto *n = tree_->state(object_);
  FieldIdentity id{};
  if (!n || !n->alive || !tree_->object_identity(object_, id) ||
      id.scene_id != data_->factory_scene_id() ||
      id.upstream_commit != data_->commit() ||
      id.source_sha256 != data_->scene_sha()) {
    e = "Dialogue lifecycle source ObjectDB/recipe identity differs";
    return false;
  }
  return true;
}
bool FieldDialogueLifecycleRuntime::open(const FieldProgrammeData &p,
                                         uint32_t programme,
                                         const FieldProgrammeContext &ctx,
                                         uint32_t generation,
                                         FieldObjectId &out, std::string &e) {
  if (&p != programmes_ || !generation ||
      (phase_ != FieldDialogueLifecyclePhase::Closed &&
       phase_ != FieldDialogueLifecyclePhase::Removed &&
       !(phase_ == FieldDialogueLifecyclePhase::Closing && done_seen_)) ||
      !data_->supports(p, programme, e) || !preflight(ctx, e)) {
    if (e.empty())
      e = "Dialogue lifecycle source open rejected";
    return false;
  }
  FieldDialogueObservation o;
  if (!host_.observe(0, o, e) || !host_.admit_parent(o.stable_canvas, e))
    return false;
  if (retired_.size() >= 64) {
    e = "Dialogue lifecycle closing owner budget exceeded";
    return false;
  }
  if (object_)
    retired_.emplace(object_,
                     Retired{animation_owner_, generation_, closing_wait_});
  was_paused_ = o.player_paused;
  parent_ = o.stable_canvas;
  context_ = ctx;
  generation_ = generation;
  programme_ = programme;
  done_seen_ = closing_wait_ = false;
  nodes_ = {};
  object_ = animation_owner_ = 0;
  if (!was_paused_ &&
      !host_.pause_player(data_->pause_args()[0], data_->pause_args()[1], e))
    return fail(e, "Dialogue lifecycle source player pause rejected");
  if (!tree_->instantiate_recipe(*recipe_, object_, e) || !owned(e) ||
      !host_.factory_created(object_, e))
    return fail(e, "Dialogue lifecycle source recipe instantiate rejected");
  const auto *n = tree_->state(object_);
  if (n->inside || n->ready_notified)
    return fail(e, "Dialogue lifecycle source factory incorrectly entered "
                   "before deferred add");
  context_.dialogue_object = object_;
  nodes_[0] = object_;
  phase_ = FieldDialogueLifecyclePhase::WaitingReady;
  const auto callback_object = object_;
  const auto callback_generation = generation_;
  if (!host_.connect_ready(
          object_, generation_, data_->ready_signal(),
          [this, callback_object, callback_generation]() {
            return ready(callback_object, callback_generation, error_);
          },
          e))
    return fail(e, "Dialogue lifecycle actual Ready waiter rejected");
  if (!run(FieldDialogueStage::OpenCreated, e))
    return false;
  out = object_;
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::admit_ready(FieldObjectId object,
                                                uint32_t generation,
                                                std::string &e) const {
  if (object != object_ || generation != generation_ ||
      phase_ != FieldDialogueLifecyclePhase::Ready || !owned(e)) {
    if (e.empty())
      e = "Dialogue lifecycle actual Ready stale/not resumed";
    return false;
  }
  const auto *n = tree_->state(object_);
  if (!n->inside || !n->ready_notified) {
    e = "Dialogue lifecycle source Ready has not actually fired";
    return false;
  }
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::ready(FieldObjectId object,
                                          uint32_t generation, std::string &e) {
  if (object != object_ || generation != generation_ ||
      phase_ != FieldDialogueLifecyclePhase::WaitingReady || !owned(e)) {
    if (e.empty())
      e = "Dialogue lifecycle stale/duplicate actual Ready";
    return false;
  }
  const auto *root = tree_->state(object_);
  if (!root->inside || !root->ready_notified) {
    e = "Dialogue lifecycle native/script Ready incomplete";
    return false;
  }
  for (uint32_t i = 1; i <= nodes_.size(); ++i) {
    FieldObjectId id = 0;
    if (!tree_->get_node(object_, data_->node(i), id, e))
      return false;
    const auto *n = tree_->state(id);
    if (!n || !n->alive || !n->inside || !n->ready_notified) {
      e = "Dialogue lifecycle original native child Ready incomplete";
      return false;
    }
    const auto *ref = data_->reference(i);
    const auto *descriptor = tree_->descriptor(id);
    if (!ref || !descriptor || n->source != ref->id ||
        descriptor->native_class != ref->native_class ||
        descriptor->ready != ref->ready) {
      e = "Dialogue lifecycle actual factory native ref source differs";
      return false;
    }
    nodes_[i - 1] = id;
  }
  phase_ = FieldDialogueLifecyclePhase::Ready;
  if (!run(FieldDialogueStage::Ready, e) ||
      !host_.start_programme(object_, generation_, e))
    return false;
  if (phase_ != FieldDialogueLifecyclePhase::Running)
    return fail(e,
                "Dialogue lifecycle real programme did not enter source Begin");
  // Source waits for done only after start_from_id returned. Real signal order
  // is registered here; a previously emitted signal is never manufactured.
  const auto callback_object = object_;
  const auto callback_generation = generation_;
  if (!host_.connect_done(
          object_, generation_, data_->done_signal(),
          [this, callback_object, callback_generation](int64_t result) {
            return done(callback_object, callback_generation, result, error_);
          },
          e))
    return fail(e, "Dialogue lifecycle real done waiter rejected");
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::run(FieldDialogueStage stage,
                                        std::string &e) {
  for (const auto &s : data_->steps())
    if (s.stage == stage && !step(s, e))
      return false;
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::step(const FieldDialogueStep &s,
                                         std::string &e) {
  using O = FieldDialogueOp;
  FieldObjectId node = s.role ? nodes_[s.role - 1] : object_;
  switch (s.op) {
  case O::DeferredAdd: {
    FieldDeferredMessage m;
    m.object = parent_;
    m.kind = FieldDeferredKind::Call;
    m.member = s.text;
    m.args = {FieldObjectRef{object_}};
    return tree_->enqueue(std::move(m), e);
  }
  case O::StoreDialogue:
  case O::PauseMenuInactive:
  case O::StackPush:
  case O::UiCutscene:
  case O::KeyClose:
  case O::BlackBars:
  case O::InfoHide:
  case O::CashClose:
  case O::KeyUpdate:
  case O::ClearDialogue:
    return host_.manager(s, object_, e);
  case O::SetProgramme:
    return host_.bind_programme(*programmes_, programme_, object_, context_, e);
  case O::GlobalCutscene:
  case O::SetTalker:
  case O::PhoneLocation:
  case O::EmitCutsceneEnded: {
    FieldObjectId talker =
        s.op == O::SetTalker && s.value != 0 && !context_.thoughts
            ? context_.actor_object
            : 0;
    return host_.global(s, object_, talker, e);
  }
  case O::ConnectName:
  case O::InputRelease:
  case O::ArrowHide:
  case O::VisibleCharacters:
  case O::TextClear:
  case O::BulletClear:
  case O::VoiceVolume:
  case O::TextHide:
  case O::ResetPhrase:
    return host_.native(s, node, e);
  case O::StopTalker: {
    FieldDialogueObservation o;
    if (!host_.observe(object_, o, e))
      return false;
    if (!o.talker || !o.talker_valid)
      return true;
    const auto *n = tree_->state(o.talker);
    if (!n || !n->alive)
      return fail(e,
                  "Dialogue lifecycle valid external talker adapter pending");
    if (n->source != context_.source_npc)
      return fail(e, "Dialogue lifecycle changed talker typed method pending");
    if (!npcs_->stop_interaction(n->source))
      return fail(e, npcs_->error().c_str());
    return true;
  }
  case O::NameClose: {
    FieldDialogueObservation o;
    if (!host_.observe(object_, o, e))
      return false;
    return !o.name_nonempty ||
           host_.play_animation(node, data_->name_close_clip(), e);
  }
  case O::TelepathyRestore:
    return host_.restore_telepathy(e);
  case O::UnpauseIfOwned:
    return was_paused_ || host_.unpause_player(e);
  case O::CloseBox:
    return close(e);
  case O::EmitDone: {
    done_seen_ = false;
    if (!host_.emit_done(object_, s.text, data_->initial_response(), e))
      return false;
    return done_seen_ || fail(e, "Dialogue lifecycle done did not reach actual "
                                 "synchronous waiter");
  }
  case O::ReturnCamera:
    return host_.return_camera(false, s.value, e);
  case O::ReturnOffset:
    return host_.return_camera(true, s.value, e);
  case O::RemoveUi:
    return remove(e);
  }
  return fail(e, "Dialogue lifecycle unknown source operation");
}
bool FieldDialogueLifecycleRuntime::apply(const DialogueAction &a,
                                          const FieldProgrammeContext &ctx,
                                          std::string &e) {
  if (ctx.source_npc != context_.source_npc ||
      ctx.actor_object != context_.actor_object ||
      ctx.dialogue_object != object_ || ctx.thoughts != context_.thoughts ||
      !owned(e))
    return false;
  switch (a.kind) {
  case DialogueActionKind::BeginCutscene:
    if (phase_ != FieldDialogueLifecyclePhase::Ready)
      return fail(e, "Dialogue lifecycle source Begin out of phase");
    if (!run(FieldDialogueStage::Begin, e))
      return false;
    phase_ = FieldDialogueLifecyclePhase::Running;
    return true;
  case DialogueActionKind::StopInteraction:
    if (phase_ != FieldDialogueLifecyclePhase::Running)
      return fail(e, "Dialogue lifecycle source Stop out of phase");
    {
      FieldDialogueObservation o;
      if (!host_.observe(object_, o, e))
        return false;
      if (o.actor_count || o.queued_battle || o.set_respawn)
        return fail(
            e,
            "Dialogue lifecycle mutated actors/battle/respawn branch pending");
    }
    if (!run(FieldDialogueStage::EndPrefix, e))
      return false;
    phase_ = FieldDialogueLifecyclePhase::Stopped;
    return true;
  case DialogueActionKind::SetTalker:
    if (phase_ != FieldDialogueLifecyclePhase::Stopped)
      return fail(e, "Dialogue lifecycle source talker clear out of phase");
    if (!run(FieldDialogueStage::AfterStop, e))
      return false;
    phase_ = FieldDialogueLifecyclePhase::TalkerCleared;
    return true;
  case DialogueActionKind::CutsceneEnded:
    if (phase_ != FieldDialogueLifecyclePhase::TalkerCleared)
      return fail(e, "Dialogue lifecycle source cutscene signal out of phase");
    if (!run(FieldDialogueStage::EndSignal, e))
      return false;
    phase_ = FieldDialogueLifecyclePhase::EndSignal;
    return true;
  case DialogueActionKind::DialogueDone:
    if (phase_ != FieldDialogueLifecyclePhase::EndSignal)
      return fail(e, "Dialogue lifecycle source done out of phase");
    phase_ = FieldDialogueLifecyclePhase::EmittingDone;
    if (!run(FieldDialogueStage::AfterDone, e))
      return false;
    phase_ = closing_wait_ ? FieldDialogueLifecyclePhase::Closing
                           : FieldDialogueLifecyclePhase::Removed;
    return true;
  default:
    return fail(e, "Dialogue lifecycle source opcode unsupported");
  }
}
bool FieldDialogueLifecycleRuntime::done(FieldObjectId object,
                                         uint32_t generation, int64_t response,
                                         std::string &e) {
  if (!data_ || !object || object != object_ || generation != generation_ ||
      response != data_->initial_response() ||
      phase_ != FieldDialogueLifecyclePhase::EmittingDone || done_seen_) {
    e = "Dialogue lifecycle stale/duplicate/non-source done";
    return false;
  }
  if (!run(FieldDialogueStage::ManagerDone, e))
    return false;
  done_seen_ = true;
  e.clear();
  return true;
}
bool FieldDialogueLifecycleRuntime::close(std::string &e) {
  FieldDialogueObservation o;
  if (!host_.observe(object_, o, e))
    return false;
  if (!std::isfinite(o.dialogue_y))
    return fail(e, "Dialogue lifecycle native source box position nonfinite");
  if (o.dialogue_y == data_->closed_y())
    return run(FieldDialogueStage::CloseFinished, e);
  animation_owner_ = nodes_[6];
  closing_wait_ = true;
  if (!host_.play_animation(animation_owner_, data_->close_clip(), e) ||
      !host_.close_sound(e))
    return false;
  const auto callback_object = animation_owner_;
  const auto callback_generation = generation_;
  return host_.connect_animation(
      animation_owner_, generation_, data_->animation_signal(),
      [this, callback_object, callback_generation]() {
        return animation_finished(callback_object, callback_generation, error_);
      },
      e);
}
bool FieldDialogueLifecycleRuntime::animation_finished(FieldObjectId object,
                                                       uint32_t generation,
                                                       std::string &e) {
  for (auto &entry : retired_) {
    auto &job = entry.second;
    if (job.animation != object || job.generation != generation)
      continue;
    if (!job.waiting) {
      e = "Dialogue lifecycle duplicate retired animation signal";
      return false;
    }
    FieldIdentity identity{};
    if (!tree_->object_identity(entry.first, identity) ||
        identity.upstream_commit != data_->commit() ||
        identity.source_sha256 != data_->scene_sha()) {
      e = "Dialogue lifecycle retired source owner identity differs";
      return false;
    }
    if (!remove_object(entry.first, e))
      return false;
    job.waiting = false;
    e.clear();
    return true;
  }
  if (object != animation_owner_ || generation != generation_ ||
      !closing_wait_ || phase_ != FieldDialogueLifecyclePhase::Closing ||
      !owned(e)) {
    if (e.empty())
      e = "Dialogue lifecycle stale/unawaited actual animation signal";
    return false;
  }
  // GDScript yield(animation_finished) resumes on the actual signal, including
  // a subsequently replaced animation; it is not filtered to a guessed name.
  closing_wait_ = false;
  if (!run(FieldDialogueStage::CloseFinished, e))
    return false;
  phase_ = FieldDialogueLifecyclePhase::Removed;
  return true;
}
bool FieldDialogueLifecycleRuntime::remove(std::string &e) {
  return remove_object(object_, e);
}
bool FieldDialogueLifecycleRuntime::remove_object(FieldObjectId owner,
                                                  std::string &e) {
  FieldDialogueObservation observation;
  if (!host_.observe(owner, observation, e))
    return false;
  // Original remove_ui does nothing if another caller already erased the stack.
  if (!observation.in_ui_stack)
    return true;
  auto it = std::find_if(
      data_->steps().begin(), data_->steps().end(),
      [](const auto &s) { return s.op == FieldDialogueOp::RemoveUi; });
  if (it == data_->steps().end() || !host_.manager(*it, owner, e))
    return false;
  // remove_ui erases the actual source stack then close_item chooses queue_free
  // (DialogueBox has no close method). Native global delete queue owns
  // teardown.
  return tree_->queue_free(owner, e);
}
bool FieldDialogueLifecycleRuntime::deleted(FieldObjectId object,
                                            std::string &e) {
  if (!data_ || !object) {
    e = "Dialogue lifecycle delete has no actual owner";
    return false;
  }
  auto retired = retired_.find(object);
  if (retired != retired_.end()) {
    if (!host_.disconnect(object, retired->second.generation, e))
      return false;
    retired_.erase(retired);
    e.clear();
    return true;
  }
  if (object != object_) {
    e = "Dialogue lifecycle deleting unknown source owner";
    return false;
  }
  if (!host_.disconnect(object_, generation_, e))
    return false;
  closing_wait_ = false;
  object_ = animation_owner_ = 0;
  nodes_ = {};
  phase_ = FieldDialogueLifecyclePhase::Closed;
  e.clear();
  return true;
}
} // namespace encore::upstream
