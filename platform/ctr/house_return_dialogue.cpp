#include "house_return_dialogue.hpp"
#include <algorithm>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string &e, const char *s) { e = s; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool same(const RoomCommand &a, const DialogueAction &b) {
  return a.opcode == uint16_t(b.kind) && a.actor_index == b.actor &&
         a.phrase == b.phrase && a.target_index == b.target_index &&
         a.auxiliary_index == b.auxiliary_index && a.flags == b.flags &&
         a.vector.x == b.vector.x && a.vector.y == b.vector.y &&
         a.value == b.value && a.duration == b.duration;
}
struct Call {
  bool &active;
  explicit Call(bool &v) : active(v) { active = true; }
  ~Call() { active = false; }
};
bool text(HouseView house, uint32_t id, HouseDialogue &out, std::string &e) {
  bool found = false;
  for (uint32_t i = 0; i < house.count(HouseSection::Dialogues); ++i) {
    const auto record = house.dialogue(i);
    if (record.id != id) continue;
    if (found || !record.segment_count ||
        record.first_segment > house.count(HouseSection::Segments) ||
        record.segment_count > house.count(HouseSection::Segments) - record.first_segment ||
        house.string(record.source_path).empty())
      return reject(e, "House dialogue text has duplicate or incomplete source segments");
    out = record; found = true;
  }
  if (!found) return reject(e, "House dialogue stable text ID is absent from the actual source");
  e.clear(); return true;
}
} // namespace

bool HouseReturnDialogue::fail(std::string &e, const char *s) {
  phase_ = Phase::Failed;
  return reject(e, s);
}
bool HouseReturnDialogue::actual(bool require_ready, std::string &e) const {
  const auto &in = input_;
  if (!in.registry || !in.registry->data() || !in.tree || !in.house ||
      !in.text.valid() || !in.source || !in.source->valid() || !in.doors ||
      !in.source_tree || !in.source_tree->valid() ||
      !in.lifecycle || !in.lifecycle->valid() || !in.recipe ||
      !in.recipe->valid() || !in.session || !in.ui || !in.script ||
      !in.random || !in.uid_ledger || !in.native ||
      !in.choice_data || !in.choice_data->valid() || !in.choices ||
      !in.session->source_scene_retired() || !in.house->world.healthy())
    return reject(e, "House dialogue requires actual retained and retired source owners");
  if ((phase_ == Phase::Unbound && in.house->world.house_programme_owner()) ||
      (phase_ != Phase::Unbound && in.house->world.house_programme_owner() != this))
    return reject(e, "House dialogue lost its exact typed World source owner binding");
  const auto room = in.house->world.content();
  if (!in.source->matches(*in.doors, room, in.text, e)) return false;
  const auto id = in.tree->root();
  const auto *node = in.tree->state(id);
  const auto *desc = in.tree->descriptor(id);
  FieldIdentity identity{};
  std::array<uint8_t, 32> scene_hash{};
  const auto &nodes = in.source->native_nodes();
  if (!id || !node || !desc || !node->alive || !node->bound || node->queued ||
      in.registry->tree_owner(id) != in.tree ||
      !in.tree->object_identity(id, identity) ||
      !same(identity, in.source_tree->identity()) || nodes.empty() ||
      identity.upstream_commit != in.source->identity().upstream_commit ||
      identity.source_sha256 != in.source->identity().source_sha256 ||
      in.source_tree->source_scene() != in.source->target_scene() ||
      !in.source_tree->source_hash(in.source->target_scene(), scene_hash) ||
      scene_hash != identity.source_sha256 ||
      desc->id != nodes.front().id || desc->path != nodes.front().node ||
      desc->native_class != nodes.front().native_class ||
      desc->script != nodes.front().script || desc->script_sha != nodes.front().script_sha ||
      node->name != in.source->target_root_name() ||
      room.string(room.scene().source_scene_string) != "res://" + in.source->target_scene() ||
      in.lifecycle->commit() != identity.upstream_commit ||
      in.recipe->identity().upstream_commit != identity.upstream_commit ||
      in.lifecycle->factory_ir_sha() != in.recipe->ir_sha256() ||
      in.lifecycle->factory_scene_id() != in.recipe->identity().scene_id ||
      in.lifecycle->factory_node_count() != in.recipe->records().size())
    return reject(e, "House dialogue source/tree/factory identity changed");
  if (require_ready && (!node->inside || !node->ready_notified || node->ready_first ||
                        in.house->scene_ready_pending() ||
                        in.registry->current_scene() != id))
    return reject(e, "House dialogue cannot open before actual mapped House Ready");
  if (context_.house && (context_.house != in.house || context_.house_root != id ||
      context_.room.bytes() != room.bytes() ||
      context_.room.byte_size() != room.byte_size() ||
      context_.text.bytes() != in.text.bytes() ||
      context_.text.byte_size() != in.text.byte_size()))
    return reject(e, "House dialogue lost its fixed Room/House resource borrow");
  e.clear(); return true;
}
bool HouseReturnDialogue::receipt(bool closed, HouseReturnDialogueReceipt &out,
                                  std::string &e) const {
  HouseReturnDialogueReceipt r;
  if (!input_.native->observe(context_, r, e)) return false;
  if (!r.inspected || !r.podunk_retired || !r.world_generation ||
      r.world_generation != input_.house->world.source_generation() ||
      r.registry != input_.registry ||
      r.lifecycle_tree != input_.tree.get() || r.house != input_.house ||
      r.world != &input_.house->world || r.runtime != &input_.house->house ||
      r.printer != &input_.house->presentation || r.session != input_.session ||
      r.script != input_.script || r.random != input_.random ||
      r.uid_ledger != input_.uid_ledger || r.choices != input_.choices ||
      r.lifecycle_phase == FieldDialogueLifecyclePhase::Error ||
      r.ready_waiting != (r.lifecycle_phase == FieldDialogueLifecyclePhase::WaitingReady))
    return reject(e, "House dialogue native callback/programme ownership is incomplete");
  HouseUiDialogueState ui;
  if (!input_.ui->source_dialogue_state(ui, e)) return false;
  if (!ui.stable_canvas || r.canvas != ui.stable_canvas ||
      r.talker != ui.talker ||
      (context_.canvas && context_.canvas != ui.stable_canvas) ||
      input_.registry->tree_owner(ui.stable_canvas) != input_.tree)
    return reject(e, "House dialogue did not retain the actual source Canvas");
  std::set<FieldObjectId> objects;
  for (auto id : r.dialogue_objects) {
    auto tree = input_.registry->tree_owner(id);
    const auto *node = tree ? tree->state(id) : nullptr;
    FieldIdentity identity{};
    if (!id || !objects.insert(id).second || !node || !node->alive ||
        !node->bound || !tree->object_identity(id, identity) ||
        !same(identity, input_.recipe->identity()) ||
        !input_.recipe->record(node->source))
      return reject(e, "House dialogue native receipt contains a foreign or stale object");
  }
  if (closed) {
    const std::set<FieldObjectId> targets(r.message_targets.begin(), r.message_targets.end());
    if (!r.source_closed || r.callback_depth || r.pending_callbacks || r.ready_waiting ||
        r.lifecycle_phase != FieldDialogueLifecyclePhase::Closed || r.pending_choice_events ||
        !r.dialogue_objects.empty() || !r.wait_receivers.empty() || r.dialogue ||
        ui.current_dialogue || !ui.stack.empty() || ui.cutscene || ui.talker ||
        input_.choices->phase() != DialogueChoicesPhase::Closed ||
        input_.registry->pending_messages_to(targets) ||
        !input_.house->presentation.source_frame_closed())
      return reject(e, "House dialogue source has live or uninspected callbacks/waits");
  } else if (r.source_closed || r.dialogue != context_.dialogue ||
             r.programme != context_.programme ||
             r.generation != context_.generation ||
             (phase_ == Phase::Running && r.world_generation != context_.generation) ||
             !objects.count(context_.dialogue) ||
             ui.current_dialogue != context_.dialogue ||
             std::count(ui.stack.begin(), ui.stack.end(), context_.dialogue) != 1) {
    return reject(e, "House dialogue lifecycle no longer owns the actual Room programme");
  }
  out = std::move(r); e.clear(); return true;
}
bool HouseReturnDialogue::bind(HouseReturnDialogueInput in, std::string &e) {
  if (phase_ != Phase::Unbound || world_call_ || action_call_)
    return reject(e, "House dialogue fixed owner was already bound");
  input_ = std::move(in);
  context_ = {};
  // Install concrete native receiver/context ports before old House retirement;
  // bind this driver after actual mapped House Ready, never as its replacement.
  if (!actual(true, e)) return false;
  context_.house = input_.house; context_.room = input_.house->world.content();
  context_.text = input_.text; context_.house_root = input_.tree->root();
  HouseReturnDialogueReceipt r;
  if (!receipt(true, r, e)) return false;
  context_.canvas = r.canvas;
  if (!input_.house->world.bind_house_programme_owner(*this, e)) return false;
  phase_ = Phase::Closed; e.clear(); return true;
}
const OpeningWorld *HouseReturnDialogue::world() const {
  return input_.house ? &input_.house->world : nullptr;
}
bool HouseReturnDialogue::source_frame_closed(const OpeningWorld &w,
                                              std::string &e) const {
  if (&w != world() || world_call_ || action_call_ ||
      (phase_ != Phase::Unbound && phase_ != Phase::Closed) ||
      (phase_ == Phase::Unbound && w.house_programme_owner()) ||
      (phase_ == Phase::Closed && w.house_programme_owner() != this))
    return reject(e, "House dialogue closed-frame admission has a foreign or live source owner");
  if (!actual(true, e)) return false;
  HouseReturnDialogueReceipt r;
  return receipt(true, r, e);
}
bool HouseReturnDialogue::unbind(std::string &e) {
  if (phase_ != Phase::Closed || !input_.house || world_call_ || action_call_ ||
      input_.house->world.house_programme_owner() != this)
    return reject(e, "House dialogue unbind requires its exact actually closed World owner");
  if (!input_.house->world.unbind_house_programme_owner(*this, e)) return false;
  // Only a closed driver's pointer borrows are released. Native/source owners,
  // callback history, queues and waits remain untouched.
  input_ = {}; context_ = {}; phase_ = Phase::Unbound;
  e.clear(); return true;
}
bool HouseReturnDialogue::programme_binding(PodunkDialogueProgrammeBinding &out,
                                            std::string &e) const {
  if (!actual(true, e)) return false;
  PodunkDialogueProgrammeBinding binding;
  binding.identity = input_.source_tree->identity();
  binding.vm_owner = &input_.house->world;
  binding.printer = &input_.house->presentation;
  binding.choices = input_.choices;
  out = binding; e.clear(); return true;
}
bool HouseReturnDialogue::observe_house_programme(HouseProgrammeState &out,
                                                 std::string &e) const {
  if (phase_ == Phase::Unbound || phase_ == Phase::Failed || !actual(true, e))
    return reject(e, "House programme snapshot has no healthy actual bound source owner");
  HouseReturnDialogueReceipt native;
  const bool closed = phase_ == Phase::Closed;
  // Opening is synchronous only inside the real factory/source UI prefix.
  // It does not grant a dialogue lease until the actual ObjectID is returned.
  if (!closed && !context_.dialogue)
    return reject(e, "House programme source factory has not published its actual dialogue lease");
  const auto &w = input_.house->world;
  if (!receipt(closed, native, e)) {
    // The actual VM can finish and delete Root before the retained source
    // Fade restoration coroutine unwinds. This observer grants only that
    // existing closing lease; dialogue context/Ready/input admission stays
    // strict and still requires the actual live Root.
    if (phase_ != Phase::Running || w.story_status() != DialogueStatus::Completed ||
        !context_.generation || w.story_generation() != context_.generation ||
        w.story_program_index() != context_.programme ||
        !input_.native->observe(context_, native, e)) return false;
    PodunkMickHouseNativeState owners;
    HouseUiDialogueState ui;
    if (!input_.session->house_native_state(*input_.house, owners, e) ||
        !owners.native || !owners.business_pending ||
        !owners.native->observes_closed_printer(input_.house->presentation, e) ||
        !input_.ui->source_dialogue_state(ui, e)) return false;
    const std::set<FieldObjectId> targets(native.message_targets.begin(), native.message_targets.end());
    if (!native.inspected || !native.podunk_retired || native.source_closed ||
        native.lifecycle_phase != FieldDialogueLifecyclePhase::Closed ||
        native.registry != input_.registry || native.lifecycle_tree != input_.tree.get() ||
        native.house != input_.house || native.world != &w ||
        native.runtime != &input_.house->house || native.printer != &input_.house->presentation ||
        native.session != input_.session || native.script != input_.script ||
        native.random != input_.random || native.uid_ledger != input_.uid_ledger ||
        native.choices != input_.choices || native.world_generation != w.source_generation() ||
        native.generation != context_.generation || native.programme != context_.programme ||
        native.dialogue || native.ready_waiting || native.callback_depth ||
        native.input_callback || native.choice_callback || native.choice_event_consumed ||
        native.selected_choice_pc != kRoomNoIndex ||
        native.pending_callbacks != 1 || native.pending_choice_events ||
        !native.dialogue_objects.empty() || !native.wait_receivers.empty() ||
        !targets.count(context_.dialogue) || input_.registry->object_exists(context_.dialogue) ||
        input_.registry->pending_messages_to(targets) ||
        ui.current_dialogue || !ui.stack.empty() || ui.cutscene || ui.talker ||
        native.canvas != ui.stable_canvas || native.talker ||
        input_.registry->tree_owner(ui.stable_canvas) != input_.tree ||
        input_.choices->phase() != DialogueChoicesPhase::Closed)
      return reject(e, "House programme retiring receipt has unknown or stale source work");
  }
  HouseProgrammeState state;
  state.runtime = &input_.house->house;
  state.world = &w;
  state.world_owner = w.house_programme_owner();
  state.printer = &input_.house->presentation;
  state.choices = input_.choices;
  state.room = w.content(); state.house = input_.text;
  state.vm_generation = w.story_generation();
  state.request_generation = closed ? 0 : context_.generation;
  state.programme = closed ? w.story_program_index() : context_.programme;
  const auto talker = w.story_talker();
  state.original_npc = closed
      ? (talker.kind == DialogueTalkerKind::OriginalNpc ? talker.index : kRoomNoIndex)
      : context_.original_npc;
  switch (phase_) {
  case Phase::Closed: state.phase = HouseProgrammePhase::Closed; break;
  case Phase::Opening: state.phase = HouseProgrammePhase::Opening; break;
  case Phase::WaitingReady: state.phase = HouseProgrammePhase::WaitingReady; break;
  case Phase::Starting: state.phase = HouseProgrammePhase::Starting; break;
  case Phase::Running: state.phase = HouseProgrammePhase::Running; break;
  default: return reject(e, "House programme snapshot encountered an unsupported source phase");
  }
  state.native_closed = closed && native.source_closed;
  if (state.world_owner != this ||
      (closed && (!state.native_closed ||
          (w.story_status() != DialogueStatus::Idle && w.story_status() != DialogueStatus::Completed))) ||
      (!closed && (!state.request_generation || native.generation != state.request_generation ||
                   native.programme != state.programme)) ||
      (phase_ == Phase::Running && (state.vm_generation != state.request_generation ||
                                   w.story_program_index() != state.programme)))
    return reject(e, "House programme snapshot differs from actual native/World source ownership");
  out = state; e.clear(); return true;
}
bool HouseReturnDialogue::dialogue_context(PodunkDialogueProgrammeContext &out,
                                           std::string &e) const {
  if ((phase_ != Phase::WaitingReady && phase_ != Phase::Starting && phase_ != Phase::Running) ||
      !actual(true, e))
    return reject(e, "House dialogue context has no actual source programme/factory");
  HouseReturnDialogueReceipt r;
  if (!receipt(false, r, e)) return false;
  PodunkDialogueProgrammeContext context;
  context.dialogue_object = context_.dialogue;
  context.actor_object = r.talker;
  // HouseRuntime's actual begin_house_program callers open ordinary dialogue;
  // no thoughts/telepathy path is inferred from a null talker.
  context.thoughts = false;
  context.generation = context_.generation;
  out = context; e.clear(); return true;
}
bool HouseReturnDialogue::dialogue_status(DialogueStatus &out,
                                          std::string &e) const {
  if (!actual(true, e)) return false;
  out = input_.house->world.story_status(); e.clear(); return true;
}
bool HouseReturnDialogue::ready_waiting(bool &out, std::string &e) const {
  if (!actual(true, e)) return false;
  HouseReturnDialogueReceipt r;
  const bool closed = phase_ == Phase::Closed || phase_ == Phase::Unbound;
  if (!receipt(closed, r, e)) return false;
  if ((phase_ == Phase::WaitingReady) != r.ready_waiting ||
      (r.ready_waiting && r.wait_receivers.empty()))
    return reject(e, "House dialogue Ready phase has no matching actual source waiter");
  out = r.ready_waiting; e.clear(); return true;
}
bool HouseReturnDialogue::source_hash(std::string_view source,
                                      std::array<uint8_t, 32> &out) const {
  if (!input_.source || !input_.source->valid() || !input_.source_tree ||
      !input_.source_tree->valid() || !input_.recipe || !input_.recipe->valid() ||
      !input_.lifecycle || !input_.lifecycle->valid() ||
      input_.source_tree->identity().upstream_commit != input_.source->identity().upstream_commit ||
      input_.recipe->identity().upstream_commit != input_.source->identity().upstream_commit ||
      input_.lifecycle->commit() != input_.source->identity().upstream_commit)
    return false;
  bool found = false;
  std::array<uint8_t, 32> hash{}, candidate{};
  auto accept = [&](bool present) {
    if (!present) return true;
    if (found && candidate != hash) return false;
    hash = candidate; found = true; return true;
  };
  // The retained actual DialogueBox recipe/lifecycle owns its shared inherited
  // scripts. House scene resources own House scripts. Disagreement rejects.
  if (!accept(input_.source->source_hash(source, candidate)) ||
      !accept(input_.source_tree->source_hash(source, candidate)) ||
      !accept(input_.recipe->source_hash(source, candidate)) ||
      !accept(input_.lifecycle->source_hash(source, candidate)) || !found)
    return false;
  out = hash; return true;
}
bool HouseReturnDialogue::request(uint32_t programme, uint32_t npc,
                                  std::string &e) {
  if (phase_ != Phase::Closed || world_call_ || action_call_ || !actual(true, e))
    return reject(e, "House dialogue opening requires a closed actual source frame");
  HouseReturnDialogueReceipt previous;
  if (!receipt(true, previous, e)) return false;
  auto next = context_;
  next.dialogue = 0; next.talker = 0;
  next.generation = previous.world_generation + 1;
  if (!next.generation) ++next.generation;
  next.programme = programme; next.original_npc = npc;
  if (programme >= next.room.program_count() ||
      input_.house->world.stage() != OpeningStage::Walking)
    return reject(e, "House dialogue programme is outside the actual walking Room");
  const auto p = next.room.program(programme);
  if (!p.stable_id || !p.command_count ||
      p.first_command > next.room.command_count() ||
      p.command_count > next.room.command_count() - p.first_command ||
      next.room.string(p.source_path_string).empty())
    return reject(e, "House dialogue programme source span is incomplete");
  if (npc != kRoomNoIndex) {
    if (npc >= input_.text.count(HouseSection::Npcs))
      return reject(e, "House dialogue talker is outside the actual House NPC source");
    const auto n = input_.text.npc(npc);
    const HouseReentryActor *binding = nullptr;
    for (const auto &a : input_.source->actors()) if (a.house_index == npc) {
      if (binding) return reject(e, "House dialogue talker has duplicate source bindings");
      binding = &a;
    }
    if (!binding || binding->body != n.body_id ||
        binding->room_index != n.room_actor_index ||
        n.room_actor_index >= next.room.actor_instance_count() ||
        next.room.actor_instance(n.room_actor_index).stable_id != binding->id)
      return reject(e, "House dialogue talker has no actual cross-bound House owner");
    if (!input_.tree->get_node(context_.house_root, binding->node, next.talker, e))
      return false;
    const auto *node = input_.tree->state(next.talker);
    const auto *desc = input_.tree->descriptor(next.talker);
    FieldIdentity identity{};
    if (!next.talker || !node || !desc || !node->alive || !node->inside ||
        !node->ready_notified || node->queued || !node->bound ||
        desc->path != binding->node || !input_.source_tree->record(desc->id) ||
        input_.source_tree->record(desc->id)->path != binding->node ||
        input_.registry->tree_owner(next.talker) != input_.tree ||
        !input_.tree->object_identity(next.talker, identity) ||
        !same(identity, input_.source_tree->identity()))
      return reject(e, "House dialogue talker source object is not actually Ready");
  }
  const auto state = input_.random->state(), draws = input_.random->raw_draw_count();
  const auto uids = *input_.uid_ledger;
  for (uint32_t pc = 0; pc < p.command_count; ++pc) {
    const auto command = next.room.command(p.first_command + pc);
    if (command.opcode > uint16_t(DialogueActionKind::AnimateSpecialActor))
      return reject(e, "House dialogue contains an unknown source command");
    if (command.opcode == uint16_t(DialogueActionKind::ShowDialogue)) {
      HouseDialogue selected;
      if (!text(next.text, command.target_index, selected, e)) return false;
    }
    if (command.opcode == uint16_t(DialogueActionKind::AwaitChoices)) {
      if (!input_.choice_data->validate_program(command.target_index,
              next.room.string(p.source_path_string), p.command_count, e)) return false;
      const auto &group = input_.choice_data->groups()[command.target_index];
      auto phrase = [&](uint32_t target) {
        return target > pc && target < p.command_count &&
            next.room.command(p.first_command + target - 1).phrase !=
            next.room.command(p.first_command + target).phrase;
      };
      if (!phrase(group.cancel_target_pc))
        return reject(e, "House dialogue choice cancel is not a source forward phrase");
      for (const auto &option : group.options) if (!phrase(option.target_pc))
        return reject(e, "House dialogue choice target is not a source forward phrase");
    }
    if (!input_.native->admit_command(next, pc, command, e)) return false;
  }
  if (state != input_.random->state() || draws != input_.random->raw_draw_count() ||
      uids != *input_.uid_ledger)
    return fail(e, "House dialogue read-only source admission mutated entropy/UID owners");
  context_ = next; phase_ = Phase::Opening;
  FieldObjectId id = 0;
  if (!input_.native->open(context_, id, e) || !id) {
    phase_ = Phase::Failed;
    if (e.empty()) e = "House dialogue actual factory/open failed";
    return false;
  }
  context_.dialogue = id; phase_ = Phase::WaitingReady;
  HouseReturnDialogueReceipt r;
  if (!receipt(false, r, e)) { phase_ = Phase::Failed; return false; }
  e.clear(); return true;
}
bool HouseReturnDialogue::native_ready(FieldObjectId id, std::string &e) {
  if (phase_ != Phase::WaitingReady || world_call_ || action_call_ ||
      id != context_.dialogue || !actual(true, e))
    return reject(e, "House dialogue Ready callback is stale or reentrant");
  HouseReturnDialogueReceipt r; HouseUiDialogueState ui;
  PodunkDialogueScriptState script;
  if (!receipt(false, r, e) || !input_.ui->source_dialogue_state(ui, e) ||
      !input_.script->state(id, script, e)) return false;
  auto tree = input_.registry->tree_owner(id);
  const auto *node = tree ? tree->state(id) : nullptr;
  if (!node || !node->alive || !node->inside || !node->ready_notified ||
      node->ready_first || node->queued || !ui.current_inside || !ui.current_ready ||
      script.object != id || !script.constructed || !script.entered || !script.ready ||
      script.printer != &input_.house->presentation ||
      !same(script.identity, input_.recipe->identity()))
    return reject(e, "House dialogue cannot start from incomplete actual native/script Ready");
  for (size_t i = 0; i < script.references.size(); ++i) {
    const auto ref = script.references[i];
    const auto *source = input_.lifecycle->reference(uint32_t(i + 1));
    auto owner = input_.registry->tree_owner(ref);
    const auto *actual = owner ? owner->state(ref) : nullptr;
    if (!source || !ref || !actual || !actual->alive || !actual->bound ||
        !actual->inside || !actual->ready_notified || actual->queued ||
        actual->source != source->id || owner != tree ||
        std::count(r.dialogue_objects.begin(), r.dialogue_objects.end(), ref) != 1)
      return reject(e, "House dialogue Ready lacks an actual registered native onready reference");
  }
  phase_ = Phase::Starting;
  Call call(world_call_);
  return world_result(input_.house->world.begin_house_program(
                          context_.programme, context_.original_npc), e);
}
bool HouseReturnDialogue::source_started(OpeningWorld &w, uint32_t generation,
                                         std::string &e) {
  if (phase_ != Phase::Starting || !world_call_ || action_call_ || !generation ||
      &w != world() || w.house_programme_owner() != this ||
      generation != w.source_generation() ||
      generation != context_.generation || !actual(true, e) ||
      input_.house->world.story_program_index() != context_.programme)
    return fail(e, "House dialogue actual World start boundary was not observed");
  const auto talker = w.story_talker();
  if ((context_.original_npc == kRoomNoIndex && talker.kind != DialogueTalkerKind::None) ||
      (context_.original_npc != kRoomNoIndex &&
       (talker.kind != DialogueTalkerKind::OriginalNpc || talker.index != context_.original_npc)))
    return fail(e, "House dialogue actual World start talker differs from its source request");
  if (!input_.native->source_started(context_, e)) { phase_ = Phase::Failed; return false; }
  HouseReturnDialogueReceipt r;
  if (!receipt(false, r, e) || r.world_generation != generation) {
    phase_ = Phase::Failed;
    if (e.empty()) e = "House dialogue Ready waiter/source World generation changed";
    return false;
  }
  phase_ = Phase::Running; e.clear(); return true;
}
bool HouseReturnDialogue::before_action(OpeningWorld &w, const DialogueAction &action,
                                        std::string &e) {
  if (phase_ != Phase::Running || action_call_ || &w != world() ||
      w.house_programme_owner() != this || w.source_generation() != context_.generation ||
      !actual(true, e))
    return fail(e, "House dialogue action has no actual running Room owner");
  const auto &world = input_.house->world;
  const auto p = context_.room.program(context_.programme);
  const auto next = world.story_next_command_index();
  if (!context_.generation || world.story_generation() != context_.generation ||
      world.story_program_index() != context_.programme || !next || next > p.command_count ||
      !same(context_.room.command(p.first_command + next - 1), action))
    return fail(e, "House dialogue action does not match the existing VM source cursor");
  Call call(action_call_);
  if (!input_.native->apply_native(context_, next - 1, action, e)) {
    phase_ = Phase::Failed; return false;
  }
  e.clear(); return true;
}
bool HouseReturnDialogue::current(FieldObjectId id, uint32_t generation,
                                  std::string &e) const {
  if (phase_ != Phase::Running || world_call_ || action_call_ ||
      !id || id != context_.dialogue || !generation || generation != context_.generation ||
      !actual(true, e) || input_.house->world.story_generation() != generation ||
      input_.house->world.story_program_index() != context_.programme)
    return reject(e, "House dialogue source input/options callback is stale or reentrant");
  HouseReturnDialogueReceipt r;
  return receipt(false, r, e);
}
bool HouseReturnDialogue::world_result(bool ok, std::string &e) {
  if (!ok || phase_ != Phase::Running || !input_.house->world.healthy() ||
      input_.house->world.story_generation() != context_.generation ||
      input_.house->world.story_program_index() != context_.programme) {
    phase_ = Phase::Failed;
    if (e.empty()) e = ok ? "House dialogue World hooks did not commit the actual programme"
                         : input_.house->world.error();
    if (e.empty()) e = "House dialogue existing World continuation rejected";
    return false;
  }
  e.clear(); return true;
}
bool HouseReturnDialogue::text_completed(std::string &e) {
  if (!current(context_.dialogue, context_.generation, e)) return false;
  if (!input_.house->presentation.dialogue_finished())
    return reject(e, "House dialogue text callback has no actual same-printer completion");
  if (input_.house->world.story_status() != DialogueStatus::AwaitChoices) {
    e.clear(); return true;
  }
  const auto &world = input_.house->world;
  const auto p = context_.room.program(context_.programme);
  const auto *group = input_.choices->group();
  if (input_.choices->phase() != DialogueChoicesPhase::WaitingText ||
      input_.choices->data() != input_.choice_data || !group ||
      world.pending_choice_group() >= input_.choice_data->groups().size() ||
      group != &input_.choice_data->groups()[world.pending_choice_group()] ||
      group->program_identity != context_.room.string(p.source_path_string) ||
      group->program_command_count != p.command_count)
    return reject(e, "House dialogue text completion has no matching actual waiting choices");
  if (!input_.choices->text_completed(e)) return false;
  input_.house->presentation.set_choice_rows(input_.choice_data->trailing_blank_lines());
  e.clear(); return true;
}
bool HouseReturnDialogue::advance(bool confirm, bool cancel, std::string &e) {
  if (!current(context_.dialogue, context_.generation, e)) return false;
  HouseReturnDialogueReceipt r;
  if (!receipt(false, r, e)) return false;
  if (input_.house->world.story_status() != DialogueStatus::AwaitDialogue ||
      !r.callback_depth || !r.input_callback || r.choice_callback)
    return reject(e, "House dialogue ordinary input has no actual source input suspension");
  // Same source printer input and advance bit as the existing programme host.
  // Printing/inline wait may consume input without advancing the actual VM.
  input_.house->presentation.input(confirm, cancel, true, confirm);
  if (!input_.house->presentation.take_dialogue_advance()) { e.clear(); return true; }
  return advance(context_.dialogue, context_.generation, false, e);
}
bool HouseReturnDialogue::source_cursor_input(uint64_t id, uint32_t generation,
                                              int32_t index, bool confirm, bool cancel,
                                              std::string &e) {
  if (!current(id, generation, e)) return false;
  HouseReturnDialogueReceipt before;
  if (!receipt(false, before, e)) return false;
  const auto &world = input_.house->world;
  const auto p = context_.room.program(context_.programme);
  const auto *group = input_.choices->group();
  if (!world.story_choices_waiting() || !before.choice_callback || !before.callback_depth ||
      before.cursor_index != index || (!confirm && !cancel) || index < 0 ||
      input_.choices->phase() != DialogueChoicesPhase::Active ||
      input_.choices->data() != input_.choice_data || !group ||
      uint32_t(index) >= group->options.size() ||
      uint32_t(index) >= input_.choice_data->child_count() ||
      world.pending_choice_group() >= input_.choice_data->groups().size() ||
      group != &input_.choice_data->groups()[world.pending_choice_group()] ||
      group->program_identity != context_.room.string(p.source_path_string) ||
      group->program_command_count != p.command_count)
    return reject(e, "House dialogue Cursor callback has no matching actual visible source option");
  const auto expected = cancel ? group->cancel_target_pc : group->options[uint32_t(index)].target_pc;
  uint32_t selected = kRoomNoIndex;
  if (!input_.native->select_choices(context_, index, confirm, cancel, selected, e)) {
    phase_ = Phase::Failed; return false;
  }
  if (selected != expected)
    return fail(e, "House dialogue actual selected event differs from the source Cursor result");
  if (!choose(id, generation, selected, e)) return false;
  if (!input_.native->choice_target_committed(context_, e)) {
    phase_ = Phase::Failed; return false;
  }
  e.clear(); return true;
}
bool HouseReturnDialogue::advance(FieldObjectId id, uint32_t generation,
                                  bool automatic, std::string &e) {
  if (!current(id, generation, e)) return false;
  if (!input_.house->world.story_input_allowed() ||
      (automatic && !input_.house->world.story_auto_advance_ready()))
    return reject(e, "House dialogue input cannot pass the actual source suspension");
  Call call(world_call_);
  return world_result(input_.house->world.finish_story_dialogue(automatic), e);
}
bool HouseReturnDialogue::choose(FieldObjectId id, uint32_t generation,
                                 uint32_t pc, std::string &e) {
  if (!current(id, generation, e)) return false;
  HouseReturnDialogueReceipt r;
  if (!receipt(false, r, e)) return false;
  const auto &world = input_.house->world;
  const auto p = context_.room.program(context_.programme);
  const auto *group = input_.choices->group();
  if (!world.story_choices_waiting() || !r.choice_callback || !r.callback_depth ||
      !r.choice_event_consumed || r.pending_choice_events ||
      r.selected_choice_pc != pc || input_.choices->phase() != DialogueChoicesPhase::Resolved ||
      input_.choices->data() != input_.choice_data || !group ||
      world.pending_choice_group() >= input_.choice_data->groups().size() ||
      group != &input_.choice_data->groups()[world.pending_choice_group()] ||
      group->program_identity != context_.room.string(p.source_path_string) ||
      group->program_command_count != p.command_count)
    return reject(e, "House dialogue choices callback has no actual choice suspension");
  const bool target = pc == group->cancel_target_pc ||
      std::any_of(group->options.begin(), group->options.end(),
                  [&](const auto &option) { return option.target_pc == pc; });
  if (!target || pc < world.story_next_command_index() || pc >= p.command_count)
    return reject(e, "House dialogue choices callback is outside its actual source menu");
  // The source consumer already consumed the real Selected event and proved
  // the queue empty; original options close precedes dispatch of its target.
  input_.choices->close();
  Call call(world_call_);
  return world_result(input_.house->world.choose_story_option(pc, generation), e);
}
bool HouseReturnDialogue::submenu_closed(FieldObjectId id, uint32_t generation,
                                         std::string &e) {
  if (!current(id, generation, e)) return false;
  if (!input_.house->world.story_submenu_waiting())
    return reject(e, "House dialogue submenu callback has no actual submenu suspension");
  Call call(world_call_);
  return world_result(input_.house->world.close_story_submenu(generation), e);
}
bool HouseReturnDialogue::source_text(uint32_t id, HouseDialogue &out,
                                     std::string &e) const {
  if (phase_ != Phase::Running || !actual(true, e))
    return reject(e, "House dialogue text requires its actual running Room owner");
  return text(context_.text, id, out, e);
}
bool HouseReturnDialogue::native_closed(FieldObjectId id, uint32_t generation,
                                        std::string &e) {
  if (phase_ != Phase::Running || world_call_ || action_call_ ||
      id != context_.dialogue || generation != context_.generation || !generation ||
      !actual(true, e) || !input_.house->world.story_completed() ||
      input_.house->world.story_generation() != generation ||
      input_.house->world.story_program_index() != context_.programme)
    return reject(e, "House dialogue closing lacks actual completed source generation");
  HouseReturnDialogueReceipt r;
  if (!receipt(true, r, e)) return false;
  // Clear only this driver's completed borrow. Source objects, source fields,
  // wait history, printer, entropy and UID owners are untouched.
  context_.dialogue = context_.talker = 0; context_.generation = 0;
  context_.programme = context_.original_npc = kRoomNoIndex;
  phase_ = Phase::Closed; e.clear(); return true;
}
} // namespace encore::ctr
