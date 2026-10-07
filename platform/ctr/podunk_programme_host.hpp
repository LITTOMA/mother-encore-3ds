#pragma once
#include "encore/dialogue_choices.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/field_programme.hpp"
#include "encore/field_psi.hpp"
#include "encore/field_scene_host.hpp"
#include <algorithm>
namespace encore::ctr {
// The platform owns genuine save/audio/UI endpoints. These closures are not
// proof of scene admission: activate checks actual source Ready and ObjectDB.
struct PodunkProgrammeOps {
  upstream::BasementFlagQuery flag;
  std::function<bool(uint64_t, std::string &, std::string &)> actual_path;
  std::function<bool(std::string_view, bool &, std::string &)> seen;
  std::function<bool(std::string_view, std::string &)> mark_seen;
  std::function<bool(std::string &)> admit_session_scene;
  std::function<bool(std::string_view, std::string &)> admit_audio, play_audio;
  std::function<bool(const upstream::FieldProgrammeText &, std::string &)>
      admit_text;
  // Original phrase reset runs once at each ShowDialogue source cursor,
  // before the shared printer accepts the phrase (including goto targets).
  std::function<bool(const upstream::FieldProgrammeText &, uint64_t,
                     std::string &)> prepare_text;
  // Actual ShowDialogue notification, after that same source printer accepts
  // the checked span. Preflight must never assign/play the phrase's stream.
  std::function<bool(const upstream::FieldProgrammeText &, uint64_t,
                     std::string &)>
      presented_text;
  std::function<bool(std::string &, std::string &)> player_name;
  std::function<bool(const upstream::DialogueAction &,
                     const upstream::FieldProgrammeContext &, std::string &)>
      admit_lifecycle, apply_lifecycle;
  std::function<bool(const upstream::FieldProgrammeData &, uint32_t,
                     const upstream::FieldProgrammeContext &, uint32_t,
                     uint64_t &, std::string &)>
      open_dialogue;
  std::function<bool(uint64_t, uint32_t, std::string &)> admit_dialogue_ready;
  std::function<bool(std::string &)> close_commands_for_telepathy;
  std::function<bool(const upstream::BasementKeyItem &, std::string &)>
      admit_key;
  upstream::BasementProgressionHost keys;
  upstream::FieldTelepathyHost telepathy;
};
class PodunkProgrammeHost final {
  const upstream::FieldProgrammeData *data_ = nullptr;
  const upstream::BasementProgressionData *basement_ = nullptr;
  const upstream::FieldNodeTreeData *tree_data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldSceneHost *scene_ = nullptr;
  upstream::FieldNpcRuntime *npc_ = nullptr;
  upstream::HouseView house_{};
  upstream::HousePresentation *presentation_ = nullptr;
  const upstream::DialogueChoicesData *choice_data_ = nullptr;
  upstream::DialogueChoices *choices_ = nullptr;
  upstream::FieldProgrammeRuntime programme_;
  upstream::FieldTelepathyRuntime telepathy_;
  PodunkProgrammeOps ops_{};
  bool prepared_ = false, active_ = false;
  bool source_cursor_owned_ = false;
  uint32_t choice_group_ = upstream::kRoomNoIndex, generation_ = 0;
  static bool fail(std::string &e, const char *m) {
    e = m;
    return false;
  }
  bool live(std::string &e) const {
    if (!active_ || !prepared_ || !tree_ || !scene_ || !scene_->scene_ready())
      return fail(e,
                  "Podunk programme scene is not completely admitted/active");
    const auto *root = tree_->state(tree_->root());
    if (!root || !root->alive || !root->inside || !root->ready_notified)
      return fail(e, "Podunk programme actual scene root Ready absent");
    return ops_.admit_session_scene(e);
  }
  const upstream::HouseDialogue *
  house_text(uint32_t id, upstream::HouseDialogue &out) const {
    const auto *t = data_->text(id);
    if (!t)
      return nullptr;
    for (uint32_t i = 0; i < house_.count(upstream::HouseSection::Dialogues);
         ++i) {
      auto h = house_.dialogue(i);
      if (h.id == id && house_.string(h.source_path) == t->source) {
        out = h;
        return &out;
      }
    }
    return nullptr;
  }
  bool choices(uint32_t index, uint32_t programme, uint32_t &group,
               std::string &e) const {
    const auto *c = data_->choice(index);
    const auto *r = data_->record(programme);
    if (!c || !r)
      return fail(e, "Podunk choice/source programme absent");
    for (uint32_t i = 0; i < choice_data_->groups().size(); ++i) {
      const auto &g = choice_data_->groups()[i];
      if (g.id != c->identity)
        continue;
      if (g.program_identity != c->program || g.source_label != c->label ||
          g.program_command_count != r->table.command_count ||
          g.initial_selection != c->initial_selection ||
          g.cancel_target_pc != c->cancel_target_pc ||
          g.options.size() != c->options.size())
        return fail(e, "Podunk original choice cross-pack schema mismatch");
      for (size_t j = 0; j < g.options.size(); ++j)
        if (g.options[j].translation_key != c->options[j].key ||
            g.options[j].target_pc != c->options[j].target_pc)
          return fail(e, "Podunk choice source targets differ");
      group = i;
      e.clear();
      return true;
    }
    return fail(e, "Podunk source choice not compiled into actual presenter");
  }
  bool admit_action(uint32_t p, const upstream::FieldProgrammeContext &ctx,
                    const upstream::DialogueAction &a, std::string &e) {
    if (!live(e))
      return false;
    using K = upstream::DialogueActionKind;
    switch (a.kind) {
    case K::ShowDialogue: {
      upstream::HouseDialogue h;
      const auto *t = data_->text(a.target_index);
      if (!t || !house_text(a.target_index, h) || h.segment_count == 0)
        return fail(e, "Podunk actual source text span missing");
      return ops_.admit_text(*t, e);
    }
    case K::PlaySound: {
      const auto *s = data_->sound(a.target_index);
      return s ? ops_.admit_audio(*s, e)
               : fail(e, "Podunk source audio absent");
    }
    case K::SetFlag: {
      const auto *s = data_->flag(a.target_index);
      bool value = false;
      return s ? ops_.flag(*s, value, e) : fail(e, "Podunk source flag absent");
    }
    case K::AwaitChoices: {
      uint32_t group = 0;
      if (!choices(a.target_index, p, group, e))
        return false;
      for (uint32_t i = 0; i < 3; ++i)
        if (!ops_.admit_audio(
                choice_data_->sound(upstream::DialogueChoiceSound(i)), e))
          return false;
      return true;
    }
    case K::GrantKeyItem: {
      const auto *k = basement_->key_item(a.target_index);
      return k ? ops_.admit_key(*k, e)
               : fail(e, "Podunk source key policy absent");
    }
    case K::AwaitDialogue:
    case K::Jump:
      return true;
    case K::BeginCutscene:
    case K::StopInteraction:
    case K::SetTalker:
    case K::CutsceneEnded:
    case K::DialogueDone:
      return ops_.admit_lifecycle(a, ctx, e);
    default:
      return fail(e, "Podunk programme action lacks source platform consumer");
    }
  }
  bool apply_action(uint32_t p, const upstream::FieldProgrammeContext &ctx,
                    const upstream::DialogueAction &a, std::string &e) {
    if (!live(e))
      return false;
    using K = upstream::DialogueActionKind;
    switch (a.kind) {
    case K::ShowDialogue: {
      upstream::HouseDialogue h;
      std::string name;
      const auto *text = data_->text(a.target_index);
      if (!text || !ctx.dialogue_object || !house_text(a.target_index, h))
        return fail(e, "Podunk actual presented text/dialogue owner absent");
      if (!ops_.player_name(name, e) ||
          !ops_.prepare_text(*text,ctx.dialogue_object,e))
        return false;
      if (!presentation_->present_story_dialogue(h.first_segment,
                                                 h.segment_count, name))
        return fail(e, presentation_->error());
      return ops_.presented_text(*text, ctx.dialogue_object, e);
    }
    case K::PlaySound: {
      const auto *s = data_->sound(a.target_index);
      return s ? ops_.play_audio(*s, e) : fail(e, "Podunk source audio absent");
    }
    case K::SetFlag: {
      const auto *s = data_->flag(a.target_index);
      return s ? scene_->set_flag(false, *s, a.value != 0, data_->flag_emit(),
                                  e)
               : fail(e, "Podunk source flag absent");
    }
    case K::AwaitChoices: {
      if (!choices(a.target_index, p, choice_group_, e))
        return false;
      const auto *r = data_->record(p);
      if (!r || !choices_->prepare(*choice_data_, choice_group_, r->path,
                                   r->table.command_count, e))
        return false;
      return true;
    }
    case K::BeginCutscene:
    case K::StopInteraction:
    case K::SetTalker:
    case K::CutsceneEnded:
    case K::DialogueDone:
      return ops_.apply_lifecycle(a, ctx, e);
    default:
      return fail(e, "Podunk source scheduler sent an unbound platform action");
    }
  }

public:
  bool prepare(const upstream::FieldProgrammeData &d,
               const upstream::BasementProgressionData &basement,
               const upstream::FieldPsiData &psi,
               const upstream::FieldNpcData &npc_data,
               upstream::FieldNpcRuntime &npc,
               const upstream::FieldNodeTreeData &tree_data,
               upstream::FieldNodeTreeRuntime &tree,
               upstream::FieldSceneHost &scene, upstream::HouseView house,
               upstream::HousePresentation &presentation,
               const upstream::DialogueChoicesData &choice_data,
               upstream::DialogueChoices &choice_runtime,
               PodunkProgrammeOps ops, std::string &e) {
    if (prepared_ || !d.valid() || !npc_data.valid() || !tree_data.valid() ||
        !house.valid() || !choice_data.valid() ||
        d.scene() != tree_data.source_scene() ||
        d.commit() != tree_data.identity().upstream_commit ||
        d.commit() != npc_data.source_pin())
      return fail(e, "Podunk source programme resources/owner mismatch");
    if (!ops.flag || !ops.actual_path || !ops.seen || !ops.mark_seen ||
        !ops.admit_session_scene || !ops.admit_key || !ops.admit_audio ||
        !ops.play_audio || !ops.admit_text || !ops.prepare_text || !ops.presented_text ||
        !ops.player_name ||
        !ops.admit_lifecycle || !ops.apply_lifecycle || !ops.open_dialogue ||
        !ops.admit_dialogue_ready || !ops.close_commands_for_telepathy)
      return fail(
          e,
          "Podunk real source session/audio/UI lifecycle endpoints incomplete");
    const auto *n = tree_data.record(d.npc().id);
    const auto *source_npc =
        static_cast<const upstream::FieldNpcDescriptor *>(nullptr);
    for (const auto &x : npc_data.npcs())
      if (x.id == d.npc().id)
        source_npc = &x;
    if (!n || !source_npc || n->path != d.npc().node ||
        source_npc->node != n->path ||
        source_npc->ready_ordinal != d.npc().ready_ordinal ||
        n->ready != source_npc->ready_ordinal ||
        source_npc->dialogues.size() != d.npc().rows.size())
      return fail(e, "Podunk actual NPC/Ready source binding differs");
    std::array<uint8_t, 32> source_sha{};
    if (!d.source_hash(d.scene(), source_sha) ||
        source_sha != tree_data.identity().source_sha256 ||
        !d.source_hash(n->script, source_sha) || source_sha != n->script_sha)
      return fail(e,
                  "Podunk programme actual scene/script source hashes differ");
    for (size_t i = 0; i < source_npc->dialogues.size(); ++i) {
      const auto &a = source_npc->dialogues[i];
      const auto &b = d.npc().rows[i];
      if (a.thoughts != b.thoughts || a.last != b.last || a.group != b.group ||
          a.ordinal != b.ordinal || a.flag != b.flag ||
          a.program != b.program || a.source != b.source)
        return fail(e, "Podunk actual NPC dialogue precedence differs");
    }
    data_ = &d;
    basement_ = &basement;
    tree_data_ = &tree_data;
    tree_ = &tree;
    scene_ = &scene;
    npc_ = &npc;
    house_ = house;
    presentation_ = &presentation;
    choice_data_ = &choice_data;
    choices_ = &choice_runtime;
    ops_ = std::move(ops);
    upstream::FieldProgrammeHost h;
    h.flag = ops_.flag;
    h.seen = ops_.seen;
    h.mark_seen = ops_.mark_seen;
    h.key_effects = ops_.keys;
    h.open_dialogue = ops_.open_dialogue;
    h.admit_dialogue_ready = ops_.admit_dialogue_ready;
    h.actor = [this](uint32_t id, uint64_t &object, std::string &path,
                     std::string &e) {
      if (!live(e) || id != data_->npc().id)
        return false;
      object = tree_->source_object(id);
      const auto *n = tree_->state(object);
      if (!n || !n->alive || !n->inside || !n->ready_notified)
        return fail(e, "Podunk actual NPC ObjectID/Ready absent");
      return ops_.actual_path(object, path, e);
    };
    h.admit = [this](const auto &, uint32_t p, const auto &ctx, const auto &a,
                     std::string &e) { return admit_action(p, ctx, a, e); };
    h.apply = [this](const auto &, uint32_t p, const auto &ctx, const auto &a,
                     std::string &e) { return apply_action(p, ctx, a, e); };
    ops_.telepathy.admit_target = [this](uint32_t id, std::string &e) {
      return admit_npc(id, true, e);
    };
    ops_.telepathy.target_telepathy = [this](uint32_t id, std::string &e) {
      if (!live(e))
        return false;
      if (!npc_->telepathy(id))
        return fail(e, npc_->error().c_str());
      return true;
    };
    if (!programme_.initialize(&d, &basement, std::move(h), e) ||
        !telepathy_.initialize(&psi, std::move(ops_.telepathy))) {
      reset();
      if (e.empty())
        e = "Podunk original Player Telepathy endpoints incomplete";
      return false;
    }
    prepared_ = true;
    e.clear();
    return true;
  }
  bool activate(std::string &e) {
    if (!prepared_ || active_ || !tree_ || tree_->lifecycle_pending())
      return fail(e, "Podunk programme candidate not prepared/already active");
    active_ = true;
    if (!live(e)) {
      active_ = false;
      return false;
    }
    const auto object = tree_->source_object(data_->npc().id);
    auto *state = tree_->state(object);
    if (!state || !state->inside || !state->ready_notified) {
      active_ = false;
      return fail(e, "Podunk actual source Mick Ready missing");
    }
    e.clear();
    return true;
  }
  void reset() {
    programme_.cancel();
    prepared_ = active_ = false;
    source_cursor_owned_ = false;
    data_ = nullptr;
    basement_ = nullptr;
    tree_data_ = nullptr;
    tree_ = nullptr;
    scene_ = nullptr;
    npc_ = nullptr;
    presentation_ = nullptr;
    choice_data_ = nullptr;
    choices_ = nullptr;
    house_ = {};
    ops_ = {};
    choice_group_ = upstream::kRoomNoIndex;
    generation_ = 0;
  }
  bool active() const { return active_; }
  bool admit_telepathy(std::string &e) {
    return live(e) && telepathy_.admit(e);
  }
  bool close_commands_for_telepathy(std::string &e) {
    return live(e) && ops_.close_commands_for_telepathy(e);
  }
  bool telepathy(std::string &e) { return live(e) && telepathy_.execute(e); }
  bool admit_npc(uint32_t id, bool thoughts, std::string &e) {
    return live(e) && programme_.admit_npc(id, thoughts, e);
  }
  bool open_selected_npc(uint32_t id, std::string_view program, bool thoughts,
                         uint32_t generation, std::string &e) {
    if (!live(e))
      return false;
    generation_ = generation;
    if (!programme_.start_selected_npc(id, program, thoughts, generation, e))
      return false;
    source_cursor_owned_ = false;
    return true;
  }
  bool dialogue_ready(uint64_t object, uint32_t generation, std::string &e) {
    return live(e) && programme_.dialogue_ready(object, generation, e);
  }
  // The actual global source idle bus calls these at its existing Room phases;
  // the platform does not tick a second synthetic dialogue timer.
  bool idle_begin(std::string &e) {
    if (!live(e))
      return false;
    if (!programme_.idle_begin())
      return fail(e, programme_.scheduler().error());
    return true;
  }
  bool idle_process(double dt, std::string &e) {
    if (!live(e))
      return false;
    if (!programme_.idle_process(dt))
      return fail(e, programme_.scheduler().error());
    return true;
  }
  bool text_completed(std::string &e) {
    if (!live(e))
      return false;
    if (programme_.scheduler().status() !=
        upstream::DialogueStatus::AwaitChoices)
      return true;
    if (!choices_->text_completed(e))
      return false;
    presentation_->set_choice_rows(choice_data_->trailing_blank_lines());
    return true;
  }
  bool input(const upstream::DialogueChoicesInput &input, double dt,
             std::string &e) {
    if (!live(e))
      return false;
    if (source_cursor_owned_ && programme_.scheduler().status() ==
                                    upstream::DialogueStatus::AwaitChoices &&
        (input.horizontal || input.vertical || dt != 0))
      return fail(e, "Podunk source Cursor owns direction and arrow clocks");
    if (programme_.scheduler().status() ==
        upstream::DialogueStatus::AwaitChoices) {
      if (choices_->phase() == upstream::DialogueChoicesPhase::WaitingText) {
        presentation_->input(input.confirm, input.cancel, true, input.confirm);
        return true;
      }
      if (!choices_->step(dt, input, e))
        return false;
      upstream::DialogueChoicesEvent event;
      while (choices_->poll_event(event)) {
        if (event.kind == upstream::DialogueChoicesEventKind::SoundRequested) {
          if (!ops_.play_audio(choice_data_->sound(event.sound), e))
            return false;
        } else {
          if (event.clear_dialogue)
            presentation_->clear_story_text();
          const auto *c = programme_.scheduler().status() ==
                                  upstream::DialogueStatus::AwaitChoices
                              ? choices_->group()
                              : nullptr;
          if (!c)
            return fail(e, "Podunk actual choice source result absent");
          auto it = std::find_if(
              c->options.begin(), c->options.end(),
              [&](const auto &o) { return o.target_pc == event.target_pc; });
          uint32_t selection =
              it == c->options.end() ? 0 : uint32_t(it - c->options.begin());
          choices_->close();
          if (!programme_.select_option(selection, event.cancelled,
                                        generation_))
            return fail(e, programme_.scheduler().error());
          if (event.sound_after_target &&
              !ops_.play_audio(choice_data_->sound(event.sound), e))
            return false;
        }
      }
      return true;
    }
    if (programme_.scheduler().status() ==
        upstream::DialogueStatus::AwaitDialogue) {
      presentation_->input(input.confirm, input.cancel, true, input.confirm);
      if (presentation_->take_dialogue_advance() &&
          !programme_.finish_dialogue())
        return fail(e, programme_.scheduler().error());
    }
    return true;
  }
  // Called from the original DialogueBox input owner only after its source
  // animation/WaitTimer/can-input gates. The real Cursor has already handled
  // direction and repeat; confirmation/cancel still use the existing checked
  // programme target and source post-target InputSound order.
  bool source_cursor_input(uint64_t actual_dialogue, uint32_t generation,
                           int32_t index, bool confirm, bool cancel,
                           std::string &e) {
    if (!live(e) || !actual_dialogue || !generation ||
        generation != generation_ ||
        programme_.context().dialogue_object != actual_dialogue ||
        programme_.scheduler().status() !=
            upstream::DialogueStatus::AwaitChoices ||
        !choices_ || !choices_->active())
      return fail(e, "Podunk stale/nonactive actual source Cursor input");
    if (!ops_.admit_dialogue_ready(actual_dialogue, generation, e) ||
        !choices_->source_cursor_selection(index, e))
      return false;
    source_cursor_owned_ = true;
    return input({0, 0, confirm, cancel}, 0, e);
  }
  const upstream::FieldProgrammeRuntime &programme() const {
    return programme_;
  }
};
} // namespace encore::ctr
