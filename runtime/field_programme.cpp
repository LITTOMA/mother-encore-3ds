#include "encore/field_programme.hpp"
namespace encore::upstream {
namespace {
DialogueAction action(const RoomCommand &c) {
  DialogueAction a;
  a.kind = DialogueActionKind(c.opcode);
  a.actor = c.actor_index;
  a.phrase = c.phrase;
  a.target_index = c.target_index;
  a.flags = c.flags;
  a.vector = c.vector;
  a.value = c.value;
  a.duration = c.duration;
  a.auxiliary_index = c.auxiliary_index;
  return a;
}
} // namespace
bool FieldProgrammeRuntime::initialize(const FieldProgrammeData *d,
                                       const BasementProgressionData *b,
                                       FieldProgrammeHost h, std::string &e) {
  if (!d || !d->valid() || !b || !b->valid() || !h.flag || !h.actor ||
      !h.seen || !h.mark_seen || !h.admit || !h.apply || !h.open_dialogue ||
      !h.admit_dialogue_ready) {
    e = "Field programme checked data/host absent";
    return false;
  }
  std::string pin;
  constexpr char hex[] = "0123456789abcdef";
  for (auto x : d->commit()) {
    pin += hex[x >> 4];
    pin += hex[x & 15];
  }
  if (!b->bind_reviewed_commit(pin, e) || d->scene() != b->mick_scene() ||
      d->npc().node != b->mick_node())
    return false;
  for (uint32_t i = 0; i < d->program_count(); ++i) {
    auto p = d->program(i);
    for (uint32_t j = 0; j < p.command_count; ++j) {
      auto c = d->command(p.first_command + j);
      if (c.opcode == uint16_t(DialogueActionKind::GrantKeyItem)) {
        auto key = d->key(c.target_index);
        auto policy = b->key_item(c.target_index);
        if (!key || !policy || key->source != policy->source ||
            key->doses != policy->doses || key->grant != policy->grant ||
            key->name_key != policy->name_key) {
          e = "Field programme key source/policy mismatch";
          return false;
        }
      }
    }
  }
  BasementProgressionConsumer effects;
  if (!effects.bind(*b, h.key_effects, e))
    return false;
  data_ = d;
  basement_ = b;
  host_ = std::move(h);
  key_effects_ = std::move(effects);
  scheduler_ = DialoguePlayer{};
  programme_ = pending_choice_ = kRoomNoIndex;
  context_ = {};
  awaiting_ready_ = false;
  source_generation_ = 0;
  error_.clear();
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::select_npc(uint32_t id, bool thoughts,
                                       uint32_t &out, std::string &seen_key,
                                       std::string &e) {
  if (!data_ || id != data_->npc().id) {
    e = "Field programme source NPC not admitted";
    return false;
  }
  uint64_t handle = 0;
  std::string path;
  if (!host_.actor(id, handle, path, e))
    return false;
  if (!handle || path.empty() || path.front() != '/') {
    e = "Field programme actual NPC object/path absent";
    return false;
  }
  const FieldProgrammeNpcRow *selected = nullptr;
  std::string selected_seen;
  uint32_t group = kRoomNoIndex;
  for (const auto &r : data_->npc().rows) {
    if (r.thoughts != thoughts)
      continue;
    bool enabled = r.flag.empty();
    if (!enabled && !host_.flag(r.flag, enabled, e))
      return false;
    if (!enabled)
      continue;
    if (group == r.group)
      continue;
    auto hash =
        path + ":" + r.flag + ":" + std::to_string(r.ordinal) + ":" + r.program;
    bool seen = false;
    if (!host_.seen(hash, seen, e))
      return false;
    if (!seen || r.last) {
      selected = &r;
      selected_seen = std::move(hash);
      group = r.group;
    }
  }
  if (!selected || !selected->supported ||
      !data_->find_program(selected->program, out)) {
    e = "Field programme selected source branch pending";
    return false;
  }
  seen_key = std::move(selected_seen);
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::admit_programme(uint32_t index,
                                            const FieldProgrammeContext &ctx,
                                            std::string &e) {
  if (!data_ || index >= data_->program_count() || !ctx.actor_object ||
      ctx.source_npc != data_->npc().id) {
    e = "Field programme context invalid";
    return false;
  }
  const auto p = data_->program(index);
  for (uint32_t j = 0; j < p.command_count; ++j) {
    const auto a = action(data_->command(p.first_command + j));
    if (a.kind == DialogueActionKind::GrantKeyItem) {
      const auto *k = basement_->key_item(a.target_index);
      if (!k || !host_.key_effects.validate_key_item ||
          !host_.key_effects.validate_key_item(*k, e))
        return false;
    }
    if (!host_.admit(*data_, index, ctx, a, e))
      return false;
  }
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::admit_npc(uint32_t id, bool thoughts,
                                      std::string &e) {
  uint32_t p = 0;
  std::string key, path;
  uint64_t object = 0;
  if (!select_npc(id, thoughts, p, key, e) || !host_.actor(id, object, path, e))
    return false;
  return admit_programme(p, {id, object, 0, thoughts}, e);
}
bool FieldProgrammeRuntime::start_npc(uint32_t id, bool thoughts,
                                      uint32_t generation, std::string &e) {
  if (scheduler_.active() || awaiting_ready_ || !generation) {
    e = "Field programme already active/generation invalid";
    return false;
  }
  uint32_t p = 0;
  std::string key, path;
  uint64_t object = 0;
  if (!select_npc(id, thoughts, p, key, e) || !host_.actor(id, object, path, e))
    return false;
  FieldProgrammeContext ctx{id, object, 0, thoughts};
  if (!admit_programme(p, ctx, e))
    return false;
  // Same live source target/path must still own the selection before
  // publishing.
  uint32_t confirm = 0;
  std::string confirm_key, confirm_path;
  uint64_t confirm_object = 0;
  if (!select_npc(id, thoughts, confirm, confirm_key, e) ||
      !host_.actor(id, confirm_object, confirm_path, e) || confirm != p ||
      confirm_key != key || confirm_object != object || confirm_path != path) {
    if (e.empty())
      e = "Field programme source selection changed during admission";
    return false;
  }
  if (!host_.mark_seen(key, e))
    return false;
  programme_ = p;
  context_ = ctx;
  pending_choice_ = kRoomNoIndex;
  scheduler_ = DialoguePlayer{};
  error_.clear();
  source_generation_ = generation;
  awaiting_ready_ = true;
  uint64_t dialogue_object = 0;
  if (!host_.open_dialogue(*data_, p, context_, generation, dialogue_object,
                           e) ||
      !dialogue_object) {
    awaiting_ready_ = false;
    if (e.empty())
      e = "Field programme actual dialogue object absent";
    return false;
  }
  context_.dialogue_object = dialogue_object;
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::start_selected_npc(uint32_t id,
                                               std::string_view source_program,
                                               bool thoughts,
                                               uint32_t generation,
                                               std::string &e) {
  if (!data_ || scheduler_.active() || awaiting_ready_ || !generation) {
    e = "Field selected programme owner busy/generation invalid";
    return false;
  }
  uint32_t p = 0;
  std::string seen_key, path;
  uint64_t object = 0;
  if (!select_npc(id, thoughts, p, seen_key, e) ||
      !host_.actor(id, object, path, e))
    return false;
  const auto *record = data_->record(p);
  if (!record || record->path != source_program) {
    e = "Field selected NPC source programme mismatch";
    return false;
  }
  bool already_seen = false;
  if (!host_.seen(seen_key, already_seen, e))
    return false;
  if (!already_seen) {
    e = "Field selected programme requires source NPC marked seen";
    return false;
  }
  FieldProgrammeContext ctx{id, object, 0, thoughts};
  if (!admit_programme(p, ctx, e))
    return false;
  programme_ = p;
  context_ = ctx;
  pending_choice_ = kRoomNoIndex;
  scheduler_ = DialoguePlayer{};
  error_.clear();
  source_generation_ = generation;
  awaiting_ready_ = true;
  uint64_t dialogue = 0;
  if (!host_.open_dialogue(*data_, p, context_, generation, dialogue, e) ||
      !dialogue) {
    awaiting_ready_ = false;
    if (e.empty())
      e = "Field selected actual dialogue object absent";
    return false;
  }
  context_.dialogue_object = dialogue;
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::dialogue_ready(uint64_t object, uint32_t generation,
                                           std::string &e) {
  if (!data_ || !awaiting_ready_ || !object ||
      object != context_.dialogue_object || generation != source_generation_) {
    e = "Field programme stale/invalid actual dialogue Ready";
    return false;
  }
  if (!host_.admit_dialogue_ready(object, generation, e))
    return false;
  awaiting_ready_ = false;
  if (!scheduler_.start(*data_, programme_, *this, generation)) {
    e = error_.empty() ? scheduler_.error() : error_;
    return false;
  }
  e.clear();
  return true;
}
bool FieldProgrammeRuntime::apply(const DialogueAction &a) {
  if (a.kind == DialogueActionKind::GrantKeyItem)
    return key_effects_.grant_key_item(a.target_index, error_);
  if (a.kind == DialogueActionKind::AwaitChoices)
    pending_choice_ = a.target_index;
  return host_.apply(*data_, programme_, context_, a, error_);
}
bool FieldProgrammeRuntime::finish_dialogue(bool automatic) {
  return scheduler_.dialogue_finished(*this, automatic);
}
bool FieldProgrammeRuntime::select_option(uint32_t selection, bool cancel,
                                          uint32_t generation) {
  if (!data_ || scheduler_.status() != DialogueStatus::AwaitChoices)
    return false;
  const auto *c = data_->choice(pending_choice_);
  if (!c || (!cancel && selection >= c->options.size()))
    return false;
  return scheduler_.choices_selected(cancel ? c->cancel_target_pc
                                            : c->options[selection].target_pc,
                                     generation, *this);
}
bool FieldProgrammeRuntime::idle_begin() {
  return scheduler_.idle_begin(*this);
}
bool FieldProgrammeRuntime::idle_process(double dt) {
  return scheduler_.idle_process(dt, *this);
}
} // namespace encore::upstream
